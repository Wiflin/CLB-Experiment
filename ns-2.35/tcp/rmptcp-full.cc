///CG add begins
void MpFullTcpAgent::restorePacket()
{
  // printf("mss(%d),last_ack_sent_(%d),maxseq(%d)\n",maxseg_,last_ack_sent_,mptcp_get_maxseq());

  if(dInfo.ifValid==0)
  {
    return;
  }
  
  int codeBlockSize=1+(dInfo.stopSeq-dInfo.startSeq)/size ();
  int ackedPacketsInBlock=0;
  if(last_ack_sent_ > dInfo.startSeq)
  {
    ackedPacketsInBlock=(last_ack_sent_-dInfo.startSeq)/size ();
  }
  int unAckedPacketsInBlock=rq_.total()/size ();
  int receivedParities=dInfo.receivedParityNumber;
  int totalReceivedPackets=ackedPacketsInBlock+unAckedPacketsInBlock+receivedParities;

  // printf("codeBlockSize(%d),last_ack_sent_(%d),ackedPacketsInBlock(%d),unAckedPacketsInBlock(%d),receivedParities(%d),totalReceivedPackets(%d)\n"
  //   ,codeBlockSize
  //   ,last_ack_sent_
  //   ,ackedPacketsInBlock
  //   ,unAckedPacketsInBlock
  //   ,receivedParities
  //   ,totalReceivedPackets);
  // rq_.dumplist();

  if(totalReceivedPackets<codeBlockSize)///Can't decode now
  {
    return;
  }

  while(rq_.getRcvNxt()<=dInfo.stopSeq)
  {
    int a,b;
    int seqno=rq_.nexthole(last_ack_sent_,a,b);
    if(seqno==-1)
    {
      seqno=last_ack_sent_;
    }
    Packet *p = allocpkt ();
    hdr_tcp *tcph = hdr_tcp::access (p);

    tcph->seqno () = seqno;
    tcph->ackno () = 1;
    tcph->sa_length () = 0;       // may be increased by build_options()
    tcph->hlen () = tcpip_base_hdr_size_;
    tcph->hlen () += build_options (tcph);
    tcph->flags() = TH_PUSH|TH_ACK;

    hdr_cmn *ch = hdr_cmn::access (p);
    ch->size () = size () + tcph->hlen ();
    ch->pInfo.ifRestored=1;

    Tcl& tcl = Tcl::instance();
    tcl.evalf("[Simulator instance] clb-decode-times-add");
    PrintPacket(p,"Receive restore",0);
    // SackFullTcpAgent::prpkt(p);
    SackFullTcpAgent::recv(p,NULL);
    mptcp_core_->send_control();
  }

  dInfo.ifValid=0;
}

void MpFullTcpAgent::recvParity(Packet *pkt)
{
  hdr_cmn *cmnh = hdr_cmn::access (pkt);
  char str[128];
  sprintf(str,"recvParity(%d.%d)",here_.addr_,here_.port_);
  PrintPacket(pkt,str,0);
  if(cmnh->pInfo.ifRecovery==0)
  {
    printf("ERROR: recvParity receive non parity packet!\n");
    exit(0);
  }

  if(dInfo.ifValid==0)
  {
    dInfo.ifValid=1;
    dInfo.genID=cmnh->pInfo.genID;
    dInfo.receivedParityNumber=1;
    dInfo.parityBitMap[cmnh->pInfo.pIndex]=1;
    dInfo.startSeq=cmnh->pInfo.startSeq;
    dInfo.stopSeq=cmnh->pInfo.stopSeq;
  }
  else////dInfo.ifValid==1
  {
    if(dInfo.genID==cmnh->pInfo.genID)
    {
      if(dInfo.parityBitMap[cmnh->pInfo.pIndex]==0)
      {
        dInfo.receivedParityNumber++;
        dInfo.parityBitMap[cmnh->pInfo.pIndex]=1;
      }
      else///
      {
        // printf("receive out-dated parity packet! Drop it.\n");
      }
    }
    else if(dInfo.genID>cmnh->pInfo.genID)
    {
      dInfo.ifValid=1;
      dInfo.genID=cmnh->pInfo.genID;
      dInfo.receivedParityNumber=1;
      dInfo.parityBitMap[cmnh->pInfo.pIndex]=1;
      dInfo.startSeq=cmnh->pInfo.startSeq;
      dInfo.stopSeq=cmnh->pInfo.stopSeq;
    }
    else////dInfo.genID<cmnh->pInfo.genID
    {
      // printf("receive out-dated parity packet! Drop it.\n");
    }
  }

  PrintDecodeBufferInfo(0);

  restorePacket();
}

void MpFullTcpAgent::recvRetrans(Packet *pkt)
{
  // printf("mss(%d),last_ack_sent_(%d),maxseq(%d)\n",maxseg_,last_ack_sent_,mptcp_get_maxseq());
  
  // printf("retranSeq(%d),last_ack_sent_(%d)\n"
  //   ,(HDR_CMN(pkt)->pInfo.startSeq)
  //   ,(int)last_ack_sent_);
  if((HDR_CMN(pkt)->pInfo.startSeq)<last_ack_sent_)
    return;

  Packet *p = allocpkt ();
  hdr_tcp *tcph = hdr_tcp::access (p);

  tcph->seqno () = (HDR_CMN(pkt)->pInfo.startSeq);
  tcph->ackno () = 1;
  tcph->sa_length () = 0;       // may be increased by build_options()
  tcph->hlen () = tcpip_base_hdr_size_;
  tcph->hlen () += build_options (tcph);
  tcph->flags() = TH_PUSH|TH_ACK;

  hdr_cmn *ch = hdr_cmn::access (p);
  ch->size () = size () + tcph->hlen ();
  ch->pInfo.ifRestored=1;

  if(mptcp_if_lost_seg(tcph->seqno ())>0)
  {
    Tcl& tcl = Tcl::instance();
    tcl.evalf("[Simulator instance] clb-decode-times-add");
  }
  PrintPacket(p,"Receive restore",0);
  // SackFullTcpAgent::prpkt(p);
  SackFullTcpAgent::recv(p,NULL);
  mptcp_core_->send_control();
}

parityInfo MpFullTcpAgent::rmptcp_ps_lookup_mapping (int seqno)
{
  if(here_.addr_==17&&here_.port_==832&&dst_.addr_==54&&dst_.port_==641)
  {
    printf ("\n============== [%lf] rmptcp_ps_lookup_mapping(%d.%d-%d.%d) seqno(%d) ==============\n"
          ,Scheduler::instance().clock()
          ,here_.addr_,here_.port_
          ,dst_.addr_,dst_.port_
          ,seqno);
    vector < parity_seq_mapping >::iterator it;
    for(it = rmptcp_psmap_.begin (); it != rmptcp_psmap_.end (); ++it)
    {
      struct parity_seq_mapping *p = &*it;
      printf ("seqno=%d\tpInfo(%d.%d-%d.%d  start=%d stop=%d)\n"
          , p->seqno
          , p->pInfo.srcAddr
          , p->pInfo.srcPort
          , p->pInfo.dstAddr
          , p->pInfo.dstPort
          , p->pInfo.startSeq
          , p->pInfo.stopSeq);
    }
  }

  vector < parity_seq_mapping >::iterator it;
  for(it = rmptcp_psmap_.begin (); it != rmptcp_psmap_.end (); ++it)
  {
    struct parity_seq_mapping *p = &*it;

    if(seqno==p->seqno)
    {
      return p->pInfo;
    }
  }
  fprintf (stderr, "%lf-----fatal. (%d.%d-%d.%d) no rmptcp_ps_lookup_mapping was found. seqno %d\n"
    ,Scheduler::instance().clock()
    ,here_.addr_,here_.port_
    ,dst_.addr_,dst_.port_
    , seqno);
  exit(0);
}


void
MpFullTcpAgent::rmptcp_ps_add_mapping (int seqno, parityInfo pInfo)
{
  struct parity_seq_mapping tmp_map;
  tmp_map.seqno=seqno;
  tmp_map.pInfo=pInfo;
  rmptcp_psmap_.push_back (tmp_map);

  if(here_.addr_==17&&here_.port_==832&&dst_.addr_==54&&dst_.port_==641)
  {
    printf ("\n============== [%lf] rmptcp_ps_add_mapping(%d.%d-%d.%d) seqno(%d) ==============\n"
          ,Scheduler::instance().clock()
          ,here_.addr_,here_.port_
          ,dst_.addr_,dst_.port_
          ,seqno);
    vector < parity_seq_mapping >::iterator it;
    for(it = rmptcp_psmap_.begin (); it != rmptcp_psmap_.end (); ++it)
    {
      struct parity_seq_mapping *p = &*it;
      printf ("seqno=%d\tpInfo(%d.%d-%d.%d  start=%d stop=%d)\n"
          , p->seqno
          , p->pInfo.srcAddr
          , p->pInfo.srcPort
          , p->pInfo.dstAddr
          , p->pInfo.dstPort
          , p->pInfo.startSeq
          , p->pInfo.stopSeq);
    }
  }
}


void
MpFullTcpAgent::parity_sendmsg(int nbytes, parityInfo pInfo)
{
  if(nbytes!=size ())
  {
    printf("ERROR: parity_sendmsg(nbytes,pInfo), nbytes must equal MSS!\n");
    exit(0);
  }

  const char *flags = 0;
  if (flags && strcmp(flags, "MSG_EOF") == 0) 
    close_on_empty_ = TRUE; 
  if (flags && strcmp(flags, "DAT_EOF") == 0) 
    signal_on_empty_ = TRUE;  

  if (nbytes == -1) {
    infinite_send_ = TRUE;
    parity_advance_bytes(0,pInfo);
  } else
    parity_advance_bytes(nbytes,pInfo);
}

void
MpFullTcpAgent::parity_advance_bytes(int nb, parityInfo pInfo)
{
  switch (state_) {

  case TCPS_CLOSED:
  case TCPS_LISTEN:
                reset();
                curseq_ = iss_ + nb;
                connect();              // initiate new connection
    break;

  case TCPS_ESTABLISHED:
  case TCPS_SYN_SENT:
  case TCPS_SYN_RECEIVED:
                if (curseq_ < iss_) 
                        curseq_ = iss_; 
                curseq_ += nb;
    break;

  default:
            if (debug_) 
              fprintf(stderr, "%f: FullTcpAgent::advance(%s): cannot advance while in state %s\n",
             now(), name(), statestr(state_));

  }

  if (state_ == TCPS_ESTABLISHED)
  {
    // if (dupacks_ < tcprexmtthresh_ && dupacks_>0)
    // {
    //   parity_send_much(1, REASON_LIMITED_TRANSMIT, maxseg_, pInfo);
    // }
    // else
    // {
      parity_send_much(0, REASON_NORMAL, maxburst_,pInfo);
    // }
  }
    

    return;
}

void
MpFullTcpAgent::parity_send_much(int force, int reason, int maxburst, parityInfo pInfo)
{
  int npackets = 0; // sent so far

//if ((int(t_seqno_)) > 1)
//printf("%f: send_much(f:%d, win:%d, pipectrl:%d, pipe:%d, t_seqno:%d, topwin:%d, maxseq_:%d\n",
//now(), force, win, pipectrl_, pipe_, int(t_seqno_), topwin, int(maxseq_));

  if (!force && (delsnd_timer_.status() == TIMER_PENDING))
    return;

  while (1) {

    /*
     * note that if output decides to not actually send
     * (e.g. because of Nagle), then if we don't break out
     * of this loop, we can loop forever at the same
     * simulated time instant
     */
    int amt;
    int seq = nxt_tseq();
    if (!force && !send_allowed(seq))
      break;
    // Q: does this need to be here too?
    if (!force && overhead_ != 0 &&
        (delsnd_timer_.status() != TIMER_PENDING)) {
      delsnd_timer_.resched(Random::uniform(overhead_));
      return;
    }
    if ((amt = parity_foutput(seq, reason,pInfo)) <= 0)
      break;
    if ((outflags() & TH_FIN))
      --amt;  // don't count FINs
    sent(seq, amt);
    force = 0;

    if ((outflags() & (TH_SYN|TH_FIN)) ||
        (maxburst && ++npackets >= maxburst))
      break;
  }
  return;
}

int
MpFullTcpAgent::parity_foutput(int seqno, int reason, parityInfo pInfo)
{
  // if maxseg_ not set, set it appropriately
  // Q: how can this happen?

    if (dctcp_)
    {
      if (maxseg_ == 0) 
        maxseg_ = size_; // Mohammad: changed from size_ - headersize();
    // Mohammad: This else condition is unnecessary and conflates with tcp.cc
    }
    else
    {
      if (maxseg_ == 0) 
        maxseg_ = size_ - headersize();
      else
        size_ =  maxseg_ + headersize();
    }
    

  int is_retransmit = (seqno < maxseq_);
  int quiet = (highest_ack_ == maxseq_);
  int pflags = outflags();
  int syn = (seqno == iss_);
  int emptying_buffer = FALSE;
  int buffered_bytes = (infinite_send_) ? TCP_MAXSEQ :
        curseq_ - highest_ack_ + 1;

  int win = window() * maxseg_; // window (in bytes)
  int off = seqno - highest_ack_; // offset of seg in window
  int datalen;
  //int amtsent = 0;

  // be careful if we have not received any ACK yet
  if (highest_ack_ < 0) {
    if (!infinite_send_)
      buffered_bytes = curseq_ - iss_;;
    off = seqno - iss_;
  }

  if (syn && !data_on_syn_)
    datalen = 0;
  else if (pipectrl_)
    datalen = buffered_bytes - off;
  else
    datalen = min(buffered_bytes, win) - off;

  if(reason==REASON_LIMITED_TRANSMIT)///CG add
    datalen=maxseg_;

        if ((signal_on_empty_) && (!buffered_bytes) && (!syn))
                  bufferempty();

  //
  // in real TCP datalen (len) could be < 0 if there was window
  // shrinkage, or if a FIN has been sent and neither ACKd nor
  // retransmitted.  Only this 2nd case concerns us here...
  //
  if (datalen < 0) {
    datalen = 0;
  } else if (datalen > maxseg_) {
    datalen = maxseg_;
  }

  //
  // this is an option that causes us to slow-start if we've
  // been idle for a "long" time, where long means a rto or longer
  // the slow-start is a sort that does not set ssthresh
  //

  if (slow_start_restart_ && quiet && datalen > 0) {
    if (idle_restart()) {
      slowdown(CLOSE_CWND_INIT);
    }
  }

  //
  // see if sending this packet will empty the send buffer
  // a dataless SYN packet counts also
  //

  if (!infinite_send_ && ((seqno + datalen) > curseq_ || 
      (syn && datalen == 0))) {
    emptying_buffer = TRUE;
    //
    // if not a retransmission, notify application that 
    // everything has been sent out at least once.
    //
    if (!syn) {
      idle();
      if (close_on_empty_ && quiet) {
        flags_ |= TF_NEEDCLOSE;
      }
    }
    pflags |= TH_PUSH;
    //
    // if close_on_empty set, we are finished
    // with this connection; close it
    //
  } else  {
    /* not emptying buffer, so can't be FIN */
    pflags &= ~TH_FIN;
  }
  if (infinite_send_ && (syn && datalen == 0))
    pflags |= TH_PUSH;  // set PUSH for dataless SYN

  /* sender SWS avoidance (Nagle) */

  if (datalen > 0) {
    // if full-sized segment, ok
    if (datalen == maxseg_)
      goto send;
    // if Nagle disabled and buffer clearing, ok
    if ((quiet || nodelay_)  && emptying_buffer)
      goto send;
    // if a retransmission
    if (is_retransmit)
      goto send;
    // if big "enough", ok...
    //  (this is not a likely case, and would
    //  only happen for tiny windows)
    if (datalen >= ((wnd_ * maxseg_) / 2.0))
      goto send;
  }

  if (need_send())
    goto send;

  /*
   * send now if a control packet or we owe peer an ACK
   * TF_ACKNOW can be set during connection establishment and
   * to generate acks for out-of-order data
   */

  if ((flags_ & (TF_ACKNOW|TF_NEEDCLOSE)) ||
      (pflags & (TH_SYN|TH_FIN))) {
    goto send;
  }

        /*      
         * No reason to send a segment, just return.
         */      
  return 0;

send:

  // is a syn or fin?

  syn = (pflags & TH_SYN) ? 1 : 0;
  int fin = (pflags & TH_FIN) ? 1 : 0;

        /* setup ECN syn and ECN SYN+ACK packet headers */
        if (ecn_ && syn && !(pflags & TH_ACK)){
                pflags |= TH_ECE;
                pflags |= TH_CWR;
        }
        if (ecn_ && syn && (pflags & TH_ACK)){
                pflags |= TH_ECE;
                pflags &= ~TH_CWR;
        }
  else if (ecn_ && ect_ && cong_action_ && 
               (!is_retransmit || SetCWRonRetransmit_)) {
    /* 
     * Don't set CWR for a retranmitted SYN+ACK (has ecn_ 
     * and cong_action_ set).
     * -M. Weigle 6/19/02
                 *
                 * SetCWRonRetransmit_ was changed to true,
                 * allowing CWR on retransmitted data packets.  
                 * See test ecn_burstyEcn_reno_full 
                 * in test-suite-ecn-full.tcl.
     * - Sally Floyd, 6/5/08.
     */
    /* set CWR if necessary */
    pflags |= TH_CWR;
    /* Turn cong_action_ off: Added 6/5/08, Sally Floyd. */
    cong_action_ = FALSE;
  }

  /* moved from sendpacket()  -M. Weigle 6/19/02 */
  //
  // although CWR bit is ordinarily associated with ECN,
  // it has utility within the simulator for traces. Thus, set
  // it even if we aren't doing ECN
  //
  if (datalen > 0 && cong_action_ && !is_retransmit) {
    pflags |= TH_CWR;
  }
  
        /* set ECE if necessary */
        if (ecn_ && ect_ && recent_ce_ ) {
    pflags |= TH_ECE;
  }

        /* 
         * Tack on the FIN flag to the data segment if close_on_empty_
         * was previously set-- avoids sending a separate FIN
         */ 
        if (flags_ & TF_NEEDCLOSE) {
                flags_ &= ~TF_NEEDCLOSE;
                if (state_ <= TCPS_ESTABLISHED && state_ != TCPS_CLOSED)
                {
                    pflags |=TH_FIN;
                    fin = 1;  /* FIN consumes sequence number */
                    newstate(TCPS_FIN_WAIT_1);
                }
        }
  parity_sendpacket(seqno, rcv_nxt_, pflags, datalen, reason,pInfo);

        /*      
         * Data sent (as far as we can tell).
         * Any pending ACK has now been sent.
         */      
  flags_ &= ~(TF_ACKNOW|TF_DELACK);

  if(dctcp_)
    delack_timer_.force_cancel();

  /*
   * if we have reacted to congestion recently, the
   * slowdown() procedure will have set cong_action_ and
   * sendpacket will have copied that to the outgoing pkt
   * CWR field. If that packet contains data, then
   * it will be reliably delivered, so we are free to turn off the
   * cong_action_ state now  If only a pure ACK, we keep the state
   * around until we actually send a segment
   */

  int reliable = datalen + syn + fin; // seq #'s reliably sent
  /* 
   * Don't reset cong_action_ until we send new data.
   * -M. Weigle 6/19/02
   */
  if (cong_action_ && reliable > 0 && !is_retransmit)
    cong_action_ = FALSE;

  // highest: greatest sequence number sent + 1
  //  and adjusted for SYNs and FINs which use up one number

  int highest = seqno + reliable;
  if (highest > dctcp_maxseq) 
    dctcp_maxseq = highest;
  if (highest > maxseq_) {
    maxseq_ = highest;
    //
    // if we are using conventional RTT estimation,
    // establish timing on this segment
    //
    if (!ts_option_ && rtt_active_ == FALSE) {
      rtt_active_ = TRUE; // set timer
      rtt_seq_ = seqno;   // timed seq #
      rtt_ts_ = now();  // when set
    }
  }

  /*
   * Set retransmit timer if not currently set,
   * and not doing an ack or a keep-alive probe.
   * Initial value for retransmit timer is smoothed
   * round-trip time + 2 * round-trip time variance.
   * Future values are rtt + 4 * rttvar.
   */
  if (rtx_timer_.status() != TIMER_PENDING && reliable) {
    set_rtx_timer();  // no timer pending, schedule one
  }

  return (reliable);
}

void
MpFullTcpAgent::parity_sendpacket(int seqno, int ackno, int pflags, int datalen, int reason, parityInfo pInfo)
{
  Packet * p=NULL;
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
    tcph->mp_ack () = mptcp_recv_getack (ackno);
    if (tcph->mp_ack ())
      mptcp_option_size_ += MPTCP_ACKOPTION_SIZE;
  }

  if (datalen && !mptcp_dsnmap_.empty ()) {
    vector < dsn_mapping >::iterator it;

///CG add

    for (it = mptcp_dsnmap_.begin (); it != mptcp_dsnmap_.end (); ++it) {
      struct dsn_mapping *p = &*it; 
      if (seqno >= p->sseqnum && seqno < p->sseqnum + p->length) {
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
    nrexmitbytes_ += datalen;
  }

  last_ack_sent_ = ackno;

  // memcpy(&(HDR_CMN(p))->pInfo,&pInfo,sizeof(pInfo));

  (HDR_CMN(p))->pInfo=pInfo;

  rmptcp_ps_add_mapping(tcph->seqno(),pInfo);

  pipe_ += datalen;

  PrintPacket(p,"Send",0);

  Tcl& tcl = Tcl::instance();
  tcl.evalf("[Simulator instance] total-packets-add");

  hdr_ip *iph = hdr_ip::access (p);

  if(tcph->seqno()<maxseq_)
  {
    retransTimes++;
    // printf("%lf-RETRANS: Packet(%d.%d-%d.%d), RetransTimes=%d, timeOutTimes=%d, SentSeq=%d, AckSeq=%d, MaxSeq=%d\n"
    //  ,Scheduler::instance().clock()
    //  ,iph->src().addr_,iph->src().port_
    //  ,iph->dst().addr_,iph->dst().port_
    //  ,retransTimes,timeOutTimes
    //  ,tcph->seqno(),tcph->ackno()
    //  ,(int)maxseq_);
  }

  if(ifGAReactive_==1)
  {
    last_sent_seq=seqno;//CG add
    last_sent_data_len=datalen;//CG add
    schedule_pto();
  }

  printSeqTimeline(p);
  send (p, 0);

  return;
}

void MpFullTcpAgent::PrintPacket(Packet* pkt, char SendorReceive[128], int force)
{
  if(!IF_PRINT_DEBUG && force==0)
  {
    return;
  }

  hdr_cmn* cmnh = hdr_cmn::access(pkt);
  hdr_tcp* tcph = hdr_tcp::access(pkt);
  hdr_ip* iph = hdr_ip::access(pkt);
  printf("%lf-NODE %d: %s packet(%d.%d-%d.%d) uid=%d, seqno=%d, ackno=%d\n"
    ,Scheduler::instance().clock()
    ,here_.addr_
    ,SendorReceive
    ,iph->src().addr_,iph->src().port_
    ,iph->dst().addr_,iph->dst().port_
    ,cmnh->uid_
    ,tcph->seqno()
    ,tcph->ackno()
    );

/*  printf("vPath=%d,genIndex=%d,feedBackVP=%d,feedBackVP_ACKedPackets=%d\n"
    ,cmnh->vPath
    ,cmnh->genIndex
    ,cmnh->feedBackVP
    ,cmnh->feedBackVP_ACKedPackets
    );*/
  if(cmnh->pInfo.ifRecovery==1)
  {
    printf("++++Parity: subFlowID=%d(%d.%d-%d.%d),genID=%d,pIndex=%d,startSeq=%d,stopSeq=%d\n"
      ,cmnh->pInfo.subFlowID
      ,cmnh->pInfo.srcAddr
      ,cmnh->pInfo.srcPort
      ,cmnh->pInfo.dstAddr
      ,cmnh->pInfo.dstPort
      ,cmnh->pInfo.genID
      ,cmnh->pInfo.pIndex
      ,cmnh->pInfo.startSeq
      ,cmnh->pInfo.stopSeq);
  }
}

void MpFullTcpAgent::PrintDecodeBufferInfo(int force)
{
  if(!IF_PRINT_DEBUG && force==0)
  {
    return;
  }

  printf("====NODE %d dBufInfo: ifValid=%d,genID=%d,receivedParityNumber=%d,startSeq=%d,stopSeq=%d,parityBitMap="
    ,here_.addr_
    ,dInfo.ifValid
    ,dInfo.genID
    ,dInfo.receivedParityNumber
    ,dInfo.startSeq
    ,dInfo.stopSeq);
  for(int i=0;i<mptcp_core_->MAX_PARITY_NUMBER;i++)
  {
    printf("%d|",dInfo.parityBitMap[i]);
  }
  printf("\n");
}