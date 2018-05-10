/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */

/*
 * Copyright (C) 1999 by the University of Southern California
 * $Id: classifier-mcast.h,v 1.7 2005/08/25 18:58:01 johnh Exp $
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


#ifndef clb_processor_h
#define clb_processor_h

#include "ip.h"
#include <math.h>
#include <time.h>  
#include <vector>
#include <sys/stat.h> 
#include <map>
#include <queue>

class Node;
class Classifier;
class CLB;
class CLBProcessor;

// #####################################
// used by a sender
struct ca_record
{
	unsigned send_cnt;
	unsigned recv_cnt;
	unsigned send_undefined;
	int recv_undefined;
	double	 flying;
	double 	 rate;
	double	 r_rate;
	double 	 fresh_time;
	double	 update_time;
	double	 r_time;	


	unsigned recv_ece_cnt; // for debug
	//just for ns2 simulator, it isn't needed in real
	// double 	 last_update_time;

	//control data
	int 	valid;
	int 	pending;
};
struct vp_record {
	unsigned hashkey;
	struct ca_record ca_row;
};

//#######################################
// used by a receiver
// round robin by a queue 
struct ca_response {
	unsigned hashkey;
	unsigned recv_cnt;
	bool	 recv_ecn;
	int 	 burst_pending;
	int 	 burst_cnt;
	double	 burst_time;
	double time;
};

// ########################################

class CLBProcessorTimer : public TimerHandler {
public: 
	CLBProcessorTimer(CLBProcessor * a) : TimerHandler() { a_ = a; }
protected:
	virtual void expire(Event *e);
	CLBProcessor *a_;
};



class CLBProcessor
{
public:
	CLBProcessor(Node*, Classifier*, CLB*, int, int);
	~CLBProcessor();

	int recv(Packet* p, Handler*h);
	int send(Packet* p, Handler*h);

	void expire(Event *e);

protected:
	
	//virtual path record reference
	struct vp_record* 	vp_alloc();
	void				vp_free(unsigned);
	struct vp_record* 	vp_next_at_ratio();
	struct vp_record* 	vp_next_at_cost();
	struct vp_record*	vp_next();	// SHOULD ALLOC INSTANCE IF IT'S NEEDED
	struct vp_record*	vp_get(unsigned);	// DO NOT NEED TO ALLOC INSTANCE
	//congestion aware record reference
	struct ca_response*	ca_alloc(unsigned);
	void				ca_free(unsigned);
	struct ca_response*	ca_next();
	struct ca_response*	ca_get(unsigned);	// DO NOT NEED TO ALLOC INSTANCE
	//init
	void vp_init();
	void vp_burst(struct vp_record* vp);

	// function s
	// double calculate_rate(struct ca_response*, int);
	double cost(struct ca_record* ca);
	void ca_record_send(struct ca_record* ca);
	void ca_record_recv(struct ca_record* ca, unsigned current_recv);
	void vp_update_rrate(struct vp_record* ca, Packet* p);
	void update_ca_record(struct ca_record* ca_row);
	void update_ca_response(struct ca_response* response, Packet* p);
	void update_ca_burst(struct ca_response* response, Packet* p);
	// function packet send
	Packet* pkt_alloc();

	Node* 		n_;
	Classifier*	c_;
	CLB* 		clb_;
	CLBProcessorTimer pt_;
	int 		src_;
	int 		dst_;

	int flag;	// for debug
	int cost_flag;	// for debug
	unsigned hashkey_counter;
	unsigned ece_cnt;
	unsigned max_ece_cnt;

	// just for fun?
	unsigned			sequence;
	struct ca_record	global_ca;
	struct ca_response	global_response;

	// queue < vp_record** >		vp_queue;
	map < unsigned, struct vp_record* >		vp_map;

	queue < unsigned >		ca_queue;
	map < unsigned, struct ca_response* >	ca_map;


	inline double init_rate() {
		// double irc = 1;
		// unsigned vpcnt = 0;
		// map < unsigned, struct vp_record* > :: iterator it = vp_map.begin();
		
		// for ( ; it != vp_map.end(); ++it)
		// {
		// 	if (it->second->ca_row.valid == 0)
		// 		continue;

		// 	vpcnt += 1;
		// 	irc += it->second->ca_row.rate;

		// }
		// return (vpcnt ? (irc / vpcnt) : irc);

		return 1.0;
	}

	inline void init_vp_record(struct vp_record* vp) {
		vp->hashkey = 0;
		init_ca_record(&vp->ca_row);
	}

	inline void init_ca_record(struct ca_record* ca_row) {
		ca_row->send_cnt = 0;
		ca_row->recv_cnt = 0;
		ca_row->send_undefined = 0;
		ca_row->recv_undefined = 0;

		// maybe change to current average of rate
		ca_row->rate = init_rate();
		ca_row->r_rate = 1E+37;
		ca_row->flying = 0;

		ca_row->update_time = Scheduler::instance().clock();
		ca_row->fresh_time = Scheduler::instance().clock();
		ca_row->r_time = Scheduler::instance().clock();
		// ca_row->last_update_time = Scheduler::instance().clock();
		ca_row->valid = 1;
		ca_row->pending = 0;
		ca_row->recv_ece_cnt = 0;
	}

	inline void init_ca_response(struct ca_response* ca) {
		ca->hashkey = 0;
		ca->recv_cnt = 0;
		ca->recv_ecn = 0;
		ca->burst_pending = 0;
		ca->burst_cnt = 0;
		ca->burst_time = Scheduler::instance().clock();
		ca->time = Scheduler::instance().clock();
	}



	void flow_debug(char* str, char* file = "Processor", Packet* p = NULL);
	void vpt_debug();
	void vpsend_debug();
	void vpcost_debug(int expire=0);
	void vpSendCnt_debug();
	void vpRecvCnt_debug();
	void vpSendNew_debug();
	void vpRecvNew_debug();
	void vpFlying_debug();
	void vpRate_debug();
	void vpRRate_debug();
	void ipECE_debug();
	void vpECE_debug();
	void vpBurstSend_debug(Packet* p);
	void vpBurstRecv_debug(Packet* p);
	void vpBurst_debug(char* str);
	
};



#endif