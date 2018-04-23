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


#define RATE_ALPHA ((double)0.25)

CLBProcessor::CLBProcessor(Node* node, Classifier* classifier, CLB* clb)
 : n_(node), c_(classifier), clb_(clb)
{
	srand(time(NULL));

	sequence = 0;

	init_ca_record(&global_ca);

	global_response = {
		0, 0, 0
	};
}



int CLBProcessor::recv(Packet* p, Handler*h)
{
	if ((clb_->VP_Module & clb_->CA_Module) == 0)
		return 0;

	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);

	// make no sense
	if (cmnh->clb_row.record_en != 1 || 
		cmnh->clb_row.response_en != 1 )
		return 0;

	// if only VP_Module is enabled, the sender will do nothing
	if (clb_->VP_Module == 1)
		;

	// if only CA_Module is enabled, the sender will 
	// 1. update the global_ca_row as a sender
	// 2. update the global_ca_response as a receiver
	// @cautious!	it make no sense when both module are enabled!
	if (clb_->CA_Module == 1)
	{
		update_ca_record(&global_ca, cmnh->clb_row.vp_rcnt);
	}


	if ((clb_->VP_Module & clb_->CA_Module) == 1)
	{


	return 0;
}



int CLBProcessor::send(Packet* p, Handler*h)
{
	if ((clb_->VP_Module & clb_->CA_Module) == 0)
		return 0;


	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);


	unsigned	clb_hashkey = 0;
	unsigned	clb_sequence = 0;
	unsigned	clb_recv_vpid = 0;
	unsigned	clb_recv_cnt = 0;
	vp_record*		vp = NULL;
	ca_response*	ca = NULL;

	// only virtual path enbled
	if (clb_->VP_Module == 1)
	{
		vp = vp_next();
		clb_hashkey = vp->hashkey;
	}

	// only congestion module enabled
	if (clb_->CA_Module == 1)
	{
		// send
		sequence += 1;
		ca_record_send(&global_ca);
		clb_sequence = sequence;
		// response
		clb_recv_cnt = global_response.recv_cnt;
	}

	// both are enabled
	if ((clb_->VP_Module & clb_->CA_Module) == 1)
	{
		ca_record_send(&vp->ca_row);

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

	return 0;
}



vp_record* CLBProcessor::vp_alloc(unsigned hashkey)
{
	vp_record*	vp = new vp_record();
	vp_record** vpp = (vp_record**) new char[sizeof(&vp)];

	// assign vp address to vp pointer
	*vpp = vp;

	assert(vp && vpp);
	assert(*vpp == vp);

	vp_queue.push(vpp);
	vp_map[hashkey] = vpp;
}


void CLBProcessor::vp_free() 
{

}

vp_record* CLBProcessor::vp_next()
{

}



ca_response* CLBProcessor::ca_alloc() 
{

}

void CLBProcessor::ca_free() 
{
	
}


ca_response* CLBProcessor::ca_next()
{
	while (!ca_queue.empty())
	{
		ca_response* t = ca_queue.pop();

	}
}



double CLBProcessor::calculate_rate(struct ca_response* record, int pkts)
{
	// @todo how to calculate the rate 
	// what is the 'minimun' and 'maximun' of the rate

	// 1. becareful when the record is first used 
	// (some variable may be initial with a uncommon value)


	// 2. becareful about the inf/nan val
	double time = Scheduler::instance().clock() - record->time;
	unsigned cnt = pkts - record->recv_cnt;

}


// send variable incr
void CLBProcessor::ca_record_send(struct ca_record* ca)
{
	// 1.update send_undefined

	// 2.update fresh time

}






void CLBProcessor::init_vp_record(struct vp_record* vp)
{
	vp->hashkey = rand();

	init_ca_record(&vp->ca_row);
}

void CLBProcessor::init_ca_record(struct ca_record* ca_row)
{
	ca_row->send_cnt = 0;
	ca_row->recv_cnt = 0;
	ca_row->send_undefined = 0;
	ca_row->recv_origin = 0;
	ca_row->update_time = Scheduler::instance().clock();
	ca_row->fresh_time = Scheduler::instance().clock();
	// ca_row->last_update_time = Scheduler::instance().clock();

	// maybe change to current average of rate
	ca_row->rate = 0;
	ca_row->valid = 0;
	ca_row->pending = 0;
}


void CLBProcessor::init_ca_response(struct ca_response* ca)
{
	ca->hashkey = 0;
	ca->recv_cnt = 0;
	ca->time = 0;
}


void CLBProcessor::update_ca_record(struct ca_record* ca_row, unsigned current_recv)
{
	double now = Scheduler::instance().clock();
	unsigned delta_recv = current_recv - ca_row->recv_origin;
	double	 delta_time = now - ca_row->update_time;

	// 1. calculate new rate
	// double rate = calculate_rate(&global_response, rcnt);
	double old_rate = ca_row->rate;
	double calculate_rate = (double) delta_recv / delta_time;
	double new_rate = (1 - RATE_ALPHA) * old_rate + RATE_ALPHA * calculate_rate;
	ca_row->rate = new_rate;

	// 2. calculate new recv count

	// 3. calculate new send count

	// 4. update recv origin count
	ca_row->recv_origin = current_recv;

	// 5. update timer
	ca_row->update_time = ca_row->fresh_time = Scheduler::instance().clock();

}