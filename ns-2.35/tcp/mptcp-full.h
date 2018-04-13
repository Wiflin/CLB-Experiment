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

#ifndef ns_mptcp_full_h
#define ns_mptcp_full_h

#include "tcp.h"
#include "rq.h"
#include "tcp-full.h"

#include "rmptcp.h"///CG add

#include <vector>
class MptcpAgent;
struct dsn_mapping
{
  dsn_mapping (int dseq, int curseq, int len):sentseq (0)
  {
    dseqnum = dseq;
    sseqnum = curseq;
    length = len;
  }
  int sseqnum;
  int dseqnum;
  int length;
  int sentseq;
};

// from RFC6824
const int MPTCP_CAPABLEOPTION_SIZE = 12;
const int MPTCP_JOINOPTION_SIZE = 16;
const int MPTCP_ACKOPTION_SIZE = 20;
const int MPTCP_DATAOPTION_SIZE = 20;

class MpFullTcpAgent;

class ReactiveTimer : public TimerHandler {
public:
  ReactiveTimer(MpFullTcpAgent *a) : TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  MpFullTcpAgent *a_;
};

class MpFullTcpAgent:public SackFullTcpAgent
{
public:
  MpFullTcpAgent ():partiyStartSeq(-1),  ifAllProtected(1), mptcp_core_ (NULL), mptcp_primary_ (false),
    mptcp_allow_slowstart_ (true), mptcp_prev_ackno_ (0), mptcp_prev_sqminseq_ (0),
    mptcp_prev_sqtotal_ (0), mptcp_option_size_(0), mptcp_last_cwnd_(0), 
    ifGAReactive_(0), ga_pto_timer_(this), consecutivePTO(0), 
    ndatapack_from_last_retran(0)

  {
    ifMultiport=0;//CG add
    memset(&dInfo,0,sizeof(dInfo));//CG add
    ecmpHashKey=rand();//CG add
  }

  virtual int command(int argc, const char*const* argv);

  /* multipath TCP */
  double mptcp_get_window ()
  {
    return (mptcp_get_cwnd () < wnd_ ? mptcp_get_cwnd () : (double)wnd_);
  }
  double mptcp_get_cwnd ()
  {
    if (fastrecov_){
        /* use ssthresh value for flows in fast retransmit
           see Section 3 for RFC6356 */
        return (double) ssthresh_;
    }
    return (double) cwnd_;
  }
  double mptcp_get_last_cwnd() 
  {
    return mptcp_last_cwnd_;
  }
  void mptcp_set_last_cwnd(double cwnd_) 
  {
    mptcp_last_cwnd_ = cwnd_;
  }
  int mptcp_get_ssthresh ()
  {
    return (int) ssthresh_;
  }
  int mptcp_get_maxseq ()
  {
    return (int) maxseq_;
  }
  int mptcp_get_highest_ack ()
  {
    return (int) highest_ack_;
  }
  double mptcp_get_srtt ()
  {
    return (double) ((t_srtt_ >> T_SRTT_BITS) * tcp_tick_);
  }
  double mptcp_get_backoff ()
  {
    return (int) t_backoff_;
  }
  int mptcp_get_numdupacks ()
  {
    return (int) dupacks_;
  }
  int mptcp_get_curseq ()
  {
    return (int) curseq_;
  }
  int mptcp_if_in_recover()////CG add
  {
    int in_recovery = ((highest_ack_ < recover_) && last_cwnd_action_!=CWND_ACTION_ECN);
    return in_recovery;
  }
  int mptcp_get_sacked_packet_number(int startSeq)///CG add
  {
    int sackedSeqSum=0;
    int sackedSegNumber=0;
    sq_.nexthole(startSeq,sackedSegNumber,sackedSeqSum);
    if(sackedSeqSum<=0)
    {
      return 0;
    }
    return sackedSeqSum/size();
  }
  int mptcp_get_next_hole_range(int seq, int& start, int& end)///CG add
  {
    return sq_.getNextHoleRange(seq,start,end);
    ///-1 means no hole in sq
    /// 1 means hole
  }
  int mptcp_if_lost_seg(int seq)///CG add
  {
    int result=sq_.ifMissSegment(seq,size());
    if(result==0)
    {
      if(seq>=last_ack_sent_)
      {
        return 1;
      }
      else
      {
        return 0;
      }
    }
    return result;
  }

  int mptcp_get_nrexmitpack () ///CG add to get retransmitted data packets
  {
    return (int) nrexmitpack_;
  }
  int mptcp_get_ndatapack() ///CG add to get total transmitted data packets
  {
    return (int) ndatapack_;
  }
  int mptcp_get_ndatapack_from_last_retran() ///CG add to get total transmitted data packets from last retransmission
  {
    return (int) ndatapack_from_last_retran;
  }

  void mptcp_sq_dumlist()//CG add
  {
    sq_.dumplist();
  }

  void mptcp_set_core (MptcpAgent *);
  void mptcp_set_primary ()
  {
    mptcp_primary_ = true;
  }
  void mptcp_add_mapping (int dseqnum, int length);

  void mptcp_add_rmt_mapping (int ifParity, int ifProtected);//CG add
  void mptcp_set_mapping_rmt_info(int seqno, int ifParity, int ifProtected);//CG add
  void mptcp_remove_rmt_mapping(int seqno);//CG add remove mapping < seqno
  int mptcp_check_rmt_mapping_ifParityorProtected(int seqno);//CG add
  /// 0 no parity or protected, 1 parity, 2 protected 
  void mptcp_update_ifAllProtected();//CG add

  void mptcp_recv_add_mapping (int dseqnum, int sseqnum, int length);
  void mptcp_remove_mapping (int seqnum);
  void mptcp_set_slowstart (bool value)
  {
    mptcp_allow_slowstart_ = value;
  }
  int mptcp_recv_getack (int acknow);
  bool mptcp_is_primary ()
  {
    return mptcp_primary_;
  }
  bool mpcapable_;
  bool mpjoin_;
  bool mpdata_;
  bool mpaddr_;
  bool mpack_;
  virtual void recv(Packet *pkt, Handler*);
  int if_can_send(int);
  int ifMultiport;

  //CG add for RMT begins!
  void parity_sendmsg(int nbytes, parityInfo pInfo); 
  int partiyStartSeq;////from which sequence does parity starts. Set to be -1 if no parity yet
  decodeBufferInfo dInfo;
  void recvParity(Packet *pkt);
  void recvRetrans(Packet *pkt);
  void restorePacket(); ///Restore packet and call TCP:recv

  void pto_timeout()///CG add for GA reactive
  {
    handle_pto();///CG add
  } 
  int ifAllProtected; /////1 means all data has been protected. 0 means not
  //CG add ends

  
protected:
  void opencwnd ();
  void dupack_action ();
  void timeout_action ();
  int headersize ();            // a tcp header w/opts
  virtual void sendpacket (int seq, int ack, int flags, int dlen, int why,
                           Packet * p = 0);
  void prpkt (Packet *);        // print packet (debugging helper)

  //CG add for CLB begins!
  void parity_advance_bytes(int, parityInfo pInfo);
  void parity_send_much(int force, int reason, int maxburst, parityInfo pInfo);
  int parity_foutput(int seqno, int reason, parityInfo pInfo);
  void parity_sendpacket (int seq, int ack, int flags, int dlen, int why, parityInfo pInfo);


  void rmptcp_ps_add_mapping (int seqno, parityInfo pInfo);
  parityInfo rmptcp_ps_lookup_mapping (int seqno);
  vector < parity_seq_mapping > rmptcp_psmap_;///for tcp retrans to carry pInfo

  vector < rmt_info_mapping > rmt_info_map_; ///stating whether or not a seqno is parity/protected 

  //CG add for CLB begins!
  void PrintDecodeBufferInfo(int force);
  void PrintPacket(Packet* pkt, char SendorReceive[128], int force);
  //CG add ends


  /* multipath TCP */
  MptcpAgent *mptcp_core_;
  bool mptcp_primary_;
  bool mptcp_allow_slowstart_;
  int mptcp_prev_ackno_;     // previous highest ack 
  int mptcp_prev_sqminseq_;  // previous minseq in sack block
  int mptcp_prev_sqtotal_;   // previous total bytes in sack blocks
  int mptcp_option_size_;
  int mptcp_last_cwnd_;
  vector < dsn_mapping > mptcp_dsnmap_;
  vector < dsn_mapping > mptcp_recv_dsnmap_; 

  virtual int ifEarlyRetransmit();///CG add


  void cancel_timers();   // cancel all timers
  int ifGAReactive_;
  ReactiveTimer ga_pto_timer_;
  void schedule_pto();
  void handle_pto();
  int consecutivePTO;

  virtual int ifAPPLimited()
  {
    if(ifAllDataSent_)
    {
      return 1;
    }
    return 0;
  }

  int ndatapack_from_last_retran; ///CG add for total transmitted data packets from last retransmission
};



#endif
