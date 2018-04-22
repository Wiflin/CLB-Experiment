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


#ifndef clb_conga_h
#define clb_conga_h

#include "ip.h"
#include <math.h>
#include <time.h>  
#include <vector>
#include <sys/stat.h> 
#include <map>



class Node;
class Queue;
class Classifier;

class Conga
{
public:
	// Conga() : n_(0)
	// 	, slots_(-1)
	// 	, leafDownPortNumber(0)
	// 	 {}
	// Conga(int slots) : n_(0)
	// 	, slots_(slots)
	// 	, leafDownPortNumber(0)
	// 	{}
	Conga(Node* node, int slots, int leafDownPortNumber);
	~Conga() {
		// @todo need to free all table row
	}

	int conga_enabled();
	int route(Packet* p, Classifier* c_);
	void recv(Packet* p, Classifier* c_);


	// for debug
	int route_queue(const char* r_, const char* q_);
	inline void route_debug_set(int flag) {
		route_debug_ = flag;
		if (route_debug_)
		{
			mkdir("CLB",0777);
			system("exec rm -r -f CLB/*");
		}
	}
	inline void all_pkts_debug_set(int flag) {
		all_pkts_debug_ = flag;
		if (all_pkts_debug_)
		{
			mkdir("CLB",0777);
			system("exec rm -r -f CLB/*");
		}
	}
	void packetPrint(Packet* p,Classifier* c,char* file_str,char* debug_str);
	
protected:
	
		
	int serv2leaf(int dst); 
	void route_debug(Packet* p, int r_);
	void packet_debug(Packet* p, int r_);

	Node* n_;
	int slots_;
	int leafDownPortNumber_;
	int route_debug_;
	int all_pkts_debug_;

	map < int, Queue* > queueMap;
	map < int, int* > route_table_;
	map < int, map < int, int > > response_table_;
	
};


#endif