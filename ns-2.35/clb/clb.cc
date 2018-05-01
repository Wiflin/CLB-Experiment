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
#include "queue.h"
#include <cstdlib>
#include <string>
#include <sys/stat.h> 
#include "clb.h"
#include "clb-processor.h"





CLB::CLB(Node* n, Classifier* c) : n_(n), c_(c)
{
	mkdir("CLB",0777);
	// system("exec rm -r -f CLB/*");

	char str1[128];
	sprintf(str1,"[Constructor] %p",this);
	debug(str1);
}

void CLB::recv(Packet* p, Handler*h)
{
	int addr = n_->address();
	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);

	if (iph->dst_.addr_ == addr)
	{
		recv_proc(p, h);
	}
	else if (iph->src_.addr_ == addr)
	{
		send_proc(p, h);
	}
}

void CLB::recv_proc(Packet* p, Handler*h)
{
	CLBProcessor* np = get_node_processor(HDR_IP(p)->src_.addr_);

	np->recv(p, h);

	// flow_debug(p, h, "[recv packet]");
}

void CLB::send_proc(Packet* p, Handler*h)
{
	CLBProcessor* np = get_node_processor(HDR_IP(p)->dst_.addr_);

	np->send(p, h);

	// flow_debug(p, h, "[send packet]");	
}


CLBProcessor* CLB::get_node_processor(int address)
{
	map < int,CLBProcessor* >::iterator it = node_table.find(address);

	if (it != node_table.end())
		return it->second;

	CLBProcessor* np = new CLBProcessor(n_, c_, this, n_->address(), address);
	// @todo do some setting

	node_table[address] = np;
	return np;
}

void CLB::flow_debug(Packet* p, Handler*h, char* str)
{
	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);

	char str1[128];
	memset(str1,0,128*sizeof(char));
	int addr = n_->address();
	if (iph->dst_.addr_ == addr)
	{
		sprintf(str1,"CLB/processor_%d-%d.tr",addr,iph->src_.addr_);
	}
	else if (iph->src_.addr_ == addr)
	{
		sprintf(str1,"CLB/processor_%d-%d.tr",addr,iph->dst_.addr_);
	}

	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n","CLB/route_debug.tr");
    	// return(TCL_ERROR);
    } else {

		fprintf(fpResult, "%lf-Node-%d-(%d->%d):flow=%d\tpacket=%d\tsize=%d\trecord=%d\tresponse=%d\tntsize=%d\t%s\n",
			Scheduler::instance().clock(),n_->nodeid(),iph->src_,iph->dst_,cmnh->flowID,p->uid(),cmnh->size_,
			cmnh->clb_row.record_en,cmnh->clb_row.response_en,node_table.size(),str);		
		fclose(fpResult);
	}
}


void CLB::debug(char* str, char* file)
{
	// hdr_ip* iph = hdr_ip::access(p);
	// hdr_cmn* cmnh = hdr_cmn::access(p);

	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"CLB/%s.tr", file);
	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n", str1);
    	// return(TCL_ERROR);
    } else {

		fprintf(fpResult, "%s\n", str);		
		fclose(fpResult);
	}
}