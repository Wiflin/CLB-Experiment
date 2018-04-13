//
// Author:    Jae Chung
// File:      dtrr-queue.cc
// Written:   07/19/99 (for ns-2.1b4a)
// Modifed:   10/14/01 (for ns-2.1b8a)
// 
#include "hcf.h"
#include "time.h"
#include "flags.h"////CG add

static class HCFQueueClass : public TclClass {
public:
        HCFQueueClass() : TclClass("Queue/HCF") { }
        TclObject* create(int, const char*const*) {
	         return (new HCFQueue);
	}
} class_hcf;

int HCFQueue::command(int argc, const char*const* argv)
{
	if (argc==3) {
		if (strcmp(argv[1], "queue-limit") == 0) {
			q_hcf_limit_=(int)qlim_/2;
			if(q_hcf_limit_<=0)
			{	
				fprintf(stderr, "queue-limit < 0!\n");
				exit(1);
			}
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

void HCFQueue::init()
{
	if(ifInitiate==0)
	{
		c=new int [hash_bin];
		ifInitiate=1;
	}
	for(int i=0;i<hash_bin;i++)
	{
		if(credits_<=0)
		{
			fprintf(stderr,"credits < 0!\n");
			exit(1);
		}
		c[i]=credits_;
	}

	ecn_threshold_=ecn_threshold_/2;

	time(&seed);
}

int HCFQueue::hashFunc(long int srcIP,long int dstIP,long int srcPort,long int dstPort)
{
	int k=0;
	k=(srcIP^dstIP^srcPort^dstPort^seed)%hash_bin;
	return k;
}

void HCFQueue::enque(Packet* p)
{
	init();
	hdr_ip* iph = hdr_ip::access(p);
	int k=hashFunc(iph->src().addr_,iph->dst().addr_,iph->src().port_,iph->dst().port_);
	if (c[k]>0 && HQ->length() < q_hcf_limit_) 
	{
		HQ->enque(p);
		c[k]=c[k]-1;
		if((ecn_threshold_>0) && ((HQ->length() + 1) >= ecn_threshold_))
		{
		//	printf("Tag ECN: length=%d,qib_=%d,qlim_=%d,qlimBytes=%d\n",q_->length(),qib_,qlim_,qlimBytes);
			hdr_flags* hf = hdr_flags::access(p);
			hf->ce() = 1;
		}
	}
	else if(LQ->length() < q_hcf_limit_)
	{
		LQ->enque(p);
		if((ecn_threshold_>0) && ((LQ->length() + 1) >= ecn_threshold_))
		{
		//	printf("Tag ECN: length=%d,qib_=%d,qlim_=%d,qlimBytes=%d\n",q_->length(),qib_,qlim_,qlimBytes);
			hdr_flags* hf = hdr_flags::access(p);
			hf->ce() = 1;
		}
	}
	else
	{
		HQ->enque(p);
		HQ->remove(p);
		drop(p);
	}
}


Packet* HCFQueue::deque()
{
	Packet *p=0;
	if(HQ->length() > 0)
	{
		p =  HQ->deque();
		hdr_cmn* cmnh = hdr_cmn::access(p);
		cmnh->last_hop_=last_hop;
		cmnh->next_hop_=next_hop;
		if(HQ->length()==0)
		{
	//		printf("queue swaped!\n");
			queueSwap();
			init();
		}
	}
	else if(LQ->length()>0)
	{
		p=LQ->deque();
		hdr_cmn* cmnh = hdr_cmn::access(p);
		cmnh->last_hop_=last_hop;
		cmnh->next_hop_=next_hop;
	}
	return (p);
}

void HCFQueue::queueSwap()
{
	PacketQueue *temp;
	temp=HQ;
	HQ=LQ;
	LQ=temp;
}

int HCFQueue::length()
{
	return qlim_;
}
