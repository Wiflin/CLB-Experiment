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



CLBProcessor::CLBProcessor(Node* node, Classifier* classifier, CLB* clb)
 : n_(node), c_(classifier), clb_(clb)
{
	srand(time(NULL));

	global_ca = {
		0, 0, Scheduler::instance().clock(), -1, 1, 1
	};
}



void CLBProcessor::recv(Packet* p, Handler*h)
{



}



void CLBProcessor::send(Packet* p, Handler*h)
{
	if ((clb_->VP_Module & clb_->CA_Module) == 0)
		return;


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
		global_ca.send_cnt += 1;
		clb_sequence = global_ca.send_cnt;
		clb_recv_cnt = global_ca.recv_cnt;
	}

	// both are enabled
	if ((clb_->VP_Module & clb_->CA_Module) == 1)
	{
		vp->ca_row.send_cnt += 1;

		if ((ca = ca_next()) != NULL)
		{
			clb_recv_vpid = ca->hashkey;
			clb_recv_cnt = ca->recv_cnt;
		}
	}



	cmnh->ecmpHashKey = clb_hashkey;
	cmnh->clb_row.vp_id = clb_hashkey;	
	cmnh->clb_row.SN_HSN = clb_sequence;
	cmnh->clb_row.record_en = 1;

	cmnh->clb_row.vp_rid = clb_recv_vpid;
	cmnh->clb_row.vp_rcnt = clb_recv_cnt;
	cmnh->clb_row.response_en = 1;
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