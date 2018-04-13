/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (C) Xerox Corporation 1997. All rights reserved.
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
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/apps/CLBTunnel.cc,v 1.21 2005/08/26 05:05:28 tomh Exp $ (Xerox)";
#endif

#include "conga.h"
#include "random.h"
#include "address.h"
#include "ip.h"
#include "time.h"
#include "tcp.h"


#define IF_PRINT_SEQTIMELINE 0
#define IF_PRINT_VPWINDOWTIMELINE 0
#define IF_PRINT_DEBUG 0

#define MAX_FLOWLET 65536
#define T_FL 5E-5

CONGAAgent::CONGAAgent(Classifier * classifier_)
	:flowletTimer_(this)
{
	ParentClassifier_=classifier_;
	LeafNumber=ParentClassifier_->LeafNumber();
	PortNumber=ParentClassifier_->PortNumber();
	if(LeafNumber==0 || PortNumber==0)
	{
		printf("%s\n","ERROR: LeafNumber or PortNumber=0!");
		printf("%lf-CONGA_Agent_: Node=%d, sLeaf_Conga_=%d, minHost_=%d, maxHost_=%d, LeafNumber=%d, PortNumber=%d\n"
			,Scheduler::instance().clock(),ParentClassifier_->nodeID()
			,ParentClassifier_->sLeaf_Conga(),ParentClassifier_->minHost()
			,ParentClassifier_->maxHost(),LeafNumber,PortNumber);
	}
//	printf("%lf-CONGA_Agent_: Node=%d, sLeaf_Conga_=%d, minHost_=%d, maxHost_=%d, LeafNumber=%d, PortNumber=%d\n"
//			,Scheduler::instance().clock(),ParentClassifier_->nodeID()
//			,ParentClassifier_->sLeaf_Conga(),ParentClassifier_->minHost()
//			,ParentClassifier_->maxHost(),LeafNumber,PortNumber);
	
	CongestionToLeafTable=new int* [LeafNumber];
	memset(CongestionToLeafTable,0,LeafNumber*sizeof(int *));
	for(int i=0;i<LeafNumber;i++)
	{
		CongestionToLeafTable[i]=new int [PortNumber];
		memset(CongestionToLeafTable[i],0,PortNumber*sizeof(int));
	}

	CongestionFromLeafTable=new int* [LeafNumber];
	memset(CongestionFromLeafTable,0,LeafNumber*sizeof(int *));
	for(int i=0;i<LeafNumber;i++)
	{
		CongestionFromLeafTable[i]=new int [PortNumber];
		memset(CongestionFromLeafTable[i],0,PortNumber*sizeof(int));
	}

	
	currentFB_LB=new int [LeafNumber];
	for(int i=0;i<LeafNumber;i++)
	{
		currentFB_LB[i]=rand()%PortNumber;
	}
	LatestChangedFB_LB=new int [LeafNumber];
	for(int i=0;i<LeafNumber;i++)
	{
		LatestChangedFB_LB[i]=-1;
	}

	flowletTable_=new FlowletTable [MAX_FLOWLET];
	for(int i=0;i<MAX_FLOWLET;i++)
	{
		flowletTable_[i].port=0;
		flowletTable_[i].valid=0;
		flowletTable_[i].age=1;
	}

	flowletTimer_.sched(T_FL);

	LBTagSelection=new int* [LeafNumber];
	memset(LBTagSelection,0,LeafNumber*sizeof(int *));
	for(int i=0;i<LeafNumber;i++)
	{
		LBTagSelection[i]=new int [PortNumber];
		memset(LBTagSelection[i],0,PortNumber*sizeof(int));
	}
	ifFirstPrintLBTag=new int [LeafNumber];
	memset(ifFirstPrintLBTag,0,LeafNumber*sizeof(int));

	dre_=new DREAgent* [PortNumber];
	memset(dre_,0,PortNumber*sizeof(DREAgent*));
	ifDRE_=new int [PortNumber];
	memset(ifDRE_,0,PortNumber*sizeof(int));

	randSalt_=rand()%65536;
//	printf("%lf-Node-%d: flowletTimer_ start!\n",Scheduler::instance().clock(),ParentClassifier_->nodeID());
}


void CONGAAgent::recv(Packet* pkt)
{
	hdr_cmn* cmnh = hdr_cmn::access(pkt);
	if(cmnh->ptype()==PT_RTPROTO_DV)////Don't deal with DV packets
	{
		return;
	}


	hdr_ip* iph = hdr_ip::access(pkt);
	if(iph->dst().addr_<=ParentClassifier_->maxHost() 
		&& iph->dst().addr_>=ParentClassifier_->minHost())
	{
		if((HDR_CMN(pkt))->ifCONGA_==1)
		{
		//	printf("%lf-Node-%d(minHost=%d,maxHost=%d): CONGA receive a packet (dst=%d)\n"
		//		,Scheduler::instance().clock(),ParentClassifier_->nodeID()
		//		,ParentClassifier_->minHost(),ParentClassifier_->maxHost(),iph->dst().addr_);
			(HDR_CMN(pkt))->ifCONGA_=0;
			PrintPacket(pkt,"receive");

			int hostInOneLeaf=ParentClassifier_->maxHost()-ParentClassifier_->minHost()+1;
			int srcLeafAlias=cmnh->srcLeaf-hostInOneLeaf*LeafNumber;

			CongestionToLeafTable[srcLeafAlias][cmnh->FB_LBTag]=cmnh->FB_Metric;
			CongestionFromLeafTable[srcLeafAlias][cmnh->LBTag]=cmnh->CE;

		//	PrintCongestionToLeafTable(cmnh->srcLeaf);
		//	PrintCongestionFromLeafTable(cmnh->srcLeaf);
		}
	}
	else 
	{
		if((HDR_CMN(pkt))->ifCONGA_==0)
		{
		//	printf("%lf-Node-%d(minHost=%d,maxHost=%d): CONGA send a packet (dst=%d)\n"
		//		,Scheduler::instance().clock(),ParentClassifier_->nodeID()
		//		,ParentClassifier_->minHost(),ParentClassifier_->maxHost(),iph->dst().addr_);

			(HDR_CMN(pkt))->ifCONGA_=1;
			Classifier * MultipathClassifier_=(Classifier *) ParentClassifier_->find(pkt);


		//	printf("%lf-Node-%d: send to MultipathClassifier_ %d, with maxslot_=%d\n"
		//		,Scheduler::instance().clock(),ParentClassifier_->nodeID()
		//		,MultipathClassifier_->nodeID(),MultipathClassifier_->maxslot());

			setPacket(pkt,MultipathClassifier_->maxslot());			
			PrintPacket(pkt,"send");
			printSeqTimeline(pkt);
		}
	}
}

int CONGAAgent::checkFlowletTable(Packet* pkt)
{
	int LBTag=-1;
	hdr_ip* iph = hdr_ip::access(pkt);
	// int offSet=floor(log(PortNumber)/log(4));
	// int key=((iph->src().addr_<<offSet)^
	// 	iph->dst().addr_^
	// 	(iph->src().port_<<offSet)^
	// 	iph->dst().port_^
	// 	randSalt_)%MAX_FLOWLET;
	// // int key=(iph->src().addr_^iph->src().port_)%MAX_FLOWLET;

	unsigned char F_Tuple[20];
	memset(F_Tuple,0,20*sizeof(unsigned char));
	memcpy(F_Tuple,&(iph->src().addr_),sizeof(int32_t));
	memcpy(F_Tuple+sizeof(int32_t),&(iph->dst().addr_),sizeof(int32_t));
	memcpy(F_Tuple+2*sizeof(int32_t),&(iph->src().port_),sizeof(int32_t));
	memcpy(F_Tuple+3*sizeof(int32_t),&(iph->dst().port_),sizeof(int32_t));
	memcpy(F_Tuple+4*sizeof(int32_t),&(randSalt_),sizeof(randSalt_));
	int crc=crc16.gen_crc16((uint8_t *)F_Tuple,20);
	int key=(int) crc/((0x10000)/(MAX_FLOWLET));
	if(flowletTable_[key].valid==1)
	{
		LBTag=flowletTable_[key].port;
	}
	flowletTable_[key].age=0;
	return LBTag;
}

void CONGAAgent::setFlowletTable(Packet* pkt)
{
	hdr_ip* iph = hdr_ip::access(pkt);
	hdr_cmn* cmnh = hdr_cmn::access(pkt);
	// int offSet=floor(log(PortNumber)/log(4));
	// int key=((iph->src().addr_<<offSet)^
	// 	iph->dst().addr_^
	// 	(iph->src().port_<<offSet)^
	// 	iph->dst().port_^
	// 	randSalt_)%MAX_FLOWLET;
	// int key=(iph->src().addr_^iph->src().port_)%MAX_FLOWLET;

	unsigned char F_Tuple[20];
	memset(F_Tuple,0,20*sizeof(unsigned char));
	memcpy(F_Tuple,&(iph->src().addr_),sizeof(int32_t));
	memcpy(F_Tuple+sizeof(int32_t),&(iph->dst().addr_),sizeof(int32_t));
	memcpy(F_Tuple+2*sizeof(int32_t),&(iph->src().port_),sizeof(int32_t));
	memcpy(F_Tuple+3*sizeof(int32_t),&(iph->dst().port_),sizeof(int32_t));
	memcpy(F_Tuple+4*sizeof(int32_t),&(randSalt_),sizeof(randSalt_));
	int crc=crc16.gen_crc16((uint8_t *)F_Tuple,20);
	int key=(int) crc/((0x10000)/(MAX_FLOWLET));
	flowletTable_[key].valid=1;
	flowletTable_[key].port=cmnh->LBTag;

	if(IF_PRINT_DEBUG)
	{
		printf("%lf-Node-%d: set flowletTable_[%d].port=%d\n",Scheduler::instance().clock(),ParentClassifier_->nodeID(),key,cmnh->LBTag);
	}
}

void CONGAAgent::setPacket(Packet* pkt, int maxslot)/////Only used while Leaf send a packet!
{
	hdr_cmn* cmnh = hdr_cmn::access(pkt);
	cmnh->srcLeaf=ParentClassifier_->nodeID();
	cmnh->dstLeaf=getDestLeaf(pkt);
	
	int hostInOneLeaf=ParentClassifier_->maxHost()-ParentClassifier_->minHost()+1;
	int leafAlias=cmnh->dstLeaf-hostInOneLeaf*LeafNumber;
	
	int LBTag=checkFlowletTable(pkt);
	if(LBTag==-1)
	{
	//	PrintCongestionToLeafTable(cmnh->dstLeaf);
		Tcl& tcl = Tcl::instance();
		tcl.evalf("[Simulator instance] conga-flowlet-add");
		cmnh->LBTag=findBestPath(leafAlias,maxslot+1);/////maxslot+1 is the maxslot number
		LBTagSelection[leafAlias][cmnh->LBTag]++;
		printLBTagTimeline(leafAlias);
		setFlowletTable(pkt);
	}
	else
	{
		cmnh->LBTag=LBTag;
	}

	////local dre
	if(cmnh->LBTag >= PortNumber)
	{
		printf("ERROR: cmnh->LBTag = %d, > portnumber %d\n"
			,cmnh->LBTag 
			,PortNumber);
		exit(0);
	}
	if(ifDRE_[cmnh->LBTag]==0)
	{
		// printf("%lf-Node-%d: ParentClassifier_->bandwidth()=%2.1e\n"
		// 	,Scheduler::instance().clock(),ParentClassifier_->nodeID()
		// 	,ParentClassifier_->bandwidth());
		if(ParentClassifier_->bandwidth()==0)
		{
			printf("ERROR-Node-%d: ParentClassifier_->bandwidth()=%2.1e\n"
			,ParentClassifier_->nodeID()
			,ParentClassifier_->bandwidth());
			exit(0);
		}

		dre_[cmnh->LBTag]=new DREAgent(ParentClassifier_->bandwidth());
		ifDRE_[cmnh->LBTag]=1;
	}
	dre_[cmnh->LBTag]->recv(pkt);
	int localCost=dre_[cmnh->LBTag]->dreCost();
	// printf("%lf-Node-%d: dre_[%d]->localCost=%d\n"
	// 		,Scheduler::instance().clock(),ParentClassifier_->nodeID()
	// 		,cmnh->LBTag
	// 		,localCost);

	if(localCost > CongestionToLeafTable[leafAlias][cmnh->LBTag])
	{
		CongestionToLeafTable[leafAlias][cmnh->LBTag]=localCost;
	}
	////local dre

	cmnh->FB_LBTag=findNextFeedbackLB(leafAlias,currentFB_LB[leafAlias]);
	currentFB_LB[leafAlias]=cmnh->FB_LBTag;
	cmnh->FB_Metric=CongestionFromLeafTable[leafAlias][cmnh->FB_LBTag];
}

int CONGAAgent::getDestLeaf(Packet* pkt)
{
	int dstLeaf;
	int hostInOneLeaf=ParentClassifier_->maxHost()-ParentClassifier_->minHost()+1;

	hdr_ip* iph = hdr_ip::access(pkt);
	if(iph->dst().addr_<=ParentClassifier_->maxHost() 
		&& iph->dst().addr_>=ParentClassifier_->minHost())
	{
		dstLeaf=ParentClassifier_->nodeID();
	}
	else if(iph->dst().addr_>ParentClassifier_->maxHost())
	{
		dstLeaf=ceil((float) (iph->dst().addr_ - ParentClassifier_->maxHost())/hostInOneLeaf) + ParentClassifier_->nodeID();
	}
	else if(iph->dst().addr_<ParentClassifier_->minHost())
	{
		dstLeaf=floor((float) (iph->dst().addr_ - ParentClassifier_->minHost())/hostInOneLeaf) + ParentClassifier_->nodeID();
	}

	return dstLeaf;
}

int CONGAAgent::findBestPath(int dstLeaf, int maxslot)
{
	int k=rand()%maxslot;
//	printf("%lf-Node-%d: k=%d\n",Scheduler::instance().clock(),ParentClassifier_->nodeID(),k);
	int min=k;
	for(int i=0;i<maxslot;i++)
	{
		if(CongestionToLeafTable[dstLeaf][(k+i)%maxslot] < CongestionToLeafTable[dstLeaf][min])
		{
			min=(k+i)%maxslot;
		}
	}
	return min;
}

int CONGAAgent::findNextFeedbackLB(int dstLeaf, int FB_LB)
{
	int nextFB_LB=FB_LB;

/*	if(LatestChangedFB_LB[dstLeaf]==-1)
	{
		nextFB_LB=(FB_LB+1)%PortNumber;
	}
	else
	{
		nextFB_LB=LatestChangedFB_LB[dstLeaf];
		LatestChangedFB_LB[dstLeaf]=-1;
	}*/

	nextFB_LB=(FB_LB+1)%PortNumber;

	return nextFB_LB;
}


void CONGAAgent::PrintPacket(Packet* pkt, char SendorReceive[10])
{
	if(!IF_PRINT_DEBUG)
	{
		return;
	}
	hdr_cmn* cmnh = hdr_cmn::access(pkt);
	printf("%lf-Node-%d: %s packet %d, srcLeaf=%d, dstLeaf=%d, LBTag=%d, CE=%d, FB_LBTag=%d, FB_Metric=%d\n"
		,Scheduler::instance().clock(),ParentClassifier_->nodeID()
		,SendorReceive,cmnh->uid_
		,cmnh->srcLeaf,cmnh->dstLeaf,cmnh->LBTag
		,cmnh->CE,cmnh->FB_LBTag,cmnh->FB_Metric);
}

void CONGAAgent::PrintCongestionToLeafTable(int dstLeaf)
{
	if(!IF_PRINT_DEBUG)
	{
		return;
	}
	int hostInOneLeaf=ParentClassifier_->maxHost()-ParentClassifier_->minHost()+1;
	int leafAlias=dstLeaf-hostInOneLeaf*LeafNumber;

	printf("%lf-Node-%d:\n",Scheduler::instance().clock(),ParentClassifier_->nodeID());
	for(int i=0;i<PortNumber;i++)
	{
		printf("CongestionToLeafTable[%d][%d]=%d\n",leafAlias,i,CongestionToLeafTable[leafAlias][i]);
	}
}

void CONGAAgent::PrintCongestionFromLeafTable(int srcLeaf)
{
	if(!IF_PRINT_DEBUG)
	{
		return;
	}
	int hostInOneLeaf=ParentClassifier_->maxHost()-ParentClassifier_->minHost()+1;
	int leafAlias=srcLeaf-hostInOneLeaf*LeafNumber;

	printf("%lf-Node-%d:\n",Scheduler::instance().clock(),ParentClassifier_->nodeID());
	for(int i=0;i<PortNumber;i++)
	{
		printf("CongestionFromLeafTable[%d][%d]=%d\n",leafAlias,i,CongestionFromLeafTable[leafAlias][i]);
	}
}


void FlowletTimer::expire(Event*)
{
	a_->flowletTimeout();
}


void CONGAAgent::flowletTimeout()
{
	flowletTimer_.resched(T_FL);
	for(int i=0;i<MAX_FLOWLET;i++)
	{
		if(flowletTable_[i].age==1 && flowletTable_[i].valid==1)
		{
			if(IF_PRINT_DEBUG)
			{
				printf("%lf-Node-%d: flowletTable_[%d] timeout!\n"
				,Scheduler::instance().clock(),ParentClassifier_->nodeID(),i);
			}
			flowletTable_[i].port=0;
			flowletTable_[i].valid=0;
		}
		flowletTable_[i].age=1;
	}
}

void CONGAAgent::printSeqTimeline(Packet* pkt)
{
	if(!IF_PRINT_SEQTIMELINE)
	{
		return;
	}
	FILE * fpSeqno;
	char str1[128];
	hdr_ip* iph = hdr_ip::access(pkt);
	sprintf(str1,"Node%d_SendSeqno.tr",iph->src().addr_);

	hdr_tcp* tcph = hdr_tcp::access(pkt);
	if(tcph->seqno()==0)
	{
		fpSeqno=fopen(str1,"w");
	}
	else
	{
		fpSeqno=fopen(str1,"a+");
	}
	fprintf(fpSeqno,"%lf %d\n",Scheduler::instance().clock(),tcph->seqno());
	fclose(fpSeqno);
}

void CONGAAgent::printLBTagTimeline(int dstLeaf)
{
	if(!IF_PRINT_VPWINDOWTIMELINE)
	{
		return;
	}
	FILE * fpLBTag;
	char str1[128];
	sprintf(str1,"Leaf%d-Leaf%d_LBTag.tr",ParentClassifier_->nodeID(),dstLeaf);

	if(ifFirstPrintLBTag[dstLeaf]==0)
	{
		fpLBTag=fopen(str1,"w");
		ifFirstPrintLBTag[dstLeaf]=1;
	}
	else
	{
		fpLBTag=fopen(str1,"a+");
	}
	fprintf(fpLBTag,"%lf ",Scheduler::instance().clock());
	for(int i=0;i<PortNumber;i++)
	{
		fprintf(fpLBTag,"%d ",LBTagSelection[dstLeaf][i]);
	}
	for(int i=0;i<PortNumber;i++)
	{
		fprintf(fpLBTag,"%3d ",CongestionToLeafTable[dstLeaf][i]);
	}
	fprintf(fpLBTag,"\n");

	fclose(fpLBTag);
}