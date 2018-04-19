/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */

/*
 * Copyright (C) 1997 by the University of Southern California
 * $Id: classifier-mpath.cc,v 1.10 2005/08/25 18:58:01 johnh Exp $
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * The copyright of this module includes the following
 * linking-with-specific-other-licenses addition:
 *
 * In addition, as a special exception, the copyright holders of
 * this module give you permission to combine (via static or
 * dynamic linking) this module with free software programs or
 * libraries that are released under the GNU LGPL and with code
 * included in the standard release of ns-2 under the Apache 2.0
 * license or under otherwise-compatible licenses with advertising
 * requirements (or modified versions of such code, with unchanged
 * license).  You may copy and distribute such a system following the
 * terms of the GNU GPL for this module and the licenses of the
 * other code concerned, provided that you include the source code of
 * that other code when and as the GNU GPL requires distribution of
 * source code.
 *
 * Note that people who make modified versions of this module
 * are not obligated to grant this special exception for their
 * modified versions; it is their choice whether to do so.  The GNU
 * General Public License gives permission to release a modified
 * version without this exception; this exception also makes it
 * possible to release a modified version which carries forward this
 * exception.
 *
 */


#include "ip.h"
#include <math.h>
#include <time.h>  
#include <vector>
#include <map>
#include <vector>
#include "classifier.h"
#include "node.h"
#include "conga.h"


int Conga::conga_enabled()
{
	if (n_)
		return n_->conga_enabled();
	return -1;
}

// @function route 
// 1.choose a output port for a packet
// 2.enable and set congaRow flag
// 3.set response route information
int Conga::route(Packet* p, Classifier* c_) 
{
	fprintf(stderr,"[conga] %lf-Node-%d get in route()\n",Scheduler::instance().clock(),n_->nodeid() );
	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);
	
	int dst = serv2leaf(iph->dst_.addr_);
	int r_ = -1, fee_ = 0x7fffffff;

	vector < int > tmp;
	map < int,int* >::iterator it = route_table_.find(dst);
	if (it != route_table_.end())
	{
		for (int i = 0; i < slots_; ++i)
		{
			if (route_table_[dst][i] < fee_) {
				
				tmp.clear();
				tmp.push_back(i);
				r_ = i;
				fee_ = route_table_[dst][i];
			} else if (route_table_[dst][i] == fee_) {
				tmp.push_back(i);
			}
		}

		r_ = tmp[rand() % tmp.size()];


	}
		fprintf(stderr,"[conga] %lf-Node-%d tmp.size=%d route=%d slots=%d  getting out route() \n"
			,Scheduler::instance().clock(),n_->nodeid(),tmp.size(),r_,slots_);
	if (r_ == -1)
		r_ = rand() % slots_;
		fprintf(stderr,"[conga] %lf-Node-%d route=%d slots=%d really out route() \n",Scheduler::instance().clock(),n_->nodeid(),r_,slots_ );


	// 2.enable and set congaRow flag
	cmnh->congaRouteRow = {
		.en_flag = 1,
		.srcLeaf = iph->src_.addr_,
		.portChose = r_,
		.congDegree = 0
	};



	// 3.set response route information
	int rnd = rand();
	
	map < int, map < int,int > >::iterator it_ = response_table_.find(dst);
	if (it_ == response_table_.end()) {
		cmnh->congaResponseRow.en_flag = 0;
	} else {
		rnd = rnd % response_table_[dst].size();
		map< int,int >::iterator iter = response_table_[dst].begin();
	    while(rnd--) {
	        iter++;
	    }

	
		cmnh->congaResponseRow = {
			.en_flag = 1,
			.srcLeaf = iph->src_.addr_,
			.portChose = iter->first,
			.congDegree = iter->second
		};
	}


 
	return r_;

}


// @function recv
// @variable $cls just for debug 
// process a packet with conga flag,
// 1.handle with route congaRow : record in responseTable 
// 2.handle with response congaRow : update in routeTable
// 3.remove the flag from the packet header
void Conga::recv(Packet* p, Classifier* c_)
{
		fprintf(stderr,"[conga] %lf-Node-%d get in route()\n",Scheduler::instance().clock(),n_->nodeid() );
	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);
	struct CongaRow* route_row = &cmnh->congaRouteRow;
	struct CongaRow* response_row = &cmnh->congaResponseRow;

	// FILE* fpResult=fopen("All-Packet-Debug.tr","a+");
	// if(fpResult==NULL)
 //    {
 //        fprintf(stderr,"Can't open file %s!\n","debug.tr");
 //    	// return(TCL_ERROR);
 //    } else {
	// 	fprintf(fpResult, "%d %lf-Node-%d-(%d->%d):flowid=%d size=%d maxslot_=%d\n"
	// 	,conga_enabled(),Scheduler::instance().clock(),n_->nodeid(),iph->src_,iph->dst_,cmnh->flowID,cmnh->size_,c_->maxslot());
	// 	fclose(fpResult);
	// }

	// effectively
	if (route_row->en_flag)
	{
		int src = serv2leaf(iph->src_.addr_),
			port = route_row->portChose,
			degree = route_row->congDegree;

		map< int, map < int, int > >::iterator it = response_table_.find(src);
		if (it == response_table_.end())
		{
			// response_table_[src] = new map < int, int >();
			map<int,int> tmp;
			response_table_.insert( map< int, map<int,int> > :: value_type(src, tmp) );
		}
		response_table_[src][port] = degree;

		route_row->en_flag = 0;
	}

	if (response_row->en_flag)
	{
		int dst = serv2leaf(iph->src_.addr_),
			port = response_row->portChose,
			degree = response_row->congDegree;

		if (port >= slots_)
			fprintf(stderr,"%lf-Node-%d-(%d->%d):Receive A Conga Packet With Larger Port No.%d! [flowid=%d size=%d] \n\n"
			,Scheduler::instance().clock(),n_->nodeid(),iph->src_,iph->dst_,port,cmnh->flowID,cmnh->size_);

		map < int, int* >::iterator it = route_table_.find(dst);
		if (it == route_table_.end())
		{
			route_table_[dst] = new int[slots_]();
		}
		route_table_[dst][port] = degree;

		response_row->en_flag = 0;
	}
}


int Conga::serv2leaf(int server) {
	if (leafDownPortNumber_ > 0)
		return int(server/leafDownPortNumber_);

	//why do u hard code this!
	else
		return int(server/40);
}


// just for debug
void Conga::packetPrint(Packet* p,Classifier* c,char* file_str,char* debug_str="")
{
	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);
	struct CongaRow* route_row = &cmnh->congaRouteRow;
	struct CongaRow* response_row = &cmnh->congaResponseRow;

	FILE* fpResult=fopen(file_str,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n",file_str);
    	// return(TCL_ERROR);
    } else {
		fprintf(fpResult, "[conga] %lf-Node-%d-(%d->%d): route:%d %d %d %d reponse:%d %d %d %d [%s]\n"
			,Scheduler::instance().clock(),n_->nodeid(),iph->src_,iph->dst_
			,route_row->en_flag,route_row->srcLeaf,route_row->portChose,route_row->congDegree
			,response_row->en_flag,response_row->srcLeaf,response_row->portChose,response_row->congDegree
			,debug_str
		);
		fclose(fpResult);
	}
}