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

#include <iostream>
#include <math.h>
#include <time.h>  
#include <math.h>
#include <vector>
#include <map>
#include <vector>
#include <queue>
#include <cstdlib>
#include <string>
#include <sstream>
#include <sys/stat.h> 
#include "classifier.h"
#include "node.h"
#include "queue.h"
#include "ip.h"
#include "clb-processor.h"

// Virtual Path Module enabled/disabled
#define VP_Module 1
// Congestion Aware Module enabled/disabled
#define CA_Module 1
// Eraser Code Module enabled/disabled
#define EC_Module 0

#define T_REFRESH  	((double)5E-4)
#define T_VP_EXPIRE	((double)0.5)
#define RATE_ALPHA 	((double)0.30)
#define FLY_ALPHA	((double)0.30)
#define VP_SIZE		((int)5)	


void CLBProcessorTimer::expire(Event *e)
{
	a_->expire(e);
}


CLBProcessor::CLBProcessor(Node* node, Classifier* classifier, CLB* clb, int src, int dst)
 : n_(node), c_(classifier), clb_(clb), pt_(this), src_(src), dst_(dst), flag(0), cost_flag(0), hashkey_counter(0)
{
	// srand(time(NULL));

	sequence = 0;

	init_ca_record(&global_ca);

	init_ca_response(&global_response);

	pt_.sched(T_REFRESH);

	stringstream dir; 
	dir << "CLB/" << src_;
	mkdir(dir.str().c_str(),0777);

	char str1[128];
	sprintf(str1,"[constructor] global_ca=%p\tglobal_response=%p",&global_ca,&global_response);
	flow_debug(str1,"Record");

}



int CLBProcessor::recv(Packet* p, Handler*h)
{


	if ((VP_Module | CA_Module) == 0)
		return 0;

	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);

	// make no sense
	if (cmnh->clb_row.record_en != 1 && 
		cmnh->clb_row.response_en != 1 )
		return 0;

	// if only VP_Module is enabled, the sender will do nothing
	if (VP_Module == 1)
		;

	// if only CA_Module is enabled, the sender will 
	// 1. update the global_ca_row as a sender
	// 2. update the global_ca_response as a receiver
	// @cautious!	it make no sense when both module are enabled!
	if (CA_Module == 1)
	{
		// flow_debug("\n");
		// work as a sender -> update sender-record according feedback vp_rcnt
		if (cmnh->clb_row.response_en == 1)
			ca_record_recv(&global_ca, cmnh->clb_row.vp_rcnt);
		// work as a receiver -> update (hashkey,recv-cnt) tuple according what?
		update_ca_response(&global_response);
	}


	if ((VP_Module & CA_Module) == 1)
	{
		// work as a sender -> update sender-record according feedback vp_rcnt
		// if (cmnh->clb_row.response_en == 1 && cmnh->clb_row.vp_rid != 0)
		if (cmnh->clb_row.response_en == 1)
		{
			struct vp_record* vp = vp_get(cmnh->clb_row.vp_rid);

			if (vp == 0)
				fprintf(stderr, "[clb-processor recv] couldn't get vp_record instance!\n");
			
			else {
				assert(vp->hashkey == cmnh->clb_row.vp_rid);

				char str1[228];
				int recv_undefined = cmnh->clb_row.vp_rcnt - vp->ca_row.recv_cnt;
				sprintf(str1,"[%lf recv]\thashkey=%6u\toddrecv=%u\tcurrecv=%u\trecv*=%d",Scheduler::instance().clock(),
					vp->hashkey,vp->ca_row.recv_cnt,cmnh->clb_row.vp_rcnt,recv_undefined);
				flow_debug(str1,"Recv");
				

				ca_record_recv(&vp->ca_row, cmnh->clb_row.vp_rcnt);
			}
		}


		// work as a receiver -> update (hashkey,recv-cnt) tuple according what?
		// assert(cmnh->clb_row.vp_id != 0);
		// fprintf(stderr, "vpid = %d\n", cmnh->clb_row.vp_id);
		struct ca_response* response = ca_get(cmnh->clb_row.vp_id);

		if (response == 0)
			response = ca_alloc(cmnh->clb_row.vp_id);

		if (response)
			update_ca_response(response);
		else 
			fprintf(stderr, "[clb-processor recv] couldn't get ca_response instance!\n");
	}


	return 0;
}


int CLBProcessor::send(Packet* p, Handler*h)
{
	// flow_debug(p);

	vpcost_debug();
	vpSendCnt_debug();
	vpRecvCnt_debug();
	vpSendNew_debug();
	vpRecvNew_debug();
	vpFlying_debug();
	vpRate_debug();

	if ((VP_Module | CA_Module) == 0)
		return 0;


	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);


	unsigned	clb_hashkey = 0;
	unsigned	clb_sequence = 0;
	unsigned	clb_recv_vpid = 0;
	unsigned	clb_recv_cnt = 0;
	struct vp_record*	vp = NULL;
	struct ca_response*	ca = NULL;

	// only virtual path enbled
	if (VP_Module == 1)
	{
		vp = vp_next();
		clb_hashkey = vp->hashkey;
	}

	// only congestion module enabled
	if (CA_Module == 1)
	{
		// send
		sequence += 1;
		ca_record_send(&global_ca);
		clb_sequence = sequence;
		// response
		clb_recv_cnt = global_response.recv_cnt;
	}

	// both are enabled
	if ((VP_Module & CA_Module) == 1)
	{
		ca_record_send(&vp->ca_row);
		// response
		if ((ca = ca_next()) != NULL)
		{
			clb_recv_vpid = ca->hashkey;
			clb_recv_cnt = ca->recv_cnt;
		}
	}

	cmnh->ecmpHashKey = clb_hashkey;
	cmnh->clb_row.vp_id = clb_hashkey;	
	cmnh->clb_row.SN_HSN = clb_sequence;
	cmnh->clb_row.record_en = 1;	// make no sense

	cmnh->clb_row.vp_rid = clb_recv_vpid;
	cmnh->clb_row.vp_rcnt = clb_recv_cnt;
	cmnh->clb_row.response_en = 1;	// make no sense

	double fly = vp->ca_row.flying + vp->ca_row.send_undefined - vp->ca_row.recv_undefined;
	double cost_ = cost(&vp->ca_row);
	char str1[128];
	sprintf(str1,"[%lf send]\thashkey=%6u\t flying=%lf\t   rate=%4.2lf\t   cost=%4.2lf\t\t\t\t\tsend*=%u\trecv*=%d",Scheduler::instance().clock(),
		clb_hashkey,fly,vp->ca_row.rate,cost_,vp->ca_row.send_undefined,vp->ca_row.recv_undefined);
	flow_debug(str1,"Send");

	return 0;
}

void CLBProcessor::expire(Event *e)
{	
	flow_debug("\n[clb-processor expire]\n","Send");
	flow_debug("\n[clb-processor expire]\n","Recv");

	// vpcost_debug(1);
	// vpsend_debug();
	// 1. foreach : update table row
	update_ca_record(&global_ca);

	double now = Scheduler::instance().clock();
	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

	while (it != vp_map.end())
	{
		if (now - it->second->ca_row.fresh_time > T_VP_EXPIRE)
		{
			unsigned hashkey = it->first;
			++it;
			vp_free(hashkey);
			continue;
		}

		update_ca_record(&it->second->ca_row);
		++it;
	}

	vpt_debug();

	// 2. resched()
	pt_.resched(T_REFRESH);
}

struct vp_record* CLBProcessor::vp_alloc()
{
	// unsigned hashkey = rand() % 65535;

	// while (vp_map.find(hashkey) != vp_map.end())
	// 	hashkey = rand() % 65535;

	unsigned hashkey = hashkey_counter ++;

	struct vp_record*	vp = new struct vp_record();

	init_vp_record(vp);
	vp->hashkey = hashkey;

	vp_map[hashkey] = vp;


	char str1[128];
	sprintf(str1,"[vp_alloc]\t%lf\t%8d\t%p\t%lf",Scheduler::instance().clock(),hashkey,vp,cost(&vp->ca_row));
	flow_debug(str1,"Record");


	return vp;
}


void CLBProcessor::vp_free(unsigned hashkey) 
{
	char str1[128];
	sprintf(str1,"[vp_free]\t%lf\t%8d",Scheduler::instance().clock(),hashkey);
	flow_debug(str1,"Record");


	map < unsigned, struct vp_record* > :: iterator it = vp_map.find(hashkey);
	
	if (it == vp_map.end())
		return;

	struct vp_record* vp = it->second;
	delete vp;
	vp = NULL;
	vp_map.erase(it);
}

struct vp_record* CLBProcessor::vp_next()
{
	if (vp_map.size() < VP_SIZE)
		return vp_alloc();

	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

	double cost_min = 1E+37;
	struct vp_record* vp = NULL;
	int vp_cnt = 0;

	for ( ; it != vp_map.end(); ++it)
	{
		if ( 0 == it->second->ca_row.valid )
			continue;

		vp_cnt += 1;

		struct vp_record* vp_tmp = it->second;
		double vp_cost = cost(&vp_tmp->ca_row);

		if (vp_cost < cost_min) 
		{
			cost_min = vp_cost;
			vp = vp_tmp;
		}
	} 

	// char str1[128];
	// sprintf(str1,"[clb-processor vp_next] %lf\thashkey=%d vpp=%p",Scheduler::instance().clock(),vp->hashkey,vp);
	// flow_debug(str1);

	if (vp != 0)
		return vp;

	
	fprintf(stderr, "[clb-processor vp_next] Could not find a vp_record! cost=%lf\tcnt=%d\tmap_size=%d\n",	
		cost_min, vp_cnt, vp_map.size());

	return NULL;
}

struct vp_record* CLBProcessor::vp_get(unsigned hashkey)
{
	map < unsigned, struct vp_record* > :: iterator it = vp_map.find(hashkey);

	if (it == vp_map.end())
		return NULL;

	return it->second;
}


struct ca_response* CLBProcessor::ca_alloc(unsigned hashkey) 
{
	struct ca_response* ca = new struct ca_response();

	init_ca_response(ca);
	ca->hashkey = hashkey;

	ca_map[hashkey] = ca;
	ca_queue.push(hashkey);

	return ca;
}

void CLBProcessor::ca_free(unsigned hashkey) 
{

	map < unsigned, struct ca_response* > :: iterator it = ca_map.find(hashkey);

	if (it == ca_map.end())
		return;

	struct ca_response* ca = it->second;
	delete ca;
	ca = NULL;
	ca_map.erase(it);
}


struct ca_response* CLBProcessor::ca_next()
{
	unsigned t = 0;
	while (!ca_queue.empty())
	{
		t = ca_queue.front();
		ca_queue.pop();

		if (ca_map.count(t) == 0)
			continue;

		if (Scheduler::instance().clock() - ca_map[t]->time > T_VP_EXPIRE)
		{
			ca_free(t);
			continue;
		}

		ca_queue.push(t);
		return ca_map[t];
	} 
	return NULL;
}

struct ca_response*	CLBProcessor::ca_get(unsigned hashkey)
{
	map < unsigned, struct ca_response* > :: iterator it = ca_map.find(hashkey);
	return ( it == ca_map.end() ? NULL : it->second );
}



double CLBProcessor::cost(struct ca_record* ca)
{
	// @todo how to calculate it

	// if (fabs(ca->rate) < 1E-2)	
		// calculate the average cost
	// double fly = ca->flying + ca->send_undefined - ca->recv_undefined;
	double fly = ca->flying + ca->send_undefined;
	double cost_ = (double)(fly) / ca->rate;
	return cost_;
}



// send variable incr
void CLBProcessor::ca_record_send(struct ca_record* ca)
{
	// 1.update send_undefined
	unsigned send_undefined = ca->send_undefined + 1;
	// 2.update fresh time
	ca->send_undefined = send_undefined;
	ca->fresh_time = Scheduler::instance().clock();

}

void CLBProcessor::ca_record_recv(struct ca_record* ca, unsigned current_recv)
{
	int recv_undefined = current_recv - ca->recv_cnt;

	ca->recv_undefined = recv_undefined;
	ca->fresh_time = Scheduler::instance().clock();
}



void CLBProcessor::update_ca_record(struct ca_record* ca_row)
{
	
	double now = Scheduler::instance().clock();
	double delta_time = now - ca_row->update_time;

	// 1. calculate new rate
	double old_rate = ca_row->rate;
	double calculate_rate = (double) ca_row->recv_undefined / delta_time;
	// double calculate_rate = (double) ca_row->recv_undefined;
	double new_rate = (1 - RATE_ALPHA) * old_rate + RATE_ALPHA * calculate_rate;
	ca_row->rate = new_rate;

	// 2. calculate new flying 
	double old_fly = ca_row->flying;
	double delta_fly = (double) ca_row->send_undefined - ca_row->recv_undefined;
	double new_fly = (1 - FLY_ALPHA) * old_fly + FLY_ALPHA * delta_fly;
	ca_row->flying = new_fly; 

	// 3. calculate new recv & new send
	ca_row->send_cnt += ca_row->send_undefined;
	ca_row->recv_cnt += ca_row->recv_undefined;

	// 4. reset send-undefine & recv-undefine
	ca_row->send_undefined = 0;
	ca_row->recv_undefined = 0;

	// 6. update timer
	ca_row->update_time = Scheduler::instance().clock();

}


// when the processor recv a packet -> should update the ca_response as a receiver
void CLBProcessor::update_ca_response(struct ca_response* response)
{
	response->recv_cnt += 1;
	response->time = Scheduler::instance().clock();

	// char str1[128];
	// sprintf(str1,"[clb-processor update_ca_response] %lf\t%p\trecv-cnt=%d",
	// 	Scheduler::instance().clock(),response,response->recv_cnt);
	// flow_debug(str1);

}




// void CLBProcessor::init_vp_record(struct vp_record* vp)
// {
// 	vp->hashkey = rand();

// 	init_ca_record(&vp->ca_row);
// }

// void CLBProcessor::init_ca_record(struct ca_record* ca_row)
// {
// 	ca_row->send_cnt = 0;
// 	ca_row->recv_cnt = 0;
// 	ca_row->send_undefined = 0;
// 	ca_row->recv_undefined = 0;

// 	// maybe change to current average of rate
// 	ca_row->rate = 0;
// 	ca_row->flying = 0;

// 	ca_row->update_time = Scheduler::instance().clock();
// 	ca_row->fresh_time = Scheduler::instance().clock();
// 	// ca_row->last_update_time = Scheduler::instance().clock();
// 	ca_row->valid = 0;
// 	ca_row->pending = 0;
// }


// void CLBProcessor::init_ca_response(struct ca_response* ca)
// {
// 	ca->hashkey = 0;
// 	ca->recv_cnt = 0;
// 	ca->time = 0;
// }



void CLBProcessor::flow_debug(char* str, char* file, Packet* p)
{
	// hdr_ip* iph = hdr_ip::access(p);
	// hdr_cmn* cmnh = hdr_cmn::access(p);

	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"CLB/%d/%s-%d.tr", src_, file, dst_);
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


void CLBProcessor::vpt_debug()
{
	// hdr_ip* iph = hdr_ip::access(p);
	// hdr_cmn* cmnh = hdr_cmn::access(p);


	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"CLB/%d/VPT-%d.tr",src_,dst_);
	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n","CLB/route_debug.tr");
        return;
    	// return(TCL_ERROR);
    } 




    map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

    if ( ! flag )
    {
    	for ( ; it != vp_map.end(); ++it)
    	{
    		fprintf(fpResult, "%u ", it->second->hashkey );
    	}

    	it = vp_map.begin();
		fprintf(fpResult, "\n");

		flag = 1;

    }


    for ( ; it != vp_map.end(); ++it)
    {
    	// fprintf(fpResult, "%lf\t%u\t%lf\t%lf\n", Scheduler::instance().clock(), 
    	// 	it->second->hashkey, it->second->ca_row.rate, it->second->ca_row.flying);
    	fprintf(fpResult, "%lf ",it->second->ca_row.rate);
    }
    fprintf(fpResult, "\n");
	fclose(fpResult);

}



void CLBProcessor::vpsend_debug()
{
	// hdr_ip* iph = hdr_ip::access(p);
	// hdr_cmn* cmnh = hdr_cmn::access(p);

	

	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"CLB/%d/VPSend-%d.tr",src_,dst_);
	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n",str1);
        return;
    	// return(TCL_ERROR);
    } 




    map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

    if ( ! flag )
    {
    	for ( ; it != vp_map.end(); ++it)
    	{
    		fprintf(fpResult, "%u ", it->second->hashkey );
    	}

    	it = vp_map.begin();
		fprintf(fpResult, "\n");

		flag = 1;

    }


    for ( ; it != vp_map.end(); ++it)
    {
    	// fprintf(fpResult, "%lf\t%u\t%lf\t%lf\n", Scheduler::instance().clock(), 
    	// 	it->second->hashkey, it->second->ca_row.rate, it->second->ca_row.flying);
    	fprintf(fpResult, "%u ",it->second->ca_row.send_undefined);
    }
    fprintf(fpResult, "\n");
	fclose(fpResult);

}


void CLBProcessor::vpcost_debug(int expire)
{
	// hdr_ip* iph = hdr_ip::access(p);
	// hdr_cmn* cmnh = hdr_cmn::access(p);

	

	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"CLB/%d/VPCost-%d.tr",src_,dst_);
	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n",str1);
        return;
    	// return(TCL_ERROR);
    } 




    map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

    if ( ! cost_flag )
    {
    	for ( ; it != vp_map.end(); ++it)
    	{
    		fprintf(fpResult, "%u ", it->second->hashkey );
    	}

    	it = vp_map.begin();
		fprintf(fpResult, "\n");

		cost_flag = 1;

    }

    fprintf(fpResult, "%lf ",Scheduler::instance().clock());

    // if (expire) {
    // 	fprintf(fpResult, "expire\n");
    // }

    for ( ; it != vp_map.end(); ++it)
    {
    	// fprintf(fpResult, "%lf\t%u\t%lf\t%lf\n", Scheduler::instance().clock(), 
    	// 	it->second->hashkey, it->second->ca_row.rate, it->second->ca_row.flying);
    	fprintf(fpResult, "%.11lf ",cost(&it->second->ca_row));
    }
    fprintf(fpResult, "\n");
	fclose(fpResult);

}



void CLBProcessor::vpSendCnt_debug()
{
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"CLB/%d/VPSendCnt-%d.tr",src_,dst_);
	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n",str1);
        return;
    	// return(TCL_ERROR);
    } 
	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();
    fprintf(fpResult, "%lf ",Scheduler::instance().clock());

    for ( ; it != vp_map.end(); ++it)
    {
    	fprintf(fpResult, "%u ",it->second->ca_row.send_cnt);
    }
    fprintf(fpResult, "\n");
	fclose(fpResult);
}


void CLBProcessor::vpRecvCnt_debug()
{
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"CLB/%d/VPRecvCnt-%d.tr",src_,dst_);
	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n",str1);
        return;
    	// return(TCL_ERROR);
    } 
	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();
    fprintf(fpResult, "%lf ",Scheduler::instance().clock());

    for ( ; it != vp_map.end(); ++it)
    {
    	fprintf(fpResult, "%u ",it->second->ca_row.recv_cnt);
    }
    fprintf(fpResult, "\n");
	fclose(fpResult);
}

void CLBProcessor::vpSendNew_debug()
{
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"CLB/%d/VPSend*-%d.tr",src_,dst_);
	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n",str1);
        return;
    	// return(TCL_ERROR);
    } 
	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();
    fprintf(fpResult, "%lf ",Scheduler::instance().clock());

    for ( ; it != vp_map.end(); ++it)
    {
    	fprintf(fpResult, "%u ",it->second->ca_row.send_undefined);
    }
    fprintf(fpResult, "\n");
	fclose(fpResult);
}


void CLBProcessor::vpRecvNew_debug()
{
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"CLB/%d/VPRecv*-%d.tr",src_,dst_);
	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n",str1);
        return;
    	// return(TCL_ERROR);
    } 
	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();
    fprintf(fpResult, "%lf ",Scheduler::instance().clock());

    for ( ; it != vp_map.end(); ++it)
    {
    	fprintf(fpResult, "%d ",it->second->ca_row.recv_undefined);
    }
    fprintf(fpResult, "\n");
	fclose(fpResult);
}


void CLBProcessor::vpFlying_debug()
{
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"CLB/%d/VPFlying-%d.tr",src_,dst_);
	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n",str1);
        return;
    	// return(TCL_ERROR);
    } 
	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();
    fprintf(fpResult, "%lf ",Scheduler::instance().clock());

    for ( ; it != vp_map.end(); ++it)
    {
    	fprintf(fpResult, "%.11lf ",it->second->ca_row.flying);
    }
    fprintf(fpResult, "\n");
	fclose(fpResult);
}


void CLBProcessor::vpRate_debug()
{
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"CLB/%d/VPRate-%d.tr",src_,dst_);
	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n",str1);
        return;
    	// return(TCL_ERROR);
    } 
	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();
    fprintf(fpResult, "%lf ",Scheduler::instance().clock());

    for ( ; it != vp_map.end(); ++it)
    {
    	fprintf(fpResult, "%.11lf ",it->second->ca_row.rate);
    }
    fprintf(fpResult, "\n");
	fclose(fpResult);
}