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

#include "clb_tunnel.h"
#include "random.h"
#include "address.h"
#include "ip.h"
#include "time.h"
#include "tcp.h"
#include <sys/stat.h> ///CG add
#include <errno.h> ///CG add



#define IF_PRINT_DEBUG 0
#define IF_PRINT_SEQTIMELINE 0
#define IF_PRINT_VPWINDOWTIMELINE 0

#define ENCODE_TIMER_LENGTH 8E-5
#define DECODE_TIMER_LENGTH ((double)(BlockSize_MaxP+1)*ENCODE_TIMER_LENGTH)
#define ALPHA 0.5
#define VPNumber 16
#define ReorderGenNumber 3
#define MAX_PACKETS_COUNT 65536
#define INITIAL_PATH_NUMBER 8
#define MAX_GENERATION 65536

#define MAINTAIN_VP_UPHOLD 30
#define MAINTAIN_VP_LOWHOLD 20


CLBTunnelAgent::CLBTunnelAgent(Classifier* agent_, int N, int P)
 :encodeTimer_(this), decodeTimer_(this)
{
	ParentClassifier_=agent_;
	BlockSize_N=N;
	BlockSize_MaxP=P;
	BlockSize_MaxK=BlockSize_N-BlockSize_MaxP;
// printf("%lf-NODE %d: BlockSize N=%d,P=%d!\n",Scheduler::instance().clock(),ParentClassifier_->nodeID(),BlockSize_N,BlockSize_MaxP);
	initCLBTunnel();
}


CLBTunnelAgent::~CLBTunnelAgent()
{
	printf("%lf-NODE %d: TCP done!\n",Scheduler::instance().clock(),ParentClassifier_->nodeID());
	encodeTimer_.cancel();
	decodeTimer_.cancel();

	delete [] VPCost;
	delete [] VPRateEstimation;
	delete [] VPLastUpdateTime;
	delete [] VPPacketsSent;
	delete [] VPPacketsAcked;
	delete [] VPPacketsOnFly;
	delete [] FeedbackVPAckedPackets;
	for(int i=0;i<ReorderGenNumber;i++)
	{
		delete [] parityBuffer_[i].buffer;
	}	
	delete [] parityBuffer_;
	delete [] reorderBuffer_.buffer;
	delete [] encodeBuffer_.buffer;
	delete [] VPSelectCondition;
	ParityWaitToBeSent=NULL;
}

void CLBTunnelAgent::initCLBTunnel()
{
	currentFeedbackVP=0;
	currentGenID=0;
	currentGenIndex=0;
	expectHandOverGenIndex=0;
	headReorderBuffer=0;

	LatestChangedFeedbackVPAckedPackets=-1;
	/////-1 means no recent changes scince last feedback. else means who is the most recently changed
	usedVP=0;
	countExploredOnceVP=0;


	VPCost=new double [VPNumber];
	memset(VPCost,0,VPNumber*sizeof(double));

	VPRateEstimation=new double [VPNumber];
	memset(VPRateEstimation,0,VPNumber*sizeof(double));
	VPLastUpdateTime=new double [VPNumber];
	memset(VPLastUpdateTime,0,VPNumber*sizeof(double));

	VPPacketsSent=new int [VPNumber];
	memset(VPPacketsSent,0,VPNumber*sizeof(int));
	VPPacketsAcked=new int [VPNumber];
	memset(VPPacketsAcked,0,VPNumber*sizeof(int));
	VPPacketsOnFly=new int [VPNumber];
	memset(VPPacketsOnFly,0,VPNumber*sizeof(int));
	FeedbackVPAckedPackets=new int [VPNumber];
	memset(FeedbackVPAckedPackets,0,VPNumber*sizeof(int));

	BlockSize_K=BlockSize_N-BlockSize_MaxP;

	parityBuffer_=new ParityBuffer [ReorderGenNumber];
	memset(parityBuffer_,0,ReorderGenNumber*sizeof(ParityBuffer));
	for(int i=0;i<ReorderGenNumber;i++)
	{
		parityBuffer_[i].capacity=BlockSize_MaxP;
		parityBuffer_[i].ifUpdated=0;

		parityBuffer_[i].count=0;
		parityBuffer_[i].genID=i;
		parityBuffer_[i].blockSize=BlockSize_K;
		parityBuffer_[i].headIndex=i;
		parityBuffer_[i].lastGenOriginalIndex=i+BlockSize_K;
		parityBuffer_[i].buffer=new Packet* [BlockSize_MaxP];
		memset(parityBuffer_[i].buffer,0,BlockSize_MaxP*sizeof(Packet*));
	}
	packetsCountInParityBuffer=0;

	reorderBuffer_.head=0;
	reorderBuffer_.count=0;
	reorderBuffer_.capacity=ReorderGenNumber*BlockSize_K;
	reorderBuffer_.headIndex=0;
	reorderBuffer_.buffer=new Packet* [ReorderGenNumber*BlockSize_K];
	memset(reorderBuffer_.buffer,0,ReorderGenNumber*BlockSize_K*sizeof(Packet*));
	packetsUnHandedInReorderBuffer=0;

	encodeBuffer_.head=0;
	encodeBuffer_.tail=0;
	encodeBuffer_.length=0;
	encodeBuffer_.capacity=BlockSize_K;
	encodeBuffer_.buffer=new Packet* [BlockSize_K];
	memset(encodeBuffer_.buffer,0,BlockSize_K*sizeof(Packet*));

	VPSelectCondition=new int [VPNumber];
	memset(VPSelectCondition,0,VPNumber*sizeof(int));

	currentGeneratedParityNumber=0;

	codingRateBlockSize=BlockSize_K;
}


void CLBTunnelAgent::recv(Packet* pkt, Handler*h)
{
	hdr_cmn* cmnh = hdr_cmn::access(pkt);
	if(cmnh->ptype()==PT_RTPROTO_DV)////Don't deal with DV packets
	{
		targetRecv(pkt,h);
	}

	hdr_ip* iph = hdr_ip::access(pkt);
	// PrintPacket(pkt,"CLBTunnelAgent recv");
	if(iph->dst().addr_==ParentClassifier_->nodeID())
	{
		if((HDR_CMN(pkt))->ifTunnel()==1)
		{
			(HDR_CMN(pkt))->ifTunnel_=0;
			decode(ParentClassifier_,pkt,h); 
		}
	}
	else 
	{
		if((HDR_CMN(pkt))->ifTunnel()==0)
		{
			(HDR_CMN(pkt))->ifTunnel_=1;
			encode(ParentClassifier_,pkt,h);//CG add!
		}
	}
}

int CLBTunnelAgent::findBestVP()
{
	int k=rand()%VPNumber;
	int min=k;

	int flag=0;
	for(int i=0;i<VPNumber;i++)/////Pick the first used VP
	{
		if(VPRateEstimation[(k+i)%VPNumber]!=0 && VPCost[(k+i)%VPNumber]!=-1)
		{
			min=(k+i)%VPNumber; 
			flag=1;
			break;
		}
	}
	if(flag==0)
	{
		printf( "%s\n", "ERROR: findBestVP called, but no active VP!");
		PrintVPWindow(1);
		exit(1);
	}

	for(int i=0;i<VPNumber;i++)
	{	
		if(VPRateEstimation[(k+i)%VPNumber]==0 )
		{
			continue;
		}
		if(VPRateEstimation[min]==0)
		{
			printf( "ERROR: VPRateEstimation[min]==0! min=%d\n",min);
			PrintVPWindow(1);
			exit(1);
		}

		if(	VPCost[(k+i)%VPNumber]!=-1 
			&& (VPCost[(k+i)%VPNumber]+1/VPRateEstimation[(k+i)%VPNumber])<(VPCost[min]+1/VPRateEstimation[min]))
		{
			min=(k+i)%VPNumber;
		} 
	}
	return min;
}

int CLBTunnelAgent::createNewVP()
{
	int k=rand()%VPNumber;
	int newPath=-1;
	// int flag=0;
	for(int i=0;i<VPNumber;i++)
	{
		if(VPRateEstimation[(k+i)%VPNumber]==0 && VPCost[(k+i)%VPNumber]!=-1)
		{
			// flag=1;
			newPath=(k+i)%VPNumber;
			break;
		}
	}

	return newPath;
}

void CLBTunnelAgent::clearWorstVP()
{
	int clearNumber=0;
	if(usedVP-MAINTAIN_VP_LOWHOLD > usedVP-countExploredOnceVP)
	{
		clearNumber=usedVP-countExploredOnceVP;
	}
	else
	{
		clearNumber=usedVP-MAINTAIN_VP_LOWHOLD;
	}

	for(int n=0;n<clearNumber;n++)
	{
		int k=rand()%VPNumber;
		int max=k;

		int flag=0;
		for(int i=0;i<VPNumber;i++)/////Pick the first used VP
		{
			if(VPRateEstimation[(k+i)%VPNumber]!=0 && VPCost[(k+i)%VPNumber]!=-1)
			{
				max=(k+i)%VPNumber; 
				flag=1;
				break;
			}
		}
		if(flag==0)
		{
			printf( "%s\n", "ERROR: clearWorstVP called, but no clearable VP!");
			PrintVPWindow(1);
			exit(1);
		}

		for(int i=0;i<VPNumber;i++)
		{	
			if(VPRateEstimation[(k+i)%VPNumber]==0)
			{
				continue;
			}

			if(VPRateEstimation[max]==0)
			{
				printf( "ERROR: VPRateEstimation[max]==0! max=%d\n",max);
				PrintVPWindow(1);
				exit(1);
			}
			if(	VPCost[(k+i)%VPNumber]!=-1 
				&& (VPCost[(k+i)%VPNumber]+1/VPRateEstimation[(k+i)%VPNumber])>(VPCost[max]+1/VPRateEstimation[max]))
			{
				max=(k+i)%VPNumber;
			} 
		}
		////Clear the worst VP's rate and cost and lastUpdateTime
		VPRateEstimation[max]=0;
		VPLastUpdateTime[max]=0;
		VPCost[max]=0;
		usedVP--;	
	}
}

int CLBTunnelAgent::setVP(Packet* pkt)
{
	// if(usedVP>countExploredOnceVP && usedVP>MAINTAIN_VP_UPHOLD)
	// {
	// 	clearWorstVP();
	// 	// printf("clearWorstVP\n");
	// 	// PrintVPWindow(1);
	// }

	int VP=0;

	VP=rand()%65536;
	return VP;

	/*if((BlockSize_MaxP>0 && (HDR_CMN(pkt))->ifParity==0)/////for coded LB
	|| (BlockSize_MaxP==0)) /////for no code LB
	{
		if(usedVP<INITIAL_PATH_NUMBER)
		{
			VP=createNewVP();
			if(VP!=-1)////New VP created			
			{
				VPLastUpdateTime[VP]=Scheduler::instance().clock();
				VPCost[VP]=-1;/////-1 means has been explored once!
				countExploredOnceVP++;
				usedVP++;
			}
			else///// all VP has been explored once!
			{
				VP=rand()%VPNumber;
				printf("Can't create new VP, randomly select %d\n",VP);
				if(usedVP!=VPNumber)
				{
					printf("All VP should be explored once at this moment\n");
					PrintVPWindow(1);
					exit(1);
				}
				if(MAINTAIN_VP_UPHOLD<VPNumber)
				{
					printf("ERROR: No way to fail creating new VP\n");
					PrintVPWindow(1);
					exit(1);
				}
			}
		}
		else if(usedVP>countExploredOnceVP)
		{
			VP=findBestVP();
		}
		else if(usedVP==countExploredOnceVP)
		{
			VP=rand()%VPNumber;
			if(VPCost[VP]!=-1)
			{
				VPLastUpdateTime[VP]=Scheduler::instance().clock();	
				VPCost[VP]=-1;
				countExploredOnceVP++;
				usedVP++;
			}
		}
		else
		{
			printf("VP select ERROR!\n");
			PrintVPWindow(1);
			exit(1);
		}
	}
	else /////////Parity packets
	{
		VP=createNewVP();
		if(VP!=-1)////New VP created	
		{
			VPLastUpdateTime[VP]=Scheduler::instance().clock();
			VPCost[VP]=-1;/////-1 means has been explored once!
			countExploredOnceVP++;
			usedVP++;
		}
		else
		{
			if(usedVP!=VPNumber)
			{
				printf("All VP should be explored at this moment\n");
				PrintVPWindow(1);
				exit(1);
			}

			if(usedVP>countExploredOnceVP)
			{
				VP=findBestVP();
			}
			else////All explored once
			{
				VP=rand()%VPNumber;
			}
		}
	}*/

	if(VPPacketsSent[VP]+1>=MAX_PACKETS_COUNT)
	{
		hdr_ip* iph = hdr_ip::access(pkt);
		printf("VPPacketsSent Overflow!\n");
		printf("%lf-Flow%d.%d-%d.%d VPPacketsSent[%d]=%d Overflow!\n"
			,Scheduler::instance().clock()
			,iph->src().addr_,iph->src().port_
			,iph->dst().addr_,iph->dst().port_
			,VP
			,VPPacketsSent[VP]);
		// PrintVPWindow(1);
		// exit(1);
	}
	VPPacketsSent[VP]=(VPPacketsSent[VP]+1)%MAX_PACKETS_COUNT;
	VPPacketsOnFly[VP]++;
	if(VPPacketsOnFly[VP]>=MAX_PACKETS_COUNT)
	{
		hdr_ip* iph = hdr_ip::access(pkt);
		printf( "%s\n", "VPPacketsOnFly Overflow!");
		printf("%lf-Flow%d.%d-%d.%d VPPacketsOnFly[%d]=%d Overflow!\n"
			,Scheduler::instance().clock()
			,iph->src().addr_,iph->src().port_
			,iph->dst().addr_,iph->dst().port_
			,VP
			,VPPacketsOnFly[VP]);
		PrintVPWindow(1);
		exit(1);
	}

	if(VPCost[VP]!=-1)/////update VPCost!!
	{
		if(VPRateEstimation[VP]==0)
		{
			/*printf("ERROR: select vp=%d, cost and rate not match!\n",VP);
			PrintVPWindow(1);
			exit(1);*/
		}
		else
		{
			VPCost[VP]=VPPacketsOnFly[VP]/VPRateEstimation[VP];
		}
	}

	return VP;
}

int CLBTunnelAgent::findNextFeedbackVP(int vp)
{
	int nextVP=(vp+1)%VPNumber;

	/*	if(LatestChangedFeedbackVPAckedPackets==-1)
	{
		nextVP=(vp+1)%VPNumber;
	}
	else
	{
		nextVP=LatestChangedFeedbackVPAckedPackets;
		LatestChangedFeedbackVPAckedPackets=-1;
	}*/

		for(int i=0;i<VPNumber;i++)
		{
			if(FeedbackVPAckedPackets[(nextVP+i)%VPNumber]!=0)
			{
				nextVP=(nextVP+i)%VPNumber;
				break;
			}
		}

		return nextVP;
	}


void CLBTunnelAgent::setPacket(Packet* pkt, int ifParity) ////No setting for parity packet!
{
	PrintVPWindow();

	hdr_cmn* cmnh = hdr_cmn::access(pkt);
	
	cmnh->ifTunnel() = 1;	
	cmnh->genIndex=currentGenIndex;
	cmnh->ifParity=ifParity;

	/*cmnh->feedBackVP=findNextFeedbackVP(currentFeedbackVP);
	cmnh->feedBackVP_ACKedPackets=FeedbackVPAckedPackets[cmnh->feedBackVP];
	cmnh->vPath=setVP(pkt);
	currentFeedbackVP=cmnh->feedBackVP;
	VPSelectCondition[cmnh->vPath]++;*/

	cmnh->vPath=setVP(pkt);
	
	if(ifParity==0)////index only count for original packets
	{
		currentGenIndex=(currentGenIndex+1)%MAX_PACKETS_COUNT;
	}

	printVPWindowTimeline(pkt);
	cmnh=NULL;
	PrintVPWindow();
}

void CLBTunnelAgent::createParityPackets(Classifier* ParentClassifier, Handler* hdl)
{
	BlockSize_K=encodeBuffer_.length;

	if(BlockSize_K<=0 || BlockSize_K>encodeBuffer_.capacity)
	{
		printf("ERROR: %lf-NODE %d: Sending parity packets, but BlockSize_K=%d\n",Scheduler::instance().clock(),ParentClassifier_->nodeID(),BlockSize_K);
		printf("encodeBuffer_.tail=%d, head=%d\n",
			encodeBuffer_.tail,
			encodeBuffer_.head);
		exit(1);
	}
	
	Packet* parityPacket=
	encodeBuffer_.buffer[(encodeBuffer_.tail-1+encodeBuffer_.capacity)%encodeBuffer_.capacity]->copy();
	parityPacket->formerCLBPacketsNumber=BlockSize_K;
	parityPacket->formerCLBPackets=new Packet* [BlockSize_K];
	memset(parityPacket->formerCLBPackets,0,BlockSize_K*sizeof(Packet*));

	
	for(int j=0;j<BlockSize_K;j++)
	{
		int position=(encodeBuffer_.head+j+encodeBuffer_.capacity)%encodeBuffer_.capacity;
		if(encodeBuffer_.buffer[position]==NULL)
		{
			printf("EncodeBuffer[%d] error!\n",position);
		}
		parityPacket->formerCLBPackets[j]=encodeBuffer_.buffer[position]->copy();
	}
	
	
	setPacket(parityPacket,1);
	hdr_cmn* cmnHead = hdr_cmn::access(encodeBuffer_.buffer[encodeBuffer_.head]);
	(HDR_CMN(parityPacket))->genID=currentGenID;
	(HDR_CMN(parityPacket))->headGenIndex=cmnHead->genIndex;
	(HDR_CMN(parityPacket))->ParityIndex=currentGeneratedParityNumber;
	(HDR_CMN(parityPacket))->blockSize=BlockSize_K;

	currentGeneratedParityNumber++;

	if((HDR_CMN(parityPacket))->blockSize<=0 
		|| (HDR_CMN(parityPacket))->blockSize>encodeBuffer_.capacity)
	{
		printf("ERROR: parityPacket->blockSize setting error!\n");
		exit(1);
	}
	

	Tcl& tcl = Tcl::instance();
	tcl.evalf("[Simulator instance] clb-parity-number-add");

	if(currentGeneratedParityNumber>BlockSize_MaxP)
	{
		printf("ERROR: currentGeneratedParityNumber=%d, > BlockSize_MaxP!\n",currentGeneratedParityNumber);
		exit(1);
	}

	PrintPacket(parityPacket,"Send parity",1);
	
	targetRecv(parityPacket,hdl);
}

void CLBTunnelAgent::encode(Classifier* ParentClassifier, Packet* pkt, Handler* hdl)
{
	// hdr_cmn* cmnh = hdr_cmn::access(pkt);
	// printf("%lf-NODE %d: Encode packet %d!\n",Scheduler::instance().clock(),ParentClassifier_->nodeID(),cmnh->uid());

	PrintEncodeBuffer(0);
	setPacket(pkt,0);
	// plug new packet in the tail
	encodeBuffer_.buffer[encodeBuffer_.tail]=pkt->copy();
	encodeBuffer_.tail=(encodeBuffer_.tail+1)%encodeBuffer_.capacity;
	encodeBuffer_.length++;

	if(encodeBuffer_.length > encodeBuffer_.capacity
		|| encodeBuffer_.length <= 0)
	{
		printf("ERROR: encodeBuffer_.length > capacity, lenth=%d\n", encodeBuffer_.length);
		PrintEncodeBuffer(1);
		exit(1);
	}
	PrintEncodeBuffer(0);

	printSeqTimeline(pkt);
	PrintPacket(pkt,"send",1);
	targetRecv(pkt,hdl);

	////Check if should generate a parity
	////Generate a parity
	if(encodeBuffer_.length >= codingRateBlockSize)
	{
		if(encodeBuffer_.capacity==encodeBuffer_.length)////Encode buffer full
		{
			Tcl& tcl = Tcl::instance();
			tcl.evalf("[Simulator instance] clb-max-encode-buffer-size %d",encodeBuffer_.length);
			tcl.evalf("[Simulator instance] clb-parity-dueToFull-add");/////Calculate total full times!
		}
		for(int i=0;i<BlockSize_MaxP;i++)
		{
			createParityPackets(ParentClassifier_,0);
		}
		encodeBuffer_.head=0;
		encodeBuffer_.tail=0;
		encodeBuffer_.length=0;
		memset(encodeBuffer_.buffer,0,BlockSize_K*sizeof(Packet*));

		currentGeneratedParityNumber=0;
		currentGenID=(currentGenID+1)%MAX_GENERATION;
		PrintEncodeBuffer(0);
	}
	
}


void CLBTunnelAgent::decode(Classifier* ParentClassifier, Packet* pkt, Handler* hdl)
{
	hdr_cmn* cmnh = hdr_cmn::access(pkt);
	cmnh->ifTunnel()=0;

	PrintVPWindow();		
	printVPWindowTimeline(pkt);
	PrintPacket(pkt,"receive");

	/*FeedbackVPAckedPackets[cmnh->vPath]=(FeedbackVPAckedPackets[cmnh->vPath]+1)%MAX_PACKETS_COUNT;
	LatestChangedFeedbackVPAckedPackets=cmnh->vPath;

	////Cleared VP!
	if(VPRateEstimation[cmnh->feedBackVP]==0 
		&& VPCost[cmnh->feedBackVP]==0)
	{
		////Do nothing about VP info
	}
	else
	{
		int formerVPPacketsAcked=VPPacketsAcked[cmnh->feedBackVP];
		if((cmnh->feedBackVP_ACKedPackets-formerVPPacketsAcked+MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT < MAX_PACKETS_COUNT/2)
		{
			VPPacketsAcked[cmnh->feedBackVP]=cmnh->feedBackVP_ACKedPackets;
		}
		VPPacketsOnFly[cmnh->feedBackVP]=(VPPacketsSent[cmnh->feedBackVP] - VPPacketsAcked[cmnh->feedBackVP] 
			+ MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT;
		double nowTime=Scheduler::instance().clock();
		double currentRate=0;
		if(nowTime-VPLastUpdateTime[cmnh->feedBackVP] == 0)
		{
			if(VPPacketsAcked[cmnh->feedBackVP]==0)////Initial state, may be time duration=0
			{
				currentRate=0;
			}
			else
			{
				printf("ERROR: nowTime=%lf, VPLastUpdateTime=%lf, nowTime-VPLastUpdateTime=0!\n"
					,nowTime,VPLastUpdateTime[cmnh->feedBackVP]);
				// exit(1);
				currentRate=0;
			}
		}
		else
		{
			currentRate=((double) VPPacketsAcked[cmnh->feedBackVP]-formerVPPacketsAcked)*1500/(nowTime-VPLastUpdateTime[cmnh->feedBackVP]);
		}
		
		if(currentRate>0)
		{
			VPRateEstimation[cmnh->feedBackVP]=ALPHA*VPRateEstimation[cmnh->feedBackVP]+(1-ALPHA)*currentRate;
			VPLastUpdateTime[cmnh->feedBackVP]=nowTime;
		}

		if(VPRateEstimation[cmnh->feedBackVP]>0)
		{
			if(VPCost[cmnh->feedBackVP]==-1)
			{
				countExploredOnceVP--;
			}
			VPCost[cmnh->feedBackVP]=VPPacketsOnFly[cmnh->feedBackVP]/VPRateEstimation[cmnh->feedBackVP];
		}
	}*/

	PrintVPWindow();

	// hdr_ip* iph = hdr_ip::access(pkt);
	// int src=iph->src().addr_;
	// int dst=iph->dst().addr_;

	// if(ParentClassifier_->nodeID()==671 && flowID==21)
	// {
	// 	PrintReorderBuffer(1);
	// 	printf("%lf-NODE %d: insertReorderBuffer!\n"
	//  	,Scheduler::instance().clock(),ParentClassifier_->nodeID());
	// }
	
	
	PrintReorderBuffer(0);

	// printf("%lf-NODE %d: insertReorderBuffer!\n"
	// 	,Scheduler::instance().clock(),ParentClassifier_->nodeID());
	insertReorderBuffer(pkt);

	PrintReorderBuffer(0);
	// if(ParentClassifier_->nodeID()==671 && flowID==21)
	// {
	// 	PrintReorderBuffer(1);
	// 	printf("%lf-NODE %d: decodeReorderBuffer!\n"
	//  	,Scheduler::instance().clock(),ParentClassifier_->nodeID());
	// }
	// printf("%lf-NODE %d: decodeReorderBuffer!\n"
	// 	,Scheduler::instance().clock(),ParentClassifier_->nodeID());

	if(BlockSize_MaxP>0)
	{
		decodeReorderBuffer(ParentClassifier,hdl);
	}

	PrintReorderBuffer(0);

	// if(ParentClassifier_->nodeID()==671 && flowID==21)
	// {
	// 	PrintReorderBuffer(1);
	// 	printf("%lf-NODE %d: checkReorderBuffer!\n"
	//  	,Scheduler::instance().clock(),ParentClassifier_->nodeID());
	// }
	// printf("%lf-NODE %d: checkReorderBuffer!\n"
	// 	,Scheduler::instance().clock(),ParentClassifier_->nodeID());

	checkReorderBuffer(ParentClassifier,hdl);

	// if(ParentClassifier_->nodeID()==671 && flowID==21)
	// {
	// 	PrintReorderBuffer(1);
	// 	printf("%lf-NODE %d: checkReorderBuffer finished!\n"
	//  	,Scheduler::instance().clock(),ParentClassifier_->nodeID());
	// }


	PrintReorderBuffer(0);
}

void CLBTunnelAgent::insertReorderBuffer(Packet* pkt)
{
	// printf("%lf-NODE %d: insertReorderBuffer!\n"
	// 	,Scheduler::instance().clock(),ParentClassifier_->nodeID());
	hdr_cmn* cmnh = hdr_cmn::access(pkt);

	if(cmnh->ifParity==0)
	{
		if((cmnh->genIndex - expectHandOverGenIndex + MAX_PACKETS_COUNT) % MAX_PACKETS_COUNT
			> MAX_GENERATION/2)
		{
			PrintPacket(pkt,"Hand to OS out-dated",1);
			targetRecv(pkt,0);
			return;
		}

		
		int headOffset=(cmnh->genIndex-reorderBuffer_.headIndex+MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT;

		if(headOffset>=reorderBuffer_.capacity)
		{
			PrintPacket(pkt,"Receive",1);
			printf("Reorder buffer index overflow! Not handle yet!\n");
			PrintReorderBuffer(1);
			exit(1);
		}

		int insertPos=
		((reorderBuffer_.head + cmnh->genIndex - reorderBuffer_.headIndex+MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT)%reorderBuffer_.capacity;
		if(reorderBuffer_.buffer[insertPos]!=NULL 
			&& (HDR_CMN(reorderBuffer_.buffer[insertPos]))->ifParity!=-1) ////-1 means recovered packets
		{
			PrintPacket(pkt,"Receive",1);
			printf("Reorder buffer error!\n");
			PrintReorderBuffer(1);
			exit(1);
		}
		else if(reorderBuffer_.buffer[insertPos]!=NULL 
			&& (HDR_CMN(reorderBuffer_.buffer[insertPos]))->ifParity==-1)////-1 means recovered packets
		{
			PrintPacket(pkt,"Drop out-dated already recovered packet");
			Packet::free(pkt);
			return;
		}

		reorderBuffer_.buffer[insertPos]=pkt;
		reorderBuffer_.count++;
		packetsUnHandedInReorderBuffer++;

		if(BlockSize_MaxP==0)
		{
			return;
		}


		if(((cmnh->genIndex - parityBuffer_[headReorderBuffer].headIndex+MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT)
			< MAX_PACKETS_COUNT/2)
		{
			if(parityBuffer_[headReorderBuffer].ifUpdated==1)
			{
				if(((parityBuffer_[headReorderBuffer].lastGenOriginalIndex - cmnh->genIndex +MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT)
					< MAX_PACKETS_COUNT/2)
				{
					parityBuffer_[headReorderBuffer].count++;
				}
			}

			if(packetsCountInParityBuffer!=0)
			{
				for(int k=1;k<ReorderGenNumber;k++)
				{
					int i=(headReorderBuffer+k)%ReorderGenNumber;
					if(parityBuffer_[i].ifUpdated==0)
					{
						continue;
					}
					if(((cmnh->genIndex - parityBuffer_[i].headIndex+MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT)
						< MAX_PACKETS_COUNT/2
						&& ((parityBuffer_[i].lastGenOriginalIndex - cmnh->genIndex + MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT)
						< MAX_PACKETS_COUNT/2)
					{
						parityBuffer_[i].count++;
					}
				}
			}	
		}
	}
	else ////Parity
	{
		if((cmnh->genID - parityBuffer_[headReorderBuffer].genID + MAX_GENERATION) % MAX_GENERATION
			> MAX_GENERATION/2)
		{
			PrintPacket(pkt,"Drop out-dated parity");
			Packet::free(pkt);
			return;
		}

		int lastIndex=(cmnh->headGenIndex+cmnh->blockSize-1)%MAX_PACKETS_COUNT;
		if((lastIndex-expectHandOverGenIndex+MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT
			> MAX_PACKETS_COUNT/2)
		{
			PrintPacket(pkt,"Drop out-dated parity");
			Packet::free(pkt);
			return;
		}


		int whichBufferShouldIn=((cmnh->genID - parityBuffer_[headReorderBuffer].genID + MAX_GENERATION) 
			% MAX_GENERATION + headReorderBuffer)%ReorderGenNumber;

		if (whichBufferShouldIn < 0 || whichBufferShouldIn >= ReorderGenNumber)
		{
			PrintPacket(pkt,"Receive",1);
			printf("whichBufferShouldIn error!\n");
			PrintReorderBuffer(1);
			exit(1);
		}

		if(cmnh->genID!=parityBuffer_[whichBufferShouldIn].genID)
		{
			PrintPacket(pkt,"Receive",1);
			printf("Reorder buffer Gen overflow!!\n");

			headReorderBuffer=whichBufferShouldIn;
			resetParityBuffer(cmnh->genID);
			//PrintReorderBuffer(1);
			// exit(1);
		}

		if(parityBuffer_[whichBufferShouldIn].buffer[cmnh->ParityIndex]!=NULL)
		{
			PrintPacket(pkt,"Receive",1);
			printf("parityBuffer_ error!\n");
			PrintReorderBuffer(1);
			exit(1);
		}

		parityBuffer_[whichBufferShouldIn].buffer[cmnh->ParityIndex]=pkt;
		packetsCountInParityBuffer++;
		if(parityBuffer_[whichBufferShouldIn].ifUpdated==0)
		{
			parityBuffer_[whichBufferShouldIn].ifUpdated=1;
			parityBuffer_[whichBufferShouldIn].headIndex=cmnh->headGenIndex;
			parityBuffer_[whichBufferShouldIn].blockSize=cmnh->blockSize;
			int lastIndex=(cmnh->headGenIndex+cmnh->blockSize-1)%MAX_PACKETS_COUNT;
			parityBuffer_[whichBufferShouldIn].lastGenOriginalIndex=lastIndex;

			countPacketsInGen(whichBufferShouldIn);
		}
		parityBuffer_[whichBufferShouldIn].count++;
	}
}

void CLBTunnelAgent::countPacketsInGen(int bufferID)
{
	// printf("%lf-NODE %d: countPacketsInGen!\n",Scheduler::instance().clock(),ParentClassifier_->nodeID());
	if(parityBuffer_[bufferID].ifUpdated==0)
	{
		printf("ERROR: countPacketsInGen should not be used to noInfo parityBuffer_!\n");
		PrintReorderBuffer(1);
		exit(1);
	}

	parityBuffer_[bufferID].count=0;
	for(int i=0;i<parityBuffer_[bufferID].blockSize;i++)
	{
		int startPos=(reorderBuffer_.head
			+(parityBuffer_[bufferID].headIndex-reorderBuffer_.headIndex+MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT)
		%reorderBuffer_.capacity;
		int k=(i+startPos)%reorderBuffer_.capacity;
		if(reorderBuffer_.buffer[k]!=NULL)
		{
			parityBuffer_[bufferID].count++;
		}
	}
}

void CLBTunnelAgent::checkReorderBuffer(Classifier* ParentClassifier, Handler* hdl)
{
	// printf("%lf-NODE %d: checking reorder buffer!\n",Scheduler::instance().clock(),ParentClassifier_->nodeID());
	if(packetsUnHandedInReorderBuffer<0)
	{
		printf("ERROR: packetsUnHandedInReorderBuffer<0!\n");
		PrintReorderBuffer(1);
		exit(1);
	}
	if(packetsUnHandedInReorderBuffer==0)
	{
		if(decodeTimer_.status()==TIMER_PENDING)
		{
			decodeTimer_.cancel();
		}
		return;
	}

	int expectPos = ((expectHandOverGenIndex - reorderBuffer_.headIndex + reorderBuffer_.head + MAX_PACKETS_COUNT)
		% MAX_PACKETS_COUNT)
	%reorderBuffer_.capacity;
	if(expectPos<0 || expectPos>=reorderBuffer_.capacity)
	{
		printf("ERROR: expectPos=%d\n",expectPos);
		PrintReorderBuffer(1);
		exit(1);
	}


	if(reorderBuffer_.buffer[expectPos]==NULL)
	{
		if(BlockSize_MaxP==0)
		{
			decodeTimer_.resched(2*ENCODE_TIMER_LENGTH);
			return;
		}
		decodeTimer_.resched(DECODE_TIMER_LENGTH);
		return;
	}

	
	Packet* pkt=reorderBuffer_.buffer[expectPos]->copy();
	hdr_cmn* cmnh = hdr_cmn::access(pkt);

	if(cmnh->ifParity==0 || cmnh->ifParity==-1)////-1 means recovered packets
	{
		PrintPacket(pkt,"Hand to OS");
		packetsUnHandedInReorderBuffer--;
		///Hand over copy. Store original packet for recovering.
		targetRecv(pkt,hdl);
	}
	else
	{
		printf("Check error! Parity in reorderBuffer_!\n");
		PrintReorderBuffer(1);
		exit(1);
	}

	

	expectHandOverGenIndex = (expectHandOverGenIndex + 1) % MAX_PACKETS_COUNT;

	if(BlockSize_MaxP==0)
	{
		Packet::free(reorderBuffer_.buffer[expectPos]);
		reorderBuffer_.buffer[expectPos]=NULL;
		reorderBuffer_.count--;
		reorderBuffer_.head=(reorderBuffer_.head+1)%reorderBuffer_.capacity;
		reorderBuffer_.headIndex=(reorderBuffer_.headIndex + 1) % MAX_PACKETS_COUNT;
		checkReorderBuffer(ParentClassifier,hdl);
		return;
	}

	if ((expectHandOverGenIndex-reorderBuffer_.headIndex+MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT
		>=parityBuffer_[headReorderBuffer].blockSize)
	{
		if(packetsCountInParityBuffer!=0)
		{
			clearParityBuffer(headReorderBuffer);
			handOverAndClearBeforeIndex(expectHandOverGenIndex);
		}
		checkReorderBuffer(ParentClassifier,hdl);
	}
	else
	{
		checkReorderBuffer(ParentClassifier,hdl);
	}

	// printf("%lf-NODE %d: checking reorder buffer finished!\n",Scheduler::instance().clock(),ParentClassifier_->nodeID());
}

void CLBTunnelAgent::decodeReorderBuffer(Classifier* ParentClassifier, Handler* hdl)
{
	// printf("%lf-NODE %d: decoding decodeReorderBuffer!\n",Scheduler::instance().clock(),ParentClassifier_->nodeID());
	if(packetsCountInParityBuffer==0)
	{
		return;
	}

	for(int p=0;p<ReorderGenNumber;p++)
	{
		int buffID=(p+headReorderBuffer)%ReorderGenNumber;
		if(parityBuffer_[buffID].count<0)
		{
			printf("ERROR: parityBuffer_[%d].count=%d\n"
				,buffID
				,parityBuffer_[buffID].count);
			PrintReorderBuffer(1);
			exit(1);
		}

		if(parityBuffer_[buffID].count==0)
		{
			continue;
		}

		if(parityBuffer_[buffID].count>=parityBuffer_[buffID].blockSize)
		{
			//////For empty parity queue
			if(parityBuffer_[buffID].ifUpdated==0)
			{
				continue;
			}

			Packet* lastParity=NULL;
			for(int i=BlockSize_MaxP-1;i>=0;i--)
			{
				if(parityBuffer_[buffID].buffer[i]!=NULL)
				{
					lastParity=parityBuffer_[buffID].buffer[i];
					break;
				}
			}
			if(lastParity==NULL)
			{
				if(parityBuffer_[buffID].count!=BlockSize_MaxK)
				{
					printf("Decoding reorder buffer: lastParity==NULL, but count!=BlockSize_MaxK!\n");
					PrintReorderBuffer(1);
					exit(1);
				}
				return;
			}

			// printf("%lf-NODE %d: Able to decode, decode it!\n",Scheduler::instance().clock(),ParentClassifier_->nodeID());
			
			///Recover missed original packets!
			for(int i=0;i<parityBuffer_[buffID].blockSize;i++)
			{
				int offSet=(parityBuffer_[buffID].headIndex-reorderBuffer_.headIndex+MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT;
				int pos=(i+offSet+reorderBuffer_.head)%reorderBuffer_.capacity;
				if(reorderBuffer_.buffer[pos]==NULL)
				{
					Tcl& tcl = Tcl::instance();
					tcl.evalf("[Simulator instance] clb-decode-times-add");
					reorderBuffer_.buffer[pos]=lastParity->formerCLBPackets[i]->copy();
					(HDR_CMN(reorderBuffer_.buffer[pos]))->ifParity=-1; ////-1 means recovered packets
					reorderBuffer_.count++;
					packetsUnHandedInReorderBuffer++;
				}
			}
		}
	}
}

void CLBTunnelAgent::resetParityBuffer(int headGenID)
{
	int tailBuff=(headReorderBuffer+ReorderGenNumber-1)%ReorderGenNumber;
	if((parityBuffer_[tailBuff].genID-headGenID+MAX_GENERATION)%MAX_GENERATION
		< MAX_GENERATION/2)
	{
		printf("resetParityBuffer error!\n");
		PrintReorderBuffer(1);
		exit(1);
	}

	for(int j=0;j<ReorderGenNumber;j++)
	{
		int bufferID=(headReorderBuffer+j)%ReorderGenNumber;
		if(parityBuffer_[bufferID].ifUpdated!=0)
		{
			for(int i=0;i<BlockSize_MaxP;i++)
			{
				if(parityBuffer_[bufferID].buffer[i]!=NULL)
				{
					Packet::free(parityBuffer_[bufferID].buffer[i]);
					packetsCountInParityBuffer--;
					parityBuffer_[bufferID].buffer[i]=NULL;
				}
			}
		}
		parityBuffer_[bufferID].ifUpdated=0;
		parityBuffer_[bufferID].count=0;
		parityBuffer_[bufferID].blockSize=BlockSize_N-BlockSize_MaxP;
		parityBuffer_[bufferID].genID=(headGenID+j)%MAX_GENERATION;
	}	
}

void EncodeTimer::expire(Event*)
{
	a_->encodeTimeout();
}

void DecodeTimer::expire(Event*)
{
	a_->decodeTimeout();
}


void CLBTunnelAgent::encodeTimeout()
{
	// printf("%lf-NODE %d: Encode Timeout!!\n",Scheduler::instance().clock(),ParentClassifier_->nodeID());
	if(currentGeneratedParityNumber<BlockSize_MaxP)
	{
		PrintEncodeBuffer(0);
		encodeTimer_.resched(ENCODE_TIMER_LENGTH);
		createParityPackets(ParentClassifier_,0);
	}
	else
	{
		if(encodeBuffer_.buffer[encodeBuffer_.head]!=NULL)
		{
			Packet::free(encodeBuffer_.buffer[encodeBuffer_.head]);
			encodeBuffer_.buffer[encodeBuffer_.head]=NULL;
			encodeBuffer_.length--;
			//shift the head polinter
			encodeBuffer_.head=(encodeBuffer_.head+1)%encodeBuffer_.capacity;
			encodeTimer_.resched(ENCODE_TIMER_LENGTH);
		}
	}
}

void CLBTunnelAgent::decodeTimeout()
{

	// if(ParentClassifier_->nodeID()==253 && flowID==26)
	// {
	// 	PrintReorderBuffer(1);
	// 	printf("==========%lf-NODE %d (Flow %d): Decode Timeout!!\n"
	// 	,Scheduler::instance().clock()
	// 	,ParentClassifier_->nodeID()
	// 	,flowID);
	// }
	
	// printf("==========%lf-NODE %d (Flow %d): Decode Timeout!!\n"
	// 	,Scheduler::instance().clock()
	// 	,ParentClassifier_->nodeID()
	// 	,flowID);
	if(packetsUnHandedInReorderBuffer<0)
	{
		printf("ERROR: packetsUnHandedInReorderBuffer<0!\n");
		PrintReorderBuffer(1);
		exit(1);
	}

	if(packetsUnHandedInReorderBuffer==0)
	{
		return;
	}

	if(BlockSize_MaxP>0)
	{
		decodeReorderBuffer(ParentClassifier_,0);
	}

	int expectPos = ((expectHandOverGenIndex - reorderBuffer_.headIndex + reorderBuffer_.head + MAX_PACKETS_COUNT) 
		% MAX_PACKETS_COUNT)
	% reorderBuffer_.capacity;
	if(expectPos<0 || expectPos>=reorderBuffer_.capacity)
	{
		printf("ERROR: expectPos=%d\n",expectPos);
		PrintReorderBuffer(1);
		exit(1);
	}

	if(reorderBuffer_.buffer[expectPos]!=NULL)
	{
		checkReorderBuffer(ParentClassifier_,0);
	}
	else
	{
		if(BlockSize_MaxP==0)
		{
			if(reorderBuffer_.headIndex!=expectHandOverGenIndex
				|| reorderBuffer_.buffer[reorderBuffer_.head]!=NULL)
			{
				printf("NCLB timeout error!\n");
				PrintReorderBuffer(1);
				exit(1);
			}
			for(int i=0;i<BlockSize_MaxK;i++)
			{
				int expectPos = reorderBuffer_.head;
				// printf("expectPos=%d\n",expectPos);
				if(reorderBuffer_.buffer[expectPos]!=NULL)
				{
					checkReorderBuffer(ParentClassifier_,0);
					break;
				}

				if(expectHandOverGenIndex==reorderBuffer_.headIndex)
				{
					expectHandOverGenIndex=(expectHandOverGenIndex+1)%MAX_PACKETS_COUNT;
				}
				reorderBuffer_.headIndex=(reorderBuffer_.headIndex + 1) % MAX_PACKETS_COUNT;
				reorderBuffer_.head=(reorderBuffer_.head + 1) % reorderBuffer_.capacity;
			}
			return;
		}



		Tcl& tcl = Tcl::instance();
		tcl.evalf("[Simulator instance] clb-decode-timeout-add");
		// printf("%lf-NODE %d: Clear head gen and hand over all packets!\n",Scheduler::instance().clock(),ParentClassifier_->nodeID());
		PrintReorderBuffer(0);

		
		clearParityBuffer(headReorderBuffer);

		//int lastIndex=(cmnh->headGenIndex+cmnh->blockSize-1)%MAX_PACKETS_COUNT;
		
		// printf("%lf-NODE %d: Clear head reorder gen finished!\n",Scheduler::instance().clock(),ParentClassifier_->nodeID());
		PrintReorderBuffer(0);

		PrintReorderBuffer(0);
		checkReorderBuffer(ParentClassifier_,0);

		// printf("%lf-NODE %d: Decode finished!\n",Scheduler::instance().clock(),ParentClassifier_->nodeID());
		PrintReorderBuffer(0);
	}
}

void CLBTunnelAgent::handOverAndClearBeforeIndex(int genIndex)
{
	int clearPackets=(genIndex-reorderBuffer_.headIndex+MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT;

	if(clearPackets>reorderBuffer_.capacity)
	{
		printf("ERROR: handOverAndClearBeforeHead(%d) clearPackets=%d!\n"
			,genIndex
			,clearPackets);
		PrintReorderBuffer(1);
		exit(1);
	}

	// printf("handOverAndClearBeforeHead(%d)! clearPackets=%d\n"
	// ,bufferID,clearPackets);		
	for(int i=0;i<clearPackets;i++)
	{
		int expectPos = reorderBuffer_.head;
		// printf("expectPos=%d\n",expectPos);
		if(reorderBuffer_.buffer[expectPos]!=NULL)
		{
			Packet* pkt=reorderBuffer_.buffer[expectPos]->copy();
			if((HDR_CMN(pkt))->genIndex==expectHandOverGenIndex)
			{
				PrintPacket(pkt,"Hand to OS");
				///Hand over copy.
				targetRecv(pkt,0);
				packetsUnHandedInReorderBuffer--;
			}
			else
			{
				Packet::free(pkt);
			}
			Packet::free(reorderBuffer_.buffer[expectPos]);
			reorderBuffer_.buffer[expectPos]=NULL;
			reorderBuffer_.count--;				
		}
		expectHandOverGenIndex=(expectHandOverGenIndex+1)%MAX_PACKETS_COUNT;

		reorderBuffer_.headIndex=(reorderBuffer_.headIndex + 1) % MAX_PACKETS_COUNT;
		reorderBuffer_.head=(reorderBuffer_.head + 1) % reorderBuffer_.capacity;
	}	
}

void CLBTunnelAgent::clearParityBuffer(int bufferID)
{
	if(bufferID!=headReorderBuffer)
	{
		printf("ERROR: Should only clearParityBuffer(headReorderBuffer)!\n");
		exit(1);
	}

	if(parityBuffer_[bufferID].ifUpdated!=0)
	{
		for(int i=0;i<BlockSize_MaxP;i++)
		{
			if(parityBuffer_[bufferID].buffer[i]!=NULL)
			{
				Packet::free(parityBuffer_[bufferID].buffer[i]);
				packetsCountInParityBuffer--;
				parityBuffer_[bufferID].buffer[i]=NULL;
			}
		}
	}
	parityBuffer_[bufferID].ifUpdated=0;
	parityBuffer_[bufferID].count=0;
	parityBuffer_[bufferID].blockSize=BlockSize_N-BlockSize_MaxP;
	parityBuffer_[bufferID].genID=(parityBuffer_[bufferID].genID+ReorderGenNumber)%MAX_GENERATION;
	headReorderBuffer=(headReorderBuffer+1)%ReorderGenNumber;

	int formerBuff=(bufferID-1+ReorderGenNumber)%ReorderGenNumber;
	updateParityBuffInfo(formerBuff);
}

void CLBTunnelAgent::updateParityBuffInfo(int baseBuffID)
{
	// if(ParentClassifier_->nodeID()==255)
	// {
	// 	printf("%lf-NODE %d: updateParityBuffInfo(baseID=%d)!\n"
	// 		,Scheduler::instance().clock()
	// 		,ParentClassifier_->nodeID()
	// 		,baseBuffID);
	// }
	
	if(baseBuffID!=headReorderBuffer)////Don't do for head
	{
		for(int i=1;i<ReorderGenNumber;i++)
		{
			int buffID=(baseBuffID-i+ReorderGenNumber)%ReorderGenNumber;

			if(parityBuffer_[buffID].ifUpdated!=0)
			{
				break;
			}
			
			if(parityBuffer_[buffID].ifUpdated==0)
			{
				parityBuffer_[buffID].lastGenOriginalIndex=(parityBuffer_[baseBuffID].lastGenOriginalIndex-i+MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT;
				parityBuffer_[buffID].headIndex
				=(parityBuffer_[baseBuffID].headIndex-i+MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT;

				parityBuffer_[buffID].blockSize
				=(parityBuffer_[buffID].lastGenOriginalIndex-parityBuffer_[buffID].headIndex+MAX_PACKETS_COUNT)%MAX_PACKETS_COUNT;
			}

			if(buffID==headReorderBuffer)
			{
				break;
			}
		}
	}


	if(baseBuffID!=(headReorderBuffer+ReorderGenNumber-1)%ReorderGenNumber)
		////Don't do for tail
	{
		for(int i=1;i<ReorderGenNumber;i++)
		{
			if((i+baseBuffID)%ReorderGenNumber==headReorderBuffer)
			{
				break;
			}

			int formerBuff=(i+baseBuffID-1+ReorderGenNumber)%ReorderGenNumber;
			
			if(parityBuffer_[(i+baseBuffID)%ReorderGenNumber].ifUpdated==0)
			{
				parityBuffer_[(i+baseBuffID)%ReorderGenNumber].headIndex=(parityBuffer_[formerBuff].headIndex+1)%MAX_PACKETS_COUNT;
				parityBuffer_[(i+baseBuffID)%ReorderGenNumber].lastGenOriginalIndex
				=(parityBuffer_[(i+baseBuffID)%ReorderGenNumber].headIndex+BlockSize_MaxK-1)%MAX_PACKETS_COUNT;
			}
			else
			{
				break;
			}
		}
	}
}

void CLBTunnelAgent::PrintVPWindow()
{
	PrintVPWindow(0);
}

void CLBTunnelAgent::PrintVPWindow(int force)
{
	if(!IF_PRINT_DEBUG && force==0)
	{
		return;
	}

	printf("%lf-NODE %d (flow %d): usedVP=%d, countExploredOnceVP=%d\n"
		,Scheduler::instance().clock()
		,ParentClassifier_->nodeID()
		,flowID
		,usedVP
		,countExploredOnceVP);
	// double nowTime=Scheduler::instance().clock();
	for(int i=0;i<VPNumber;i++)
	{
		printf("NODE %d VP[%d]: lastUpdateTime=%3.2e, rate=%3.2e, Sent=%d, Acked=%d, OnFly=%d, Cost=%3.2e, FB_Acked=%d\n"
			,ParentClassifier_->nodeID()
			,i
			,VPLastUpdateTime[i]
			,VPRateEstimation[i]
			,VPPacketsSent[i]
			,VPPacketsAcked[i]
			,VPPacketsOnFly[i]
			,VPCost[i]
			,FeedbackVPAckedPackets[i]);
	}
}

void CLBTunnelAgent::PrintPacket(Packet* pkt, char SendorReceive[128])
{
	// hdr_ip* iph = hdr_ip::access(pkt);
	// // if((iph->src().addr_==14 && iph->dst().addr_==255)
	// // 	&& (iph->src().port_==1 && iph->dst().port_==20))
	// if(ParentClassifier_->nodeID()==671 && flowID==21)
	// {
	// 	PrintPacket(pkt,SendorReceive,1);
	// }
	// else
	// {
	// 	return;
	// }
	PrintPacket(pkt,SendorReceive,0);
}

void CLBTunnelAgent::PrintPacket(Packet* pkt, char SendorReceive[128], int force)
{
	if(!IF_PRINT_DEBUG && force==0)
	{
		return;
	}

	hdr_cmn* cmnh = hdr_cmn::access(pkt);
	hdr_tcp* tcph = hdr_tcp::access(pkt);
	hdr_ip* iph = hdr_ip::access(pkt);
	printf("%lf-NODE %d (flow %d): %s packet(%d.%d-%d.%d) uid=%d, seqno=%d, ackno=%d\n"
		,Scheduler::instance().clock()
		,ParentClassifier_->nodeID()
		,flowID
		,SendorReceive
		,iph->src().addr_,iph->src().port_
		,iph->dst().addr_,iph->dst().port_
		,cmnh->uid_
		,tcph->seqno()
		,tcph->ackno()
		);

	printf("vPath=%d,genIndex=%d,feedBackVP=%d,feedBackVP_ACKedPackets=%d\n"
		,cmnh->vPath
		,cmnh->genIndex
		,cmnh->feedBackVP
		,cmnh->feedBackVP_ACKedPackets
		);
	if(cmnh->ifParity==1)
	{
		printf("++++Parity: genID=%d,ParityIndex=%d,headGenIndex=%d,blockSize=%d,formerCLBPackets="
			,cmnh->genID
			,cmnh->ParityIndex
			,cmnh->headGenIndex
			,cmnh->blockSize);
		if(pkt->formerCLBPackets!=NULL)
		{
			for(int i=0;i<(pkt->formerCLBPacketsNumber);i++)
			{
				hdr_cmn* cmnhFormer = hdr_cmn::access(pkt->formerCLBPackets[i]);
				printf("%d,",cmnhFormer->uid_);
				cmnhFormer=NULL;
			}
			printf("\n");
		}
		else
		{
			printf("ERROR: parity have no formerCLBPackets!\n");
			exit(1);
		}
	}
}

void CLBTunnelAgent::PrintReorderBuffer(int force)
{
	if(!IF_PRINT_DEBUG && force==0)
	{
		return;
	}
	

	printf("%lf-NODE %d (flow %d): ReorderBuffer headReorderBuffer=%d,packetsCountInParityBuffer=%d!\n"
		,Scheduler::instance().clock()
		,ParentClassifier_->nodeID()
		,flowID
		,headReorderBuffer
		,packetsCountInParityBuffer);

	for(int i=0;i<ReorderGenNumber;i++)
	{
		printf("NODE %d: parityBuffer_[%d].genID=%d,count=%d,blockSize=%d,headIndex=%d,lastGenOriginalIndex=%d,ifUpdated=%d\n"
			,ParentClassifier_->nodeID()
			,i
			,parityBuffer_[i].genID
			,parityBuffer_[i].count
			,parityBuffer_[i].blockSize
			,parityBuffer_[i].headIndex
			,parityBuffer_[i].lastGenOriginalIndex
			,parityBuffer_[i].ifUpdated);
		for(int j=0;j<BlockSize_MaxP;j++)
		{
			if(parityBuffer_[i].buffer[j]==NULL)
			{
				printf("NODE %d: parityBuffer_[%d][%d]=NULL\n",ParentClassifier_->nodeID(),i,j);
			}
			else
			{
				printf("NODE %d: parityBuffer_[%d][%d]=%d(%d)\n"
					,ParentClassifier_->nodeID()
					,i,j
					,(HDR_CMN(parityBuffer_[i].buffer[j]))->uid_
					,(HDR_CMN(parityBuffer_[i].buffer[j]))->ParityIndex);
			}
		}
	}

	printf("NODE %d: reorderBuffer_.head=%d,count=%d,headIndex=%d,expectHandOverGenIndex=%d,packetsUnHandedInReorderBuffer=%d\n"
		,ParentClassifier_->nodeID()
		,reorderBuffer_.head
		,reorderBuffer_.count
		,reorderBuffer_.headIndex
		,expectHandOverGenIndex
		,packetsUnHandedInReorderBuffer
		);

	for(int i=0;i<ReorderGenNumber*BlockSize_MaxK;i++)
	{
		if(reorderBuffer_.buffer[i]==NULL)
		{
			printf("NODE %d: ReorderBuffer[%d]=NULL\n",ParentClassifier_->nodeID(),i);
		}
		else
		{
			printf("NODE %d: ReorderBuffer[%d]=%d(%d)\n"
				,ParentClassifier_->nodeID()
				,i
				,(HDR_CMN(reorderBuffer_.buffer[i]))->uid_
				,(HDR_CMN(reorderBuffer_.buffer[i]))->genIndex);
		}
	}
}

void CLBTunnelAgent::PrintEncodeBuffer(int force)
{
	if(!IF_PRINT_DEBUG && force==0)
	{
		return;
	}

	printf("%lf-NODE %d (flow %d): EncodeBuffer head=%d, tail=%d, length=%d, capacity=%d! currentGen=%d.%d\n"
		,Scheduler::instance().clock()
		,ParentClassifier_->nodeID()
		,flowID
		,encodeBuffer_.head
		,encodeBuffer_.tail
		,encodeBuffer_.length
		,encodeBuffer_.capacity
		,currentGenID
		,currentGenIndex);
	for(int i=0;i<BlockSize_N-BlockSize_MaxP;i++)
	{
		if(encodeBuffer_.buffer[i]==NULL)
		{
			printf("NODE %d: EncodeBuffer[%d]=NULL\n",ParentClassifier_->nodeID(),i);
		}
		else
		{
			hdr_cmn* cmnh = hdr_cmn::access(encodeBuffer_.buffer[i]);
			printf("NODE %d: EncodeBuffer[%d]=%d(%d)\n"
				,ParentClassifier_->nodeID()
				,i
				,cmnh->uid_
				,cmnh->genIndex);
		}
	}
}

void CLBTunnelAgent::printSeqTimeline(Packet* pkt)
{
	if(!IF_PRINT_SEQTIMELINE)
	{
		return;
	}
	FILE * fpSeqno;
	char str1[128];
	memset(str1,0,128*sizeof(char));
	hdr_ip* iph = hdr_ip::access(pkt);
	sprintf(str1,"Flow%d.%d-%d.%d_SendSeqno.tr"
		,iph->src().addr_,iph->src().port_
		,iph->dst().addr_,iph->dst().port_);

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

void CLBTunnelAgent::printVPWindowTimeline(Packet* pkt)
{
	// if(ParentClassifier_->nodeID()==35 && flowID==1)
	// {
	// 	printVPWindowTimeline(pkt,1);
	// 	return;
	// }
	printVPWindowTimeline(pkt,0);
}

void CLBTunnelAgent::printVPWindowTimeline(Packet* pkt, int force)
{
	if(!IF_PRINT_VPWINDOWTIMELINE && force==0)
	{
		return;
	}
	FILE * fpVP;
	char str1[128];
	memset(str1,0,128*sizeof(char));
	mkdir("VPWindow",0777);
	sprintf(str1,"VPWindow/Node%d.%d_VPWindow.tr"
		,ParentClassifier_->nodeID()
		,flowID);

	hdr_cmn* cmnh = hdr_cmn::access(pkt);
	if(cmnh->genID==0 && cmnh->genIndex==0)
	{
		fpVP=fopen(str1,"w");
	}
	else
	{
		fpVP=fopen(str1,"a+");
	}
	if(fpVP==NULL)
	{
		fprintf(stderr,"%s, Can't open file %s!\n",strerror(errno),str1);
	}

	fprintf(fpVP,"%lf ",Scheduler::instance().clock());
	for(int i=0;i<VPNumber;i++)
	{
		fprintf(fpVP,"%d ",VPSelectCondition[i]);
	}
	for(int i=0;i<VPNumber;i++)
	{
		fprintf(fpVP,"%3.2e ",VPCost[i]);
	}
	fprintf(fpVP,"%d\n",usedVP);

	fclose(fpVP);
}
