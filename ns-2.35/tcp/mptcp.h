/*
 * Copyright (C) 2011 WIDE Project.  All rights reserved.
 *
 * Yoshifumi Nishida  <nishida@sfc.wide.ad.jp>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "tcp-full.h"
#include "mptcp-full.h"
#include "agent.h"
#include "node.h"
#include "packet.h"
#include <vector>

#define MAX_SUBFLOW 20

struct subflow
{
  subflow ():used (false), addr_ (0), port_ (0), daddr_ (-1), dport_ (-1),
    link_ (NULL), target_ (NULL), tcp_ (NULL), dstid_ (0), scwnd_ (0)
  {
  };
  bool used;
  int addr_;
  int port_;
  int daddr_;
  int dport_;
  NsObject *link_;
  NsObject *target_;
  MpFullTcpAgent *tcp_;
  int dstid_;
  double scwnd_;
};

struct dstinfo
{
  dstinfo ():addr_ (-1), port_ (-1), active_ (false)
  {
  };
  int addr_;
  int port_;
  bool active_;
};

struct dack_mapping
{
  int ackno;
  int length;
};

class MptcpAgent:public Agent
{
  friend class XcpEndsys;
  virtual void sendmsg (int nbytes, const char *flags = 0);
public:
    MptcpAgent();
   ~MptcpAgent ()
  {
  };
  void recv (Packet * pkt, Handler *);
  void set_dataack (int ackno, int length);
  int get_dataack ()
  {
    return mackno_;
  }
  double get_alpha ()
  {
    return alpha_;
  }
  double get_totalcwnd ()
  {
    totalcwnd_ = 0;
    for (int i = 0; i < sub_num_; i++) {
       totalcwnd_ += subflows_[i].tcp_->mptcp_get_cwnd ();
    }
    return totalcwnd_;
  }
  int get_totalbytes()
  {
    return total_bytes_;
  }
  int command (int argc, const char *const *argv);
  void calculate_alpha ();
  TracedInt curseq_;
  void send_control ();///CG modify from protected to public
  int MAX_PARITY_NUMBER;////CG add
  int get_ifRMPTCP()
  {
    return ifRMPTCP;
  }
  int get_ifRMPTCPSink()
  {
    return ifRMPTCPSink;
  }
  int ifAllDataHasBeenSent; /////1 means all data has been sent. 0 means not


protected:
  virtual void delay_bind_init_all();
  virtual int delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer);
  int get_subnum ();
  int find_subflow (int addr, int port);
  int find_subflow (int addr);
  void add_destination (int addr, int port);
  bool check_routable (int sid, int addr, int port);

  Classifier *core_;
  int sub_num_;
  int dst_num_;
  int total_bytes_;
  int mcurseq_;
  int mackno_;
  bool infinite_send_;
  int use_olia_;
  double totalcwnd_;
  double alpha_;
  struct subflow subflows_[MAX_SUBFLOW];
  struct dstinfo dsts_[MAX_SUBFLOW];
  vector < dack_mapping > dackmap_;

  //CG add
  int maxSubID;
  int maxDstID;


  int subFlowSendBytes[MAX_SUBFLOW];

  //CG add for rmptcp
  int ifRMPTCP;/////Bool to state whether using RMPTCP
  int ifRMPTCPSink;/////Bool to state whether using RMPTCPSink
  void sendRecoveryPackets();/////CG add
  int findBestSubflow(); ////Return the current best flow. Best flow must have cwnd left. If no such flows, return -1;
  int findWorstSubflow();///Return the current worst flow. Worst flow must have unACKed packets. 
                            ///////////////////////If no such flows, return -1;
  parityInfo setParityPInfo(int subFlowID);
  parityInfo setRetranPInfo(int subFlowID);

  flowCodingInfo fCodingInfo[MAX_SUBFLOW];
};
