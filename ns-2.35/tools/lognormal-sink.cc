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
 * @(#) $Header: /cvsroot/nsnam/ns-2/apps/lognormal_sink.cc,v 1.5 1998/08/14 20:09:33 tomh Exp $
 */

#include "lognormal-sink.h"

static class LogNormal_SinkClass : public TclClass {
 public:
	LogNormal_SinkClass() : TclClass("Application/LogNormalSink") {}
	TclObject* create(int, const char*const*) {
		return (new LogNormal_Sink);
	}
} class_app_lognormal_sink;


LogNormal_Sink::LogNormal_Sink() 
{
	finish_=1;
	receivedBytes=0;
	flowID_=0;
	memset(flowSize,0,MaxFlowNumber*sizeof(flowSize[0]));
	memset(flowStopTime,0,MaxFlowNumber*sizeof(flowStopTime[0]));
	memset(flowStartTime,0,MaxFlowNumber*sizeof(flowStartTime[0]));
	memset(FCT,0,MaxFlowNumber*sizeof(FCT[0]));
}

int LogNormal_Sink::command(int argc, const char*const* argv)
{   
    if (argc == 3)
    {  
		Tcl& tcl = Tcl::instance();
        if (strcmp(argv[1], "attach-app") == 0)
        {   
            lognormal_source = (LogNormal_Traffic*) TclObject::lookup(argv[2]);
			lognormal_source->connectDst(this);
            if (lognormal_source == 0)
            {   
                tcl.resultf("no such app-agent %s", argv[2]);
                return(TCL_ERROR);
            }
			if(lognormal_source->dstAPP()!=this)
            {
                tcl.resultf("sink app %s is not matched with source app",argv[0],argv[2]);
                return(TCL_ERROR);
            }
            return(TCL_OK);
         }
		if (strcmp(argv[1], "attach-agent") == 0) {
			agent_ = (Agent*) TclObject::lookup(argv[2]);
			if (agent_ == 0) {
			    tcl.resultf("no such agent %s", argv[2]);
			    return(TCL_ERROR);
			}   
			agent_->attachApp(this);
			return(TCL_OK);
		}
    }
    return Application::command(argc,argv);
}

void LogNormal_Sink::recv(int nbytes)
{
	receivedBytes=receivedBytes+nbytes;
//	printf("%f---flow%d received %d bytes: flowsize is %dbytes, start at %f\n",Scheduler::instance().clock(),flowID_,receivedBytes,flowSize[flowID_],flowStartTime[flowID_]);
	if(receivedBytes==flowSize[flowID_])
	{
		double now = Scheduler::instance().clock();
		flowStopTime[flowID_]=now;
		FCT[flowID_]=flowStopTime[flowID_]-flowStartTime[flowID_];
		receivedBytes=0;
		printf("%f---flow%d complete: FCT is %f\n",Scheduler::instance().clock(),flowID_,FCT[flowID_]);
		flowID_++;
	}
	if(flowID_ >=MaxFlowNumber)
    {
        fprintf(stderr,"flowID_%d error!",flowID_);
        exit(1);
    }
}
