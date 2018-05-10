/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1994 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
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


#include "tcp.h"////CG add
#include "flags.h"////CG add
#include "clb-queue.h"

static class CLBQueueClass : public TclClass {
 public:
	CLBQueueClass() : TclClass("Queue/DropTail/CLBQueue") {}
	TclObject* create(int, const char*const*) {
		return (new CLBQueue);
	}
} class_clb_queue;

void CLBQueue::reset()
{
	DropTail::reset();
}

int 
CLBQueue::command(int argc, const char*const* argv) 
{
	if (argc==2) {
		if (strcmp(argv[1], "printstats") == 0) {
			print_summarystats();
			return (TCL_OK);
		}
 		if (strcmp(argv[1], "shrink-queue") == 0) {
 			shrink_queue();
 			return (TCL_OK);
 		}
	}
	if (argc == 3) {
		if (!strcmp(argv[1], "packetqueue-attach")) {
			delete q_;
			if (!(q_ = (PacketQueue*) TclObject::lookup(argv[2])))
				return (TCL_ERROR);
			else {
				pq_ = q_;
				return (TCL_OK);
			}
		}
		///CG add!
		else if (strcmp(argv[1], "port-number") == 0)
		{
		//	printf("OQ: srcNode=%d,dstNode=%d\n",last_hop,next_hop);
			return (TCL_OK);
		}
		else if (strcmp(argv[1], "ecn-threshold") == 0)
		{
			ecn_threshold_=atoi(argv[2]);
			//printf("ecn_threshold_=%d\n",ecn_threshold_);
			return (TCL_OK);
		}
		//CG add ends!
	}
	return Queue::command(argc, argv);
}

/*
 * drop-tail
 */
void CLBQueue::enque(Packet* p)
{

	int qlimBytes = qlim_ * mean_pktsize_;
	
	if(hdr_cmn::access(p)->clb_row.burst_id > 0){
		int dst = hdr_ip::access(p)->dst_.addr_;
		int vp = hdr_cmn::access(p)->ecmpHashKey;
		char dp[128];
		sprintf(dp,"%d-%d",dst,vp);
		string dstvp(dp);

		map < string, PacketQueue*> :: iterator it = bqMap.find(dstvp);
		if (it == bqMap.end())
		{
			bqMap[dstvp] = new PacketQueue;
			// fprintf(stderr, "[CLBQueue] new pq %s\n", dstvp.c_str());
		}

		PacketQueue* bq_ = bqMap[dstvp];


		if((ecn_threshold_>0) && ((bq_->length() + 1) >= ecn_threshold_))
		{
		//	printf("Tag ECN: length=%d,qib_=%d,qlim_=%d,qlimBytes=%d\n",q_->length(),qib_,qlim_,qlimBytes);
			hdr_flags* hf = hdr_flags::access(p);
			hf->ce() = 1;
		}

		if ((!qib_ && (bq_->length() + 1) >= qlim_) ||
		  	(qib_ && (bq_->byteLength() + hdr_cmn::access(p)->size()) >= qlimBytes)){
			drop(p);
		} else {
			bq_->enque(p);

			// fprintf(stderr, "[bq_] %lf %d-%d %d %d\n",
			// Scheduler::instance().clock(),
			// hdr_ip::access(p)->src_.addr_,
			// hdr_ip::access(p)->dst_.addr_,
			// hdr_cmn::access(p)->ecmpHashKey,
			// hdr_cmn::access(p)->clb_row.burst_id);
		}

		return;
	}

	//printf("last hop is %d\n",last_hop);
	if (summarystats) {
                Queue::updateStats(qib_?q_->byteLength():q_->length());
	}


	///CG add begins
	if((ecn_threshold_>0) && ((q_->length() + 1) >= ecn_threshold_))
	{
	//	printf("Tag ECN: length=%d,qib_=%d,qlim_=%d,qlimBytes=%d\n",q_->length(),qib_,qlim_,qlimBytes);
		hdr_flags* hf = hdr_flags::access(p);
		hf->ce() = 1;
	}
	///CG add ends!


	// //CG add for not drop routing packets!
	// if(cmnh->ptype()==PT_RTPROTO_DV)
	// {
	// 	q_->enque(p);
	// 	return;
	// }
	// //CG add ends

	if ((!qib_ && (q_->length() + 1) >= qlim_) ||
  	(qib_ && (q_->byteLength() + hdr_cmn::access(p)->size()) >= qlimBytes)){
		// if the queue would overflow if we added this packet...
		if (drop_front_) { /* remove from head of queue */
			q_->enque(p);
			Packet *pp = q_->deque();
			drop(pp);
		} else {
			drop(p);
		}
	} else {
		q_->enque(p);
	}
}

//AG if queue size changes, we drop excessive packets...
void CLBQueue::shrink_queue() 
{
    int qlimBytes = qlim_ * mean_pktsize_;
	if (debug_)
		printf("shrink-queue: time %5.2f qlen %d, qlim %d\n",
 			Scheduler::instance().clock(),
			q_->length(), qlim_);
        while ((!qib_ && q_->length() > qlim_) || 
            (qib_ && q_->byteLength() > qlimBytes)) {
                if (drop_front_) { /* remove from head of queue */
                        Packet *pp = q_->deque();
                        drop(pp);
                } else {
                        Packet *pp = q_->tail();
                        q_->remove(pp);
                        drop(pp);
                }
        }
}

Packet* CLBQueue::deque()
{
	// PacketQueue* pq = 

	map < string, PacketQueue*> :: iterator tmpIt = bqIt;

	while (true)
	{
		if (bqIt == bqMap.end())
			bqIt = bqMap.begin();

		if (bqIt == bqMap.end())
			break;

		if (bqIt->second->length() > 0)
		{
			Packet* pb = (bqIt++)->second->deque();
			return pb;
		}
		else
		{
			bqIt++;
		}

		if (bqIt == tmpIt)
			break;

	}



	if (summarystats && &Scheduler::instance() != NULL) {
		Queue::updateStats(qib_?q_->byteLength():q_->length());
	}
	//CG add
	Packet* p=q_->deque();

	if(p)	printFlowPath(p);///CG add

	return p;
}

void CLBQueue::print_summarystats()
{
	//double now = Scheduler::instance().clock();
        printf("True average queue: %5.3f", true_ave_);
        if (qib_)
                printf(" (in bytes)");
        printf(" time: %5.3f\n", total_time_);
}
