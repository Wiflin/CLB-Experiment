//
// Author:    Jae Chung
// File:      dtrr-queue.cc
// Written:   07/19/99 (for ns-2.1b4a)
// Modifed:   10/14/01 (for ns-2.1b8a)
// 
#include "cqrr.h"
#include "random.h"
#include <math.h>
#include "template.h"
#include "tcp.h"
#include "flags.h"////CG add

static class CqRrQueueClass : public TclClass {
public:
        CqRrQueueClass() : TclClass("Queue/CQRR") { }
        TclObject* create(int, const char*const*) {
	         return (new CqRrQueue);
	}
} class_cqrr;

int CqRrQueue::command(int argc, const char*const* argv)
{
	if (argc==3) {
		if (strcmp(argv[1], "queue-limit") == 0) {
			q_cq_limit_=(int)qlim_/portnumber_;
			return (TCL_OK);
		}
		else if (strcmp(argv[1], "port-number") == 0)
		{
		//	printf("CQ: srcNode=%d,dstNode=%d\n",last_hop,next_hop);
			portnumber_=atoi(argv[2]);
			if(ifInitiate==0)
			{
				CqRrQueueInit();
				ifInitiate=1;
			}
			else
			{
				fprintf(stderr,"CQRR initiate error!\n");
			}
			return (TCL_OK);
		}
		else if (strcmp(argv[1], "ecn-threshold") == 0)
		{
			ecn_threshold_=atoi(argv[2]);
			//printf("ecn_threshold_=%d\n",ecn_threshold_);
			return (TCL_OK);
		}
	}
 	return Queue::command(argc, argv);
}


void CqRrQueue::CqRrQueueInit()///create portnumber_ queues for each input!
{
	q_cq_limit_=(int)qlim_/portnumber_;
//	printf("the queue capacity is %d\n",qlim_);
//	printf("the queue capacity of each input is %d\n",q_cq_limit_);
	if(portnumber_>0)
	{
		q_=new PacketQueue  [portnumber_];
		inputState=new nsaddr_t [portnumber_];
			
		for(int i=0;i<portnumber_;i++)
		{
			inputState[i]=-1;
		}

		ecn_threshold_=ecn_threshold_/portnumber_;
//		printf("CQ has been initialized!\n");
	}
	else
	{
		fprintf(stderr, "portnumber wrong!\n");
		exit(1);
	}
}

void CqRrQueue::enque(Packet* p)
{
//	printf("packet queued!\n");
//	if(ifInitiate == 0)
//	{
//		printf("the queue capacity is %d\n",qlim_);
//		CqRrQueueInit();
//		ifInitiate=1;
//	}
//
	hdr_cmn* cmnh = hdr_cmn::access(p);
	if(cmnh->ptype()==PT_RTPROTO_DV)
	{
		cmnh->last_hop_=last_hop;
		cmnh->next_hop_=next_hop;
	}
/*	hdr_ip* iph = hdr_ip::access(p);
	hdr_tcp* tcph = hdr_tcp::access(p);
	double now = Scheduler::instance().clock();
	printf("%3.8f %2d %4d %2d %2d ----- %d.%d %d.%d %d %d\n",now,cmnh->ptype(),cmnh->size(),last_hop,next_hop,iph->src().addr_,iph->src().port_,iph->dst().addr_,iph->dst().port_,tcph->seqno(),cmnh->uid());
	printf("last_hop=%d,next_hop=%d\n",cmnh->last_hop_,cmnh->next_hop_);
*/	
	if(cmnh->last_hop_==cmnh->next_hop_)
	{
		fprintf(stderr, "inputPort=%d is the same as outputPort=%d!\n",cmnh->last_hop_,cmnh->next_hop_);
		exit(1);
	}
	int inputPort=-1;
	for(int i=0;i<portnumber_;i++)
	{
		if(inputState[i]==cmnh->last_hop_) /////check if this flow has been matched with inputState[i]
		{
			inputPort=i;
			break;
		}

		if(inputState[i]==-1) /////haven't  matched before, match with inputState[i]
		{
			inputState[i]=cmnh->last_hop_;
			inputPort=i;
			break;
		}
	}

	if(inputState[inputPort]!=cmnh->last_hop_)
	{
		fprintf(stderr, "inputPort state wrong!\n");
		exit(1);
	}

/*	if(next_hop==10 && inputPort==8)
	{
		hdr_ip* iph = hdr_ip::access(p);
		hdr_tcp* tcph = hdr_tcp::access(p);
		double now = Scheduler::instance().clock();
		printf("+ %3.8f %2d %4d %2d %2d ----- %d.%d %d.%d %d %d\n",now,cmnh->ptype(    ),cmnh->size(),last_hop,next_hop,iph->src().addr_,iph->src().port_,iph->dst().addr_    ,iph->dst().port_,tcph->seqno(),cmnh->uid());
	}
*/
	q_[inputPort].enque(p);

////Random ECN mark!
	if((ecn_threshold_>0) && ((q_[inputPort].length() + 1) >= ecn_threshold_))
	{
	//	printf("Tag ECN: length=%d,qib_=%d,qlim_=%d,qlimBytes=%d\n",q_->length(),qib_,qlim_,qlimBytes);
		Packet * pkt=pickPacketToDrop(q_[inputPort]);
		hdr_flags* hf = hdr_flags::access(pkt);
		hf->ce() = 1;
	}

	if (q_[inputPort].length() > q_cq_limit_) {

		Packet * pkt=pickPacketToDrop(q_[inputPort]);
		q_[inputPort].remove(pkt);
		drop(pkt);
	}

}


Packet* CqRrQueue::deque()
{
	Packet *p=0;
	
	for(int i=0;i<portnumber_;i++)
	{
		int j=(deq_turn_+i)%portnumber_;

		if(q_[j].length() > 0)
		{
//			for(int k=0;k<portnumber_;k++)
//			{
//				printf("queue length of %ld is %d\n",inputState[k],q_[k].length());
//			}	
//			printf("the %d queue was selected!\n",inputState[j]);
//			printf("deq_turn_ is %d\n\n",deq_turn_);
			p =  q_[j].deque();
			hdr_cmn* cmnh = hdr_cmn::access(p);
			cmnh->last_hop_=last_hop;
			cmnh->next_hop_=next_hop;
			deq_turn_=(j+1)%portnumber_;
			break;
		}
	}

/*	if(this->length()>0 && p==0)
	{
		fprintf(stderr,"CQ is not empty but no packets scheduled!\n");
		exit(1);
	}*/
	return (p);
}

int CqRrQueue::length()
{
	int length=0;
	for(int i=0;i<portnumber_;i++)
	{
		length=length+q_[i].length();
	}
	return length;
}

Packet* CqRrQueue::pickPacketToDrop(PacketQueue qTmp)
{
	int victim;
	if (drop_front_)
	    victim = min(1, qTmp.length()-1);
	else if (drop_rand_)
	    victim = Random::integer(qTmp.length());
	else            /* default is drop_tail_ */
	    victim = qTmp.length() - 1;
	 
	return(qTmp.lookup(victim));
}

