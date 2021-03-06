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
#include "hermes-processor.h"

#define PRE_RTT		((double)1.6E-4)


// define for hermes
#define ECN_PCNT 	((int)25)
#define TH_ECN		((double)0.4)
#define TH_RTT_HIGH	((double)1.5E-4)
#define TH_RTT_LOW	((double)1E-4)
#define TH_BIAS		((double)5E-5)
#define T_REFRESH  	((double)3E-3)
#define T_ECN_EXPIRE	((double)25*PRE_RTT)
#define T_RTT_EXPIRE 	((double)10*PRE_RTT)

// #define T_ECN_EXPIRE

// vp rrate reduce when received ecn 
#define Hermes_WR	((double)3)
#define Hermes_WI	((double)100)
#define T_WEIGHT_EXPIRE	((double)PRE_RTT * 15)

// Virtual Path Module enabled/disabled
#define VP_Module 1
// Congestion Aware Module enabled/disabled
#define CA_Module 1
// Eraser Code Module enabled/disabled
#define EC_Module 0


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
// #define INIT_RRATE 	((double)830000)	// 10G
#define INIT_RRATE 	((double)3333333)	// 40G


#define _S_ 			((double)400)
#define _R_ 			((double)0.75*INIT_RRATE)
#define DELTA_RTT	((double)0.2E-4)
#define DELTA_ECN 	((double)0.05)

void HermesProcessorTimer::expire(Event *e)
{
	a_->expire(e);
}


HermesProcessor::HermesProcessor(Node* node, Classifier* classifier, Hermes* hermes, int src, int dst, int size)
 : n_(node), c_(classifier), hermes_(hermes)
 	, pt_(this)
 	, src_(src), dst_(dst), VP_SIZE(size)
 	, flag(0), cost_flag(0), hashkey_counter(0)
 	, ece_cnt(0), max_ece_cnt(1), burst_pending(0), burst_vp(NULL)
{
	// srand(time(NULL));

	init_rrate = INIT_RRATE;

	sequence = 0;

	init_ca_record(&global_ca);

	init_ca_response(&global_response);

	pt_.sched(T_REFRESH);

	// stringstream dir; 
	// dir << "Hermes/" << src_;
	// mkdir(dir.str().c_str(),0777);



	vp_init();

	// char str1[128];
	// sprintf(str1,"[constructor] %lf node=%d src=%d dst=%d",Scheduler::instance().clock(),n_->address(),src_,dst_);
	// flow_debug(str1,"Record");


	// char str2[128];
	// sprintf(str2, "time-record-%d", n_->address());
	// time_rf = fopen(str2, "w");

}


void HermesProcessor::init_vp_record(struct vp_record* vp) 
{
	vp->hashkey = 0;
	init_ca_record(&vp->ca_row);
}

void HermesProcessor::init_ca_record(struct ca_record* ca_row) 
{
	ca_row->send_cnt = 0;
	ca_row->recv_cnt = 0;
	ca_row->send_undefined = 0;
	ca_row->recv_undefined = 0;

	// maybe change to current average of rate
	ca_row->rate = 1;
	ca_row->r_rate = init_rrate;
	ca_row->weight = Hermes_WI;
	ca_row->current_weight = 0;
	ca_row->flying = 0;

	ca_row->update_time = Scheduler::instance().clock();
	ca_row->fresh_time = Scheduler::instance().clock();
	ca_row->r_time = Scheduler::instance().clock();

	ca_row->c_rtt = 0;
	ca_row->rank = GOOD;
	ca_row->vp_ecn_ratio = 0;
	// ca_row->last_update_time = Scheduler::instance().clock();
	ca_row->valid = 1;
	ca_row->pending = 0;
	ca_row->recv_ece_cnt = 0;
}

void HermesProcessor::init_ca_response(struct ca_response* ca) 
{
	ca->hashkey = 0;
	ca->recv_cnt = 0;
	ca->recv_ecn = 0;
	ca->recv_rtt = 0;
	ca->burst_pending = 0;
	ca->burst_cnt = 0;
	ca->burst_time = Scheduler::instance().clock();
	ca->time = Scheduler::instance().clock();
	ca->ecn_time = Scheduler::instance().clock();
	ca->rtt_time = Scheduler::instance().clock();
}


int HermesProcessor::recv(Packet* p, Handler*h)
{
	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);

	double now = Scheduler::instance().clock();

	// if only CA_Module is enabled, the sender will 
	// 1. update the global_ca_row as a sender
	// 2. update the global_ca_response as a receiver
	// @cautious!	it make no sense when both module are enabled!


	if (cmnh->hermes_row.response_en == 1)
	{
		struct vp_record* vp = vp_get(cmnh->hermes_row.vp_rid);

		if (vp == 0)
			fprintf(stderr, "[hermes-processor recv] couldn't get vp_record instance!\n");
		
		else {
			assert(vp->hashkey == cmnh->hermes_row.vp_rid);


			// vp_ecn feedback
			if (cmnh->hermes_row.vp_recn == 1)
			{
				// if (vp->ca_row.weight > 1 && now - vp->ca_row.r_time > 5*PRE_RTT)
				// {
				// 	vp->ca_row.weight -= (vp->ca_row.weight / 3);
				// 	// vp->ca_row.weight = 0;
				// 	vp->ca_row.r_time = now;
				// }
				vp->ca_row.recv_ece_cnt += 1;
			}

			vp->ca_row.c_rtt = cmnh->hermes_row.vp_rtt;
			vp->ca_row.vp_ecn_ratio = cmnh->hermes_row.ecn_ratio;

			if (vp->ca_row.vp_ecn_ratio < TH_ECN && vp->ca_row.c_rtt < TH_RTT_LOW)
			{
				vp->ca_row.rank = GOOD;
			}
			else if (vp->ca_row.vp_ecn_ratio > TH_ECN && vp->ca_row.c_rtt > TH_RTT_HIGH)
			{
				vp->ca_row.rank = CONGESTED;
			}
			else 
			{
				vp->ca_row.rank = GREY;
			}

		}
	}
	
	// tcp ecn feed back
	// if (hdr_flags::access(p)->ecnecho())
	// {
	// 	ecn_truncate(p);				
	// }
	if (hdr_flags::access(p)->ecnecho())
	{
		ece_cnt += 1;
	}
	// work as a receiver

	// fprintf(stderr, "vpid = %d\n", cmnh->hermes_row.vp_id);
	struct ca_response* response = ca_get(cmnh->hermes_row.vp_id);

	if (response == 0)
		response = ca_alloc(cmnh->hermes_row.vp_id);

	if (response == 0)
	{
		fprintf(stderr, "[hermes-processor recv] couldn't get ca_response instance!\n");
		return 0;
	}

	response->recv_rtt = Scheduler::instance().clock() - cmnh->hermes_row.vp_time + ((double) rand() / (RAND_MAX))*TH_BIAS;
	response->rtt_time = Scheduler::instance().clock();

	response->ecn_bitmap.push_back(bool(hdr_flags::access(p)->ce()));
	while (response->ecn_bitmap.size() > ECN_PCNT)
		response->ecn_bitmap.erase(response->ecn_bitmap.begin());
	response->ecn_time = Scheduler::instance().clock();
	
	if (hdr_flags::access(p)->ce())
	{
		response->recv_ecn_cnt += 1;
		response->recv_ecn = 1;
	}

	return 0;
}


int HermesProcessor::send(Packet* p, Handler*h)
{
	// vpCARecvEcnCnt_debug();
	// vpRecvEcnCnt_debug();
	// ipECE_debug();
	// vpECE_debug();
	// vpSendCnt_debug();
	// vpRecvRTT_debug();
	// vpCARecvRTT_debug();
	// vpWeight_debug();
	// vpCWeight_debug();
	// vpRecvEcnRatio_debug();
	// vpCARecvEcnRatio_debug();
	// vpRank_debug();
	// vpSendNew_debug();
	// vpRate_debug();

	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);


	unsigned	hermes_hashkey = 0;		// vp_id
	unsigned	hermes_recv_vpid = 0;	// vp_rid
	bool		hermes_recv_ecn = 0;		// vp_recn
	bool 		hermes_ren = 0;
	double 		hermes_recv_rtt = 0;
	double 		hermes_recv_ecn_ratio = 0;

	struct vp_record*	vp = NULL;
	struct ca_response*	ca = NULL;


	unsigned fid = cmnh->flowID;

	// a new
	if (flow_table.find(fid) == flow_table.end())
	{
		vp = vp_next();
		flow_table[fid] = vp->hashkey;

		// fprintf(stderr, "[hermes %u] %lf new vp !  %lf \n",
		// 		 n_->address(), Scheduler::instance().clock(),
		// 		 vp->ca_row.rate);
	}
	else 
	{
		unsigned hashkey = flow_table[fid];
		vp = vp_get(hashkey);

		if (vp->ca_row.rank == CONGESTED 
			&& vp->ca_row.rate < _R_
			)
		{
				// fprintf(stderr, "[hermes %u] %lf slide to another road!  %lf < %lf  ;;; _R_\n",
				//  n_->address(), Scheduler::instance().clock(),
				//  vp->ca_row.rate, _R_);

				// fprintf(stderr, "[hermes %u] fid=%d  hashkey=%u,  %p\n",
				//  n_->address(), fid, hashkey, vp
				//  );
				

			struct vp_record* tmp_vp = vp_next();	
			if (vp->ca_row.c_rtt - tmp_vp->ca_row.c_rtt > DELTA_RTT && 
				vp->ca_row.vp_ecn_ratio -  tmp_vp->ca_row.vp_ecn_ratio > DELTA_ECN)
			{


				vp = tmp_vp;
			}
		}
		flow_table[fid] = vp->hashkey;

	}

	// VP
	// vp = vp_next();
	hermes_hashkey = vp->hashkey;

	// record 
	vp->ca_row.send_cnt += 1;
	vp->ca_row.send_undefined += 1;

	// VP & CA
	// response
	if ((ca = ca_next()) != NULL)
	{
		hermes_recv_vpid = ca->hashkey;
		hermes_recv_ecn = ca->recv_ecn;
		ca->recv_ecn = 0;
		hermes_ren = 1;

		// refresh rtt if rtt_time expire
		if (Scheduler::instance().clock() - ca->rtt_time > T_RTT_EXPIRE)
		{
			ca->recv_rtt = 0;
			ca->rtt_time = Scheduler::instance().clock();
		}

		hermes_recv_rtt = ca->recv_rtt;

		// caculate ratio
		if (Scheduler::instance().clock() - ca->ecn_time > T_ECN_EXPIRE)
		{
			ca->ecn_bitmap.push_back(0);
			while (ca->ecn_bitmap.size() > ECN_PCNT)
				ca->ecn_bitmap.erase(ca->ecn_bitmap.begin());
		}

		int ecn_cnt = 0;
		for (int i = 0; i < ca->ecn_bitmap.size(); ++i)
			ecn_cnt += (ca->ecn_bitmap[i] ? 1 : 0);
		hermes_recv_ecn_ratio = (double) ecn_cnt / ca->ecn_bitmap.size();
	}
	

	cmnh->ecmpHashKey = hermes_hashkey;
	cmnh->hermes_row.vp_id = hermes_hashkey;	
	cmnh->hermes_row.vp_time = Scheduler::instance().clock();
	cmnh->hermes_row.record_en = 1;	// make no sense

	cmnh->hermes_row.vp_rid = hermes_recv_vpid;
	cmnh->hermes_row.vp_recn = hermes_recv_ecn;
	cmnh->hermes_row.vp_rtt = hermes_recv_rtt;
	cmnh->hermes_row.ecn_ratio = hermes_recv_ecn_ratio;
	cmnh->hermes_row.response_en = hermes_ren;	

	return 0;
}

void HermesProcessor::expire(Event *e)
{	
	// flow_debug("\n[hermes-processor expire]\n","Send");
	// flow_debug("\n[hermes-processor expire]\n","Recv");

	// vpcost_debug(1);
	// vpsend_debug();
	// 1. foreach : update table row
	// update_ca_record(&global_ca);

	double now = Scheduler::instance().clock();

	struct vp_record* vp = NULL;
	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

	for ( ; it != vp_map.end(); ++it)
	{
		vp = it->second;

		vp->ca_row.rate = (double)vp->ca_row.send_undefined / T_REFRESH;

		vp->ca_row.send_undefined = 0;
	}

// 	// vpt_debug();

// 	// 2. resched()
	pt_.resched(T_REFRESH);
}

struct vp_record* HermesProcessor::vp_alloc()
{
	// unsigned hashkey = rand() % 65535;

	// while (vp_map.find(hashkey) != vp_map.end())
	// 	hashkey = rand() % 65535;

	unsigned hashkey = hashkey_counter ++;

	struct vp_record*	vp = new struct vp_record();

	init_vp_record(vp);
	vp->hashkey = hashkey;
	// vp->ca_row.weight = Hermes_WI / hashkey_counter;

	vp_map[hashkey] = vp;


	// char str1[128];
	// sprintf(str1,"[vp_alloc]\t%lf\t%8d\t%p\n",Scheduler::instance().clock(),hashkey,vp);
	// flow_debug(str1,"Record");


	return vp;
}


void HermesProcessor::vp_free(unsigned hashkey) 
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


// struct vp_record* HermesProcessor::vp_next_at_ratio()
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
// 	// sprintf(str1,"[hermes-processor vp_next] %lf\thashkey=%d vpp=%p",Scheduler::instance().clock(),vp->hashkey,vp);
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

	
// 	fprintf(stderr, "[hermes-processor vp_next] Could not find a vp_record! cost=%lf\tcnt=%d\tmap_size=%d\n",	
// 		cost_min, vp_cnt, vp_map.size());

// 	return NULL;

// }

// struct vp_record* HermesProcessor::vp_next_at_cost()
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
// 	// sprintf(str1,"[hermes-processor vp_next] %lf\thashkey=%d vpp=%p",Scheduler::instance().clock(),vp->hashkey,vp);
// 	// flow_debug(str1);

// 	if (vp_tr.size() > 0)
// 		vp = vp_tr[rand() % vp_tr.size()];

// 	if (vp != 0)
// 		return vp;

	
// 	// fprintf(stderr, "[hermes-processor vp_next] Could not find a vp_record! cost=%lf\tcnt=%d\tmap_size=%d\n",	
// 	// 	cost_min, vp_cnt, vp_map.size());

// 	return NULL;
// }

struct vp_record* HermesProcessor::vp_next()
{
	double now = Scheduler::instance().clock();
	struct vp_record* vp = NULL;

	vector < struct vp_record* > fail, cong, grey, good;
	vector < struct vp_record* > :: iterator sit;

	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();


	for ( ; it != vp_map.end(); ++it )
	{
		struct vp_record* vpr = it->second;
		struct ca_record* car = &it->second->ca_row;

		switch (car->rank)
		{
			case FAILED:
				fail.push_back(vpr);
				break;
			case CONGESTED:
				cong.push_back(vpr);
				break;
			case GREY:
				grey.push_back(vpr);
				break;
			case GOOD:
				good.push_back(vpr);
				break;
			default:
				fprintf(stderr, "[hermes vp_next] could dispatch virtual path %d %d\n", it->second->hashkey, car->rank );
			break;
		}
	}

	if (good.size() > 0)
	{
		// vp = good[rand() % good.size()];
		sit = good.begin(); 
		vp = *sit;

		for ( ; sit != good.end(); sit++)
		{
			if ((*sit)->ca_row.send_undefined < vp->ca_row.send_undefined)
			{
				vp = *sit;
			}
		}

	}
	else if (grey.size() > 0)
	{
		// vp = grey[rand() % grey.size()];
		sit = grey.begin(); 
		vp = *sit;

		for ( ; sit != grey.end(); sit++)
		{
			if ((*sit)->ca_row.send_undefined < vp->ca_row.send_undefined)
			{
				vp = *sit;
			}
		}
	}
	else if (cong.size() > 0)
	{
		// vp = cong[rand() % cong.size()];
		sit = cong.begin(); 
		vp = *sit;

		for ( ; sit != cong.end(); sit++)
		{
			if ((*sit)->ca_row.send_undefined < vp->ca_row.send_undefined)
			{
				vp = *sit;
			}
		}
	}
	else 
	{
		map < unsigned, struct vp_record* > :: iterator iter = vp_map.begin();
		
		int r = rand() % vp_map.size();

		while(r--)
		{
			iter++;
		}

		vp = iter->second;
	}


	if (vp == NULL)
	{
		fprintf(stderr, "[hermes-processor vp_next] max_vp==NULL  DID NOT GET A VALID VP.\n");
	}


	return vp;
}


struct vp_record* HermesProcessor::vp_get(unsigned hashkey)
{
	map < unsigned, struct vp_record* > :: iterator it = vp_map.find(hashkey);

	if (it == vp_map.end())
		return NULL;

	return it->second;
}


// struct vp_record* HermesProcessor::vp_burst_get(struct vp_record* vp)
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

struct ca_response* HermesProcessor::ca_alloc(unsigned hashkey) 
{
	struct ca_response* ca = new struct ca_response();

	init_ca_response(ca);
	ca->hashkey = hashkey;

	ca_map[hashkey] = ca;
	ca_queue.push(hashkey);

	return ca;
}

void HermesProcessor::ca_free(unsigned hashkey) 
{

	map < unsigned, struct ca_response* > :: iterator it = ca_map.find(hashkey);

	if (it == ca_map.end())
		return;

	struct ca_response* ca = it->second;
	delete ca;
	ca = NULL;
	ca_map.erase(it);
}


struct ca_response* HermesProcessor::ca_next()
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

struct ca_response*	HermesProcessor::ca_get(unsigned hashkey)
{
	map < unsigned, struct ca_response* > :: iterator it = ca_map.find(hashkey);
	return ( it == ca_map.end() ? NULL : it->second );
}


void HermesProcessor::vp_init()
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


// void HermesProcessor::vp_burst(struct vp_record* vp)
// {
// 	struct ca_record* ca = &vp->ca_row;
// 	double now = Scheduler::instance().clock();

// 	for (int i = BURST_PKTCNT; i > 0; i--)
// 	{

// 		Packet* p = pkt_alloc();
// 		hdr_cmn::access(p)->hermes_row.burst_id = i;
// 		hdr_cmn::access(p)->hermes_row.vp_id = vp->hashkey;
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



bool HermesProcessor::ecn_truncate(Packet* p)
{
	int flag = 1;
	map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();

	for ( ; it != vp_map.end(); ++it)
	{
		if (fabs(it->second->ca_row.weight - Hermes_WI) < 1)
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

// double HermesProcessor::cost(struct ca_record* ca)
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
// void HermesProcessor::ca_record_send(struct ca_record* ca)
// {
// 	// 1.update send_undefined
// 	unsigned send_undefined = ca->send_undefined + 1;
// 	// 2.update fresh time
// 	ca->send_undefined = send_undefined;
// 	ca->fresh_time = Scheduler::instance().clock();

// }

// void HermesProcessor::ca_record_recv(struct ca_record* ca, unsigned current_recv)
// {
// 	int recv_undefined = current_recv - ca->recv_cnt;

// 	if (recv_undefined < 0)
// 		return;

// 	ca->recv_undefined = recv_undefined;
// 	ca->fresh_time = Scheduler::instance().clock();
// }



// void HermesProcessor::vp_update_rrate(struct vp_record* vp, Packet* p)
// {
// 	struct ca_record* ca = &vp->ca_row;
// 	double now = Scheduler::instance().clock();

// 	if (hdr_cmn::access(p)->hermes_row.vp_recn == true 
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


// void HermesProcessor::update_ca_record(struct ca_record* ca_row)
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
// void HermesProcessor::update_ca_response(struct ca_response* response, Packet* p)
// {
// 	response->recv_cnt += 1;
// 	response->recv_ecn = (bool)hdr_flags::access(p)->ce() || 
// 						 (bool)hdr_cmn::access(p)->hermes_row.road_ecn;
// 	response->recv_ecn_cnt += response->recv_ecn;
// 	response->time = Scheduler::instance().clock();

// 	// char str1[128];
// 	// sprintf(str1,"[hermes-processor update_ca_response] %lf\t%p\trecv-cnt=%d",
// 	// 	Scheduler::instance().clock(),response,response->recv_cnt);
// 	// flow_debug(str1);

// }


// void HermesProcessor::update_ca_burst(struct ca_response* response, Packet* p)
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


// 	if (hdr_cmn::access(p)->hermes_row.burst_id == 1)
// 	{
// 		double delta_time = now - response->burst_time;
// 		double r_rate = (double) response->burst_cnt / delta_time;

// 		Packet* rp = pkt_alloc();
// 		struct hdr_cmn* cmnh = hdr_cmn::access(rp);
		
// 		cmnh->hermes_row.response_en = 1;
// 		cmnh->hermes_row.vp_rid = response->hashkey;
// 		cmnh->hermes_row.burst_rate = r_rate;

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


// Packet* HermesProcessor::pkt_alloc()
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
















// void HermesProcessor::init_vp_record(struct vp_record* vp)
// {
// 	vp->hashkey = rand();

// 	init_ca_record(&vp->ca_row);
// }

// void HermesProcessor::init_ca_record(struct ca_record* ca_row)
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


// void HermesProcessor::init_ca_response(struct ca_response* ca)
// {
// 	ca->hashkey = 0;
// 	ca->recv_cnt = 0;
// 	ca->time = 0;
// }



void HermesProcessor::flow_debug(char* str, char* file, Packet* p)
{
	// hdr_ip* iph = hdr_ip::access(p);
	// hdr_cmn* cmnh = hdr_cmn::access(p);

	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/%s-%d.tr", src_, file, dst_);
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


// void HermesProcessor::vpt_debug()
// {
// 	// hdr_ip* iph = hdr_ip::access(p);
// 	// hdr_cmn* cmnh = hdr_cmn::access(p);


// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"Hermes/%d/VPT-%d.tr",src_,dst_);
// 	FILE* fpResult=fopen(str1,"a+");
// 	if(fpResult==NULL)
//     {
//         fprintf(stderr,"Can't open file %s!\n","Hermes/route_debug.tr");
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



// void HermesProcessor::vpsend_debug()
// {
// 	// hdr_ip* iph = hdr_ip::access(p);
// 	// hdr_cmn* cmnh = hdr_cmn::access(p);

//     clock_t begin_time = clock();
	

// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"Hermes/%d/VPSend-%d.tr",src_,dst_);
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


// void HermesProcessor::vpcost_debug(int expire)
// {
//     clock_t begin_time = clock();
// 	// hdr_ip* iph = hdr_ip::access(p);
// 	// hdr_cmn* cmnh = hdr_cmn::access(p);

	

// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"Hermes/%d/VPCost-%d.tr",src_,dst_);
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



void HermesProcessor::vpSendCnt_debug()
{
    clock_t begin_time = clock();
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/VPSendCnt-%d.tr",src_,dst_);
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

    clock_t end_time = clock();
  	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    // fprintf(time_rf, "[vpSendCnt_debug elapse %lf]\n", elapsed_secs);
}


// void HermesProcessor::vpRecvCnt_debug()
// {
//     clock_t begin_time = clock();

// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"Hermes/%d/VPRecvCnt-%d.tr",src_,dst_);
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

void HermesProcessor::vpSendNew_debug()
{
	// clock_t begin_time = clock();
	
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/VPSend*-%d.tr",src_,dst_);
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


	// clock_t end_time = clock();
 //  	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    
    // fprintf(time_rf, "[vpSendNew_debug elapse %lf]\n", elapsed_secs);
}


// void HermesProcessor::vpRecvNew_debug()
// {
//     clock_t begin_time = clock();

// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"Hermes/%d/VPRecv*-%d.tr",src_,dst_);
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


// void HermesProcessor::vpFlying_debug()
// {
//     clock_t begin_time = clock();

// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"Hermes/%d/VPFlying-%d.tr",src_,dst_);
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

void HermesProcessor::vpRate_debug()
{

	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/VPRate-%d.tr",src_,dst_);
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
    	fprintf(fpResult, "%lf ",it->second->ca_row.rate);
    }
    
    fprintf(fpResult, "\n");
	fclose(fpResult);
}


void HermesProcessor::vpCWeight_debug()
{
	clock_t begin_time = clock();

	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/VPCWeight-%d.tr",src_,dst_);
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
    	fprintf(fpResult, "%lf ",it->second->ca_row.current_weight);
    }
    
    fprintf(fpResult, "\n");
	fclose(fpResult);

    clock_t end_time = clock();
  	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    
    // fprintf(time_rf, "[vpRate_debug elapse %lf] \n", elapsed_secs);
}

void HermesProcessor::vpWeight_debug()
{
    clock_t begin_time = clock();
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/VPWeight-%d.tr",src_,dst_);
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
    	fprintf(fpResult, "%lf ",it->second->ca_row.weight);
    }
    
    fprintf(fpResult, "\n");
	fclose(fpResult);

    clock_t end_time = clock();
  	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    
    // fprintf(time_rf, "[vpRRate_debug elapse %lf]\n", elapsed_secs);
}


void HermesProcessor::ipECE_debug()
{
    clock_t begin_time = clock();
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/IPECE-%d.tr",src_,dst_);
	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n",str1);
        return;
    	// return(TCL_ERROR);
    } 

    fprintf(fpResult, "%lf %u\n", Scheduler::instance().clock(),ece_cnt);

	fclose(fpResult);


	clock_t end_time = clock();
  	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    
    // fprintf(time_rf, "[ipECE_debug elapse %lf]\n", elapsed_secs);
}



void HermesProcessor::vpECE_debug()
{
    clock_t begin_time = clock();

	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/VPECE-%d.tr",src_,dst_);
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
    
    clock_t end_time = clock();
  	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
    
    // fprintf(time_rf, "[vpECE_debug elapse %lf]\n",elapsed_secs);
}


// void HermesProcessor::vpBurstSend_debug(Packet* p)
// {
// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"Hermes/%d/VPBurstSend-%d.tr",src_,dst_);
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
//     	hdr_cmn::access(p)->hermes_row.vp_id,
//     	hdr_cmn::access(p)->hermes_row.burst_id);

// 	fclose(fpResult);
// }

// void HermesProcessor::vpBurstRecv_debug(Packet* p)
// {
// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"Hermes/%d/VPBurstRecv-%d.tr",src_,dst_);
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
//     	hdr_cmn::access(p)->hermes_row.vp_id,
//     	hdr_cmn::access(p)->hermes_row.burst_id,
//     	hdr_cmn::access(p)->hermes_row.burst_rate);

// 	fclose(fpResult);
// }



// void HermesProcessor::vpBurst_debug(char* str)
// {
// 	char str1[128];
// 	memset(str1,0,128*sizeof(char));
// 	sprintf(str1,"Hermes/%d/VPBurstRecv-%d.tr",src_,dst_);
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


void HermesProcessor::vpRecvEcnCnt_debug()
{
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/VPRecvEce-%d.tr",src_,dst_);
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

void HermesProcessor::vpCARecvEcnCnt_debug()
{
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/VPCARecvEcnCnt-%d.tr",src_,dst_);
	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n",str1);
        return;
    	// return(TCL_ERROR);
    } 



	map < unsigned, struct ca_response* > :: iterator it = ca_map.begin();
    fprintf(fpResult, "%lf ",Scheduler::instance().clock());

    for ( ; it != ca_map.end(); ++it)
    {
    	fprintf(fpResult, "%u ",it->second->recv_ecn_cnt);
    }

    fprintf(fpResult, "\n");
	fclose(fpResult);
    
    
    // fprintf(time_rf, "[vpECE_debug elapse %lf]\n",elapsed_secs);
}


void HermesProcessor::vpRecvRTT_debug()
{
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/VPRTT-%d.tr",src_,dst_);
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
    	fprintf(fpResult, "%lf ",it->second->ca_row.c_rtt);
    }

    fprintf(fpResult, "\n");
	fclose(fpResult);
    
}


void HermesProcessor::vpCARecvRTT_debug()
{
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/VPCARecvRTT-%d.tr",src_,dst_);
	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n",str1);
        return;
    	// return(TCL_ERROR);
    } 



	map < unsigned, struct ca_response* > :: iterator it = ca_map.begin();
    fprintf(fpResult, "%lf ",Scheduler::instance().clock());

    for ( ; it != ca_map.end(); ++it)
    {
    	fprintf(fpResult, "%lf ",it->second->recv_rtt);
    }

    fprintf(fpResult, "\n");
	fclose(fpResult);
    
}


void HermesProcessor::vpRecvEcnRatio_debug()
{
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/VPEcnRatio-%d.tr",src_,dst_);
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
    	fprintf(fpResult, "%lf ",it->second->ca_row.vp_ecn_ratio);
    }

    fprintf(fpResult, "\n");
	fclose(fpResult);
}


void HermesProcessor::vpCARecvEcnRatio_debug()
{
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/VPCARecvEcnRatio-%d.tr",src_,dst_);
	FILE* fpResult=fopen(str1,"a+");
	if(fpResult==NULL)
    {
        fprintf(stderr,"Can't open file %s!\n",str1);
        return;
    	// return(TCL_ERROR);
    } 



	map < unsigned, struct ca_response* > :: iterator it = ca_map.begin();
    fprintf(fpResult, "%lf ",Scheduler::instance().clock());

    for ( ; it != ca_map.end(); ++it)
    {
    	vector < bool > & vt = it->second->ecn_bitmap;
    	int ecn_cnt = 0;
    	double ratio = 0;

    	for (int i = 0; i < vt.size(); i++)
    		ecn_cnt += vt[i];

    	ratio = (double) ecn_cnt / vt.size();

    	fprintf(fpResult, "%d <-> %lf ", ecn_cnt, ratio);
    }

    fprintf(fpResult, "\n");
	fclose(fpResult);
}


void HermesProcessor::vpRank_debug()
{
	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"Hermes/%d/VPRank-%d.tr",src_,dst_);
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
    	fprintf(fpResult, "%d ",it->second->ca_row.rank);
    }

    fprintf(fpResult, "\n");
	fclose(fpResult);
}