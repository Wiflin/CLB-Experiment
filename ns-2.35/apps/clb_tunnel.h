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

#ifndef ns_clb_tunnel_h
#define ns_clb_tunnel_h

#include "agent.h"
#include "packet.h"
#include "classifier.h"

class CLBTunnelAgent;

class EncodeTimer : public TimerHandler {
public: 
	EncodeTimer(CLBTunnelAgent *a) : TimerHandler() { a_ = a; }
protected:
	virtual void expire(Event *e);
	CLBTunnelAgent *a_;
};

class DecodeTimer : public TimerHandler {
public: 
	DecodeTimer(CLBTunnelAgent *a) : TimerHandler() { a_ = a; }
protected:
	virtual void expire(Event *e);
	CLBTunnelAgent *a_;
};


class CLBTunnelAgent {
public:
	CLBTunnelAgent(Classifier* agent_, int N, int P);
	~CLBTunnelAgent();	
	int BlockSize_K;
	Classifier* ParentClassifier_;
	void recv(Packet* pkt, Handler*h);
	void encode(Classifier* ParentClassifier, Packet* pkt, Handler* hdl);
	void decode(Classifier* ParentClassifier, Packet* pkt, Handler* hdl);
	void encodeTimeout();
	void decodeTimeout();
	int flowID;
	

protected:
	int currentFeedbackVP;
	int currentGenID;
	int currentGenIndex;
	int expectHandOverGenIndex;
	int headReorderBuffer;

	int * FeedbackVPAckedPackets;
	int LatestChangedFeedbackVPAckedPackets;
	int * VPPacketsSent;
	int * VPPacketsAcked;
	int * VPPacketsOnFly;
	double * VPRateEstimation;
	double * VPLastUpdateTime;
	double * VPCost;
	int usedVP;
	int countExploredOnceVP;


	
	void setPacket(Packet* pkt, int ifParity);
	void initCLBTunnel();
	
	void checkReorderBuffer(Classifier* ParentClassifier, Handler* hdl);////Decode reorder buffer and hand over in sequence packets
	void decodeReorderBuffer(Classifier* ParentClassifier, Handler* hdl);
	void createParityPackets(Classifier* ParentClassifier, Handler* hdl);
	
	
	///These two functions clear reorder gen. But don't set new genID and headIndex for this buffer
	void handOverAndClearBeforeIndex(int genIndex);
	void handOverAndClearAll();
	void clearParityBuffer(int bufferID);

	void resetParityBuffer(int headGenID);

	void insertReorderBuffer(Packet* pkt);

	void updateParityBuffInfo(int baseBuffID);
	void countPacketsInGen(int bufferID);


	int packetsUnHandedInReorderBuffer;
	struct ReorderBuffer{
		Packet** buffer;
		int head; ////head of reorderBuffer_
		int count;///How many packets are in reorderbuffer right now.
		int capacity;
		int headIndex;
	} reorderBuffer_;///reorderBuffer_[ReorderGenNumber*BlockSize_K]

	int packetsCountInParityBuffer;
	struct ParityBuffer{
		Packet** buffer;
		int capacity;
		int ifUpdated;

		///For reorderBuffer
		int genID;
		int count;///How many packets are in reorderbuffer right now.
		int blockSize;
		int headIndex;
		int lastGenOriginalIndex;
	}* parityBuffer_;///parityBuffer_[ReorderGenNumber]*[BlockSize_MaxP]

	struct EncodeBuffer{
		Packet** buffer;
		int head; 
		int tail;  
		int length;
		int capacity;
	} encodeBuffer_;////////////encodeBuffer_[BlockSize_K]


	Packet* ParityWaitToBeSent;

	EncodeTimer encodeTimer_;
	DecodeTimer decodeTimer_;

	int * VPSelectCondition;

	void printSeqTimeline(Packet* pkt);
	void printVPWindowTimeline(Packet* pkt);
	void printVPWindowTimeline(Packet* pkt, int force);
	void PrintVPWindow();
	void PrintVPWindow(int force);
	void PrintPacket(Packet* pkt, char SendorReceive[10]);
	void PrintPacket(Packet* pkt, char SendorReceive[10], int force);
	void PrintReorderBuffer(int force);
	void PrintEncodeBuffer(int force);

	int findNextFeedbackVP(int vp);
	int setVP(Packet* pkt);
	int createNewVP();
	int findBestVP();
	void clearWorstVP();
	

	int BlockSize_N;
	int BlockSize_MaxP;
	int BlockSize_MaxK;
	int currentGeneratedParityNumber;

	void targetRecv(Packet* p, Handler*h)
	{
		NsObject* node = ParentClassifier_->find(p);
		if (node == NULL) {
	 		Packet::free(p);
			return;
		}
		node->recv(p,h);///original ends
		// ParentClassifier_->recv(p,h);
	};

	double codingRateBlockSize;/////Defined to be originalPacketNumber in a generation
};




#endif
