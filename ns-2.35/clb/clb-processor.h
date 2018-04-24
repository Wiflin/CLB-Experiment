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
#include "clb.h"

class Node;
class Classifier;

// #####################################
// used by a sender
struct ca_record
{
	unsigned send_cnt;
	unsigned recv_cnt;
	unsigned send_undefined;
	unsigned recv_origin;
	double 	 rate;
	double 	 fresh_time;
	double	 update_time;

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
	double time;
};

// ########################################

class CLBProcessor
{
public:
	CLBProcessor(Node*, Classifier*, CLB*);
	~CLBProcessor();

	int recv(Packet* p, Handler*h);
	int send(Packet* p, Handler*h);

private:
	//virtual path record reference
	struct vp_record* 	vp_alloc();
	void				vp_free();
	struct vp_record*	vp_next();
	struct vp_record*	vp_get(unsigned);
	//congestion aware record reference
	struct ca_response*	ca_alloc(unsigned);
	void				ca_free();
	struct ca_response*	ca_next();
	struct ca_response*	ca_get(unsigned);

	// function s
	// double calculate_rate(struct ca_response*, int);
	void ca_record_send(struct ca_record* ca);

	void init_vp_record(struct vp_record* vp);
	void init_ca_record(struct ca_record* ca_row);
	void init_ca_response(struct ca_response* ca);
	void update_ca_record(struct ca_record* ca_row, unsigned current_recv);
	void update_ca_response(struct ca_response* response);

	Node* 		n_;
	Classifier*	c_;
	CLB* 		clb_;

	// just for fun?
	unsigned			sequence;
	struct ca_record	global_ca;
	struct ca_response	global_response;

	queue < vp_record** >		vp_queue;
	map < int, vp_record** >		vp_map;

	queue < ca_response** >		ca_queue;
	map < int, ca_response** >	ca_map;
};



#endif