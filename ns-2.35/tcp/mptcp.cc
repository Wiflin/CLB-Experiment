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
#include "flags.h"
#include "random.h"
#include "template.h"
#include "mptcp.h"

#include "rmptcp.cc" ///CG add

#include <iostream>     // std::cout
#include <algorithm>    // std::sort
#include <vector>       // std::vector

static class MptcpClass:public TclClass
{
public:
  MptcpClass ():TclClass ("Agent/MPTCP")
  {
  }
  TclObject *create (int, const char *const *)
  {
    return (new MptcpAgent ());
  }
} class_mptcp;

MptcpAgent::MptcpAgent ():Agent (PT_TCP), MAX_PARITY_NUMBER(-1),
ifAllDataHasBeenSent(1), sub_num_ (0), dst_num_ (0), 
total_bytes_ (0), mcurseq_ (1), mackno_ (1), infinite_send_(false), 
totalcwnd_ (0), alpha_ (0), maxSubID(0), maxDstID(0), ifRMPTCP(0), ifRMPTCPSink(0)
{
  memset(subflows_,0,MAX_SUBFLOW*sizeof(subflow));
  memset(subFlowSendBytes,0,MAX_SUBFLOW*sizeof(int));

  //CG add
  memset(fCodingInfo,0,MAX_SUBFLOW*sizeof(flowCodingInfo));
}

void
MptcpAgent::delay_bind_init_all ()
{
  Agent::delay_bind_init_all ();
  delay_bind_init_one("use_olia_");
}

/* haven't implemented yet */
int
MptcpAgent::delay_bind_dispatch (const char *varName,
                                 const char *localName,
                                 TclObject * tracer)
{
  if (delay_bind_bool(varName, localName, "use_olia_", &use_olia_, tracer)) return TCL_OK;
  return Agent::delay_bind_dispatch (varName, localName, tracer);
}

#if 0
void
MptcpAgent::TraceAll ()
{
}

void
MptcpAgent::TraceVar (const char *cpVar)
{
}

void
MptcpAgent::trace (TracedVar * v)
{
  if (eTraceAll == TRUE)
    TraceAll ();
  else
    TraceVar (v->name ());
}
#endif

int
MptcpAgent::command (int argc, const char *const *argv)
{
  if (argc == 2) {
    if (strcmp (argv[1], "listen") == 0) {
      for (int i = 0; i < sub_num_; i++) {
        if (subflows_[i].tcp_->command (argc, argv) != TCL_OK)
          return (TCL_ERROR);
      }
      return (TCL_OK);
    }
    if (strcmp (argv[1], "reset") == 0) {

      /* reset used flag information */
      bool used_dst[dst_num_];
      for (int j = 0; j < dst_num_; j++)
      {
        used_dst[j] = false;
      }

      for (int i = 0; i < sub_num_; i++) {
        for (int j = 0; j < dst_num_; j++) {

          /* if this destination is already used by other subflow, don't use it */
          if (used_dst[j])
          {
            continue;
          }

          if (check_routable (i, dsts_[j].addr_, dsts_[j].port_)) {
            subflows_[i].daddr_ = dsts_[j].addr_;
            subflows_[i].dport_ = dsts_[j].port_;
            subflows_[i].tcp_->daddr () = dsts_[j].addr_;
            subflows_[i].tcp_->dport () = dsts_[j].port_;
            used_dst[j] = true; 
            break;
          }
        }
      }
      subflows_[0].tcp_->mptcp_set_primary ();
      return (TCL_OK);
    }
  }
  if (argc == 3) {
    if (strcmp (argv[1], "attach-tcp") == 0) {
      // int id = get_subnum ();
      int id = sub_num_;
      subflows_[id].tcp_ = (MpFullTcpAgent *) TclObject::lookup (argv[2]);
      subflows_[id].used = true;
      subflows_[id].addr_ = subflows_[id].tcp_->addr ();
      subflows_[id].port_ = subflows_[id].tcp_->port ();
      subflows_[id].tcp_->mptcp_set_core (this);
     // subflows_[id].tcp_->attachApp(app_); ////CG add
      sub_num_++;
      return (TCL_OK);
    }
    else if (strcmp (argv[1], "set-multihome-core") == 0) {
      core_ = (Classifier *) TclObject::lookup (argv[2]);
      if (core_ == NULL) {
        return (TCL_ERROR);
      }
      return (TCL_OK);
    }
    else if (strcmp (argv[1], "rmptcp") == 0) {///CG add
      ifRMPTCP=1;

      MAX_PARITY_NUMBER=atoi(argv[2]);

      if(MAX_PARITY_NUMBER<0)
      {
        printf("MAX_PARITY_NUMBER=%d\n",MAX_PARITY_NUMBER);
        return (TCL_ERROR);
      }

      for (int i = 0; i < sub_num_; i++) {
        if (subflows_[i].tcp_ == NULL)
        {
          printf("sub-flow should be attached first!\n");
          return (TCL_ERROR);
        }

        subflows_[i].tcp_->dInfo.parityBitMap=new int [MAX_PARITY_NUMBER];

        for(int j=0;j<MAX_PARITY_NUMBER;j++)
        {
          subflows_[i].tcp_->dInfo.parityBitMap[j]=0;
        }
      }

      return (TCL_OK);
    }
    else if (strcmp (argv[1], "rmptcp-sink") == 0) {///CG add
      ifRMPTCPSink=1;

      MAX_PARITY_NUMBER=atoi(argv[2]);

      if(MAX_PARITY_NUMBER<0)
      {
        printf("MAX_PARITY_NUMBER=%d\n",MAX_PARITY_NUMBER);
        return (TCL_ERROR);
      }

      for (int i = 0; i < sub_num_; i++) {
        if (subflows_[i].tcp_ == NULL)
        {
          printf("sub-flow should be attached first!\n");
          return (TCL_ERROR);
        }

        subflows_[i].tcp_->dInfo.parityBitMap=new int [MAX_PARITY_NUMBER];

        for(int j=0;j<MAX_PARITY_NUMBER;j++)
        {
          subflows_[i].tcp_->dInfo.parityBitMap[j]=0;
        }
      }

      return (TCL_OK);
    }
  }
  if (argc == 4) {
    if (strcmp (argv[1], "add-multihome-destination") == 0) {
      add_destination (atoi (argv[2]), atoi (argv[3]));
      return (TCL_OK);
    }
     //CG add
    else if (strcmp (argv[1], "add-subflow-target") == 0) {
      Agent * tcpSub=(Agent *) TclObject::lookup (argv[2]);
      int addr=tcpSub->addr();
      int port=tcpSub->port();
      int id = find_subflow (addr, port);
      subflows_[id].target_ = (NsObject *) TclObject::lookup (argv[3]);
      subflows_[id].tcp_->ifMultiport = 1;
      here_.addr_=tcpSub->addr();
      return (TCL_OK);
    }
    ///CG add
    /// $mptcp attach-tcp $tcp $node
    if (strcmp (argv[1], "attach-tcp") == 0) {
      int id = maxSubID;
      if(subflows_[id].used == true)
      {
        printf("Attach-tcp id error! %d has been used\n",id);
        exit(1);
      }
      subflows_[id].tcp_ = (MpFullTcpAgent *) TclObject::lookup (argv[2]);
      subflows_[id].used = true;
      subflows_[id].addr_ = subflows_[id].tcp_->addr ();
      subflows_[id].port_ = subflows_[id].tcp_->port ();
      subflows_[id].tcp_->mptcp_set_core (this);
     // subflows_[id].tcp_->attachApp(app_); ////CG add
      sub_num_++;
      maxSubID++;

      Agent * tcpSub=(Agent *) TclObject::lookup (argv[2]);
      subflows_[id].target_ = (NsObject *) TclObject::lookup (argv[3]);
      subflows_[id].tcp_->ifMultiport = 1;
      here_.addr_=tcpSub->addr();

      return (TCL_OK);
    }
  }
  if (argc == 6) {
    if (strcmp (argv[1], "add-multihome-interface") == 0) {
      /* argv[2] indicates the addresses of the mptcp session */

      /* find the id for tcp bound to this address */
      int id = find_subflow (atoi (argv[2]));
      if (id < 0) {
        fprintf (stderr, "cannot find tcp bound to interface addr [%s]",
                 argv[2]);
        return (TCL_ERROR);
      }
      subflows_[id].tcp_->port () = atoi (argv[3]);
      subflows_[id].port_ = atoi (argv[3]);
      subflows_[id].target_ = (NsObject *) TclObject::lookup (argv[4]);
      subflows_[id].link_ = (NsObject *) TclObject::lookup (argv[5]);
      if (subflows_[id].target_ == NULL || subflows_[id].link_ == NULL)
        return (TCL_ERROR);

      return (TCL_OK);
    }
  }
  return (Agent::command (argc, argv));
}

int
MptcpAgent::get_subnum ()
{
  for (int i = 0; i < MAX_SUBFLOW; i++) {
    if (!subflows_[i].used)
      return i;
  }
  return -1;
}

int
MptcpAgent::find_subflow (int addr, int port)
{
  for (int i = 0; i < MAX_SUBFLOW; i++) {
    if (subflows_[i].addr_ == addr && subflows_[i].port_ == port)
      return i;
  }
  return -1;
}

int
MptcpAgent::find_subflow (int addr)
{
  for (int i = 0; i < MAX_SUBFLOW; i++) {
    if (subflows_[i].addr_ == addr)
      return i;
  }
  return -1;
}

void
MptcpAgent::recv (Packet * pkt, Handler * h)
{
  hdr_ip *iph = hdr_ip::access (pkt);
  hdr_tcp *tcph = hdr_tcp::access (pkt);

///CG add
 //  if(iph->src().addr_==5 && iph->src().port_==1)
 // //   || (iph->src().addr_==1 && iph->dst().addr_==61))
 //  {
 //    printf("%lf-Node%d: recv Packet(%d.%d-%d.%d), SentSeq=%d, AckSeq=%d\n"
 //       ,Scheduler::instance().clock()
 //       ,here_.addr_
 //       ,iph->src().addr_,iph->src().port_
 //       ,iph->dst().addr_,iph->dst().port_
 //       ,tcph->seqno(),tcph->ackno());
 //  }
///CG add

  /* find subflow id from the destination address */
  int id = find_subflow (iph->daddr (), iph->dport ());///CG add iph->dport ()
  if (id < 0) {
    fprintf (stderr,
             "MptcpAgent:recv() fatal error. cannot find destination\n");
    abort ();
  }

  /* processing mptcp options */
  if (tcph->mp_capable ()) {
    /* if we receive mpcapable option, return the same option as response */
    subflows_[id].tcp_->mpcapable_ = true;
  }
  if (tcph->mp_join ()) {
    /* if we receive mpjoin option, return the same option as response */
    subflows_[id].tcp_->mpjoin_ = true;
  }
  if (tcph->mp_ack ()) {

///CG add
   //  if(iph->src().addr_==17 && iph->src().port_==0)
   // //   || (iph->src().addr_==1 && iph->dst().addr_==61))
   //  {
   //    printf("%lf-Node%d: recv mpACK Packet(%d.%d-%d.%d), SentSeq=%d, AckSeq=%d, mp_ack=%d\n"
   //     ,Scheduler::instance().clock()
   //     ,here_.addr_
   //     ,iph->src().addr_,iph->src().port_
   //     ,iph->dst().addr_,iph->dst().port_
   //     ,tcph->seqno(),tcph->ackno()
   //     ,tcph->mp_ack ());
   //  }
///CG add

    /* when we receive mpack, erase the acked record */
    subflows_[id].tcp_->mptcp_remove_mapping (tcph->mp_ack ());
  }

  if (tcph->mp_dsn ()) {
    /* when we receive mpdata, update new mapping */
    subflows_[id].tcp_->mpack_ = true;
    subflows_[id].tcp_->mptcp_recv_add_mapping (tcph->mp_dsn (),
                                                tcph->mp_subseq (),
                                                tcph->mp_dsnlen ());

// ///CG add
//    if (subflows_[id].addr_==17&&subflows_[id].port_==0)
//     {
//       printf("%lf-(%d.%d-%d.%d): mptcp_recv_add_mapping dseqnum=%d\tsseqnum=%d\tlength=%d\n"
//         ,Scheduler::instance().clock()
//         ,subflows_[id].addr_,subflows_[id].port_
//         ,subflows_[id].daddr_,subflows_[id].dport_
//         ,tcph->mp_dsn ()
//         ,tcph->mp_subseq ()
//         ,tcph->mp_dsnlen ());
//     }
// ///CG add  
  }

  /* make sure packet will be return to the src addr of the packet */
  subflows_[id].tcp_->daddr () = iph->saddr ();
  subflows_[id].tcp_->dport () = iph->sport ();

  //CG add
  if(subflows_[id].tcp_->ifMultiport==0)
  {
    printf("MPTCP error!\n");
    /* call subflow's recv function *//////
    subflows_[id].tcp_->recv (pkt, h);////original
    send_control ();//original
  }///CG add

///CG add begins
  hdr_cmn *cmnh = hdr_cmn::access (pkt);
  if(cmnh->pInfo.ifRecovery==1)
  {
    int fID=find_subflow (cmnh->pInfo.dstAddr, cmnh->pInfo.dstPort);

    if(cmnh->pInfo.RorC==2) ////Coding packet
    {
      subflows_[fID].tcp_->recvParity(pkt);
    }
    else if(cmnh->pInfo.RorC==1) ////Simple Retran packet
    {
      subflows_[fID].tcp_->recvRetrans(pkt);
    }
  }
}

/*
 * add possible destination address
 */
void
MptcpAgent::add_destination (int addr, int port)
{
  ///CG add
  int i=maxDstID;
  if (dsts_[i].active_)
  {
    printf("add_destination id error! %d has been used\n",i);
    exit(1);
  }
  dsts_[i].addr_ = addr;
  dsts_[i].port_ = port;
  dsts_[i].active_ = true;
  dst_num_++;
  maxDstID++;
  return;


  ////Original
  // for (int i = 0; i < MAX_SUBFLOW; i++) {
  //   if (dsts_[i].active_)
  //     continue;
  //   dsts_[i].addr_ = addr;
  //   dsts_[i].port_ = port;
  //   dsts_[i].active_ = true;
  //   dst_num_++;
  //   return;
  // }
  // fprintf (stderr, "fatal error. cannot add destination\n");
  // abort ();
}

/*
 * check if this subflow can reach to the specified address
 */
bool
MptcpAgent::check_routable (int sid, int addr, int port)
{
  Packet *p = allocpkt ();
  hdr_ip *iph = hdr_ip::access (p);
  iph->daddr () = addr;
  iph->dport () = port;
  bool
    result = (static_cast < Classifier * >
              (subflows_[sid].target_)->classify (p) > 0) ? true : false;
  Packet::free (p);

  return result;
}

void
MptcpAgent::sendmsg (int nbytes, const char * /*flags */ )
{
  if (nbytes == -1) {
    infinite_send_ = true;
    total_bytes_ = TCP_MAXSEQ;
  } else
    total_bytes_ = total_bytes_+nbytes;

  send_control ();
}

/*
 * control sending data
 */

typedef std::pair<double,int> mypair;
bool comparator ( const mypair& l, const mypair& r)
{ 
  return l.first < r.first; 
}

void
MptcpAgent::send_control ()
{
//  printf("%f---Begin Send: %d bytes waiting to be sent\n",Scheduler::instance().clock(),total_bytes_);
  if (total_bytes_ > 0 || infinite_send_) {
    /* one round */
    // bool slow_start = false;

    int k=rand();
    for (int j = 0; j < sub_num_; j++) {  ///Original
      int i=(k+j)%sub_num_;

      int mss = subflows_[i].tcp_->size ();
      while(total_bytes_>0) {
        ifAllDataHasBeenSent=0;
        int curseq = subflows_[i].tcp_->mptcp_get_curseq();
        int curSubSeqToSend=curseq+1;
        if(!subflows_[i].tcp_->if_can_send(curSubSeqToSend))
        {
          break;
        }
        // printf("%lf---Node[%d].subflows_[%d] total_bytes_(%d): send %d bytes. curSubSeqToSend=%d,curseq=%d,mcurseq_=%d,mss=%d\n"
        //   ,Scheduler::instance().clock()
        //   ,here_.addr_
        //   ,i
        //   ,total_bytes_
        //   ,mss
        //   ,curSubSeqToSend
        //   ,subflows_[i].tcp_->mptcp_get_curseq()
        //   ,mcurseq_
        //   ,mss);
        
        // subflows_[i].tcp_->mptcp_add_mapping (mcurseq_, mss);
        if(ifRMPTCP==1)
          subflows_[i].tcp_->mptcp_add_rmt_mapping (0, 0); //CG add
        subflows_[i].tcp_->ifAllProtected=0;
        subflows_[i].tcp_->sendmsg (mss);
        mcurseq_ += mss;
        if (!infinite_send_)
          total_bytes_ -= mss;
        if (total_bytes_<=0)
        {
          total_bytes_=0;

          ifAllDataHasBeenSent=1;

          for (int i = 0; i < sub_num_; i++) {
            if(subflows_[i].tcp_->ifAllDataSent_==0)
            {
              subflows_[i].tcp_->ifAllDataSent_=1;
              subflows_[i].tcp_->initRetranFile();
              subflows_[i].tcp_->printSpareWindow();
            }
          }

          if(ifRMPTCP==1)
            sendRecoveryPackets();
        } 
      }
    }
  }
 ///CG add 
  else if(total_bytes_<=0)
  {
    ifAllDataHasBeenSent=1;
    for (int i = 0; i < sub_num_; i++) {
      if(subflows_[i].tcp_->ifAllDataSent_==0)
      {
        subflows_[i].tcp_->ifAllDataSent_=1;
        subflows_[i].tcp_->printSpareWindow();
      }
    }

    if(ifRMPTCP==1)
      sendRecoveryPackets();
  }
}

/*
 *  calculate alpha based on the equation in draft-ietf-mptcp-congestion-05
 *
 *  Peforme ths following calculation

                                      cwnd_i
                                 max --------
                                  i         2
                                      rtt_i
             alpha = tot_cwnd * ----------------
                               /      cwnd_i \ 2
                               | sum ---------|
                               \  i   rtt_i  /


 */

void
MptcpAgent::calculate_alpha ()
{
  double max_i = 0.001;
  double sum_i = 0;
  double totalcwnd = 0;

  for (int i = 0; i < sub_num_; i++) {
#if 1
    int backoff = subflows_[i].tcp_->mptcp_get_backoff ();
    // we don't utlize a path which has lots of timeouts
    if (backoff >= 4) 
       continue;
#endif

    double rtt_i  = subflows_[i].tcp_->mptcp_get_srtt ();
    double cwnd_i = subflows_[i].tcp_->mptcp_get_cwnd ();

    if (rtt_i < 0.000001) // too small. Let's not update alpha
      return; 

    double tmp_i = cwnd_i / (rtt_i * rtt_i);
    if (max_i < tmp_i)
      max_i = tmp_i;

    sum_i += cwnd_i / rtt_i;
    totalcwnd += cwnd_i;
  }
  if (sum_i < 0.001)
    return;

  alpha_ = totalcwnd * max_i / (sum_i * sum_i);
}

/*
 * create ack block based on data ack information
 */
void
MptcpAgent::set_dataack (int ackno, int length)
{
  bool found = false;
  vector < dack_mapping >::iterator it = dackmap_.begin ();

  while (it != dackmap_.end ()) {
    struct dack_mapping *p = &*it;

    /* find matched block for this data */
    if (p->ackno <= ackno && p->ackno + p->length >= ackno &&
        p->ackno + p->length < ackno + length) {
      p->length = ackno + length - p->ackno;
      found = true;
      break;
    }
    else
      ++it;
  }

  /* if there's no matching block, add new one */
  if (!found) {
    struct dack_mapping tmp_map = { ackno, length };
    dackmap_.push_back (tmp_map);


    /* re-calculate cumlative ack and erase old records */
    it = dackmap_.begin ();
    while (it != dackmap_.end ()) {
      struct dack_mapping *p = &*it;
      if (mackno_ >= p->ackno && mackno_ <= p->ackno + p->length)
        mackno_ = ackno + length;
      if (mackno_ > p->ackno + p->length) {
        it = dackmap_.erase (it);

        // if (here_.addr_==17)
        // {
        //   printf("%lf-==========dackmap_ erase========, mackno_=%d\n"
        //     ,Scheduler::instance().clock()
        //     ,mackno_);
        //    vector < dack_mapping >::iterator itTmp;
        //     for (itTmp = dackmap_.begin (); itTmp != dackmap_.end (); ++itTmp) {
        //       struct dack_mapping *p = &*it;

        //       printf ("ackno=%d\tlength=%d\n"
        //         , p->ackno
        //         ,p->length);
        //     }
        // }
      }
      else
        ++it;
    }
  }
}
