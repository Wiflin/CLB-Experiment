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

#include <ctime>
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
#include "flags.h"
#include "clove-processor.h"

// vp rrate reduce when received ecn 
#define CLOVE_WR	((int)3)
#define CLOVE_WI	((int)3)
#define PRE_RTT		((double)1.6E-4)
#define T_WEIGHT_E	((double)PRE_RTT * 3)

// Virtual Path Module enabled/disabled
#define VP_Module 1
// Congestion Aware Module enabled/disabled
#define CA_Module 1
// Eraser Code Module enabled/disabled
#define EC_Module 0

#define T_REFRESH  	((double)PRE_RTT)
#define T_RTIME		((double)PRE_RTT * 5)
#define T_RSSTIME	((double)PRE_RTT * 5)
#define T_RSSMAX	((double)PRE_RTT * 3000)
#define T_VP_EXPIRE	((double)0.5)
#define T_BURST_EXPIRE	((double)PRE_RTT * 15)
#define RATE_ALPHA 	((double)0.3)
#define FLY_ALPHA	((double)0.30)
// #define VP_SIZE		((int)3)	
#define COST_EQUAL  ((double)1E-4)
#define BURST_PKTCNT	((int)10)
#define INIT_RRATE 	((double)830000)

// void CLOVEProcessorTimer::expire(Event *e)
// {
// 	a_->expire(e);
// }


CLOVEProcessor::CLOVEProcessor(Node* node, Classifier* classifier, CLOVE* clove, int src, int dst, int size)
 : n_(node), c_(classifier), clove_(clove)
 // , pt_(this)
 	, src_(src), dst_(dst), VP_SIZE(size)
 	, flag(0), cost_flag(0), hashkey_counter(0)
 	, ece_cnt(0), max_ece_cnt(1), burst_pending(0), burst_vp(NULL)
{
	// srand(time(NULL));

	init_rrate = INIT_RRATE;

	sequence = 0;

	init_ca_record(&global_ca);

	init_ca_response(&global_response);

	// pt_.sched(T_REFRESH);

	stringstream dir; 
	dir << "CLOVE/" << src_;
	mkdir(dir.str().c_str(),0777);



	vp_init();

	// char str1[128];
	// sprintf(str1,"[constructor] %lf node=%d src=%d dst=%d",Scheduler::instance().clock(),n_->address(),src_,dst_);
	// flow_debug(str1,"Record");


	// char str2[128];
	// sprintf(str2, "time-record-%d", n_->address());
	// time_rf = fopen(str2, "w");

}


void CLOVEProcessor::init_vp_record(struct vp_record* vp) 
{
	vp->hashkey = 0;
	init_ca_record(&vp->ca_row);
}

void CLOVEProcessor::init_ca_record(struct ca_record* ca_row) 
{
	ca_row->send_cnt = 0;
	ca_row->recv_cnt = 0;
	ca_row->send_undefined = 0;
	ca_row->recv_undefined = 0;

	// maybe change to current average of rate
	ca_row->rate = 1;
	ca_row->r_rate = init_rrate;
	ca_row->weight = CLOVE_WI;
	ca_row->current_weight = 0;
	ca_row->flying = 0;

	ca_row->update_time = Scheduler::instance().clock();
	ca_row->fresh_time = Scheduler::instance().clock();
	ca_row->r_time = Scheduler::instance().clock();
	// ca_row->last_update_time = Scheduler::instance().clock();
	ca_row->valid = 1;
	ca_row->pending = 0;
	ca_row->recv_ece_cnt = 0;
}

void CLOVEProcessor::init_ca_response(struct ca_response* ca) 
{
	ca->hashkey = 0;
	ca->recv_cnt = 0;
	ca->recv_ecn = 0;
	ca->burst_pending = 0;
	ca->burst_cnt = 0;
	ca->burst_time = Scheduler::instance().clock();
	ca->time = Scheduler::instance().clock();
}


int CLOVEProcessor::recv(Packet* p, Handler*h)
{
	


	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);

	double now = Scheduler::instance().clock();

	// if only CA_Module is enabled, the sender will 
	// 1. update the global_ca_row as a sender
	// 2. update the global_ca_response as a receiver
	// @cautious!	it make no sense when both module are enabled!


	if (cmnh->clove_row.response_en == 1)
	{
		struct vp_record* vp = vp_get(cmnh->clove_row.vp_rid);

		if (vp == 0)
			fprintf(stderr, "[clove-processor recv] couldn't get vp_record instance!\n");
		
		else {
			assert(vp->hashkey == cmnh->clove_row.vp_rid);


			// vp_ecn feedback
			if (cmnh->clove_row.vp_recn == 1)
			{
				if (vp->ca_row.weight > 0 && now - vp->ca_row.r_time > PRE_RTT)
					vp->ca_row.weight -= 1;
					// vp->ca_row.weight = 0;
				vp->ca_row.r_time = now;
				vp->ca_row.recv_ece_cnt += 1;
			}

			
		}
	}
	
	// tcp ecn feed back
	if (hdr_flags::access(p)->ecnecho())
	{
		ece_cnt += 1;
		ecn_truncate(p);				
	}

	// work as a receiver

	// fprintf(stderr, "vpid = %d\n", cmnh->clove_row.vp_id);
	struct ca_response* response = ca_get(cmnh->clove_row.vp_id);

	if (response == 0)
		response = ca_alloc(cmnh->clove_row.vp_id);

	if (response == 0)
	{
		fprintf(stderr, "[clove-processor recv] couldn't get ca_response instance!\n");
		return 1;
	}
	
	if (hdr_flags::access(p)->ce())
	{
		response->recv_cnt += 1;
		response->recv_ecn = 1;
	}

	return 0;
}


int CLOVEProcessor::send(Packet* p, Handler*h)
{

	vpRecvEcnCnt_debug();

	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);


	unsigned	clove_hashkey = 0;		// vp_id
	unsigned	clove_recv_vpid = 0;	// vp_rid
	bool		clove_recv_ecn = 0;		// vp_recn
	bool 		clove_ren = 0;

	struct vp_record*	vp = NULL;
	struct ca_response*	ca = NULL;

	// VP
	vp = vp_next();
	clove_hashkey = vp->hashkey;


	// VP & CA
	// response
	if ((ca = ca_next()) != NULL)
	{
		clove_recv_vpid = ca->hashkey;
		clove_recv_ecn = ca->recv_ecn;
		ca->recv_ecn = 0;
		clove_ren = 1;
	}
	

	cmnh->ecmpHashKey = clove_hashkey;
	cmnh->clove_row.vp_id = clove_hashkey;	
	cmnh->clove_row.record_en = 1;	// make no sense

	cmnh->clove_row.vp_rid = clove_recv_vpid;
	cmnh->clove_row.vp_recn = clove_recv_ecn;
	cmnh->clove_row.response_en = clove_ren;	

	return 0;
}

// void CLOVEProcessor::expire(Event *e)
// {	
// 	// flow_debug("\n[clove-processor expire]\n","Send");
// 	// flow_debug("\n[clove-processor expire]\n","Recv");

// 	// vpcost_debug(1);
// 	// vpsend_debug();
// 	// 1. foreach : update table row
// 	update_ca_record(&global_ca);

// 	double now = Scheduler::instance().clock();
// 	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

// 	while (it != vp_map.end())
// 	{
// 		if (now - it->second->ca_row.fresh_time > T_VP_EXPIRE)
// 		{
// 			unsigned hashkey = it->first;
// 			++it;
// 			vp_free(hashkey);
// 			continue;
// 		}

// 		update_ca_record(&it->second->ca_row);
// 		++it;
// 	}

// 	// vpt_debug();

// 	// 2. resched()
// 	pt_.resched(T_REFRESH);
// }

struct vp_record* CLOVEProcessor::vp_alloc()
{
	// unsigned hashkey = rand() % 65535;

	// while (vp_map.find(hashkey) != vp_map.end())
	// 	hashkey = rand() % 65535;

	unsigned hashkey = hashkey_counter ++;

	struct vp_record*	vp = new struct vp_record();

	init_vp_record(vp);
	vp->hashkey = hashkey;

	vp_map[hashkey] = vp;


	// char str1[128];
	// sprintf(str1,"[vp_alloc]\t%lf\t%8d\t%p\t%lf\t%u",Scheduler::instance().clock(),hashkey,vp,cost(&vp->ca_row),vp->ca_row.recv_ece_cnt);
	// flow_debug(str1,"Record");


	return vp;
}


void CLOVEProcessor::vp_free(unsigned hashkey) 
{

	clock_t begin_time = clock();
	
	map < unsigned, struct vp_record* > :: iterator it = vp_map.find(hashkey);
	
	if (it == vp_map.end())
		return;

	struct vp_record* vp = it->second;
	delete vp;
	vp = NULL;
	vp_map.erase(it);



	// char str1[128];
	// sprintf(str1,"[vp_free]\t%lf\t%8d",Scheduler::instance().clock(),hashkey);
	// flow_debug(str1,"Record");

  	clock_t end_time = clock();
  	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
	// fprintf(time_rf, "[vp_free elapse %lf]\n", elapsed_secs);

}


// struct vp_record* CLOVEProcessor::vp_next_at_ratio()
// {
// 	if (vp_map.size() < VP_SIZE)
// 		return vp_alloc();

// 	if (burst_pending == true)
// 		return burst_vp;

// 	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

// 	double cost_min = 1E+37;
// 	double cfr = 0.0; // Capacity fire rate
// 	struct vp_record* vp = NULL;
// 	int vp_cnt = 0;
// 	vector <struct vp_record*> vp_tr;

// 	double rate_cnt = 0.0;

// 	for ( ; it != vp_map.end(); ++it)
// 	{
// 		if ( 0 == it->second->ca_row.valid )
// 			continue;

// 		vp_cnt += 1;

// 		struct vp_record* vp_tmp = it->second;
		
// 		if (fabs(vp_tmp->ca_row.r_rate - 1E+37) < 1E+36) 
// 		{
// 			vp_tr.push_back(vp_tmp);
// 		} 
// 		else if (vp_tr.size() > 0)
// 		{
// 			continue;
// 		}
// 		else 
// 		{
// 			if (fabs(vp_tmp->ca_row.r_rate - vp_tmp->ca_row.rate) < 1E-1)
// 				continue;

// 			cfr = vp_tmp->ca_row.r_rate - vp_tmp->ca_row.rate;
// 			fprintf(stderr, "%lf\n", cfr);
// 			fflush(stderr);
// 			rate_cnt += cfr / 100;


// 			// rate_cnt += vp_tmp->ca_row.r_rate / 100;
// 		}

// 	} 

// 	// fprintf(stderr, "rate_cnt=%lf", rate_cnt);
// 	// char str1[128];
// 	// sprintf(str1,"[clove-processor vp_next] %lf\thashkey=%d vpp=%p",Scheduler::instance().clock(),vp->hashkey,vp);
// 	// flow_debug(str1);

// 	if (vp_tr.size() > 0)
// 		vp = vp_tr[rand() % vp_tr.size()];

// 	else if(rate_cnt < 1)
// 	{
// 		int vp_cnt = vp_map.size();
// 		int vp_choose = rand() % vp_cnt;
// 		// fprintf(stderr, "。vp_cnt=%d vp_choose=%d\n", vp_cnt, vp_choose);


// 		map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

// 		for ( ; it != vp_map.end(); it++, vp_choose --)
// 		{
// 			if (vp_choose == 0)
// 				break;
// 		}

// 		vp = it->second;
// 	}

// 	else 
// 	{
// 		// fprintf(stderr, "?\n");
// 		double rand_cnt = (double)((int)rand() % ((int) rate_cnt));
// 		struct vp_record* vp_tmp = NULL;

// 		for ( it = vp_map.begin(); it != vp_map.end(); ++it)
// 		{
// 			if ( 0 == it->second->ca_row.valid )
// 				continue;

// 		 	vp_tmp = it->second;
			
// 		 	if (fabs(vp_tmp->ca_row.r_rate - vp_tmp->ca_row.rate) < 1E-1)
// 		 		continue;

// 		 	cfr = vp_tmp->ca_row.r_rate - vp_tmp->ca_row.rate;

// 	 		rand_cnt -= cfr / 100;


// 			// rand_cnt -= vp_tmp->ca_row.r_rate / 100;

// 			if (rand_cnt <= 0) break;
// 		} 

// 		vp = vp_tmp;
// 	}



// 	if (vp != 0)
// 		return vp;

	
// 	fprintf(stderr, "[clove-processor vp_next] Could not find a vp_record! cost=%lf\tcnt=%d\tmap_size=%d\n",	
// 		cost_min, vp_cnt, vp_map.size());

// 	return NULL;

// }

// struct vp_record* CLOVEProcessor::vp_next_at_cost()
// {
// 	if (vp_map.size() < VP_SIZE)
// 		return vp_alloc();

// 	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

// 	double cost_min = 1E+37;
// 	struct vp_record* vp = NULL;
// 	int vp_cnt = 0;
// 	vector <struct vp_record*> vp_tr;

// 	for ( ; it != vp_map.end(); ++it)
// 	{
// 		if ( 0 == it->second->ca_row.valid )
// 			continue;

// 		vp_cnt += 1;

// 		struct vp_record* vp_tmp = it->second;
// 		double vp_cost = cost(&vp_tmp->ca_row);

// 		if (fabs(vp_cost - cost_min) < COST_EQUAL)
// 		{
// 			vp_tr.push_back(vp_tmp);
// 		}
// 		else if (vp_cost < cost_min) 
// 		{
// 			cost_min = vp_cost;
// 			// vp = vp_tmp;
// 			vp_tr.clear();
// 			vp_tr.push_back(vp_tmp);
// 		}
// 	} 

// 	// char str1[128];
// 	// sprintf(str1,"[clove-processor vp_next] %lf\thashkey=%d vpp=%p",Scheduler::instance().clock(),vp->hashkey,vp);
// 	// flow_debug(str1);

// 	if (vp_tr.size() > 0)
// 		vp = vp_tr[rand() % vp_tr.size()];

// 	if (vp != 0)
// 		return vp;

	
// 	// fprintf(stderr, "[clove-processor vp_next] Could not find a vp_record! cost=%lf\tcnt=%d\tmap_size=%d\n",	
// 	// 	cost_min, vp_cnt, vp_map.size());

// 	return NULL;
// }

struct vp_record* CLOVEProcessor::vp_next()
{
	double now = Scheduler::instance().clock();

	struct vp_record* vp = NULL;
	int total = 0;
	int max_weight = -100000000;
	struct vp_record* max_vp = NULL;

	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();


	for ( ; it != vp_map.end(); ++it )
	{
		struct ca_record* car = &it->second->ca_row;

		// check age 
		if (now - car->r_time > T_WEIGHT_E)
		{
			car->weight = CLOVE_WI;
			car->r_time = now;

			fprintf(stderr, "[clove-processor vp_next] vp weight refresh %d %d\n", it->second->hashkey, car->weight);
		}

		car->current_weight += car->weight;
		total += car->weight;

		if (car->current_weight > max_weight)
		{
			max_weight = car->current_weight;
			max_vp = it->second;
		}
	}

	if (max_vp == NULL)
	{
		fprintf(stderr, "[clove-processor vp_next] max_vp==NULL  DID NOT GET A VALID VP.\n");
	}

	// algorithm : smooth weight round robin
	else
	{
		max_vp->ca_row.current_weight -= total;
		fprintf(stderr, "[clove-processor vp_next] max_vp=%d  \n", max_vp->hashkey);
	}

	vpWeight_debug();
	vpCWeight_debug();

	return max_vp;
}


struct vp_record* CLOVEProcessor::vp_get(unsigned hashkey)
{
	map < unsigned, struct vp_record* > :: iterator it = vp_map.find(hashkey);

	if (it == vp_map.end())
		return NULL;

	return it->second;
}


// struct vp_record* CLOVEProcessor::vp_burst_get(struct vp_record* vp)
// {
// 	double expire_time = vp->ca_row.r_time;
// 	struct vp_record* found = NULL;

// 	map < unsigned, struct vp_record* >::iterator it = vp_map.begin();
// 	for ( ; it != vp_map.end(); ++it )
// 	{
// 		if (it->second->ca_row.r_time < expire_time)
// 		{
// 			found = it->second;
// 			expire_time = found->ca_row.r_time;
// 		}
// 	}

// 	if (found)
// 		return found;
// 	else 
// 		return vp;
// }

struct ca_response* CLOVEProcessor::ca_alloc(unsigned hashkey) 
{
	struct ca_response* ca = new struct ca_response();

	init_ca_response(ca);
	ca->hashkey = hashkey;

	ca_map[hashkey] = ca;
	ca_queue.push(hashkey);

	return ca;
}

void CLOVEProcessor::ca_free(unsigned hashkey) 
{

	map < unsigned, struct ca_response* > :: iterator it = ca_map.find(hashkey);

	if (it == ca_map.end())
		return;

	struct ca_response* ca = it->second;
	delete ca;
	ca = NULL;
	ca_map.erase(it);
}


struct ca_response* CLOVEProcessor::ca_next()
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

struct ca_response*	CLOVEProcessor::ca_get(unsigned hashkey)
{
	map < unsigned, struct ca_response* > :: iterator it = ca_map.find(hashkey);
	return ( it == ca_map.end() ? NULL : it->second );
}


void CLOVEProcessor::vp_init()
{
	while (vp_map.size() < VP_SIZE)
		vp_alloc();

	// map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

	// for ( ; it != vp_map.end(); it++ )
	// {
	// 	vp_burst(it->second);
	// 	fprintf(stderr, "[vp_burst] %lf %d\n", Scheduler::instance().clock(), it->first);
	// }

}


// void CLOVEProcessor::vp_burst(struct vp_record* vp)
// {
// 	struct ca_record* ca = &vp->ca_row;
// 	double now = Scheduler::instance().clock();

// 	for (int i = BURST_PKTCNT; i > 0; i--)
// 	{

// 		Packet* p = pkt_alloc();
// 		hdr_cmn::access(p)->clove_row.burst_id = i;
// 		hdr_cmn::access(p)->clove_row.vp_id = vp->hashkey;
// 		hdr_cmn::access(p)->ecmpHashKey = vp->hashkey;
// 		// vpBurstSend_debug(p);

// 		NsObject* target = c_->find(p);
// 		if (target == NULL) 
// 		{
// 				/*
// 				 * XXX this should be "dropped" somehow.  Right now,
// 				 * these events aren't traced.
// 				 */
// 		 		Packet::free(p);
// 				// return;
// 			}
// 		else 
// 			target->recv(p, (Handler*) 0);///original ends
			
// 		// fprintf(stderr, "burst sent from %d to %d uid=%d\n", src_, dst_, p->uid());
			

// 	}



bool CLOVEProcessor::ecn_truncate(Packet* p)
{
	int flag = 1;
	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

	for ( ; it != vp_map.end(); ++it)
	{
		if (it->second->ca_row.weight == CLOVE_WI)
		{
			flag = 0;
			break;
		}
	}

	if ( flag == 0 )
	{
		// truncate
		hdr_flags::access(p)->ecnecho() = 0;
		return true;
	}

	return false;

}

// }

// double CLOVEProcessor::cost(struct ca_record* ca)
// {
// 	// @todo how to calculate it

// 	// if (fabs(ca->rate) < 1E-2)	
// 		// calculate the average cost
// 	// double fly = ca->flying + ca->send_undefined - ca->recv_undefined;
// 	double fly = ca->flying + ca->send_undefined;
// 	double cost_ = (double)(fly) / ca->r_rate;
// 	return cost_;
// }



// // send variable incr
// void CLOVEProcessor::ca_record_send(struct ca_record* ca)
// {
// 	// 1.update send_undefined
// 	unsigned send_undefined = ca->send_undefined + 1;
// 	// 2.update fresh time
// 	ca->send_undefined = send_undefined;
// 	ca->fresh_time = Scheduler::instance().clock();

// }

// void CLOVEProcessor::ca_record_recv(struct ca_record* ca, unsigned current_recv)
// {
// 	int recv_undefined = current_recv - ca->recv_cnt;

// 	if (recv_undefined < 0)
// 		return;

// 	ca->recv_undefined = recv_undefined;
// 	ca->fresh_time = Scheduler::instance().clock();
// }



// void CLOVEProcessor::vp_update_rrate(struct vp_record* vp, Packet* p)
// {
// 	struct ca_record* ca = &vp->ca_row;
// 	double now = Scheduler::instance().clock();

// 	if (hdr_cmn::access(p)->clove_row.vp_recn == true 
// 		// && now - ca->r_time > T_RTIME
// 		)
// 	{

// 		ca->r_rate = ca->rate;

// 		ca->recv_ece_cnt += 1;

// 		ca->r_time = now - (T_RSSTIME - PRE_RTT);

// 		if (ca->recv_ece_cnt > max_ece_cnt)
// 			max_ece_cnt = ca->recv_ece_cnt;
	
// 		// fprintf(stderr, "【vp_update_rrate】timeout_rate = %lf\n", ((double)ca->recv_ece_cnt / max_ece_cnt) );
// 	}


// 	if (now - ca->r_time > T_RSSTIME && burst_pending == false )
// 		// && 
// 		// now - ca->r_time > T_RSSMAX * ((double)ca->recv_ece_cnt / max_ece_cnt) &&
// 		// fabs(ca->r_rate - 1E+37) >= 1E+36))
// 	{
// 		struct vp_record* burst_cvp = vp_burst_get(vp);

// 		fprintf(stderr, "%d %u ecn expire \n", n_->address(), burst_cvp->hashkey );

// 		burst_pending = 1;
// 		burst_vp = burst_cvp;
// 		burst_cnt = BURST_PKTCNT;
// 		burst_time = Scheduler::instance().clock();
// 		// ca->pending = 1;
// 		// vp_burst(vp);
// 		// ca->r_time = now;
// 	}
// }


// void CLOVEProcessor::update_ca_record(struct ca_record* ca_row)
// {
	
// 	double now = Scheduler::instance().clock();
// 	double delta_time = now - ca_row->update_time;

// 	// 1. calculate new rate
// 	double old_rate = ca_row->rate;
// 	double calculate_rate = (double) ca_row->recv_undefined / delta_time;
// 	// double calculate_rate = (double) ca_row->recv_undefined;
// 	double new_rate = (1 - RATE_ALPHA) * old_rate + RATE_ALPHA * calculate_rate;
// 	ca_row->rate = new_rate;

// 	// 2. calculate new flying 
// 	double old_fly = ca_row->flying;
// 	double delta_fly = (double) ca_row->send_undefined - ca_row->recv_undefined;
// 	double new_fly = (1 - FLY_ALPHA) * old_fly + FLY_ALPHA * delta_fly;
// 	ca_row->flying = new_fly; 

// 	// 3. calculate new recv & new send
// 	ca_row->send_cnt += ca_row->send_undefined;
// 	ca_row->recv_cnt += ca_row->recv_undefined;

// 	// 4. reset send-undefine & recv-undefine
// 	ca_row->send_undefined = 0;
// 	ca_row->recv_undefined = 0;

// 	// 6. update timer
// 	ca_row->update_time = Scheduler::instance().clock();

// }


// when the processor recv a packet -> should update the ca_response as a receiver
// void CLOVEProcessor::update_ca_response(struct ca_response* response, Packet* p)
// {
// 	response->recv_cnt += 1;
// 	response->recv_ecn = (bool)hdr_flags::access(p)->ce() || 
// 						 (bool)hdr_cmn::access(p)->clove_row.road_ecn;
// 	response->recv_ecn_cnt += response->recv_ecn;
// 	response->time = Scheduler::instance().clock();

// 	// char str1[128];
// 	// sprintf(str1,"[clove-processor update_ca_response] %lf\t%p\trecv-cnt=%d",
// 	// 	Scheduler::instance().clock(),response,response->recv_cnt);
// 	// flow_debug(str1);

// }


// void CLOVEProcessor::update_ca_burst(struct ca_response* response, Packet* p)
// {
// 	// vpBurstRecv_debug(p);

// 	double now = Scheduler::instance().clock();

// 	if (response->burst_pending == 0 || now - response->burst_time > (T_RSSTIME) )
// 	{
// 		response->burst_pending = 1;
// 		response->burst_cnt = 1;
// 		response->burst_time = now;
		
// 		return;
// 	}

// 	response->burst_cnt ++;


// 	if (hdr_cmn::access(p)->clove_row.burst_id == 1)
// 	{
// 		double delta_time = now - response->burst_time;
// 		double r_rate = (double) response->burst_cnt / delta_time;

// 		Packet* rp = pkt_alloc();
// 		struct hdr_cmn* cmnh = hdr_cmn::access(rp);
		
// 		cmnh->clove_row.response_en = 1;
// 		cmnh->clove_row.vp_rid = response->hashkey;
// 		cmnh->clove_row.burst_rate = r_rate;

// 		char debug[300];
// 		sprintf(debug, "%lf %d-%d (vp %u) start=%lf cnt=%d time=%lf new_rate=%lf\n", now,
// 		 hdr_ip::access(rp)->dst_.addr_, n_->address(),  response->hashkey, response->burst_time, response->burst_cnt, delta_time, r_rate);
// 		vpBurst_debug(debug);


// 		NsObject* target = c_->find(rp);
// 		if (target == NULL) 
// 		{
// 	 		Packet::free(rp);
// 			fprintf(stderr, "[update_ca_burst] cannot get targe.\n");
// 		}

// 		else 
// 			target->recv(rp, (Handler*) 0);///original ends
				

// 		response->burst_pending = 0;

// 	}



// }


// Packet* CLOVEProcessor::pkt_alloc()
// {
// 	Packet* p = Packet::alloc();

// 	hdr_cmn* ch = hdr_cmn::access(p);

// 	// ch->uid() = uidcnt_++;
// 	// ch->ptype() = type_;
// 	ch->size() = 1500;
// 	ch->timestamp() = Scheduler::instance().clock();
// 	// ch->iface() = UNKN_IFACE.value(); // from packet.h (agent is local)
// 	// ch->direction() = hdr_cmn::NONE;

// 	ch->error() = 0;	/* pkt not corrupt to start with */

// 	hdr_ip* iph = hdr_ip::access(p);
// 	iph->saddr() = src_;
// 	iph->sport() = 100;
// 	iph->daddr() = dst_;
// 	iph->dport() = 100;
	
// 	//DEBUG
// 	//if (dst_ != -1)
// 	//  printf("pl break\n");
	
// 	// iph->flowid() = fid_;
// 	// iph->prio() = prio_;
// 	iph->ttl() = 128;

// 	hdr_flags* hf = hdr_flags::access(p);
// 	hf->ecn_capable_ = 0;
// 	hf->ecn_ = 0;
// 	hf->eln_ = 0;
// 	hf->ecn_to_echo_ = 0;
// 	hf->fs_ = 0;
// 	hf->no_ts_ = 0;
// 	hf->pri_ = 0;
// 	hf->cong_action_ = 0;
// 	hf->qs_ = 0;

// 	return p;

// }
















// void CLOVEProcessor::init_vp_record(struct vp_record* vp)
// {
// 	vp->hashkey = rand();

// 	init_ca_record(&vp->ca_row);
// }

// void CLOVEProcessor::init_ca_record(struct ca_record* ca_row)
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


// void CLOVEProcessor::init_ca_response(struct ca_response* ca)
// {
// 	ca->hashkey = 0;
// 	ca->recv_cnt = 0;
// 	ca->time = 0;
// }



// void CLOVEProcessor::flow_debug(char* str, char* file, Packet* p)
// {
// 	// hdr_ip* iph = hdr_ip::access(p);
// 	// hdr_cmn* cmnh = hdr_cmn::access(p);

// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"CLOVE/%d/%s-%d.tr", src_, file, dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n", str1);
//     	// return(TCL_ERROR);
//     } else {

// 		fprintf(fpResult, "%s\n", str);		
// 		fclose(fpResult);
// 	}
// }


// void CLOVEProcessor::vpt_debug()
// {
// 	// hdr_ip* iph = hdr_ip::access(p);
// 	// hdr_cmn* cmnh = hdr_cmn::access(p);


// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"CLOVE/%d/VPT-%d.tr",src_,dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n","CLOVE/route_debug.tr");
//         return;
//     	// return(TCL_ERROR);
//     } 




//     map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

//     if ( ! flag )
//     {
//     	for ( ; it != vp_map.end(); ++it)
//     	{
//     		fprintf(fpResult, "%u ", it->second->hashkey );
//     	}

//     	it = vp_map.begin();
// 		fprintf(fpResult, "\n");

// 		flag = 1;

//     }


//     for ( ; it != vp_map.end(); ++it)
//     {
//     	// fprintf(fpResult, "%lf\t%u\t%lf\t%lf\n", Scheduler::instance().clock(), 
//     	// 	it->second->hashkey, it->second->ca_row.rate, it->second->ca_row.flying);
//     	fprintf(fpResult, "%lf ",it->second->ca_row.rate);
//     }
//     fprintf(fpResult, "\n");
// 	fclose(fpResult);

// }



// void CLOVEProcessor::vpsend_debug()
// {
// 	// hdr_ip* iph = hdr_ip::access(p);
// 	// hdr_cmn* cmnh = hdr_cmn::access(p);

//     clock_t begin_time = clock();
	

// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"CLOVE/%d/VPSend-%d.tr",src_,dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n",str1);
//         return;
//     	// return(TCL_ERROR);
//     } 


	


//     map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

//     if ( ! flag )
//     {
//     	for ( ; it != vp_map.end(); ++it)
//     	{
//     		fprintf(fpResult, "%u ", it->second->hashkey );
//     	}

//     	it = vp_map.begin();
// 		fprintf(fpResult, "\n");

// 		flag = 1;

//     }


//     for ( ; it != vp_map.end(); ++it)
//     {
//     	// fprintf(fpResult, "%lf\t%u\t%lf\t%lf\n", Scheduler::instance().clock(), 
//     	// 	it->second->hashkey, it->second->ca_row.rate, it->second->ca_row.flying);
//     	fprintf(fpResult, "%u ",it->second->ca_row.send_undefined);
//     }


//     fprintf(fpResult,"\n");
// 	fclose(fpResult);

//     clock_t end_time = clock();
//   	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;

//   	// fprintf(time_rf,  "[vpsend elapse %lf]\n", elapsed_secs);

// }


// void CLOVEProcessor::vpcost_debug(int expire)
// {
//     clock_t begin_time = clock();
// 	// hdr_ip* iph = hdr_ip::access(p);
// 	// hdr_cmn* cmnh = hdr_cmn::access(p);

	

// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"CLOVE/%d/VPCost-%d.tr",src_,dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n",str1);
//         return;
//     	// return(TCL_ERROR);
//     } 



//     map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

//     if ( ! cost_flag )
//     {
//     	for ( ; it != vp_map.end(); ++it)
//     	{
//     		fprintf(fpResult, "%u ", it->second->hashkey );
//     	}

//     	it = vp_map.begin();
// 		fprintf(fpResult, "\n");

// 		cost_flag = 1;

//     }

//     fprintf(fpResult, "%lf ",Scheduler::instance().clock());

//     // if (expire) {
//     // 	fprintf(fpResult, "expire\n");
//     // }

//     for ( ; it != vp_map.end(); ++it)
//     {
//     	// fprintf(fpResult, "%lf\t%u\t%lf\t%lf\n", Scheduler::instance().clock(), 
//     	// 	it->second->hashkey, it->second->ca_row.rate, it->second->ca_row.flying);
//     	fprintf(fpResult, "%.11lf ",cost(&it->second->ca_row));
//     }


//   	fprintf(fpResult, "\n");
// 	fclose(fpResult);


//     clock_t end_time = clock();
//   	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    
//     // fprintf(time_rf, "[vpcost_debug elapse %lf]\n", elapsed_secs);
// }



// void CLOVEProcessor::vpSendCnt_debug()
// {
//     clock_t begin_time = clock();
// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"CLOVE/%d/VPSendCnt-%d.tr",src_,dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n",str1);
//         return;
//     	// return(TCL_ERROR);
//     } 



// 	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();
//     fprintf(fpResult, "%lf ",Scheduler::instance().clock());

//     for ( ; it != vp_map.end(); ++it)
//     {
//     	fprintf(fpResult, "%u ",it->second->ca_row.send_cnt);
//     }
    
//     fprintf(fpResult, "\n");
// 	fclose(fpResult);

//     clock_t end_time = clock();
//   	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
//     // fprintf(time_rf, "[vpSendCnt_debug elapse %lf]\n", elapsed_secs);
// }


// void CLOVEProcessor::vpRecvCnt_debug()
// {
//     clock_t begin_time = clock();

// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"CLOVE/%d/VPRecvCnt-%d.tr",src_,dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n",str1);
//         return;
//     	// return(TCL_ERROR);
//     } 

    


// 	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();
//     fprintf(fpResult, "%lf ",Scheduler::instance().clock());

//     for ( ; it != vp_map.end(); ++it)
//     {
//     	fprintf(fpResult, "%u ",it->second->ca_row.recv_cnt);
//     }

//     fprintf(fpResult, "\n");
// 	fclose(fpResult);

//     clock_t end_time = clock();
//   	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    
//     // fprintf(time_rf, "[vpRecvCnt_debug elapse %lf]\n", elapsed_secs);
// }

// void CLOVEProcessor::vpSendNew_debug()
// {
// 	clock_t begin_time = clock();
	
// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"CLOVE/%d/VPSend*-%d.tr",src_,dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n",str1);
//         return;
//     	// return(TCL_ERROR);
//     } 
// 	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();
//     fprintf(fpResult, "%lf ",Scheduler::instance().clock());

//     for ( ; it != vp_map.end(); ++it)
//     {
//     	fprintf(fpResult, "%u ",it->second->ca_row.send_undefined);
//     }
//     fprintf(fpResult, "\n");
// 	fclose(fpResult);


// 	clock_t end_time = clock();
//   	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    
//     // fprintf(time_rf, "[vpSendNew_debug elapse %lf]\n", elapsed_secs);
// }


// void CLOVEProcessor::vpRecvNew_debug()
// {
//     clock_t begin_time = clock();

// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"CLOVE/%d/VPRecv*-%d.tr",src_,dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n",str1);
//         return;
//     	// return(TCL_ERROR);
//     } 

    
// 	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();
//     fprintf(fpResult, "%lf ",Scheduler::instance().clock());

//     for ( ; it != vp_map.end(); ++it)
//     {
//     	fprintf(fpResult, "%d ",it->second->ca_row.recv_undefined);
//     }
    
//     fprintf(fpResult, "\n");
// 	fclose(fpResult);

//     clock_t end_time = clock();
//   	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    
//     // fprintf(time_rf, "[vpRecvNew_debug elapse %lf]\n", elapsed_secs);
// }


// void CLOVEProcessor::vpFlying_debug()
// {
//     clock_t begin_time = clock();

// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"CLOVE/%d/VPFlying-%d.tr",src_,dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n",str1);
//         return;
//     	// return(TCL_ERROR);
//     } 



// 	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();
//     fprintf(fpResult, "%lf ",Scheduler::instance().clock());

//     for ( ; it != vp_map.end(); ++it)
//     {
//     	fprintf(fpResult, "%.11lf ",it->second->ca_row.flying);
//     }
    
//     fprintf(fpResult, "\n");
// 	fclose(fpResult);

//     clock_t end_time = clock();
//   	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    
//     // fprintf(time_rf, "[vpFlying_debug elapse %lf]\n", elapsed_secs);
// }


void CLOVEProcessor::vpCWeight_debug()
{
	clock_t begin_time = clock();

	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"CLOVE/%d/VPCWeight-%d.tr",src_,dst_);
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
    	fprintf(fpResult, "%d ",it->second->ca_row.current_weight);
    }
    
    fprintf(fpResult, "\n");
	fclose(fpResult);

    clock_t end_time = clock();
  	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    
    // fprintf(time_rf, "[vpRate_debug elapse %lf] \n", elapsed_secs);
}

void CLOVEProcessor::vpWeight_debug()
{
    clock_t begin_time = clock();
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"CLOVE/%d/VPWeight-%d.tr",src_,dst_);
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
    	fprintf(fpResult, "%d ",it->second->ca_row.weight);
    }
    
    fprintf(fpResult, "\n");
	fclose(fpResult);

    clock_t end_time = clock();
  	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    
    // fprintf(time_rf, "[vpRRate_debug elapse %lf]\n", elapsed_secs);
}


// void CLOVEProcessor::ipECE_debug()
// {
//     clock_t begin_time = clock();
// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"CLOVE/%d/IPECE-%d.tr",src_,dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n",str1);
//         return;
//     	// return(TCL_ERROR);
//     } 

//     fprintf(fpResult, "%lf %u\n", Scheduler::instance().clock(),ece_cnt);

// 	fclose(fpResult);


// 	clock_t end_time = clock();
//   	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    
//     // fprintf(time_rf, "[ipECE_debug elapse %lf]\n", elapsed_secs);
// }



// void CLOVEProcessor::vpECE_debug()
// {
//     clock_t begin_time = clock();

// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"CLOVE/%d/VPECE-%d.tr",src_,dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n",str1);
//         return;
//     	// return(TCL_ERROR);
//     } 



// 	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();
//     fprintf(fpResult, "%lf ",Scheduler::instance().clock());

//     for ( ; it != vp_map.end(); ++it)
//     {
//     	fprintf(fpResult, "%u ",it->second->ca_row.recv_ece_cnt);
//     }

//     fprintf(fpResult, "\n");
// 	fclose(fpResult);
    
//     clock_t end_time = clock();
//   	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    
//     // fprintf(time_rf, "[vpECE_debug elapse %lf]\n",elapsed_secs);
// }


// void CLOVEProcessor::vpBurstSend_debug(Packet* p)
// {
// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"CLOVE/%d/VPBurstSend-%d.tr",src_,dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n",str1);
//         return;
//     	// return(TCL_ERROR);
//     } 

//     fprintf(fpResult, "%lf ",Scheduler::instance().clock());

//     fprintf(fpResult, "node=%d vp_id=%u %u\n", 
//     	n_->address(),
//     	hdr_cmn::access(p)->clove_row.vp_id,
//     	hdr_cmn::access(p)->clove_row.burst_id);

// 	fclose(fpResult);
// }

// void CLOVEProcessor::vpBurstRecv_debug(Packet* p)
// {
// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"CLOVE/%d/VPBurstRecv-%d.tr",src_,dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n",str1);
//         return;
//     	// return(TCL_ERROR);
//     } 

//     fprintf(fpResult, "%lf  ",Scheduler::instance().clock());

//     fprintf(fpResult, "node=%d vp=%u burst_id=%u %lf\n", 
//     	n_->address(),
//     	hdr_cmn::access(p)->clove_row.vp_id,
//     	hdr_cmn::access(p)->clove_row.burst_id,
//     	hdr_cmn::access(p)->clove_row.burst_rate);

// 	fclose(fpResult);
// }



// void CLOVEProcessor::vpBurst_debug(char* str)
// {
// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"CLOVE/%d/VPBurstRecv-%d.tr",src_,dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n",str1);
//         return;
//     	// return(TCL_ERROR);
//     } 

//     // fprintf(fpResult, "%lf %s",Scheduler::instance().clock());

//     fprintf(fpResult, str);

// 	fclose(fpResult);
// }


void CLOVEProcessor::vpRecvEcnCnt_debug()
{
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"CLOVE/%d/VPRecvEce-%d.tr",src_,dst_);
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
    	fprintf(fpResult, "%u ",it->second->ca_row.recv_ece_cnt);
    }

    fprintf(fpResult, "\n");
	fclose(fpResult);
    
    
    // fprintf(time_rf, "[vpECE_debug elapse %lf]\n",elapsed_secs);
}