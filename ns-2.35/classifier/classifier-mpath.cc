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

#include "classifier.h"
#include "ip.h"
#include <math.h>
#include <time.h>  

#include "crc16.h"  ////CG add

class MultiPathForwarder : public Classifier {
public:
	MultiPathForwarder() : ns_(0) {
		bind("fLayer_", &fLayer_);
		bind("sLeaf_Conga_", &sLeaf_Conga_);
		bind("nodeID_", &nodeID_);
		bind("randSalt_", &randSalt_);///CG add
		bind("loadBalancePerPacket_", &loadBalancePerPacket_); //WF add
	} 
	virtual int classify(Packet* p) {
//CG add!
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
			srand((unsigned)time(NULL));  
			key=rand();
			FILE* fpResult=fopen("debug.tr","a+");
			if(fpResult==NULL)
		    {
		        fprintf(stderr,"Can't open file %s!\n","debug.tr");
		    	// return(TCL_ERROR);
		    } else {
				fprintf(fpResult, "%lf-Node-%d: key=%d maxslot_=%d \n"
					,Scheduler::instance().clock()
					,nodeID_
					,key
					,maxslot_
					);
				fclose(fpResult);
			}
		}
		else /////For ECMP
		{
			unsigned char F_Tuple[24];
			memset(F_Tuple,0,24*sizeof(unsigned char));
			memcpy(F_Tuple,&(iph->src().addr_),sizeof(int32_t));
			memcpy(F_Tuple+sizeof(int32_t),&(iph->dst().addr_),sizeof(int32_t));
			memcpy(F_Tuple+2*sizeof(int32_t),&(iph->src().port_),sizeof(int32_t));
			memcpy(F_Tuple+3*sizeof(int32_t),&(iph->dst().port_),sizeof(int32_t));
			memcpy(F_Tuple+4*sizeof(int32_t),&(randSalt_),sizeof(int32_t));
			memcpy(F_Tuple+5*sizeof(int32_t),&(cmnh->ecmpHashKey),sizeof(int32_t));

			// unsigned char F_Tuple[8];
			// memset(F_Tuple,0,8*sizeof(unsigned char));
			// memcpy(F_Tuple,&(randSalt_),sizeof(randSalt_));
			// memcpy(F_Tuple+sizeof(int32_t),&(cmnh->ecmpHashKey),sizeof(int32_t));

			// unsigned char F_Tuple[8];
			// memset(F_Tuple,0,8*sizeof(unsigned char));
			// memcpy(F_Tuple,&(cmnh->ecmpHashKey),sizeof(int32_t));
			// memcpy(F_Tuple+sizeof(int32_t),&(randSalt_),sizeof(int32_t));

			uint16_t regionStart=randSalt_%(0x10000);

			uint16_t crc=crc16.gen_crc16((uint8_t *)F_Tuple,24);

			uint16_t regionOffSet=(crc-regionStart+(0x10000))%0x10000;

			key=(int) regionOffSet/((0x10000)/(maxslot_+1));


			////Hack for debug mode
			if(cmnh->ecmpHashKey==-1)
			{
				key=0;
			}
			else if(cmnh->ecmpHashKey==-2)
			{
				key=1;
			}

			key=(int) cmnh->ecmpHashKey/((0x10000)/(maxslot_+1));
			

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

			FILE* fpResult=fopen("debug.tr","a+");
			if(fpResult==NULL)
		    {
		        fprintf(stderr,"Can't open file %s!\n","debug.tr");
		    	// return(TCL_ERROR);
		    } else {
				fprintf(fpResult, "%lf-Node-%d: key=%d randSalt=%u ecmpHashKey=%u maxslot_=%d crc=%u regionStart=%u regionOffSet=%u\n"
				,Scheduler::instance().clock()
				,nodeID_
				,key
				,randSalt_
				,cmnh->ecmpHashKey
				,maxslot_
				,crc
				,regionStart
				,regionOffSet
				);
				fclose(fpResult);
			}
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
		}
		int cl=0;
		cl=key%(maxslot_+1);
		/*printf("%lf-Node-%d: maxslot=%d, key=%d, cl=%d, sLeaf_Conga_=%d. randSalt_=%u\n"
			,Scheduler::instance().clock(),nodeID_,maxslot_,key,cl,sLeaf_Conga_,randSalt_);*/
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
		
/*///original		
		int fail = ns_;
		do {
			cl = ns_++;
			ns_ %= (maxslot_ + 1);
		} while (slot_[cl] == 0 && ns_ != fail);
*///
		return cl;
	}
private:
	int ns_;
	CRC_Generator crc16;  ///CG add
protected:
	virtual int command(int argc, const char*const* argv);//CG add
};

static class MultiPathClass : public TclClass {
public:
	MultiPathClass() : TclClass("Classifier/MultiPath") {} 
	TclObject* create(int, const char*const*) {
		return (new MultiPathForwarder());
	}
} class_multipath;


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