/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1996-1997 Regents of the University of California.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the MASH Research
 * 	Group at the University of California Berkeley.
 * 4. Neither the name of the University nor of the Research Group may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) $Header: /cvsroot/nsnam/ns-2/classifier/classifier.h,v 1.34 2010/03/08 05:54:49 tom_henderson Exp $ (LBL)
 */

#ifndef ns_classifier_h
#define ns_classifier_h

#include "object.h"
// #include "node.h"


class Node;
class Packet;

class Conga;
class CLB;
class CLBFlowClassifier;
class CLOVE;
class Hermes;

class Classifier : public NsObject {
public:
	Classifier(); 
	virtual ~Classifier();

	inline int maxslot() const { return maxslot_; }
	inline NsObject* slot(int slot) {
		if ((slot >= 0) && (slot < nslot_))
			return slot_[slot];
		return 0;
	}
	inline int mshift(int val) { return ((val >> shift_) & mask_); }
	inline void set_default_target(NsObject *obj) { 
		default_target_ = obj;
	}

	virtual void recv(Packet* p, Handler* h);
	virtual NsObject* find(Packet*);
	virtual int classify(Packet *);
	virtual void clear(int slot);
	enum classify_ret {ONCE= -2, TWICE= -1};
	
	virtual void do_install(char* dst, NsObject *target) {
		int slot = atoi(dst);
		install(slot, target); }
	int install_next(NsObject *node);
	virtual void install(int slot, NsObject*);

	// function to set the rtg table size
	void set_table_size(int nn);
	// hierarchical specific
	virtual void set_table_size(int level, int nn);

	int allocPort (NsObject *);	

	inline int nodeID() {return nodeID_;}///CG add
	inline int sLeaf_Conga() {return sLeaf_Conga_;}///CG add
	inline int minHost() {return minHost_;}///CG add
	inline int maxHost() {return maxHost_;}///CG add
	inline int LeafNumber() {return LeafNumber_;}///CG add
	inline int PortNumber() {return PortNumber_;}///CG add
	inline int CLB_Node() {return CLB_Node_;}///CG add
	inline double bandwidth() {return bandwidth_;}////CG add

	int conga_enabled();	//WF add
	Conga* conga_(); 	//WF add

protected:
	char instance_name[40];
	virtual int getnxt(NsObject *);
	virtual int command(int argc, const char*const* argv);
	void alloc(int);
	NsObject** slot_;	/* table that maps slot number to a NsObject */
	int nslot_;
	int maxslot_;
	int offset_;		// offset for Packet::access()
	int shift_;
	int mask_;
	NsObject *default_target_;
	int nsize_;       //what size of nslot_ should be

	int fLayer_;
	int nodeID_; ///CG add!
	int sLeaf_Conga_;///CG add!
	int minHost_;///CG add!
	int maxHost_;///CG add!
	int LeafNumber_;///CG add!
	int PortNumber_;///CG add
	int randSalt_;////CG add!
	int loadBalancePerPacket_; //WF add
	int loadBalanceFlowlet_;	//WF add

	int CLB_Node_;///CG add!

	int BlockSize_N_; ///CG add!
	int BlockSize_P_;
	// CLBTunnelAgent* clb_tunnel_;//CG add
	CLBFlowClassifier* clb_flow_classifier_;///CG add
	int ifTunnel;//CG add
	double bandwidth_;//CG add!
	
	Node *n_;		//WF add	/* container node pointer */	

	Conga* conga_instance; //WF add
	
	int clb_enabled;	//clb flag
	CLB* clb_; 		//CLB instance 


	int clove_enabled;	//clove flag
	CLOVE* clove_; 		//CLOVE instance 

	int hermes_enabled;	//hermes flag
	Hermes* hermes_; 		//Hermes instance 

};

#endif
