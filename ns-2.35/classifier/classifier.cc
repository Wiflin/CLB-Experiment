/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1996 Regents of the University of California.
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
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/classifier/classifier.cc,v 1.44 2010/03/08 05:54:49 tom_henderson Exp $";
#endif

#include <stdlib.h>
#include "config.h"
#include "classifier.h"
#include "packet.h"
#include "node.h"
#include "clb_flow_classify.h"///CG add
#include "clb_tunnel.h"///CG add
#include "ip.h"	///CG add
#include "clb/conga.h"	////WF add
#include "clb/clb.h"	////WF add 

static class ClassifierClass : public TclClass {
public:
	ClassifierClass() : TclClass("Classifier") {}
	TclObject* create(int, const char*const*) {
		return (new Classifier());
	}
} class_classifier;


Classifier::Classifier() : 
	slot_(0), nslot_(0), maxslot_(-1), shift_(0), mask_(0xffffffff), nsize_(0)
	, CLB_Node_(0), BlockSize_N_(0), BlockSize_P_(0), ifTunnel(0), bandwidth_(0)
	, n_(NULL), conga_instance(NULL), clb_(NULL), clb_enabled(0), nodeID_(-1)
{
	default_target_ = 0;

	sprintf(instance_name,"Null-Classifier");

	bind("offset_", &offset_);
	bind("shift_", &shift_);
	bind("mask_", &mask_);
}

int Classifier::classify(Packet *p)
{
	return (mshift(*((int*) p->access(offset_))));
}

Classifier::~Classifier()
{
	delete [] slot_;
}

void Classifier::set_table_size(int nn)
{
	nsize_ = nn;
}

void Classifier::alloc(int slot)
{
	NsObject** old = slot_;
	int n = nslot_;
	if (old == 0) 
	    {	
		if (nsize_ != 0) {
			//printf("classifier %x set to %d....%dth visit\n", this, nsize_, i++);
			nslot_ = nsize_;
		}
		else {
			//printf("classifier %x set to 32....%dth visit\n", this, j++);
			nslot_ = 32;
		}
	    }
	while (nslot_ <= slot) 
		nslot_ <<= 1;
	slot_ = new NsObject*[nslot_];
	memset(slot_, 0, nslot_ * sizeof(NsObject*));
	for (int i = 0; i < n; ++i)
		slot_[i] = old[i];
	delete [] old;
}


void Classifier::install(int slot, NsObject* p)
{
	if (slot >= nslot_)
		alloc(slot);
	slot_[slot] = p;
	if (slot >= maxslot_)
	{
		maxslot_ = slot;
		randSalt_=rand(); //CG add
	}
}

void Classifier::clear(int slot)
{
	// printf("%lf-Node-%d: clear slot=%d, maxslot_=%d,"
	// 	,Scheduler::instance().clock()
	// 	,nodeID_
	// 	,slot
	// 	,maxslot_
	// 	);
	slot_[slot] = 0;
	if (slot == maxslot_) {
		while (--maxslot_ >= 0 && slot_[maxslot_] == 0)
			;
		randSalt_=rand(); //CG add
	}
	// printf("after cleared maxslot_=%d\n"
	// 	,maxslot_
	// 	);
	// fflush(stdout);
}

int Classifier::allocPort (NsObject *nullagent)
{
	return getnxt (nullagent);
}

int Classifier::getnxt(NsObject *nullagent)
{
	int i;
	for (i=0; i < nslot_; i++)
		if (slot_[i]==0 || slot_[i]==nullagent)
			return i;
	i=nslot_;
	alloc(nslot_);
	return i;
}

/*
 * objects only ever see "packet" events, which come either
 * from an incoming link or a local agent (i.e., packet source).
 */
void Classifier::recv(Packet* p, Handler*h)
{

	clock_t begin_time = clock();
	// if (hdr_ip::access(p)->src_.port_ == 100)
	// {
	// 	hdr_cmn* cmnh = hdr_cmn::access(p);
	// 	fprintf(stderr, "[clb-processor recv %d]some packet uncaughted! %u vp=%u burst_id=%u\n", 
	// 		nodeID_, p->uid(), cmnh->clb_row.vp_id, cmnh->clb_row.burst_id);
	// 	return;
	// }

	////WF add
	// record packet if conga flag is set in packet header
	if (conga_enabled() == 1) 
	{
		// if (hdr_cmn::access(p)->congaResponseRow.en_flag == 1)
		// 	conga_()->packetPrint(p, this, "All-Packet-Debug.tr", "classifier-recv with route en_flag");
		// else
		// 	conga_()->packetPrint(p, this, "All-Packet-Debug.tr", "classifier-recv");

		conga_()->recv(p, this);


		hdr_ip* iph = hdr_ip::access(p);
		hdr_cmn* cmnh = hdr_cmn::access(p);
		
		// FILE* fpResult=fopen("All-Packet-Debug.tr","a+");
		// if(fpResult==NULL)
	 //    {
	 //        fprintf(stderr,"Can't open file %s!\n","debug.tr");
	 //    } else {
		// 	fprintf(fpResult, "%d %lf-Node-%d-(%d->%d):flowid=%d size=%d ecmpHashKey=%u maxslot_=%d flowlet=%u\n"
		// 	,conga_enabled(),Scheduler::instance().clock(),nodeID_,iph->src_,iph->dst_,cmnh->flowID,cmnh->size_,cmnh->ecmpHashKey,maxslot_,loadBalanceFlowlet_);
		// 	fclose(fpResult);
		// }		
	}

	if (clb_enabled == 1)
	{
		assert(clb_ != 0);

		

		int truncate = clb_->recv(p, h);


		


		if (truncate)
		{

			Packet::free(p);
			return;
		}

		if (hdr_ip::access(p)->src_.port_ == 100)
		{
			hdr_cmn* cmnh = hdr_cmn::access(p);
			fprintf(stderr, "[clb-processor recv %d]%lf some packet(%u) uncaughted!  vp=%u vprid=%u burst_id=%u\n", 
				nodeID_,Scheduler::instance().clock(),p->uid(),cmnh->clb_row.vp_id, cmnh->clb_row.vp_rid, cmnh->clb_row.burst_id);
			return;
		}
	}


	double end_time1 = double(clock() - begin_time) / CLOCKS_PER_SEC;
	double end_time2 = 0;
	NsObject* node_r = NULL;
	char portr[100];
	char mode[100];
	memset(mode, 0, sizeof(mode));
	memset(portr, 0, sizeof(portr));

	/////CG add
	if(CLB_Node_==1 && ifTunnel==0)
	{
		// printf("BlockSize_N=%d,BlockSize_P=%d\n",BlockSize_N_,BlockSize_P_);
		if(BlockSize_N_==0)
		{
			fprintf(stderr,"ERROR: BlockSize_N=0!\n");
			exit(0);
		}
		clb_flow_classifier_=new CLBFlowClassifier(this,BlockSize_N_,BlockSize_P_);
		// clb_tunnel_=new CLBTunnelAgent(this,BlockSize_N_,BlockSize_P_);
		ifTunnel=1;
	}

	if(CLB_Node_==1)
	{
		hdr_ip* iph = hdr_ip::access(p);
		int otherIP=0;
		if(iph->src().addr_==this->nodeID() && iph->dst().addr_==this->nodeID())
		{
			fprintf(stderr,"ERROR: srcIP=dstIP=this->nodeID()=%d!\n",this->nodeID());
			exit(0);
		}

		if(iph->src().addr_==this->nodeID())
		{
			otherIP=iph->dst().addr_;
		}
		else
		{
			otherIP=iph->src().addr_;
		}
		CLBTunnelAgent* clb_tunnel = clb_flow_classifier_->getCLBTunnelAgent(otherIP);
		
		clb_tunnel->recv(p,h);	
	}
	else
	{
		///original begins
		NsObject* node = find(p);
		if (node == NULL) {
			/*
			 * XXX this should be "dropped" somehow.  Right now,
			 * these events aren't traced.
			 */
	 		Packet::free(p);
			return;
		}


		sprintf(portr, "(%d.%d-%d.%d)",
		hdr_ip::access(p)->src().addr_,hdr_ip::access(p)->src().port_,
		hdr_ip::access(p)->dst().addr_,hdr_ip::access(p)->dst().port_ );


		if(hdr_ip::access(p)->src().addr_==this->nodeID())
		{
			sprintf(mode,"send");
		}
		else if(hdr_ip::access(p)->dst().addr_==this->nodeID())
		{
			sprintf(mode,"recv");
		}
		else
		{
			sprintf(mode,"pass");
		}

		end_time2 = double(clock() - begin_time) / CLOCKS_PER_SEC;
		node_r = node;



		node->recv(p,h);///original ends
	}
	
	clock_t end_time = clock();



	// char str2[128];
	// sprintf(str2, "time-record-%d-%p.tr", (n_ != NULL ? n_->address() : nodeID_), this);
	// FILE* time_rf = fopen(str2, "a+");
 //  	double elapsed_secs = double(end_time - begin_time) / CLOCKS_PER_SEC;
	// fprintf(time_rf, "[%s::recv elapse][%s]  %lf %lf %lf %lf %s next->%p\n", instance_name, mode, Scheduler::instance().clock(), 
	// 	end_time1, end_time2, elapsed_secs,portr,node_r);
	// fclose(time_rf);
}

/*
 * perform the mapping from packet to object
 * perform upcall if no mapping
 */

NsObject* Classifier::find(Packet* p)
{
	NsObject* node = NULL;
	int cl = classify(p);

	if (cl < 0 || cl >= nslot_ || (node = slot_[cl]) == 0) { 
		if (default_target_) 
			return default_target_;
		/*
		 * Sigh.  Can't pass the pkt out to tcl because it's
		 * not an object.
		 */
		Tcl::instance().evalf("%s no-slot %ld", name(), cl);
		if (cl == TWICE) {
			/*
			 * Try again.  Maybe callback patched up the table.
			 */
			cl = classify(p);
			if (cl < 0 || cl >= nslot_ || (node = slot_[cl]) == 0)
				return (NULL);
		}
	}
	return (node);
}

int Classifier::install_next(NsObject *node) {
	int slot = maxslot_ + 1;
	install(slot, node);
	return (slot);
}

int Classifier::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	if(argc == 2) {
                if (strcmp(argv[1], "defaulttarget") == 0) {
                        if (default_target_ != 0)
                                tcl.result(default_target_->name());
                        return (TCL_OK);
                }

                if (strcmp(argv[1], "enable-clb") == 0) {
                        if (clb_enabled == 1 && clb_ != 0)
                        	return (TCL_OK);

                        if (n_ == 0) {
                        	tcl.resultf("Classifier::%p instance did not attach to a node!", this);
                        	return(TCL_ERROR);
                        }
                        
                        clb_ = new CLB(n_, this);
                        clb_enabled = 1;

                        fprintf(stderr,"%lf-Node-%d: clb_=%p, clb_enabled_=%d initializing clb(%p,%p) \n"
							,Scheduler::instance().clock(),n_->address(),clb_,clb_enabled,n_,this);
						return TCL_OK;
                }


    } else if (argc == 3) {
		/*
		 * $classifier alloc-port nullagent
		 */
		if (strcmp(argv[1],"alloc-port") == 0) {
			int slot;
			NsObject* nullagent =
				(NsObject*)TclObject::lookup(argv[2]);
			slot = getnxt(nullagent);
			tcl.resultf("%u",slot);
			return(TCL_OK);
		}
		/*
		 * $classifier clear $slot
		 */
		if (strcmp(argv[1], "clear") == 0) {
			int slot = atoi(argv[2]);
			clear(slot);
			return (TCL_OK);
		}
		/*
		 * $classifier installNext $node
		 */
		if (strcmp(argv[1], "installNext") == 0) {
			//int slot = maxslot_ + 1;
			NsObject* node = (NsObject*)TclObject::lookup(argv[2]);
			if (node == NULL) {
				tcl.resultf("Classifier::installNext attempt "
		    "to install non-object %s into classifier", argv[2]);
				return TCL_ERROR;
			};
			int slot = install_next(node);
			tcl.resultf("%u", slot);
			return TCL_OK;
		}
		/*
		 * $classifier slot snum
		 * returns the name of the object in slot # snum
		 */
		if (strcmp(argv[1], "slot") == 0) {
			int slot = atoi(argv[2]);
			if (slot >= 0 && slot < nslot_ && slot_[slot] != NULL) {
				tcl.resultf("%s", slot_[slot]->name());
				return TCL_OK;
			}
			tcl.resultf("Classifier: no object at slot %d", slot);
			return (TCL_ERROR);
		}
		/*
		 * $classifier findslot $node
		 * finds the slot containing $node
		 */
		if (strcmp(argv[1], "findslot") == 0) {
			int slot = 0;
			NsObject* node = (NsObject*)TclObject::lookup(argv[2]);
			if (node == NULL) {
				return (TCL_ERROR);
			}
			while (slot < nslot_) {
				// check if the slot is empty (xuanc, 1/14/02) 
				// fix contributed by Frank A. Zdarsky 
				// <frank.zdarsky@kom.tu-darmstadt.de>
				if (slot_[slot] && 
				    strcmp(slot_[slot]->name(), argv[2]) == 0){
					tcl.resultf("%u", slot);
					return (TCL_OK);
				}
				slot++;
			}
			tcl.result("-1");
			return (TCL_OK);
		}
		if (strcmp(argv[1], "defaulttarget") == 0) {
			default_target_=(NsObject*)TclObject::lookup(argv[2]);
			if (default_target_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		}
		
		if (strcmp(argv[1], "attach-node") == 0) {
			n_ = (Node*)TclObject::lookup(argv[2]);
			if (n_ == NULL) {
				tcl.add_errorf("Wrong object name %s",argv[2]);
				return TCL_ERROR;
			}
			return TCL_OK;
		}
	} else if (argc == 4) {
		/*
		 * $classifier install $slot $node
		 */
		if (strcmp(argv[1], "install") == 0) {
			int slot = atoi(argv[2]);
			NsObject* node = (NsObject*)TclObject::lookup(argv[3]);
			install(slot, node);
			return (TCL_OK);
		}
	}
	return (NsObject::command(argc, argv));
}

void Classifier::set_table_size(int, int)
{}


int Classifier::conga_enabled()
{
	if (n_)
		return n_->conga_enabled(); 
	return -1;
}

Conga* Classifier::conga_() 
{
	if(conga_instance)
		return conga_instance;

	if(n_ == NULL) {
		fprintf(stderr,"Can't get bind Node.\n");
	}

	if( (conga_instance = n_->conga_get_instance()) == NULL) {
		fprintf(stderr,"Did not Enable conga!\n");
	}
	return conga_instance;
}



