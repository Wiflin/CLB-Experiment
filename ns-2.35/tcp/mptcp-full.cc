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

#include "ip.h"
#include "tcp-full.h"
#include "mptcp-full.h"
#include "flags.h"
#include "random.h"
#include "template.h"
#include "mptcp.h"

#include "rmptcp-full.cc" ///CG add

#ifndef TRUE
#define	TRUE 	1
#endif

#ifndef FALSE
#define	FALSE 	0
#endif

static class MpFullTcpClass:public TclClass
{
public:
  MpFullTcpClass ():TclClass ("Agent/TCP/FullTcp/Sack/Multipath")
  {
  }
  TclObject *create (int, const char *const *)
  {
    return (new MpFullTcpAgent ());
  }
} class_mpfull;

int
MpFullTcpAgent::command(int argc, const char*const* argv)
{
  if (argc == 2) {
    if (strcmp(argv[1], "reactive") == 0) {
      ifGAReactive_=1;
      return (TCL_OK);
    }
  }
  return (SackFullTcpAgent::command(argc, argv));
}

void
MpFullTcpAgent::prpkt (Packet * pkt)
{
  hdr_tcp *tcph = hdr_tcp::access (pkt);        // TCP header
  hdr_cmn *th = hdr_cmn::access (pkt);  // common header (size, etc)
  //hdr_flags *fh = hdr_flags::access(pkt);       // flags (CWR, CE, bits)
  hdr_ip *iph = hdr_ip::access (pkt);
  int datalen = th->size () - tcph->hlen ();    // # payload bytes

  fprintf (stdout,
           " [%d:%d.%d>%d.%d] (hlen:%d, dlen:%d, seq:%d, ack:%d, flags:0x%x (%s), salen:%d, reason:0x%x)\n",
           th->uid (), iph->saddr (), iph->sport (), iph->daddr (),
           iph->dport (), tcph->hlen (), datalen, tcph->seqno (),
           tcph->ackno (), tcph->flags (), flagstr (tcph->flags ()),
           tcph->sa_length (), tcph->reason ());
}

/*
 * this function should be virtual so others (e.g. SACK) can override
 */
int
MpFullTcpAgent::headersize ()
{
  int total = tcpip_base_hdr_size_;
  if (total < 1) {
    fprintf (stderr,
             "%f: MpFullTcpAgent(%s): warning: tcpip hdr size is only %d bytes\n",
             now (), name (), tcpip_base_hdr_size_);
  }

  if (ts_option_)
    total += ts_option_size_;

  total += mptcp_option_size_;

  return (total);
}

/*
 * sendpacket: 
 *	allocate a packet, fill in header fields, and send
 *	also keeps stats on # of data pkts, acks, re-xmits, etc
 *
 * fill in packet fields.  Agent::allocpkt() fills
 * in most of the network layer fields for us.
 * So fill in tcp hdr and adjust the packet size.
 *
 * Also, set the size of the tcp header.
 */
void
MpFullTcpAgent::sendpacket (int seqno, int ackno, int pflags, int datalen,
                            int reason, Packet * p)
{
  if (!p)
    p = allocpkt ();
  hdr_tcp *tcph = hdr_tcp::access (p);
  hdr_flags *fh = hdr_flags::access (p);

  /* build basic header w/options */


  tcph->seqno () = seqno;
  tcph->ackno () = ackno;
  tcph->flags () = pflags;
  tcph->reason () |= reason;    // make tcph->reason look like ns1 pkt->flags?
  tcph->sa_length () = 0;       // may be increased by build_options()
  tcph->hlen () = tcpip_base_hdr_size_;
  tcph->hlen () += build_options (tcph);


  // hdr_ip *iph = hdr_ip::access (p);
  // if((iph->src().addr_==61 && iph->dst().addr_==1)
  //   || (iph->src().addr_==1 && iph->dst().addr_==61))
  // {
  //   printf("%lf-Node%d: sendpacket(%d.%d-%d.%d), SentSeq=%d, AckSeq=%d\n"
  //      ,Scheduler::instance().clock()
  //      ,here_.addr_
  //      ,iph->src().addr_,iph->src().port_
  //      ,iph->dst().addr_,iph->dst().port_
  //      ,tcph->seqno(),tcph->ackno());
  // }

  /*
   * Explicit Congestion Notification (ECN) related:
   * Bits in header:
   *      ECT (EC Capable Transport),
   *      ECNECHO (ECHO of ECN Notification generated at router),
   *      CWR (Congestion Window Reduced from RFC 2481)
   * States in TCP:
   *      ecn_: I am supposed to do ECN if my peer does
   *      ect_: I am doing ECN (ecn_ should be T and peer does ECN)
   */

  if (datalen > 0 && ecn_) {
    // set ect on data packets 
    fh->ect () = ect_;          // on after mutual agreement on ECT
  }
  else if (ecn_ && ecn_syn_ && ecn_syn_next_ && (pflags & TH_SYN)
           && (pflags & TH_ACK)) {
    // set ect on syn/ack packet, if syn packet was negotiating ECT
    fh->ect () = ect_;
  }
  else {
    /* Set ect() to 0.  -M. Weigle 1/19/05 */
    fh->ect () = 0;
  }
  if (ecn_ && ect_ && recent_ce_) {
    // This is needed here for the ACK in a SYN, SYN/ACK, ACK
    // sequence.
    pflags |= TH_ECE;
  }
  // fill in CWR and ECE bits which don't actually sit in
  // the tcp_flags but in hdr_flags
  if (pflags & TH_ECE) {
    fh->ecnecho () = 1;
  }
  else {
    fh->ecnecho () = 0;
  }
  if (pflags & TH_CWR) {
    fh->cong_action () = 1;
  }
  else {
    /* Set cong_action() to 0  -M. Weigle 1/19/05 */
    fh->cong_action () = 0;
  }

  /*
   * mptcp processing
   */
  mptcp_option_size_ = 0;
  if (pflags & TH_SYN) {
    if (!(pflags & TH_ACK)) {
      if (mptcp_is_primary ()) {
        tcph->mp_capable () = 1;
        mptcp_option_size_ += MPTCP_CAPABLEOPTION_SIZE;
      }
      else {
        tcph->mp_join () = 1;
        mptcp_option_size_ += MPTCP_JOINOPTION_SIZE;
      }
    }
    else {
      if (mpcapable_) {
        tcph->mp_capable () = 1;
        mptcp_option_size_ += MPTCP_CAPABLEOPTION_SIZE;
      }
      if (mpjoin_) {
        tcph->mp_join () = 1;
        mptcp_option_size_ += MPTCP_JOINOPTION_SIZE;
      }
    }
  }
  else {
    // tcph->mp_ack () = mptcp_recv_getack (ackno);
    // if (tcph->mp_ack ())
    //   mptcp_option_size_ += MPTCP_ACKOPTION_SIZE;
  }

  if (datalen && !mptcp_dsnmap_.empty ()) {
    vector < dsn_mapping >::iterator it;

///CG add
    // hdr_ip *iph = hdr_ip::access (p);///CG add
    //  if(iph->src().addr_==5 && iph->src().port_==1)
    //    //   || (iph->src().addr_==1 && iph->dst().addr_==61))
    //     {
    //       printf("%lf-Node%d: send Packet(%d.%d-%d.%d), SentSeq=%d, AckSeq=%d\n"
    //          ,Scheduler::instance().clock()
    //          ,here_.addr_
    //          ,iph->src().addr_,iph->src().port_
    //          ,iph->dst().addr_,iph->dst().port_
    //          ,tcph->seqno(),tcph->ackno());

    //       printf ("==============mptcp_dsnmap_==============\n");
    //       for (it = mptcp_dsnmap_.begin (); it != mptcp_dsnmap_.end (); ++it) {
    //         struct dsn_mapping *p = &*it;

    //         printf ("sentseq=%d\tsseqnum=%d\tdseqnum=%d\tlength=%d\n"
    //           , p->sentseq
    //           , p->sseqnum
    //           ,p->dseqnum
    //           ,p->length);
    //       }
    //     }
///CG add

    for (it = mptcp_dsnmap_.begin (); it != mptcp_dsnmap_.end (); ++it) {
      struct dsn_mapping *p = &*it;

      /* 
         check if this is the mapping for this packet 
         if so, attach the mapping info to the packet 
       */


      if (seqno >= p->sseqnum && seqno < p->sseqnum + p->length) {

      
        
        /* 
           if this is first transssion for this mapping or
           if this is retransmited packet, attach mapping 
         */
        if (!p->sentseq || p->sentseq == seqno) {
          tcph->mp_dsn () = p->dseqnum;
          tcph->mp_subseq () = p->sseqnum;
          tcph->mp_dsnlen () = p->length;
          p->sentseq = seqno;
          mptcp_option_size_ += MPTCP_DATAOPTION_SIZE;
          break;
        }
      }
    }
  }
  tcph->hlen () += mptcp_option_size_;

  /* actual size is data length plus header length */

  hdr_cmn *ch = hdr_cmn::access (p);
  ch->size () = datalen + tcph->hlen ();

  if (datalen <= 0)
    ++nackpack_;
  else {
    ++ndatapack_;
    ndatapack_from_last_retran++;
    ndatabytes_ += datalen;
    last_send_time_ = now ();   // time of last data
  }
  if (reason == REASON_TIMEOUT || reason == REASON_DUPACK
      || reason == REASON_SACK) {
    ++nrexmitpack_;
    ndatapack_from_last_retran=1; //CG add
    nrexmitbytes_ += datalen;
  }

  last_ack_sent_ = ackno;

//if (state_ != TCPS_ESTABLISHED) {
//printf("MP %f(%s)[state:%s]: sending pkt ", now(), name(), statestr(state_));
//prpkt(p);
//}
  ////CG add begins
  if(reason!=REASON_REACTIVE_PTO && reason!=REASON_PROACTIVE_REP)
    pipe_ += datalen;
  
  PrintPacket(p,"Send",0);

  //hdr_ip *iph = hdr_ip::access (p);

  if(tcph->seqno()<maxseq_)
  {
    retransTimes++;
    // if(flowID==5335)
    //   printf("%lf-RETRANS: Packet(%d.%d-%d.%d), RetransTimes=%d, timeOutTimes=%d, SentSeq=%d, AckSeq=%d, MaxSeq=%d\n"
    //  ,Scheduler::instance().clock()
    //  ,iph->src().addr_,iph->src().port_
    //  ,iph->dst().addr_,iph->dst().port_
    //  ,retransTimes,timeOutTimes
    //  ,tcph->seqno(),tcph->ackno()
    //  ,(int)maxseq_);

    if(mptcp_core_->get_ifRMPTCP() && mptcp_check_rmt_mapping_ifParityorProtected(tcph->seqno())==1)
    {
      parityInfo pInfo=rmptcp_ps_lookup_mapping(tcph->seqno());
      (HDR_CMN(p))->pInfo=pInfo;
      parityRetransTimes++;
    }
  }

  if(restore_by_other_==0)///CG add
  {
    if(ifGAReactive_==1)
    {
      last_sent_seq=seqno;//CG add
      last_sent_data_len=datalen;//CG add
      schedule_pto();
    }

    Tcl& tcl = Tcl::instance();
    tcl.evalf("[Simulator instance] total-packets-add");
    printSeqTimeline(p);
    send(p, 0);//Original
  }
  else
  {
    Packet::free(p);
  }
  ////CG add ends!

  return;
}

void
MpFullTcpAgent::mptcp_set_core (MptcpAgent * core)
{
  mptcp_core_ = core;
}

void
MpFullTcpAgent::mptcp_add_mapping (int dseqnum, int length)
{
  int sseqnum = (int) curseq_ + 1;
  struct dsn_mapping tmp_map (dseqnum, sseqnum, length);
  mptcp_dsnmap_.push_back (tmp_map);

  // printf("(%d.%d-%d.%d): mptcp_add_mapping(%d,%d,%d)\n"
  //   ,here_.addr_,here_.port_
  //   ,dst_.addr_,dst_.port_
  //   ,dseqnum,sseqnum,length);
}

void MpFullTcpAgent::mptcp_add_rmt_mapping (int ifParity, int ifProtected) ///CG add for setting parity flag
{
  struct rmt_info_mapping tmp_map;
  tmp_map.seqno=(int) curseq_ + 1;
  tmp_map.ifParity=ifParity;
  tmp_map.ifProtected=ifProtected;
  rmt_info_map_.push_back (tmp_map);

  // printf("(%d.%d-%d.%d): mptcp_add_rmt_mapping(%d,%d,%d)\n"
  //   ,here_.addr_,here_.port_
  //   ,dst_.addr_,dst_.port_
  //   ,tmp_map.seqno,tmp_map.ifParity,tmp_map.ifProtected);
}

void
MpFullTcpAgent::mptcp_recv_add_mapping (int dseqnum, int sseqnum, int length)
{
  // struct dsn_mapping tmp_map (dseqnum, sseqnum, length);
  // mptcp_recv_dsnmap_.push_back (tmp_map);

  // printf("(%d.%d-%d.%d): mptcp_recv_add_mapping(%d,%d,%d)\n"
  //   ,here_.addr_,here_.port_
  //   ,dst_.addr_,dst_.port_
  //   ,dseqnum,sseqnum,length);
}

/*
 * return dsn for mptcp from subseqnum
 */
int
MpFullTcpAgent::mptcp_recv_getack (int ackno)
{
  if (ackno <= 1)
    return 0;                   /* we don't receive any data */

  vector < dsn_mapping >::iterator it = mptcp_recv_dsnmap_.begin ();
  while (it != mptcp_recv_dsnmap_.end ()) {
    struct dsn_mapping *p = &*it;

    /* check if this is the mapping for this packet */
    if (ackno >= p->sseqnum && ackno <= p->sseqnum + p->length) {
      int offset = ackno - p->sseqnum;

      mptcp_core_->set_dataack (p->dseqnum, offset);
     
      return mptcp_core_->get_dataack ();
    }
    if (ackno > p->sseqnum + p->length) {

      it = mptcp_recv_dsnmap_.erase (it);

    }
    else
    {
      ++it;
    }
  }
  fprintf (stderr, "%lf-----fatal. (%d.%d-%d.%d) no mapping info was found. ackno %d\n"
    ,Scheduler::instance().clock()
    ,here_.addr_,here_.port_
    ,dst_.addr_,dst_.port_
    , ackno);
  fprintf (stderr, "==============mptcp_recv_dsnmap_==============\n");
  for(it = mptcp_recv_dsnmap_.begin (); it != mptcp_recv_dsnmap_.end (); ++it)
 {
    struct dsn_mapping *p = &*it;

    fprintf (stderr, "sseqnum=%d\tlength=%d\n", p->sseqnum,p->length);
  }

  abort ();
}

void MpFullTcpAgent::mptcp_set_mapping_rmt_info (int seqno, int ifParity, int ifProtected) //CG add
{
  // printf ("\n============== [%lf] mptcp_set_mapping_rmt_info(%d.%d-%d.%d)==============\n"
  //       ,Scheduler::instance().clock()
  //       ,here_.addr_,here_.port_
  //       ,dst_.addr_,dst_.port_);

  vector < rmt_info_mapping >::iterator it;
  for(it = rmt_info_map_.begin (); it != rmt_info_map_.end (); ++it)
  {
    struct rmt_info_mapping *p = &*it;
    if(seqno==p->seqno)
    {
      p->ifParity=ifParity;
      p->ifProtected=ifProtected;
      // printf ("seqno=%d\tifParity=%d\tifProtected=%d\n"
      // , p->seqno,p->ifParity,p->ifProtected);
      return;
    }
    // printf ("seqno=%d\tifParity=%d\tifProtected=%d\n"
    //   , p->seqno,p->ifParity,p->ifProtected);
  }
  fprintf (stderr, "%lf-----fatal. (%d.%d-%d.%d) no mptcp_set_mapping_rmt_info was found. seqno %d\n"
    ,Scheduler::instance().clock()
    ,here_.addr_,here_.port_
    ,dst_.addr_,dst_.port_
    , seqno);
  exit(0);
}

void MpFullTcpAgent::mptcp_remove_rmt_mapping (int seqno)
{
  // printf ("\n============== [%lf] mptcp_remove_rmt_mapping(%d.%d-%d.%d)==============\n"
  //   ,Scheduler::instance().clock()
  //   ,here_.addr_,here_.port_
  //   ,dst_.addr_,dst_.port_);

  vector < rmt_info_mapping >::iterator it;

  for (it = rmt_info_map_.begin (); it != rmt_info_map_.end ();) {
    struct rmt_info_mapping *p = &*it;

    if (seqno > p->seqno)
    {
      // struct rmt_info_mapping *p_tmp = &*it;
      // printf ("erase: seqno=%d\tifParity=%d\tifProtected=%d\n"
      //     , p_tmp->seqno,p_tmp->ifParity,p_tmp->ifProtected);
      it = rmt_info_map_.erase (it);
    }
    else
    {
      // printf("After erasion:\n");
      // vector < rmt_info_mapping >::iterator it_tmp;
      // for (it_tmp = rmt_info_map_.begin (); it_tmp != rmt_info_map_.end (); ++it_tmp) {
      //   struct rmt_info_mapping *p_tmp = &*it_tmp;
      //   printf ("seqno=%d\tifParity=%d\tifProtected=%d\n"
      //       , p_tmp->seqno,p_tmp->ifParity,p_tmp->ifProtected);
      // }
      return;
    }
  }
  /*fprintf (stderr, "%lf-----fatal. (%d.%d-%d.%d) no mptcp_remove_rmt_mapping was found. seqno %d\n"
    ,Scheduler::instance().clock()
    ,here_.addr_,here_.port_
    ,dst_.addr_,dst_.port_
    , seqno);*/

  //exit(0);
}

void MpFullTcpAgent::mptcp_update_ifAllProtected()//CG add
{
  // printf ("\n============== [%lf] before mptcp_update_ifAllProtected(%d.%d-%d.%d) ifAllProtected=%d ==============\n"
  //   ,Scheduler::instance().clock()
  //   ,here_.addr_,here_.port_
  //   ,dst_.addr_,dst_.port_
  //   ,ifAllProtected);

  if(ifAllProtected==1)
  {
    vector < rmt_info_mapping >::iterator it;
    for (it = rmt_info_map_.begin (); it != rmt_info_map_.end (); it++) {
      struct rmt_info_mapping *p = &*it;
      if(!mptcp_check_rmt_mapping_ifParityorProtected(p->seqno))
      {
        ifAllProtected=0;
        break;
      }
    }
  }
  else
  {
    vector < rmt_info_mapping >::iterator it;
    int flag=0;
    for (it = rmt_info_map_.begin (); it != rmt_info_map_.end (); it++) {
      struct rmt_info_mapping *p = &*it;
      if(!mptcp_check_rmt_mapping_ifParityorProtected(p->seqno))
      {
        ifAllProtected=0;
        flag=1;
        break;
      }
    }
    if(!flag)
      ifAllProtected=1;
  }
  
  // vector < rmt_info_mapping >::iterator it;
  // for (it = rmt_info_map_.begin (); it != rmt_info_map_.end (); it++) {
  //     struct rmt_info_mapping *p_tmp = &*it;
  //     printf ("seqno=%d\tifParity=%d\tifProtected=%d\n"
  //         , p_tmp->seqno,p_tmp->ifParity,p_tmp->ifProtected);
  // }
  // printf ("\n============== [%lf] after mptcp_update_ifAllProtected(%d.%d-%d.%d) ifAllProtected=%d ==============\n"
  //   ,Scheduler::instance().clock()
  //   ,here_.addr_,here_.port_
  //   ,dst_.addr_,dst_.port_
  //   ,ifAllProtected);
}

int MpFullTcpAgent::mptcp_check_rmt_mapping_ifParityorProtected(int seqno)
{
  vector < rmt_info_mapping >::iterator it;

  for (it = rmt_info_map_.begin (); it != rmt_info_map_.end (); ++it) {
    struct rmt_info_mapping *p = &*it;
      // printf ("\n============== [%lf] mptcp_check_rmt_mapping_ifParityorProtected(%d.%d-%d.%d)==============\n"
      //   ,Scheduler::instance().clock()
      //   ,here_.addr_,here_.port_
      //   ,dst_.addr_,dst_.port_);
      // printf ("seqno=%d\tifParity=%d\tifProtected=%d\n"
      //       , p->seqno,p->ifParity,p->ifProtected);

    if (seqno == p->seqno)
    {
      if(p->ifParity)
        return 1;
      if(p->ifProtected)
        return 2;

      return 0;
    }
  }
  fprintf (stderr, "%lf-----fatal. (%d.%d-%d.%d) no mptcp_check_rmt_mapping_ifParityorProtected was found. seqno %d\n"
    ,Scheduler::instance().clock()
    ,here_.addr_,here_.port_
    ,dst_.addr_,dst_.port_
    , seqno);
  exit(0);
}

void
MpFullTcpAgent::mptcp_remove_mapping (int seqnum)
{
  vector < dsn_mapping >::iterator it;

  for (it = mptcp_dsnmap_.begin (); it != mptcp_dsnmap_.end (); ++it) {
    struct dsn_mapping *p = &*it;

    if (seqnum > p->dseqnum + p->length) {
      it = mptcp_dsnmap_.erase (it);

///CG add
      // if (here_.addr_==5&&here_.port_==1
      //   &&dst_.addr_==17&&dst_.port_==0)
      // {
          // printf ("==============mptcp_remove_mapping==============\n");
          // for (it = mptcp_dsnmap_.begin (); it != mptcp_dsnmap_.end (); ++it) {
          //   struct dsn_mapping *p = &*it;

          //   printf ("sentseq=%d\tsseqnum=%d\tdseqnum=%d\tlength=%d\n"
          //     , p->sentseq
          //     , p->sseqnum
          //     ,p->dseqnum
          //     ,p->length);
          // }
      // }
///CG add
      return;
    }
  }
}

/*
 * open up the congestion window based on linked increase algorithm
 * in rfc6356

   The logic in the document is: 
     For each ack received on subflow i, increase cwnd_i by min
     (alpha*bytes_acked*mss_i/tot_cwnd , bytes_acked*mss_i/cwnd_i )
   
   Since ns-2's congestion control logic is packet base, the logic 
   in here is rather simplified. Please note the following difference from
   the original one.
     o we don't use byte_acked. use increase_num_ instead. 
     o we don't use mss_i. use 1 packet instead.
 *
 */
void
MpFullTcpAgent::opencwnd ()
{
  // calculate alpha here
  mptcp_core_->calculate_alpha ();

  double increment;
  if (cwnd_ < ssthresh_ && mptcp_allow_slowstart_) {
    /* slow-start (exponential) */
    cwnd_ += 1;
  }
  else {
#if 1
    double alpha = mptcp_core_->get_alpha ();
    double totalcwnd = mptcp_core_->get_totalcwnd ();
    if (totalcwnd > 0.1) {

      // original increase logic. 
      double oincrement = increase_num_ / cwnd_;

      /*
        Subflow i will increase by alpha*cwnd_i/tot_cwnd segments per RTT.
      */
      increment = increase_num_ / totalcwnd * alpha;

      if (oincrement < increment) 
         increment = oincrement;
      cwnd_ += increment;
    }
#else
    // if totalcwnd is too small, use the following logic
    // original increase logic
    increment = increase_num_ / cwnd_;

    if ((last_cwnd_action_ == 0 || last_cwnd_action_ == CWND_ACTION_TIMEOUT)
        && max_ssthresh_ > 0) {
      increment = limited_slow_start (cwnd_, max_ssthresh_, increment);
    }
    cwnd_ += increment;
#endif
  }
  // if maxcwnd_ is set (nonzero), make it the cwnd limit
  if (maxcwnd_ && (int (cwnd_) > maxcwnd_))
    cwnd_ = maxcwnd_;

  return;
}

void
MpFullTcpAgent::timeout_action()
{
  SackFullTcpAgent::timeout_action();

  // calculate alpha here
  mptcp_core_->calculate_alpha ();
}

void
MpFullTcpAgent::dupack_action()
{
  SackFullTcpAgent::dupack_action();

  // calculate alpha here
  mptcp_core_->calculate_alpha ();
}

///CG add
void MpFullTcpAgent::recv(Packet *pkt, Handler* hdl)
{

  // {
  //   hdr_ip *iph = hdr_ip::access (pkt);
  //   hdr_tcp *tcph = hdr_tcp::access (pkt);
  //   if(iph->src().addr_==0 && iph->src().port_==0
  //      && iph->dst().addr_==59)
  //   {
  //     if(tcph->seqno()>currentDropSeq && 
  //       (tcph->seqno()==169361)
  //       )
  //     {
  //       printf("%lf-Node%d: drop Packet(%d.%d-%d.%d), SentSeq=%d, AckSeq=%d\n"
  //        ,Scheduler::instance().clock()
  //        ,here_.addr_
  //        ,iph->src().addr_,iph->src().port_
  //        ,iph->dst().addr_,iph->dst().port_
  //        ,tcph->seqno(),tcph->ackno());

  //       currentDropSeq=tcph->seqno();
  //       Packet::free(pkt);
  //       return;
  //     }
  //   }
  // }


  hdr_tcp *tcph = hdr_tcp::access(pkt); // TCP header
  hdr_cmn *cmnh = hdr_cmn::access(pkt); // common header (size, etc)
  int datalen = cmnh->size() - tcph->hlen(); // # payload bytes

  if(ifMultiport==1)
  {
    mptcp_core_->recv(pkt,hdl);  
    PrintPacket(pkt,"Receive",0);
    // SackFullTcpAgent::prpkt(pkt);
    if(mptcp_core_->get_ifRMPTCP())
    {
      int ackno = tcph->ackno();     // ack # from packet

      mptcp_remove_rmt_mapping(ackno);

      if(ifAllProtected==0)
        mptcp_update_ifAllProtected();
    }

    SackFullTcpAgent::recv(pkt,hdl);
    // printf("mss(%d),last_ack_sent_(%d),maxseq(%d)\n",maxseg_,last_ack_sent_,mptcp_get_maxseq());
    // rq_.dumplist();

    ////Reactive sender
    if (datalen == 0 && ifGAReactive_)
    {
      if(ga_pto_timer_.status()==TIMER_PENDING)
      {
        ga_pto_timer_.cancel(); 
      }
      schedule_pto();
    }

    if(mptcp_core_->get_ifRMPTCPSink())
      restorePacket();

    mptcp_core_->send_control();
  }
  else
  {
    SackFullTcpAgent::recv(pkt,hdl);
  }
}

int
MpFullTcpAgent::if_can_send(int seq)
{
  int topInitialWin=wnd_init_*size ();

  /*printf("=============%lf---[%d.%d-%d.%d]: if_can_sendseq=%d==============\n"
          ,Scheduler::instance().clock()
          ,here_.addr_,here_.port_
          ,dst_.addr_,dst_.port_
          ,seq);*/

  if(seq<topInitialWin)
  {
    // printf("%lf---[%d.%d-%d.%d]: seq=%d,topInitialWin=%d\n"
    //       ,Scheduler::instance().clock()
    //       ,here_.addr_,here_.port_
    //       ,dst_.addr_,dst_.port_
    //       ,seq
    //       ,topInitialWin
    //       );
    return 1;
  }
  else
  {
    // if(here_.addr_==0 && here_.port_==0
    //     && dst_.addr_==2 && dst_.port_==0)
    // {

    // not in pipe control, so use regular control
    if (!pipectrl_)
    {
      int win = window() * maxseg_;
     
      int topwin = highest_ack_ + win;

      //CG add

      // if(here_.addr_==0 && here_.port_==0
      //   && dst_.addr_==2 && dst_.port_==0)
      // printf("%lf---[%d.%d-%d.%d]: if_can_send seq=%d,topwin=%d !pipectrl_\n"
      //       ,Scheduler::instance().clock()
      //       ,here_.addr_,here_.port_
      //       ,dst_.addr_,dst_.port_
      //       ,seq
      //       ,topwin);        

      return (seq < topwin);
    }
    else
    {
      // don't overshoot receiver's advertised window
        int topawin = highest_ack_ + int(wnd_) * maxseg_;
        if (seq >= topawin) {
          return FALSE;
        }

      int win = window()*maxseg_;      
      
      int topseq=maxseq_+win-pipe_;

      // if(here_.addr_==0 && here_.port_==0
      //   && dst_.addr_==2 && dst_.port_==0)
      // printf("%lf---[%d.%d-%d.%d]: if_can_send seq=%d,topseq=%d,maxseq_=%d pipectrl_\n"
      //       ,Scheduler::instance().clock()
      //       ,here_.addr_,here_.port_
      //       ,dst_.addr_,dst_.port_
      //       ,seq
      //       ,topseq
      //       ,(int)maxseq_);

      if(nxt_tseq()!=maxseq_) 
        return 0;       

      return (seq < topseq);
    }
  }
}

int MpFullTcpAgent::ifEarlyRetransmit()//CG add
{
  int total_bytes_=mptcp_core_->get_totalbytes();
  // printf("%lf---[%d.%d-%d.%d]: ifDataAppLimited total_bytes_=%d,onFlight=%d\n"
  //           ,Scheduler::instance().clock()
  //           ,here_.addr_,here_.port_
  //           ,dst_.addr_,dst_.port_
  //           ,total_bytes_
  //           ,maxseq_ - highest_ack_ - sq_.total());

  if(total_bytes_<=0)
  {
    if(maxseq_ - highest_ack_ - sq_.total()==size())
      return 1;  ///No app data left, limited
    else
      return 0;
  }
  else
    return 0;////Not all app data has been sent
            ///Not limited
}

void MpFullTcpAgent::cancel_timers()
{

  // cancel: rtx, burstsend, delsnd
  FullTcpAgent::cancel_timers();      
  ga_pto_timer_.force_cancel();//CG add
}

void MpFullTcpAgent::handle_pto()
{
  if(ifGAReactive_!=1)
  {
    fprintf(stderr,"ERROR: no reactive TCP call handle_pto\n");
    exit(1);
  }

  consecutivePTO++;
  if(consecutivePTO>2)
  {
    fprintf(stderr,"consecutivePTO>2\n");
    exit(1);
  }

  if(!ifAPPLimited() && 0) ////CG add for not send new packet
  {
    int seq = nxt_tseq();
    int datalen=curseq_-seq+1;
    if(datalen>size())
    {
      datalen=size();
    }

    // printf("%f(%d.%d-%d.%d): handle_pto send new packet seq=%d,datalen=%d!\n",
    //   now(),
    //   here_.addr_,
    //   here_.port_,
    //   dst_.addr_,
    //   dst_.port_,
    //   seq,
    //   datalen
    //   );

    // foutput(seq, REASON_REACTIVE_PTO);
    SackFullTcpAgent::sendpacket(seq, rcv_nxt_, outflags(), datalen, REASON_REACTIVE_PTO, NULL);
  }
  else
  {
    // printf("%f(%d.%d-%d.%d): handle_pto retran last packet seq=%d,datalen=%d!\n",
    //   now(),
    //   here_.addr_,
    //   here_.port_,
    //   dst_.addr_,
    //   dst_.port_,
    //   last_sent_seq,
    //   last_sent_data_len
    //   );

    if(last_sent_data_len==0)
    {
      fprintf(stderr,"%f(%d.%d-%d.%d): last_sent_data_len=0!\n",
        now(),
        here_.addr_,
        here_.port_,
        dst_.addr_,
        dst_.port_
        );
      exit(1);
    }
    SackFullTcpAgent::sendpacket(last_sent_seq, rcv_nxt_, outflags(), last_sent_data_len, REASON_REACTIVE_PTO, NULL);
        // foutput(last_sent_seq, REASON_REACTIVE_PTO);
  }

  Tcl& tcl = Tcl::instance();
    tcl.evalf("[Simulator instance] clb-parity-number-add");

    recoveryPacketsGenerated_++;
    printRecoveryNumber();
}

void MpFullTcpAgent::schedule_pto()
{
  if(ifGAReactive_!=1)
  {
    fprintf(stderr,"ERROR: no reactive TCP call schedule_pto\n");
    exit(1);
  }

  int in_recovery = ((highest_ack_ < recover_) && last_cwnd_action_!=CWND_ACTION_ECN);
  if(in_recovery 
    || highest_ack_>=maxseq_)
  {
    return;
  }

  // printf("%lf---[%d.%d-%d.%d]: highest_ack_=%d,maxseq_=%d\n"
 //            ,Scheduler::instance().clock()
 //            ,here_.addr_,here_.port_
 //            ,dst_.addr_,dst_.port_
 //            ,(int)highest_ack_
 //            ,(int)maxseq_); 


  if(state_==TCPS_ESTABLISHED 
      && (ifCWNDLimited() || ifAPPLimited())
      && consecutivePTO<2)
  {
    double pto_length=(double) 2*((t_srtt_ >> T_SRTT_BITS) * tcp_tick_);
    pto_length = pto_length < rtt_timeout() ? pto_length:rtt_timeout();

    if(pto_length==0)
    {
      pto_length=4E-4;
    }
    ga_pto_timer_.resched(pto_length);

    // printf("%lf---[%d.%d-%d.%d]: ga_pto_timer_.resched(%lf)\n"
  //           ,Scheduler::instance().clock()
  //           ,here_.addr_,here_.port_
  //           ,dst_.addr_,dst_.port_
  //           ,pto_length);   
  }
  // else
  // {
  //  double pto_length=rtt_timeout();
  //  ga_pto_timer_.resched(pto_length);

  //  printf("%lf---[%d.%d-%d.%d]: ga_pto_timer_.resched(%lf)\n"
 //            ,Scheduler::instance().clock()
 //            ,here_.addr_,here_.port_
 //            ,dst_.addr_,dst_.port_
 //            ,pto_length);   
  // }
}

void ReactiveTimer::expire(Event *) {
    // printf("%lf---[%d.%d-%d.%d]: Hello PTO\n"
    //   ,Scheduler::instance().clock()
    //     ,a_->addr(),a_->port()
    //     ,a_->daddr(),a_->dport());
    a_->pto_timeout();
}
