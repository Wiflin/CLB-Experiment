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


#ifndef hermes_processor_h
#define hermes_processor_h

#include "ip.h"
#include <math.h>
#include <time.h>  
#include <vector>
#include <sys/stat.h> 
#include <map>
#include <queue>

class Node;
class Classifier;
class Hermes;
class HermesProcessor;

enum ptype {FAILED = 0, CONGESTED, GREY, GOOD};

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
	int 	 r_rate;
	double 	 weight;
	double 	 current_weight;
	double 	 fresh_time;
	double	 update_time;
	double	 r_time;	
	double 	 c_rtt;
	ptype 	 rank;
	double 	 vp_ecn_ratio;

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
	double 	 recv_rtt;
	unsigned recv_ecn_cnt;
	int 	 burst_pending;
	int 	 burst_cnt;
	vector<bool> ecn_bitmap;
	double 	 ecn_time;
	double 	 rtt_time;
	double	 burst_time;
	double time;
};

// ########################################

class HermesProcessorTimer : public TimerHandler {
public: 
	HermesProcessorTimer(HermesProcessor * a) : TimerHandler() { a_ = a; }
protected:
	virtual void expire(Event *e);
	HermesProcessor *a_;
};



class HermesProcessor
{
public:
	HermesProcessor(Node*, Classifier*, Hermes*, int, int, int);
	~HermesProcessor();

	int recv(Packet* p, Handler*h);
	int send(Packet* p, Handler*h);

	void expire(Event *e);

protected:
	
	//virtual path record reference
	struct vp_record* 	vp_alloc();
	void				vp_free(unsigned);
	// struct vp_record* 	vp_next_at_ratio();
	// struct vp_record* 	vp_next_at_cost();     
	struct vp_record*	vp_next();	// SHOULD ALLOC INSTANCE IF IT'S NEEDED
	struct vp_record*	vp_get(unsigned);	// DO NOT NEED TO ALLOC INSTANCE
	// struct vp_record*	vp_burst_get(struct vp_record* );

	//congestion aware record reference
	struct ca_response*	ca_alloc(unsigned);
	void				ca_free(unsigned);
	struct ca_response*	ca_next();
	struct ca_response*	ca_get(unsigned);	// DO NOT NEED TO ALLOC INSTANCE
	//init
	void vp_init();
	// void vp_burst(struct vp_record* vp);

	// hermes will truncate ecn before all vp receive ecn
	bool ecn_truncate(Packet* p);

	// function s
	// double calculate_rate(struct ca_response*, int);
	// double cost(struct ca_record* ca);
	// void ca_record_send(struct ca_record* ca);
	// void ca_record_recv(struct ca_record* ca, unsigned current_recv);
	// void vp_update_rrate(struct vp_record* ca, Packet* p);
	// void update_ca_record(struct ca_record* ca_row);
	// void update_ca_response(struct ca_response* response, Packet* p);
	// void update_ca_burst(struct ca_response* response, Packet* p);
	// function packet send
	// Packet* pkt_alloc();

	Node* 		n_;
	Classifier*	c_;
	Hermes* 		hermes_;
	HermesProcessorTimer pt_;
	int 		src_;
	int 		dst_;
	int 		VP_SIZE;


	// debug begin!
	int flag;	// for debug
	int cost_flag;	// for debug
	unsigned hashkey_counter;
	unsigned ece_cnt;
	unsigned max_ece_cnt;
	FILE* time_rf;
	// debug end!

	// just for fun?
	double 				init_rrate;
	unsigned			sequence;
	struct ca_record	global_ca;
	struct ca_response	global_response;

	double 				burst_time;
	bool 				burst_pending;	// if it is enable burst now
	struct vp_record* 	burst_vp;
	int 				burst_cnt;

	// queue < vp_record** >		vp_queue;
	map < unsigned, struct vp_record* >		vp_map;

	queue < unsigned >		ca_queue;
	map < unsigned, struct ca_response* >	ca_map;

	map < unsigned, unsigned > flow_table;

	

	void init_vp_record(struct vp_record* vp);
	void init_ca_record(struct ca_record* ca_row);
	void init_ca_response(struct ca_response* ca);



	void flow_debug(char* str, char* file = "Processor", Packet* p = NULL);
	// void vpt_debug();
	// void vpsend_debug();
	// void vpcost_debug(int expire=0);
	void vpSendCnt_debug();
	// void vpRecvCnt_debug();
	void vpSendNew_debug();
	// void vpRecvNew_debug();
	// void vpFlying_debug();
	void vpRate_debug();
	void vpCWeight_debug();
	void vpWeight_debug();
	void ipECE_debug();
	void vpECE_debug();
	// void vpBurstSend_debug(Packet* p);
	// void vpBurstRecv_debug(Packet* p);
	// void vpBurst_debug(char* str);
	void vpRecvEcnCnt_debug();
	void vpCARecvEcnCnt_debug();
	void vpRecvRTT_debug();
	void vpCARecvRTT_debug();
	void vpRecvEcnRatio_debug();
	void vpCARecvEcnRatio_debug();
	void vpRank_debug();

};



#endif