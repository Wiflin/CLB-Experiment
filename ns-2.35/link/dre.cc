#include "dre.h"

#define MAX_CE 8
#define T_DRE 4E-5
#define ALPHA 0.5

DREAgent::DREAgent(LinkDelay *link_)
	:decayTimer_(this), X(0), dreCost_(0), util(0)
{
	ParrentLink_=link_;
	decayTimer_.sched(T_DRE);
	bandWidth=ParrentLink_->bandwidth();
	fprintf(stderr,"%lf-Link(%d-%d, bw=%2.1e): Start timer %2.1e\n"
		,Scheduler::instance().clock()
		,ParrentLink_->fromNodeID(),ParrentLink_->toNodeID()
		,ParrentLink_->bandwidth()
		,T_DRE);
}

DREAgent::DREAgent(double band_)
	:decayTimer_(this), X(0), dreCost_(0), util(0)
{
	bandWidth=band_;
	decayTimer_.sched(T_DRE);
	// printf("%lf-Link(%d-%d, bw=%2.1e): Start timer %2.1e\n"
	// 	,Scheduler::instance().clock()
	// 	,ParrentLink_->fromNodeID(),ParrentLink_->toNodeID()
	// 	,ParrentLink_->bandwidth()
	// 	,T_DRE);
}



DREAgent::~DREAgent()
{
	
}

void DREAgent::recv(Packet* pkt)
{
	hdr_cmn* cmnh = hdr_cmn::access(pkt);
	X=X+cmnh->size()*8;
	updateDRECost();

	
/*	printf("%lf-Link(%d-%d, bw=%2.1e): Receive a packet %d. Packet CE=%d, link util=%2.1e, CE=%d!\n"
		,Scheduler::instance().clock()
		,ParrentLink_->fromNodeID(),ParrentLink_->toNodeID()
		,ParrentLink_->bandwidth()
		,cmnh->uid_,cmnh->CE
		,util,dreCost());
*/
}

void DecayTimer::expire(Event*)
{
	a_->decayTimeout();
}

void DREAgent::decayTimeout()
{
	decayTimer_.resched(T_DRE);
	X=X*(1-ALPHA);
}

void DREAgent::updateDRECost()
{
	util=X*ALPHA/(T_DRE*bandWidth);
	dreCost_=(int) X*ALPHA/(T_DRE*bandWidth)*MAX_CE;
	if(dreCost_>=MAX_CE)
	{
		dreCost_=MAX_CE-1;
	}
}

int DREAgent::dreCost()
{
	updateDRECost();
	return dreCost_;
}