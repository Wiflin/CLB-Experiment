/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) Xerox Corporation 1997. All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linking this file statically or dynamically with other modules is making
 * a combined work based on this file.  Thus, the terms and conditions of
 * the GNU General Public License cover the whole combination.
 *
 * In addition, as a special exception, the copyright holders of this file
 * give you permission to combine this file with free software programs or
 * libraries that are released under the GNU LGPL and with code included in
 * the standard release of ns-2 under the Apache 2.0 license or under
 * otherwise-compatible licenses with advertising requirements (or modified
 * versions of such code, with unchanged license).  You may copy and
 * distribute such a system following the terms of the GNU GPL for this
 * file and the licenses of the other code concerned, provided that you
 * include the source code of that other code when and as the GNU GPL
 * requires distribution of source code.
 *
 * Note that people who make modified versions of this file are not
 * obligated to grant this special exception for their modified versions;
 * it is their choice whether to do so.  The GNU General Public License
 * gives permission to release a modified version without this exception;
 * this exception also makes it possible to release a modified version
 * which carries forward this exception. 
 *  
 * @(#) $Header: /cvsroot/nsnam/ns-2/apps/udp.h,v 1.15 2005/08/26 05:05:28 tomh Exp $ (Xerox)
 */

#ifndef ns_clb_flow_classify_h
#define ns_clb_flow_classify_h

#include "agent.h"
#include "packet.h"
#include "clb_tunnel.h"
#include "classifier.h"

class CLBTunnelAgent;

// class EncodeTimer : public TimerHandler {
// public: 
// 	EncodeTimer(CLBTunnelAgent *a) : TimerHandler() { a_ = a; }
// protected:
// 	virtual void expire(Event *e);
// 	CLBTunnelAgent *a_;
// };

class CLBFlowClassifier
{
public:
	CLBFlowClassifier(Classifier* classifier_, int N, int P)
	{
		ParentClassifier_ = classifier_;
		BlockSize_N=N;
		BlockSize_P=P;
		init();
		// printf("%lf-NODE %d: CLBFlowClassifier finish intialization\n",Scheduler::instance().clock(),ParentClassifier_->nodeID());
	};
	~CLBFlowClassifier();
	CLBTunnelAgent* getCLBTunnelAgent(int otherIP);
	void shrinkFlowTable();

protected:
	Classifier* ParentClassifier_;
	int BlockSize_N;
	int BlockSize_P;
	void init();
	void expandOneFlowTableEntry(int otherIP);
	struct FLOW_TABLE
	{
		FLOW_TABLE(): otherIP(0), clbTunnelAgent(NULL), age(0), valid(0), hitTimes(0)
		{}
		int otherIP;
		CLBTunnelAgent* clbTunnelAgent; 
		int age; //////Every hitting packet will set the bit.  Timer will check if the bit was set, and reset it 
		int valid;
		int hitTimes;
	}* flowTable;
	unsigned short maxActiveFlowNumber;
	unsigned short invalidEntryNumber;
};

#endif
