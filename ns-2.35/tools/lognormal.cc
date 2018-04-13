/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) Xerox Corporation 1997. All rights reserved.
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
    "@(#) $Header: /cvsroot/nsnam/ns-2/tools/lognormal.cc,v 1.15 2005/08/26 05:05:30 tomh Exp $ (Xerox)";
#endif

#include "lognormal.h"


/* implement an on/off source with exponentially distributed on and
 * off times.  parameterized by average burst time, average idle time,
 * burst rate and packet size.
 */


static class LOGNORMALTrafficClass : public TclClass {
 public:
	LOGNORMALTrafficClass() : TclClass("Application/Traffic/LogNormal") {}
	TclObject* create(int, const char*const*) {
		return (new LogNormal_Traffic());
	}
} class_lognormal_traffic;

// Added by Debojyoti Dutta October 12th 2000
// This is a new command that allows us to use 
// our own RNG object for random number generation
// when generating application traffic

int LogNormal_Traffic::command(int argc, const char*const* argv)
{
    if (argc == 3) 
	{
		Tcl& tcl = Tcl::instance();
		if (strcmp(argv[1], "attach-app") == 0) 
		{
	        lognormal_sink = (LogNormal_Sink*) TclObject::lookup(argv[2]);
			lognormal_sink->connectSource(this);
            if (lognormal_sink == 0) 
			{
                tcl.resultf("no such app-agent %s", argv[2]);
                return(TCL_ERROR);
            }
			if(lognormal_sink->sourceAPP()!=this)
			{
				tcl.resultf("source app %s is not matched with sink app",argv[0],argv[2]);
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


LogNormal_Traffic::LogNormal_Traffic()
{
	bind_time("burst_time_", &ontime_);
	bind_time("burst_time_std_",&ontime_std_);
	bind_time("idle_time_", &offtime_);
	bind_time("idle_time_std_",&offtime_std_);
	bind_bw("rate_", &rate_);
	bind("packetSize_", &size_);
}

void LogNormal_Traffic::init()
{
        /* compute inter-packet interval during bursts based on
	 * packet size and burst rate.  then compute average number
	 * of packets in a burst.
	 */
	interval_ = (double)(size_ << 3)/(double)rate_;
	burstlen_.setavg(ontime_);
	burstlen_.setstd(ontime_std_);
	Offtime_.setavg(offtime_);
	Offtime_.setstd(offtime_std_);
	//printf("burstlen: avg=%f std=%f\n",*burstlen_.avgp(),*burstlen_.stdp());
	rem_ = 0;
	flowID_=0;
	if (agent_)
		agent_->set_pkttype(PT_TCP);
}

double LogNormal_Traffic::next_interval(int& size)
{
	double t = interval_;

	if (rem_ == 0) {
		/* compute number of packets in next burst */
		rem_ = int(burstlen_.value()/interval_ + .5);
	//	rem_ = 1;/////for debug
		/* make sure we got at least 1 */
		if (rem_ == 0)
		{
			rem_ = 1;
		}
		flowsize_=rem_*size_; //////calculate the flow size (bytes)
		if(flowID_ >=MaxFlowNumber)
		{
			fprintf(stderr,"flowID_%d exceeds MaxFlowNumber!",flowID_);
			exit(1);
		}
		if(lognormal_sink->flowSize[flowID_]==0)
		{
			double now = Scheduler::instance().clock();
			lognormal_sink->flowSize[flowID_]=flowsize_;
			/* start of an idle period, compute idle time */
			t=t+Offtime_.value();
			lognormal_sink->flowStartTime[flowID_]=now+t;
		//	printf("%f---flow%d generated:  %d packets, flowsize is %dbytes\n",now+t,flowID_,rem_,flowsize_);
			flowID_=flowID_+1;
		}
		else
		{
			fprintf(stderr,"flowID_%d error!",flowID_);
		    exit(1);
		}
	}	
	rem_--;

	size = size_;
	return(t);
}

void LogNormal_Traffic::timeout()
{
	if (! running_)
		return;
	// printf("nextPkttime_ is %lf\n",nextPkttime_);
	/* send a packet */
	// The test tcl/ex/test-rcvr.tcl relies on the "NEW_BURST" flag being 
	// set at the start of any exponential burst ("talkspurt").
	double now = Scheduler::instance().clock();
	if (nextPkttime_ != interval_ || nextPkttime_ == -1)
	{
	//	printf("%f---flow%d send a packet with size of %d\n",now,flowID_-1,size_);
		agent_->sendmsg(size_,"NEW_BURST");
	}
	else
	{
	//	printf("%f---flow%d send a packet with size of %d\n",now,flowID_-1,size_);
		agent_->sendmsg(size_);
	}


	/* figure out when to send the next one */
	nextPkttime_ = next_interval(size_);
	/* schedule it */
	if (nextPkttime_ > 0)
		timer_.resched(nextPkttime_);
}

