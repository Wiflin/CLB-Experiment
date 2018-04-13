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
 * @(#) $Header: /cvsroot/nsnam/ns-2/apps/telnet.h,v 1.7 2002/12/22 17:22:39 sfloyd Exp $
 */

#ifndef ns_ftp_sink_h
#define ns_ftp_sink_h

#include <stdlib.h>
#include "app.h"

#include <vector>

struct long_connection_flow_info {
	long_connection_flow_info (int flowID_, double flowStartTime_, int flowSize_):flowStopTime (-1),
	FCT(0), finish_(0), receivedBytes(0)
	{
		flowID = flowID_;
		flowStartTime = flowStartTime_;
		flowSize = flowSize_;
	}
	int flowID;
	double flowStartTime;
	double flowStopTime;
	double FCT;
	int finish_;
	int flowSize;
	int receivedBytes;
};

class FTP_Traffic;

class FTP_Sink : public Application {
 public:
	FTP_Sink();
	int finish_;
	double flowStartTime;
	double flowStopTime;
	double FCT;
	virtual void recv(int nbytes);
	int flowSize;

	
 protected:
	int receivedBytes;
	virtual int command(int argc, const char*const* argv);
	int flowID;
	vector < long_connection_flow_info > flowInfo_; 
	vector < long_connection_flow_info >::iterator current_flow;

	FILE * fpResult;
	int flowNumber;
	double avgFCT;  /// seconds
	double avgThroughput; /// Bps
};


#endif
