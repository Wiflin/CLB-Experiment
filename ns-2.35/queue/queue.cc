/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1996-1997 The Regents of the University of California.
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
 * 	This product includes software developed by the Network Research
 * 	Group at Lawrence Berkeley National Laboratory.
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

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/queue/queue.cc,v 1.29 2004/10/28 01:22:48 sfloyd Exp $ (LBL)";
#endif

#include "queue.h"
#include <math.h>
#include <stdio.h>

#include <sys/stat.h> ///CG add
#include <errno.h> ///CG add

void PacketQueue::remove(Packet* target)
{
	for (Packet *pp= 0, *p= head_; p; pp= p, p= p->next_) {
		if (p == target) {
			if (!pp) deque();
			else {
				if (p == tail_) 
					tail_= pp;
				pp->next_= p->next_;
				--len_;
				bytes_ -= hdr_cmn::access(p)->size();
			}
			return;
		}
	}
	fprintf(stderr, "PacketQueue:: remove() couldn't find target\n");
	abort();
}

/*
 * Remove packet pkt located after packet prev on the queue.  Either p or prev
 * could be NULL.  If prev is NULL then pkt must be the head of the queue.
 */
void PacketQueue::remove(Packet* pkt, Packet *prev) //XXX: screwy
{
	if (pkt) {
		if (head_ == pkt)
			PacketQueue::deque(); /* decrements len_ internally */
		else {
			prev->next_ = pkt->next_;
			if (tail_ == pkt)
				tail_ = prev;
			--len_;
			bytes_ -= hdr_cmn::access(pkt)->size();
		}
	}
	return;
}

void QueueHandler::handle(Event*)
{
	queue_.resume();
}

Queue::~Queue() {
}

Queue::Queue() : Connector(), blocked_(0), unblock_on_resume_(1), qh_(*this),
		 pq_(0), 
		 last_change_(0), /* temporarily NULL */
		 old_util_(0), period_begin_(0), cur_util_(0), buf_slot_(0),
		 util_buf_(NULL),ifMoniterQueueLen_(0),ifMoniterE2EDelay_(0),
		 ifMoniterFlowPath_(0),ifMoniterPathTrace_(0),
		 ifTagTimeStamp_(0),ifHavePrint(0),
		 ifMoniterFlowSpeed_(0),pFlowSpeedTimer(NULL)
{
	bind("limit_", &qlim_);
	bind("util_weight_", &util_weight_);
	bind_bool("blocked_", &blocked_);
	bind_bool("unblock_on_resume_", &unblock_on_resume_);
	bind("util_check_intv_", &util_check_intv_);
	bind("util_records_", &util_records_);

	bind("packetDropped_", &packetDropped_); ///CG add
	bind("parityDropped_", &parityDropped_); ///CG add
	bind("srcID", &last_hop);  ////CG add
	bind("dstID", &next_hop);
	bind("ecn_threshold_",&ecn_threshold_);

	if (util_records_ > 0) {
		util_buf_ = new double[util_records_];
		if (util_buf_ == NULL) {
			printf("Error allocating util_bufs!");
			util_records_ = 0;
		}
		for (int i = 0; i < util_records_; i++) {
			util_buf_[i] = 0;
		}
	}
}

////CG add begins
int Queue::command(int argc, const char*const* argv) 
{
	if (argc==2) {
		if (strcmp(argv[1], "monitor-E2EDelay") == 0) { /////CG add
			ifMoniterE2EDelay_=1;
			mkdir("PathDelay",0777);
			system("exec rm -r -f PathDelay/*");
			return TCL_OK;
		}
		if (strcmp(argv[1], "monitor-QueueLen") == 0) { /////CG add
			ifMoniterQueueLen_=1;
			mkdir("QueueLen",0777);
			system("exec rm -r -f QueueLen/*");
			return TCL_OK;
		}
		if (strcmp(argv[1], "tag-timestamp") == 0) { /////CG add
			ifTagTimeStamp_=1;
			return TCL_OK;
		}
		if (strcmp(argv[1], "monitor-FlowPath") == 0) { /////CG add
			ifMoniterFlowPath_=1;
			mkdir("FlowPath",0777);
			system("exec rm -r -f FlowPath/*");
			return TCL_OK;
		}
		if (strcmp(argv[1], "monitor-PathTrace") == 0) { /////WF add
			ifMoniterPathTrace_=1;
			mkdir("PathTrace",0777);
			system("exec rm -r -f PathTrace/*");
			return TCL_OK;
		}
		if (strcmp(argv[1], "monitor-FlowSpeed") == 0) { /////WF add
			ifMoniterFlowSpeed_=1;
			schedDelay = 1E-3;
			qFlowSize = lastFlowSize = 0;
			mkdir("FlowSpeed",0777);
			system("exec rm -r -f FlowSpeed/*");
			return TCL_OK;
		}
	}
	return Connector::command(argc, argv);
}
////CG add ends

void Queue::recv(Packet* p, Handler*)
{
	printDelayTimeline(p);////CG add

	printPathTrace(p);///WF add

	if(ifTagTimeStamp_==1)
	{
		// printf("%lf-queue(%d-%d): tag timestamp\n",
		// 	Scheduler::instance().clock(),
		// 	last_hop,
		// 	next_hop);
		hdr_cmn::access(p)->timestamp()=Scheduler::instance().clock();
	}

	hdr_cmn* cmnh = hdr_cmn::access(p);/////CG add
	cmnh->last_hop_=last_hop;
	cmnh->next_hop_=next_hop;
	if(cmnh->last_hop_==cmnh->next_hop_)
	{
		fprintf(stderr, "inputPort=%d is the same as outputPort=%d!\n",cmnh->last_hop_,cmnh->next_hop_);
		exit(1);
	}

	double now = Scheduler::instance().clock();
	enque(p);

	printQueueLenTimeline(); ///CG add

	if (!blocked_) {
		/*
		 * We're not blocked.  Get a packet and send it on.
		 * We perform an extra check because the queue
		 * might drop the packet even if it was
		 * previously empty!  (e.g., RED can do this.)
		 */
		p = deque();

		printQueueLenTimeline(); ///CG add
		if (p != 0) {
			utilUpdate(last_change_, now, blocked_);
			last_change_ = now;
			blocked_ = 1;
			target_->recv(p, &qh_);
		}
	}
	
}

void Queue::utilUpdate(double int_begin, double int_end, int link_state) {
double decay;

	decay = exp(-util_weight_ * (int_end - int_begin));
	old_util_ = link_state + (old_util_ - link_state) * decay;

	// PS: measuring peak utilization
	if (util_records_ == 0)
		return; // We don't track peak utilization

	double intv = int_end - int_begin;
	double tot_intv = int_begin - period_begin_;
	if (intv || tot_intv) {
		int guard = 0; // for protecting against long while loops 
		cur_util_ = (link_state * intv + cur_util_ * tot_intv) /
			(intv + tot_intv);
		while (tot_intv + intv > util_check_intv_ &&
		       guard++ < util_records_) {

			period_begin_ = int_end;
			util_buf_[buf_slot_] = cur_util_;
			buf_slot_ = (buf_slot_ + 1) % util_records_;
			cur_util_ = link_state;
			intv -= util_check_intv_;
		}
	}
}

double Queue::utilization(void) 
{
	double now = Scheduler::instance().clock();
	
	utilUpdate(last_change_, now, blocked_);
	last_change_ = now;

	return old_util_;
			
}

double Queue::peak_utilization(void)
{
	double now = Scheduler::instance().clock();
	double peak = 0;
	int i;
	
	// PS: if peak_utilization tracking is disabled,
	// return the weighed avg instead
	if (util_records_ == 0)
		return utilization();

	utilUpdate(last_change_, now, blocked_);
	last_change_ = now;

	for (i = 0; i < util_records_; i++) {
		if (util_buf_[i] > peak)
			peak = util_buf_[i];
	}
	return peak;
}

void Queue::updateStats(int queuesize)
{
        double now = Scheduler::instance().clock();
        double newtime = now - total_time_;
        if (newtime > 0.0) {
                double oldave = true_ave_;
                double oldtime = total_time_;
                double newtime = now - total_time_;
                true_ave_ = (oldtime * oldave + newtime * queuesize) /now;
                total_time_ = now;
        }
}

void Queue::resume()
{
	double now = Scheduler::instance().clock();
	Packet* p = deque();
	if (p != 0) {
		target_->recv(p, &qh_);
	} else {
		if (unblock_on_resume_) {
			utilUpdate(last_change_, now, blocked_);
			last_change_ = now;
			blocked_ = 0;
		}
		else {
			utilUpdate(last_change_, now, blocked_);
			last_change_ = now;
			blocked_ = 1;
		}
	}
}

void Queue::reset()
{
	Packet* p;
	total_time_ = 0.0;
	true_ave_ = 0.0;
	while ((p = deque()) != 0)
		drop(p);
}

void Queue::printDelayTimeline(Packet* pkt)///CG add
{
	if(!ifMoniterE2EDelay_)
    {
        return;
    }

    hdr_cmn* cmnh = hdr_cmn::access(pkt);
    //// time sendSeq ackSeq uid cwnd

    char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"PathDelay/Path%d-%d_E2EDelay.tr"
		,cmnh->last_hop_
		,cmnh->next_hop_);

	if(ifHavePrint==0)
	{
		ifHavePrint=1;
	}
	
	fpDelay=fopen(str1,"a");
	

	
	if(fpDelay==NULL)
	{
		fprintf(stderr,"%s, Can't open file %s!\n",strerror(errno),str1);
	}


    double now_ = Scheduler::instance().clock();
    double e2eDelay=now_ - cmnh->timestamp();

    /*hdr_ip* iph = hdr_ip::access(pkt);
    printf("%d|%d|%d|%d|%e\n",iph->src().addr_,iph->dst().addr_,iph->src().port_,iph->dst().port_,e2eDelay);*/


   /* printf("%.9f\t%e\n",
        Scheduler::instance().clock()
        ,e2eDelay);*/
    fprintf(fpDelay,"%.9f\t%e\n",
        Scheduler::instance().clock()
        ,e2eDelay);

    fclose(fpDelay);
}


void Queue::printQueueLenTimeline()///CG add
{
	if(!ifMoniterQueueLen_)
    {
        return;
    }

    char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"QueueLen/Queue%d-%d_Len.tr"
		,last_hop
		,next_hop);

	if(ifHavePrint==0)
	{
		ifHavePrint=1;
	}
	
	fpQueueLen=fopen(str1,"a");
	

	
	if(fpQueueLen==NULL)
	{
		fprintf(stderr,"%s, Can't open file %s!\n",strerror(errno),str1);
	}

    
    fprintf(fpQueueLen,"%.9f\t%d\n",
        Scheduler::instance().clock()
        ,length());

    fclose(fpQueueLen);
}


void Queue::printFlowPath(Packet* pkt)
{
	if(!ifMoniterFlowPath_)
    {
        return;
    }

	if(ifMoniterFlowSpeed_ && pFlowSpeedTimer == NULL)
    {
        pFlowSpeedTimer = new FlowSpeedTimer(this);
		pFlowSpeedTimer->sched(schedDelay);
    }

			
    hdr_ip *iph = hdr_ip::access(pkt);
    hdr_cmn* cmnh = hdr_cmn::access(pkt);

	// it means just count the what flow get pass by the node
    qFlowSize += cmnh->size_;
	#define DO_NOT_COUNT_FLOW_SIZE
	#ifdef DO_NOT_COUNT_FLOW_SIZE
		vector < flowInfo >::iterator it;
		for(it = FlowTable.begin (); it != FlowTable.end (); ++it)
		{
			if( it->srcAddr==iph->src().addr_
				&& it->srcPort==iph->src().port_
				&& it->dstAddr==iph->dst().addr_
				&& it->dstPort==iph->dst().port_
				&& it->hashkey == cmnh->ecmpHashKey)
			{
				it->flowSize += cmnh->size_;
				return;
			}
		}

		struct flowInfo tmp_map;
		tmp_map.srcAddr=iph->src().addr_;
		tmp_map.srcPort=iph->src().port_;
		tmp_map.dstAddr=iph->dst().addr_;
		tmp_map.dstPort=iph->dst().port_;
		tmp_map.hashkey=cmnh->ecmpHashKey;
		tmp_map.flowSize=cmnh->size_;
		tmp_map.lastSize=0;
		FlowTable.push_back (tmp_map);
	#endif

	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"FlowPath/Path%d-%d_Flow.tr"
		,last_hop
		,next_hop);

	fpFlowPath=fopen(str1,"a");
	if(fpFlowPath==NULL)
	{
		fprintf(stderr,"%s, Can't open file %s!\n",strerror(errno),str1);
	}
    

    fprintf(fpFlowPath,"%.9f\t%d.%d-%d.%d %u %d\n",
        Scheduler::instance().clock()
        ,iph->src().addr_,iph->src().port_
		,iph->dst().addr_,iph->dst().port_
		,cmnh->ecmpHashKey
		,cmnh->flowID);

    fclose(fpFlowPath);
}



void Queue::printPathTrace(Packet* pkt)
{
	if(!ifMoniterPathTrace_)
    {
        return;
    }
    hdr_ip *iph = hdr_ip::access(pkt);
    hdr_cmn* cmnh = hdr_cmn::access(pkt);

    char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"PathTrace/Flowid-%d.tr"
		,cmnh->flowID);

	fpPathTrace=fopen(str1,"a");
	if(fpPathTrace==NULL)
	{
		fprintf(stderr,"%s, Can't open file %s!\n",strerror(errno),str1);
	}
    
    fprintf(fpPathTrace,"%.9f\t%d-%d %u\n",
        Scheduler::instance().clock()
        ,last_hop
        ,next_hop
		,cmnh->size_);

    fclose(fpPathTrace);
}


void Queue::printFlowSpeed()
{
	if(!ifMoniterFlowSpeed_)
    {
        return;
    }

	char str1[128];
	memset(str1,0,128*sizeof(char));
	sprintf(str1,"FlowSpeed/Path%d-%d_Speed.tr"
		,last_hop
		,next_hop);

	fpFlowSpeed=fopen(str1,"a");
	if(fpFlowSpeed==NULL)
	{
		fprintf(stderr,"%s, Can't open file %s!\n",strerror(errno),str1);
	}
    
    double qSpeed = (double)(qFlowSize - lastFlowSize) / (schedDelay * 1000 * 1000) * 8;
    fprintf(fpFlowSpeed, "%.lf\t", qSpeed);
    lastFlowSize = qFlowSize;


    vector < flowInfo >::iterator it;
	for(it = FlowTable.begin (); it != FlowTable.end (); ++it)
	{
		
		double fSpeed = (double)(it->flowSize - it->lastSize) / (schedDelay * 1000 * 1000) * 8;
		fprintf(fpFlowSpeed, "%d.%d-%d.%d-%u=%.lf\t" 
			,it->srcAddr,it->srcPort
			,it->dstAddr,it->dstPort
			,it->hashkey
			,fSpeed);

		it->lastSize = it->flowSize;
	}

    fprintf(fpFlowSpeed, "\n");
    fclose(fpFlowSpeed);
	pFlowSpeedTimer->resched(schedDelay);
}


void FlowSpeedTimer::expire(Event *e)
{
	a_->printFlowSpeed();
}