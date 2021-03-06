/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */

/*
 * Copyright (C) 1997 by the University of Southern California
 * $Id: classifier-mpath.cc,v 1.10 2005/08/25 18:58:01 johnh Exp $
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * The copyright of this module includes the following
 * linking-with-specific-other-licenses addition:
 *
 * In addition, as a special exception, the copyright holders of
 * this module give you permission to combine (via static or
 * dynamic linking) this module with free software programs or
 * libraries that are released under the GNU LGPL and with code
 * included in the standard release of ns-2 under the Apache 2.0
 * license or under otherwise-compatible licenses with advertising
 * requirements (or modified versions of such code, with unchanged
 * license).  You may copy and distribute such a system following the
 * terms of the GNU GPL for this module and the licenses of the
 * other code concerned, provided that you include the source code of
 * that other code when and as the GNU GPL requires distribution of
 * source code.
 *
 * Note that people who make modified versions of this module
 * are not obligated to grant this special exception for their
 * modified versions; it is their choice whether to do so.  The GNU
 * General Public License gives permission to release a modified
 * version without this exception; this exception also makes it
 * possible to release a modified version which carries forward this
 * exception.
 *
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/classifier/classifier-mpath.cc,v 1.10 2005/08/25 18:58:01 johnh Exp $ (USC/ISI)";
#endif

#include "ip.h"
#include <ctime>
#include <math.h>
#include <time.h>  
#include "crc16.h"  ////CG add
#include "classifier.h"
#include "clb/conga.h" ////WF add
#include "classifier-mpath.h"
#include "node.h"


static class MultiPathClass : public TclClass {
public:
	MultiPathClass() : TclClass("Classifier/MultiPath") {} 
	TclObject* create(int, const char*const*) {
		return (new MultiPathForwarder());
	}
} class_multipath;



MultiPathForwarder::MultiPathForwarder() : Classifier(), ns_(0) {

	sprintf(instance_name,"Mpath-Classifier");

	bind("fLayer_", &fLayer_);
	bind("sLeaf_Conga_", &sLeaf_Conga_);
	bind("nodeID_", &nodeID_);
	bind("randSalt_", &randSalt_);///CG add
	bind("loadBalancePerPacket_", &loadBalancePerPacket_); //WF add
	bind("loadBalanceFlowlet_", &loadBalanceFlowlet_); //WF add
	// loadBalanceFlowlet_ = 1;
	srand((unsigned)time(NULL));
	r = 0;
	
} 

int MultiPathForwarder::classify(Packet* p) {
//CG add!

	// clock_t begin_time = clock();
	// int method = 0;


	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);
	int key;
	if(cmnh->ifTunnel() == 1)//CG add!
	{
		///For CLB
		unsigned char F_Tuple[24];
		memset(F_Tuple,0,24*sizeof(unsigned char));
		memcpy(F_Tuple,&(iph->src().addr_),sizeof(int32_t));
		memcpy(F_Tuple+sizeof(int32_t),&(iph->dst().addr_),sizeof(int32_t));
		memcpy(F_Tuple+2*sizeof(int32_t),&(cmnh->vPath),sizeof(int32_t));   /////Modify source port
		memcpy(F_Tuple+3*sizeof(int32_t),&(iph->dst().port_),sizeof(int32_t));
		memcpy(F_Tuple+4*sizeof(int32_t),&(randSalt_),sizeof(randSalt_));
		memcpy(F_Tuple+5*sizeof(int32_t),&(cmnh->ecmpHashKey),sizeof(int32_t));

		int crc=crc16.gen_crc16((uint8_t *)F_Tuple,24);
		/*printf("CRC: 0x%04X\n",crc);*/

		key=(int) crc/((0x10000)/(maxslot_+1));
	}
	else if(sLeaf_Conga_==1 && cmnh->ifCONGA()==1) ////For CONGA
	{
		key=cmnh->LBTag;
	}
	else if(loadBalancePerPacket_ == 1)
	{
		// key = rand();
		key = mpath_route(p);
		// if(conga_enabled() == 1)
		// {
		// 	conga_()->packetPrint(p, this, "All-Packet-Debug.tr","mpath-classifier");
		// }
	}
	else if(loadBalanceFlowlet_ == 1)
	{
		key = balance_flowlet(p);
	}
	else /////For ECMP
	{
		key = balance_ecmp(p);
	}
	int cl=0;
	cl=key%(maxslot_+1);
	/*printf("%lf-Node-%d: maxslot=%d, key=%d, cl=%d, sLeaf_Conga_=%d. randSalt_=%u\n"
		,Scheduler::instance().clock(),nodeID_,maxslot_,key,cl,sLeaf_Conga_,randSalt_);*/
	

	// conga_get_instance();
	// #define All-Packet-Debug
	// #ifdef All-Packet-Debug
	// FILE* fpResult=fopen("All-Packet-Debug.tr","a+");
	// if(fpResult==NULL)
 //    {
 //        fprintf(stderr,"Can't open file %s!\n","debug.tr");
 //    	// return(TCL_ERROR);
 //    } else {
	// 	fprintf(fpResult, "%d %lf-Node-%d-(%d->%d):flowid=%d size=%d key=%d randSalt=%u ecmpHashKey=%u maxslot_=%d cl=%d flowlet=%u\n"
	// 	,conga_enabled(),Scheduler::instance().clock(),nodeID_,iph->src_,iph->dst_,cmnh->flowID,cmnh->size_,key,randSalt_,cmnh->ecmpHashKey,maxslot_,cl,loadBalanceFlowlet_);
	// 	fclose(fpResult);
	// }
	// if(conga_enabled() == 1)
	// {
	// 	conga_()->packetPrint(p, this, "All-Packet-Debug.tr");
	// }

	// #endif

	if(slot_[cl] == 0)
	{
		fprintf(stderr,"slot number error in MultiPathForwarder\n");
		exit(1);
	}
	if(fLayer_==1)
	{
		printf("Node:%d  fLayer_ successfully binded!\n",nodeID_);
	}
	///CG add ends!

	// char mode[100];
	// if(hdr_ip::access(p)->src().addr_==this->nodeID())
	// {
	// 	sprintf(mode,"send");
	// }
	// else if(hdr_ip::access(p)->dst().addr_==this->nodeID())
	// {
	// 	sprintf(mode,"recv");
	// }
	// else
	// {
	// 	sprintf(mode,"pass");
	// }

	// char str2[128];
	// sprintf(str2, "time-record-%d-%p", nodeID_,this);
	// FILE* time_rf = fopen(str2, "a+");
 //  	double elapsed_secs = double(clock() - begin_time) / CLOCKS_PER_SEC;
	// fprintf(time_rf, "[mpath-classifier::recv elapse][%s]  %lf %lf method=%d (%d.%d-%d.%d)\n", mode, Scheduler::instance().clock(), 
	// 	elapsed_secs, method,
	// 	hdr_ip::access(p)->src().addr_,hdr_ip::access(p)->src().port_,
	// 	hdr_ip::access(p)->dst().addr_,hdr_ip::access(p)->dst().port_);
	// fclose(time_rf);	
	


	/*///original		
		int fail = ns_;
		do {
			cl = ns_++;
			ns_ %= (maxslot_ + 1);
		} while (slot_[cl] == 0 && ns_ != fail);
	*///
	return cl;
}



int MultiPathForwarder::mpath_route(Packet* p)
{
	// fprintf(stderr,"[mpath-classifier] %lf-Node-%d conga=%d\n",Scheduler::instance().clock(),n_->nodeid(),conga_enabled());


	// r = (++r) % (maxslot_+1);
	// return r;


	if(conga_enabled() == 1)
		return conga_()->route(p, this);
	else {

		int r = rand();

		// char str[100];
		// sprintf(str,"Mpath-Rand-%d.tr",nodeID_);
		// FILE* fpRand = fopen(str,"a+");

		// fprintf(fpRand, "%d %d\n", r, r%(maxslot_+1));

		// fclose(fpRand);

		return r;
	}
}

//CG add
int MultiPathForwarder::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	if(argc == 2) {
        if (strcmp(argv[1], "show-route") == 0) {
        	printf("%lf-Node-%d:\tmaxslot_=%d,\n"
				,Scheduler::instance().clock()
				,nodeID_
				,maxslot_
				);	
            for (int i=0; i <= maxslot_; i++)
            {
            	if (slot_[i]==0)
            		printf("\tslot[%d]=NULL\n",i);
            	else
            		printf("\tslot[%d]=%s\n",i,slot_[i]->name());
            }
            fflush(stdout);
            return (TCL_OK);
        }
	}
	return (Classifier::command(argc, argv));
}






int MultiPathForwarder::balance_flowlet(Packet* p)
{
	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);
	
	int key;
	
	unsigned char F_Tuple[24];
	memset(F_Tuple,0,24*sizeof(unsigned char));
	memcpy(F_Tuple,&(iph->src().addr_),sizeof(int32_t));
	memcpy(F_Tuple+sizeof(int32_t),&(iph->dst().addr_),sizeof(int32_t));
	memcpy(F_Tuple+2*sizeof(int32_t),&(iph->src().port_),sizeof(int32_t));
	memcpy(F_Tuple+3*sizeof(int32_t),&(iph->dst().port_),sizeof(int32_t));
	memcpy(F_Tuple+4*sizeof(int32_t),&(randSalt_),sizeof(int32_t));
	memcpy(F_Tuple+5*sizeof(int32_t),&(cmnh->ecmpHashKey),sizeof(int32_t));

	uint16_t regionStart=randSalt_%(0x10000);

	// calculate the crc hash key to identify a flow
	uint16_t crc=crc16.gen_crc16((uint8_t *)F_Tuple,24);


	vector < FlowletRecord >::iterator it;
	for (it = FlowletTable.begin(); it != FlowletTable.end(); it ++)
	{
		if(it->flowHashKey == crc)
		{
			
			// if packet interval is little than the flowlet avg interval,
			//	just use the old routel; otherwise choose a new route for it
			double clk = Scheduler::instance().clock();
			double timestamp = it->timeStamp;
			double devTime = fabs(clk - timestamp);
			if(devTime <= 1E-4)
			{
				it->timeStamp = clk;
				key = it->flowChosenKey;
			}
			else 
			{
				key = mpath_route(p);
				it->flowChosenKey = key;
				it->timeStamp = clk;
			}
			break;
		}
	}
	// if the flow has not get in this node, choose a route for it
	if(it == FlowletTable.end())
	{
		key = mpath_route(p);
		struct FlowletRecord flow = {
			.flowHashKey = crc, 
			.flowChosenKey = key, 
			.timeStamp = Scheduler::instance().clock()
		};
		FlowletTable.push_back(flow);
		// FILE* fpResult=fopen("debug.tr","a+");
	}
	return key;
}

int MultiPathForwarder::balance_ecmp(Packet* p)
{

	


	hdr_ip* iph = hdr_ip::access(p);
	hdr_cmn* cmnh = hdr_cmn::access(p);
	int key;
	// unsigned char F_Tuple[24];
	// memset(F_Tuple,0,24*sizeof(unsigned char));
	// memcpy(F_Tuple,&(iph->src().addr_),sizeof(int32_t));
	// memcpy(F_Tuple+sizeof(int32_t),&(iph->dst().addr_),sizeof(int32_t));
	// memcpy(F_Tuple+2*sizeof(int32_t),&(iph->src().port_),sizeof(int32_t));
	// memcpy(F_Tuple+3*sizeof(int32_t),&(iph->dst().port_),sizeof(int32_t));
	// memcpy(F_Tuple+4*sizeof(int32_t),&(randSalt_),sizeof(int32_t));
	// memcpy(F_Tuple+5*sizeof(int32_t),&(cmnh->ecmpHashKey),sizeof(int32_t));

	// // unsigned char F_Tuple[8];
	// // memset(F_Tuple,0,8*sizeof(unsigned char));
	// // memcpy(F_Tuple,&(randSalt_),sizeof(randSalt_));
	// // memcpy(F_Tuple+sizeof(int32_t),&(cmnh->ecmpHashKey),sizeof(int32_t));

	// // unsigned char F_Tuple[8];
	// // memset(F_Tuple,0,8*sizeof(unsigned char));
	// // memcpy(F_Tuple,&(cmnh->ecmpHashKey),sizeof(int32_t));
	// // memcpy(F_Tuple+sizeof(int32_t),&(randSalt_),sizeof(int32_t));

	// uint16_t regionStart=randSalt_%(0x10000);

	// uint16_t crc=crc16.gen_crc16((uint8_t *)F_Tuple,24);

	// uint16_t regionOffSet=(crc-regionStart+(0x10000))%0x10000;

	// key=(int) regionOffSet/((0x10000)/(maxslot_+1));


	// ////Hack for debug mode
	// if(cmnh->ecmpHashKey==-1)
	// {
	// 	key=0;
	// }
	// else if(cmnh->ecmpHashKey==-2)
	// {
	// 	key=1;
	// }


	//key=(int) cmnh->ecmpHashKey/((0x10000)/(maxslot_+1));
	
	key=(unsigned) cmnh->ecmpHashKey%(maxslot_+1);

	// fprintf(stderr, "im in! %u %d\n",cmnh->ecmpHashKey,key);

	// if(maxslot_>=1)
	// {
	// 	// printf("====srcIP|dstIP|srcPort|dstPort|randSalt|ecmpHashKey(DEC)====\n");
	// 	// printf("%d|%d|%d|%d|%u|%u\n",iph->src().addr_,iph->dst().addr_,iph->src().port_,iph->dst().port_,randSalt_,cmnh->ecmpHashKey);

	// 	// printf("====randSalt|ecmpHashKey(HEX)====\n");
	// 	// for(int i=0;i<8;i++)
	// 	// {
	// 	// 	if(i!=0&&i%4==0)
	// 	// 	{
	// 	// 		printf("|");
	// 	// 	}
	// 	// 	printf("%02X",F_Tuple[i]);
	// 	// }
	// 	// printf("\n");

	// }
	

	/*int offSet=0;
	if(maxslot_>0)
	{
		offSet=floor(log(maxslot_+1)/log(4));
		// printf("maxslot_=%d, offSet=%d\n"
		// 	,maxslot_,offSet);
	}*/
	
	/*key=((iph->src().addr_ << offSet)
		^iph->dst().addr_
		^(iph->src().port_<< offSet)
		^iph->dst().port_)+randSalt_;*/

	return key;
}