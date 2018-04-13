/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1997 Regents of the University of California.
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
 *	This product includes software developed by the Daedalus Research
 *	Group at the University of California Berkeley.
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
 *
 * @(#) $Header: /cvsroot/nsnam/ns-2/apps/ftp_sink.cc,v 1.5 1998/08/14 20:09:33 tomh Exp $
 */

#include "ftp-sink.h"

static class FTP_SinkClass : public TclClass {
 public:
	FTP_SinkClass() : TclClass("Application/FTPSink") {}
	TclObject* create(int, const char*const*) {
		return (new FTP_Sink);
	}
} class_app_ftp_sink;


FTP_Sink::FTP_Sink()
{
	finish_=0;
	receivedBytes=0;
	flowSize=0;
	flowStopTime=-1;
	flowStartTime=-1;
	FCT=0;
	flowID=-1;
	fpResult=NULL;
	flowNumber=0;
	avgFCT=0;
	avgThroughput=0;
	bind("fct_",&FCT);
	bind("flowSize_",&flowSize);
	bind("receivedBytes_",&receivedBytes);
	bind("flowStartTime_",&flowStartTime);
	bind("flowStopTime_",&flowStopTime);
	bind("flow_ID_",&flowID);
	bind("flow_num_",&flowNumber);
	bind("avg_fct_",&avgFCT);
	bind("avg_throughput_",&avgThroughput);
}

int FTP_Sink::command(int argc, const char*const* argv)
{   
	if (argc == 2)
    {  
		Tcl& tcl = Tcl::instance();
		if (strcmp(argv[1], "print-flow-list") == 0) {
			for (vector < long_connection_flow_info >::iterator it = flowInfo_.begin (); it != flowInfo_.end (); ++it) {
				struct long_connection_flow_info *p = &*it;
				printf ("flowID=%d\tflowStartTime=%f\tflowSize=%d\n"
						,p->flowID,p->flowStartTime,p->flowSize);
			}
			return(TCL_OK);
		}
		if (strcmp(argv[1], "get-statistic") == 0) {
			if(flowInfo_.empty())
			{
				avgFCT=FCT;
				avgThroughput=(receivedBytes)/FCT;
			}
			else
			{
				for (vector < long_connection_flow_info >::iterator it = flowInfo_.begin (); it != flowInfo_.end (); ++it) {
					struct long_connection_flow_info *p = &*it;
					avgFCT=avgFCT+p->FCT;
					avgThroughput=avgThroughput+(p->receivedBytes)/p->FCT;
				}
				avgFCT=avgFCT/flowNumber;
				avgThroughput=avgThroughput/flowNumber;
			}

			if(avgFCT<0||avgThroughput<0)
			{
				tcl.resultf("get-statistic error, avgFCT<0||avgThroughput<0");
			    return(TCL_ERROR);
			}
			return(TCL_OK);
		}
	}
    if (argc == 3)
    {  
		Tcl& tcl = Tcl::instance();
		if (strcmp(argv[1], "attach-agent") == 0) {
			agent_ = (Agent*) TclObject::lookup(argv[2]);
			if (agent_ == 0) {
			    tcl.resultf("no such agent %s", argv[2]);
			    return(TCL_ERROR);
			}   
			agent_->attachApp(this);
			return(TCL_OK);
		}
		if (strcmp(argv[1], "flow-size") == 0) {
			flowSize=atoi(argv[2]);
			if(flowSize<=0)
			{
				tcl.resultf("flow size error!");
				return(TCL_ERROR);
			}
			return(TCL_OK);
		}
		if (strcmp(argv[1], "flow-id") == 0) {
		    flowID=atoi(argv[2]);
			if(flowID<0)
			{
                tcl.resultf("flow ID error!");
                return(TCL_ERROR);
            }
            return(TCL_OK);
		}
		if (strcmp(argv[1], "flow-start-time") == 0) {
	        flowStartTime=atof(argv[2]);
            if(flowStartTime<0)
            {
                tcl.resultf("flow start time error!");
                return(TCL_ERROR);
            }
            return(TCL_OK);
        }
        if (strcmp(argv[1], "print-result-file") == 0) {
        	if(fpResult==NULL)
		    	fpResult=fopen(argv[2],"a+");
		    else
		    	fprintf(stderr,"Warning: ftp-sink already set result file %s!\n",argv[2]);

		    if(fpResult==NULL)
		    {
		        fprintf(stderr,"Can't open file %s!\n",argv[2]);
		    	return(TCL_ERROR);
		    }

		    if(flowInfo_.empty())
			{
				fprintf(fpResult,"%d\t%d\t%f\n"
					,flowID
					,flowSize
					,FCT);
			}
			else
			{
				for (vector < long_connection_flow_info >::iterator it = flowInfo_.begin (); it != flowInfo_.end (); ++it) 
				{
					struct long_connection_flow_info *p = &*it;
					fprintf(fpResult,"%d\t%d\t%f\n"
						,p->flowID
						,p->flowSize
						,p->FCT);
				}
			}
			fclose(fpResult);
            return(TCL_OK);
		}
    }
    if(argc==5)
    {
    	Tcl& tcl = Tcl::instance();
    	if (strcmp(argv[1], "set-flow-info") == 0) {
        	///set-flow-info flowID flowStartTime flowSize
	        int tmp_flowID=atoi(argv[2]);
	        double tmp_flowStartTime=atof(argv[3]);
	        int tmp_flowSize=atoi(argv[4]);
            if(tmp_flowStartTime<0||tmp_flowSize==0)
            {
                tcl.resultf("set-flow-info error!");
                return(TCL_ERROR);
            }

            struct long_connection_flow_info tmp_flow_info(tmp_flowID,tmp_flowStartTime,tmp_flowSize);

   //          printf ("\n============== [%lf] add flow-info [conn:%d] (%d,%f,%d)==============\n"
			// ,Scheduler::instance().clock()
			// ,flowID
			// ,tmp_flow_info.flowID
			// ,tmp_flow_info.flowStartTime
			// ,tmp_flow_info.flowSize);

            if(flowInfo_.empty()|| flowInfo_.back().flowStartTime<=tmp_flowStartTime)
			{
				flowInfo_.push_back(tmp_flow_info);
				// for (vector < long_connection_flow_info >::iterator it = flowInfo_.begin (); it != flowInfo_.end (); ++it) {
				// 	struct long_connection_flow_info *p = &*it;
				// 	printf ("flowID=%d\tflowStartTime=%f\tflowSize=%d\n"
				// 			,p->flowID,p->flowStartTime,p->flowSize);
				// }
				flowNumber=flowInfo_.size();
				return(TCL_OK);
			}

			vector < long_connection_flow_info >::iterator it;
			for (it = flowInfo_.begin (); it != flowInfo_.end (); it++) {
				struct long_connection_flow_info *p = &*it;
				if(p->flowStartTime > tmp_flowStartTime)
				{
					flowInfo_.insert(it,tmp_flow_info);
					break;
				}
			}

			// for (vector < long_connection_flow_info >::iterator it = flowInfo_.begin (); it != flowInfo_.end (); ++it) {
			// 	struct long_connection_flow_info *p = &*it;
			// 	printf ("flowID=%d\tflowStartTime=%f\tflowSize=%d\n"
			// 			,p->flowID,p->flowStartTime,p->flowSize);
			// }
			flowNumber=flowInfo_.size();
            return(TCL_OK);
        }
    }
    return Application::command(argc,argv);
}

void FTP_Sink::recv(int nbytes)
{
	if(flowInfo_.empty())
	{
		if(finish_)
		{
			fprintf(stderr,"ERROR: %f---conn[%d](%d.%d-%d.%d) all flows complete, but still receiving\n"
				,Scheduler::instance().clock()
				,agent_->get_flowID()
				,agent_->addr(),agent_->port()
				,agent_->daddr(),agent_->dport());
			exit(1);
		}
		receivedBytes=receivedBytes+nbytes;
		// FILE * fp;
		// char str[30];
		// sprintf(str,"ftpDebug-flow%d.tr",flowID);
		// fp=fopen(str,"at");
		// fprintf(fp,"%f---flow%d received %d bytes: flowsize is %dbytes, start at %f\n",Scheduler::instance().clock(),flowID,receivedBytes,flowSize,flowStartTime);
			// printf("%f---flow%d received %d bytes: flowsize is %d bytes, start at %f\n"
			// 	,Scheduler::instance().clock(),flowID,receivedBytes,flowSize,flowStartTime);
		// fprintf (stderr,"%f---flow%d received %d bytes: flowsize is %d bytes, start at %f\n"
		// 	,Scheduler::instance().clock(),flowID,receivedBytes,flowSize,flowStartTime);
		if(receivedBytes>=flowSize)
		{
			finish_=1;
			receivedBytes=flowSize;
			double now = Scheduler::instance().clock();
			flowStopTime=now;
			FCT=flowStopTime-flowStartTime;
			Tcl& tcl = Tcl::instance();
			tcl.evalf("[Simulator instance] finished-flow-add");
			fprintf(stderr,"%f---flow[%d] conn[%d](%d.%d-%d.%d) complete: flowSize=%d, FCT=%f\n"
				,Scheduler::instance().clock()
				,flowID
				,agent_->get_flowID()
				,agent_->addr(),agent_->port()
				,agent_->daddr(),agent_->dport()
				,flowSize,FCT);
		}
	}	
	else
	{
		if(receivedBytes==0)
			current_flow=flowInfo_.begin ();
		if(current_flow==flowInfo_.end ())
		{
			fprintf(stderr,"ERROR: %f---conn[%d](%d.%d-%d.%d) all flows complete, but still receiving\n"
				,Scheduler::instance().clock()
				,agent_->get_flowID()
				,agent_->addr(),agent_->port()
				,agent_->daddr(),agent_->dport());
			exit(1);
		}

		receivedBytes=receivedBytes+nbytes;

		struct long_connection_flow_info *p = &*current_flow;
		p->receivedBytes=p->receivedBytes+nbytes;
		// FILE * fp;
		// char str[30];
		// sprintf(str,"ftpDebug-flow%d.tr",flowID);
		// fp=fopen(str,"at");
		// fprintf(fp,"%f---flow%d received %d bytes: flowsize is %dbytes, start at %f\n",Scheduler::instance().clock(),flowID,receivedBytes,flowSize,flowStartTime);
			// printf("%f---flow[%d] conn[%d](%d.%d-%d.%d) received %d bytes: flowsize is %d bytes, start at %f\n"
			// 	,Scheduler::instance().clock()
			// 	,p->flowID
			//  	,agent_->get_flowID()
			// 	,agent_->addr(),agent_->port()
			// 	,agent_->daddr(),agent_->dport()
			// 	,p->receivedBytes
			// 	,p->flowSize
			// 	,p->flowStartTime);
		// fprintf (stderr,"%f---flow%d received %d bytes: flowsize is %d bytes, start at %f\n"
		// 	,Scheduler::instance().clock(),flowID,receivedBytes,flowSize,flowStartTime);
		if(p->receivedBytes>=p->flowSize)
		{
			p->finish_=1;
			p->receivedBytes=p->flowSize;
			double now = Scheduler::instance().clock();
			p->flowStopTime=now;
			p->FCT=p->flowStopTime-p->flowStartTime;
			Tcl& tcl = Tcl::instance();
			tcl.evalf("[Simulator instance] finished-flow-add");
			fprintf(stderr,"%f---flow[%d] conn[%d](%d.%d-%d.%d) complete: flowSize=%d, FCT=%f\n"
				,Scheduler::instance().clock()
				,p->flowID
				,agent_->get_flowID()
				,agent_->addr(),agent_->port()
				,agent_->daddr(),agent_->dport()
				,p->flowSize,p->FCT);
			current_flow++;
		}
	}
}
