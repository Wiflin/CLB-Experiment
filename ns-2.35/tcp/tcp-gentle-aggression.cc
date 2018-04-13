#include "tcp-gentle-aggression.h"//CG add
#include "ip.h"

static class GAFullTcpClass:public TclClass
{
public:
  GAFullTcpClass ():TclClass ("Agent/TCP/FullTcp/Sack/GA")
  {
  }
  TclObject *create (int, const char *const *)
  {
    return (new GAFullTcpAgent ());
  }
} class_gafull;

int
GAFullTcpAgent::command(int argc, const char*const* argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "reactive") == 0) {
			ifGAReactive_=1;
			return (TCL_OK);
		}
		if (strcmp(argv[1], "corrective") == 0) {
			ifGACorrective_=1;
			return (TCL_OK);
		}
		if (strcmp(argv[1], "corrective-sink") == 0) {
			ifGACorrectiveSink_=1;
			return (TCL_OK);
		}
		if (strcmp(argv[1], "proactive") == 0) {
			ifGAProactive_=1;
			RepFlowHashKey_=ecmpHashKey;
			return (TCL_OK);
		}
		if (strcmp(argv[1], "proactive-sink") == 0) {
			ifGAProactiveSink_=1;
			return (TCL_OK);
		}
		if (strcmp(argv[1], "repflow") == 0) {
			ifGAProactive_=1;
			return (TCL_OK);
		}
	}
	if (argc == 3) {
		if (strcmp(argv[1], "ecmp-hash-repflow-key") == 0) {
			RepFlowHashKey_=atoi(argv[2]);
			return (TCL_OK);
		}
	}
	return (SackFullTcpAgent::command(argc, argv));
}


void GAFullTcpAgent::GA_init()
{
	currentDropSeq=28500;

	ifGAReactive_=0;
	consecutivePTO=0;
	ifGACorrective_=0;
	MaxBLockNumber=16;
   	ifGAProactive_=0;
   	numberInCurrentBlock=0;
   	ifGACorrectiveSink_=0;
   	ifFinshed=0;
   	ifGAProactiveSink_=0;
   	memset(&encodeBuff_,0,sizeof(encodeBuff_));
}

int GAFullTcpAgent::process_coding_packet(CodingINFO cInfo)
{
	if(last_ack_sent_ >= cInfo.stopSeq 
		|| state_==TCPS_LAST_ACK)
	{
		////No need to decode. Already received all
		return 0;
	}


	int codeBlockSize=1+(cInfo.stopSeq-cInfo.startSeq)/size ();
	int ackedPacketsInBlock=0;
	if(last_ack_sent_ > cInfo.startSeq)
	{
		ackedPacketsInBlock=(last_ack_sent_-cInfo.startSeq)/size ();
	}
	int unAckedPacketsInBlock=rq_.total()/size ();
	int totalReceivedPackets=ackedPacketsInBlock+unAckedPacketsInBlock+1;


	// printf("%lf---[%d.%d-%d.%d]:codeBlockSize(%d),last_ack_sent_(%d),ackedPacketsInBlock(%d),unAckedPacketsInBlock(%d),totalReceivedPackets(%d)\n"
	//     ,Scheduler::instance().clock()
 //        ,here_.addr_,here_.port_
 //        ,dst_.addr_,dst_.port_
 //        ,codeBlockSize
	//     ,last_ack_sent_
	//     ,ackedPacketsInBlock
	//     ,unAckedPacketsInBlock
	//     ,totalReceivedPackets);
	// rq_.dumplist();

	if(totalReceivedPackets<codeBlockSize)///Can't decode now
	{
		// Generate_Lost_Info_ACK();
		return 0;
	}
	
	Packet *p = allocpkt ();
    hdr_tcp *tcph = hdr_tcp::access (p);

    tcph->seqno () = last_ack_sent_;
    if(tcph->seqno ()==0)
    	tcph->seqno ()=1;
    tcph->ackno () = 1;
    tcph->sa_length () = 0;       // may be increased by build_options()
    tcph->hlen () = tcpip_base_hdr_size_;
    tcph->hlen () += build_options (tcph);
    tcph->flags() = TH_PUSH|TH_ACK;

    hdr_cmn *ch = hdr_cmn::access (p);
    ch->size () = size () + tcph->hlen ();
    ch->gaInfo.ifRestored=1;

    Tcl& tcl = Tcl::instance();
    tcl.evalf("[Simulator instance] clb-decode-times-add");
    // SackFullTcpAgent::prpkt(p);
    SackFullTcpAgent::recv(p,NULL);

    return 1;
}

void GAFullTcpAgent::ga_encode(int seqno, int datalen)
{
	// printf("%lf---[%d.%d-%d.%d]: ga_encode\n"
 //            ,Scheduler::instance().clock()
 //            ,here_.addr_,here_.port_
 //            ,dst_.addr_,dst_.port_
 //            );  
	numberInCurrentBlock++;

	if(numberInCurrentBlock==MaxBLockNumber)
	{
		// printf("%lf---[%d.%d-%d.%d]: encodeBuff_ full\n"
  //           ,Scheduler::instance().clock()
  //           ,here_.addr_,here_.port_
  //           ,dst_.addr_,dst_.port_
  //           );  
		encodeBuff_.stopSeq=seqno+datalen-1;
		if(encodeBuff_.stopSeq<=encodeBuff_.startSeq)
		{
			fprintf(stderr,"ERROR: GA flowCodingInfo.stopSeq<=flowCodingInfo.startSeq\n");
			exit(1);
		}
		generate_GA_Parity();
		ga_encode_timer.cancel();
	}
	else if(numberInCurrentBlock==1)
	{
		encodeBuff_.startSeq=seqno;
		encodeBuff_.stopSeq=seqno+datalen-1;

		double encode_time=(double) ((t_srtt_ >> T_SRTT_BITS) * tcp_tick_)/2;
		if(encode_time>0)
		{
			encode_time = encode_time < rtt_timeout() ? encode_time:rtt_timeout();
		}
		else
		{
			encode_time=1E-4;
		}
		ga_encode_timer.sched(encode_time);

		// printf("%lf---[%d.%d-%d.%d]: encodeBuff_ sched(%lf)\n"
  //           ,Scheduler::instance().clock()
  //           ,here_.addr_,here_.port_
  //           ,dst_.addr_,dst_.port_
  //           ,encode_time);  
	}
	else if(numberInCurrentBlock<MaxBLockNumber
		&& numberInCurrentBlock>1)
	{
		encodeBuff_.stopSeq=seqno+datalen-1;
	}
	else
	{
		fprintf(stderr, "ERROR: numberInCurrentBlock=%d\n"
			,numberInCurrentBlock );
		exit(1);
	}
}

void GAFullTcpAgent::generate_GA_Parity()
{
	if(sendTimer_.status()!= TIMER_IDLE)
	{
		encodeBuff_.startSeq=0;
    	encodeBuff_.stopSeq=0;
		numberInCurrentBlock=0;
		return;
	}

	// printf("%lf---[%d.%d-%d.%d]: generate_GA_Parity\n"
 //        ,Scheduler::instance().clock()
 //        ,here_.addr_,here_.port_
 //        ,dst_.addr_,dst_.port_
 //        );  
	Packet *p = allocpkt ();
    hdr_tcp *tcph = hdr_tcp::access (p);

    // tcph->seqno () = encodeBuff_.startSeq;
    tcph->seqno () = encodeBuff_.stopSeq-1;
    tcph->ackno () = 1;
    tcph->sa_length () = 0;       // may be increased by build_options()
    tcph->hlen () = tcpip_base_hdr_size_;
    tcph->hlen () += build_options (tcph);
    tcph->flags() = TH_PUSH|TH_ACK;

    hdr_cmn *ch = hdr_cmn::access (p);
    ch->size () = 1+tcph->hlen ();
    ch->gaInfo.RorCorP=2;
    ch->gaInfo.cInfo.startSeq=encodeBuff_.startSeq;
    ch->gaInfo.cInfo.stopSeq=encodeBuff_.stopSeq;


    encodeBuff_.startSeq=0;
    encodeBuff_.stopSeq=0;
	numberInCurrentBlock=0;

    sendTimer_.setPacket(p);
    double send_time=(double) ((t_srtt_ >> T_SRTT_BITS) * tcp_tick_)/4;
	if(send_time>0)
	{
		send_time = send_time < rtt_timeout() ? send_time:rtt_timeout();
	}
	else
	{
		send_time=4E-5;
	}

    sendTimer_.sched(send_time);
    // printf("%lf---[%d.%d-%d.%d]: sendTimer_ sched(%lf)\n"
    //         ,Scheduler::instance().clock()
    //         ,here_.addr_,here_.port_
    //         ,dst_.addr_,dst_.port_
    //         ,send_time);  
}

void GAFullTcpAgent::ga_encode_timeout()
{
	generate_GA_Parity();
}


void GAFullTcpAgent::handle_pto()
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

	// if(!ifAPPLimited())
	if(0)
	{
		int seq = nxt_tseq();
		int datalen=curseq_-seq+1;
		if(datalen>size())
		{
			datalen=size();
		}

		// printf("%f(%d.%d-%d.%d): handle_pto send new packet seq=%d,datalen=%d!\n",
		// 	now(),
		// 	here_.addr_,
		// 	here_.port_,
		// 	dst_.addr_,
		// 	dst_.port_,
		// 	seq,
		// 	datalen
		// 	);

		// foutput(seq, REASON_REACTIVE_PTO);
		SackFullTcpAgent::sendpacket(seq, rcv_nxt_, outflags(), datalen, REASON_REACTIVE_PTO, NULL);
	}
	else
	{
		// if(here_.addr_==36 && here_.port_==0
  //       && dst_.addr_==127 && dst_.port_==1)
		// printf("%f(%d.%d-%d.%d): handle_pto retran last packet seq=%d,datalen=%d!\n",
		// 	now(),
		// 	here_.addr_,
		// 	here_.port_,
		// 	dst_.addr_,
		// 	dst_.port_,
		// 	last_sent_seq,
		// 	last_sent_data_len
		// 	);

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

void GAFullTcpAgent::schedule_pto()
{
	if(ifGAReactive_!=1)
	{
		fprintf(stderr,"ERROR: no reactive TCP call schedule_pto\n");
		exit(1);
	}

	int in_recovery = ((highest_ack_ < recover_) && last_cwnd_action_!=CWND_ACTION_ECN);

	// if(here_.addr_==36 && here_.port_==0
 //        && dst_.addr_==127 && dst_.port_==1)
	// printf("%lf---[%d.%d-%d.%d] schedule_pto: highest_ack_=%d,maxseq_=%d,recover_=%d,in_recovery=%d\n"
 //            ,Scheduler::instance().clock()
 //            ,here_.addr_,here_.port_
 //            ,dst_.addr_,dst_.port_
 //            ,(int)highest_ack_
 //            ,(int)maxseq_
 //            ,recover_
 //            ,in_recovery); 

	if(in_recovery 
		|| highest_ack_>=maxseq_)
	{
		return;
	}

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

		// if(here_.addr_==36 && here_.port_==0
  //       && dst_.addr_==127 && dst_.port_==1)
		// printf("%lf---[%d.%d-%d.%d]: ga_pto_timer_.resched(%lf)\n"
  //           ,Scheduler::instance().clock()
  //           ,here_.addr_,here_.port_
  //           ,dst_.addr_,dst_.port_
  //           ,pto_length);   
	}
	// else
	// {
	// 	double pto_length=rtt_timeout();
	// 	ga_pto_timer_.resched(pto_length);

	// 	printf("%lf---[%d.%d-%d.%d]: ga_pto_timer_.resched(%lf)\n"
 //            ,Scheduler::instance().clock()
 //            ,here_.addr_,here_.port_
 //            ,dst_.addr_,dst_.port_
 //            ,pto_length);   
	// }
}

void GAFullTcpAgent::cancel_timers()
{

	// cancel: rtx, burstsend, delsnd
	FullTcpAgent::cancel_timers();      
	ga_pto_timer_.force_cancel();//CG add
	ga_encode_timer.force_cancel();//CG add
}

void 
GAFullTcpAgent::sendpacket(int seqno, int ackno, int pflags, int datalen, int reason, Packet *p)
{
	SackFullTcpAgent::sendpacket(seqno,ackno,pflags,datalen,reason,p);
	last_sent_seq=seqno;//CG add
	last_sent_data_len=datalen;//CG add

	if(ifGAReactive_==1 && reason!=REASON_REACTIVE_PTO)
	{
		consecutivePTO=0;
	}

	if(seqno>=maxseq_)
	{
		if(ifGAReactive_==1)
		{
			schedule_pto();
		}
		if(ifGACorrective_==1)
		{
			if(datalen>0)
				ga_encode(seqno,datalen);
		}
		if(ifGAProactive_==1)
		{
			Tcl& tcl = Tcl::instance();
    		tcl.evalf("[Simulator instance] clb-parity-number-add");
    		recoveryPacketsGenerated_++;
    		printRecoveryNumber();

			SackFullTcpAgent::sendpacket(seqno,ackno,pflags,datalen,REASON_PROACTIVE_REP,p);
			retransTimes++;
			return;
		}
	}

	if(ifGAProactive_==1)
	{		
		Tcl& tcl = Tcl::instance();
    	tcl.evalf("[Simulator instance] clb-parity-number-add");
    	recoveryPacketsGenerated_++;
    	printRecoveryNumber();
    	SackFullTcpAgent::sendpacket(seqno,ackno,pflags,datalen,REASON_PROACTIVE_REP,p);
	}
}

void GAFullTcpAgent::recv(Packet *pkt, Handler *hdl)
{
	// if((HDR_IP(pkt))->src().addr_==0 && (HDR_IP(pkt))->src().port_==0
	// && (HDR_IP(pkt))->dst().addr_==2 && (HDR_IP(pkt))->dst().port_==0)
	// {
	// 	if((HDR_TCP(pkt))->seqno()>currentDropSeq && (HDR_TCP(pkt))->seqno()<30000)
	// 	{
	// 	  printf("%lf-Node%d: drop Packet(%d.%d-%d.%d), SentSeq=%d, AckSeq=%d, currentDropSeq=%d\n"
	// 	   ,Scheduler::instance().clock()
	// 	   ,here_.addr_
	// 	   ,(HDR_IP(pkt))->src().addr_,(HDR_IP(pkt))->src().port_
	// 	   ,(HDR_IP(pkt))->dst().addr_,(HDR_IP(pkt))->dst().port_
	// 	   ,(HDR_TCP(pkt))->seqno(),(HDR_TCP(pkt))->ackno()
	// 	   ,currentDropSeq);

	// 	  currentDropSeq=(HDR_TCP(pkt))->seqno();
	// 	  Packet::free(pkt);
	// 	  return;
	// 	}
	// }


	// printf("%lf-Node%d: recv Packet(%d.%d-%d.%d), SentSeq=%d, AckSeq=%d, state=%d\n"
	// 	   ,Scheduler::instance().clock()
	// 	   ,here_.addr_
	// 	   ,(HDR_IP(pkt))->src().addr_,(HDR_IP(pkt))->src().port_
	// 	   ,(HDR_IP(pkt))->dst().addr_,(HDR_IP(pkt))->dst().port_
	// 	   ,(HDR_TCP(pkt))->seqno(),(HDR_TCP(pkt))->ackno()
	// 	   ,state_);


	hdr_tcp *tcph = hdr_tcp::access(pkt);	// TCP header
	hdr_cmn *cmnh = hdr_cmn::access(pkt);	// common header (size, etc)
	int datalen = cmnh->size() - tcph->hlen(); // # payload bytes

	// if(datalen<size())
	// 	ifFinshed=1;

///Corrective receiver
	if(cmnh->gaInfo.RorCorP==2
		&& ifGACorrectiveSink_==1)
	{
		// if(ifFinshed==1)
		// {
		// 	Packet::free(pkt);
		// 	return;
		// }
		CodingINFO cInfo;
		memcpy(&cInfo,&(cmnh->gaInfo.cInfo),sizeof(cInfo));
		if(process_coding_packet(cInfo)==1) 
		{
			Packet::free(pkt); ///if decode!
			return;////Don't handover coding packet
		}		
	}

///Proactive receiver	
	if(cmnh->gaInfo.RorCorP==3
		&& ifGAProactiveSink_==1)
	{
		if(tcph->seqno()>=last_ack_sent_)
		{
			if(rq_.ifMissSegment(tcph->seqno(),size())!=-1)
			{
				Tcl& tcl = Tcl::instance();
	    		tcl.evalf("[Simulator instance] clb-decode-times-add");
			}
			else
			{
				Packet::free(pkt); ///if not usefull!
				return;////Don't handover coding packet
			}
		}
		else
		{
			Packet::free(pkt); ///if not usefull!
			return;////Don't handover coding packet
		}
	}

	 // printf("%lf-Node%d: recv Packet(%d.%d-%d.%d), SentSeq=%d, AckSeq=%d, state=%d\n"
		//    ,Scheduler::instance().clock()
		//    ,here_.addr_
		//    ,(HDR_IP(pkt))->src().addr_,(HDR_IP(pkt))->src().port_
		//    ,(HDR_IP(pkt))->dst().addr_,(HDR_IP(pkt))->dst().port_
		//    ,(HDR_TCP(pkt))->seqno(),(HDR_TCP(pkt))->ackno()
		//    ,state_);
	SackFullTcpAgent::recv(pkt, hdl);

	////Reactive sender
	if (datalen == 0 && ifGAReactive_)
	{
		if(ga_pto_timer_.status()==TIMER_PENDING)
		{
			ga_pto_timer_.cancel();

			// if(here_.addr_==36 && here_.port_==0
   //      	&& dst_.addr_==127 && dst_.port_==1)
			// printf("%lf---[%d.%d-%d.%d]: ga_pto_timer_.cancel()\n"
	  //           ,Scheduler::instance().clock()
	  //           ,here_.addr_,here_.port_
	  //           ,dst_.addr_,dst_.port_
	  //           );  
		}
		schedule_pto();
	}
}

///CG add begins
int GAFullTcpAgent::ifEarlyRetransmit()//CG add
{
	// printf("%lf---[%d.%d-%d.%d]: ifDataAppLimited maxseq_=%d,curseq_=%d,onFlight=%d\n"
 //        ,Scheduler::instance().clock()
 //        ,here_.addr_,here_.port_
 //        ,dst_.addr_,dst_.port_
 //        ,(int)maxseq_
 //        ,(int)curseq_
 //        ,maxseq_ - highest_ack_ - sq_.total());   
	if(maxseq_<=curseq_)
	{
		return 0; ////Not all app data has been sent
					///Not limited
	}
	else
	{
		if(maxseq_ - highest_ack_==tcprexmtthresh_*size() && sq_.total()==size() && ifGAReactive_==1)
		{
			return 1;
		}
			
		if(maxseq_ - highest_ack_ - sq_.total()==size())
	      return 1;  ///No app data left, limited
	    else
	      return 0;
	}
}


///CG add begins
void GAReactiveTimer::expire(Event *) {
	// if(a_->addr()==36 && a_->port()==0
 //        && a_->daddr()==127 && a_->dport()==1)
 //    printf("%lf---[%d.%d-%d.%d]: Hello PTO\n"
 //    	,Scheduler::instance().clock()
 //        ,a_->addr(),a_->port()
 //        ,a_->daddr(),a_->dport());
    a_->pto_timeout();
}

void GACorrectiveTimer::expire(Event *) {
    // printf("%lf---[%d.%d-%d.%d]: Hello Corrective Encode\n"
    // 	,Scheduler::instance().clock()
    //     ,a_->addr(),a_->port()
    //     ,a_->daddr(),a_->dport());
    a_->ga_encode_timeout();
}

void GADelaySendTimer::expire(Event *) {
    // printf("%lf---[%d.%d-%d.%d]: Hello Delay Send\n"
    // 	,Scheduler::instance().clock()
    //     ,a_->addr(),a_->port()
    //     ,a_->daddr(),a_->dport());
    Tcl& tcl = Tcl::instance();
    tcl.evalf("[Simulator instance] clb-parity-number-add");
    a_->incrementRecoveryNumber();
    a_->printRecoveryNumber();

	tcl.evalf("[Simulator instance] total-packets-add");
      
    if(pkt_==NULL)
    {
    	fprintf(stderr, "ERROR: DelaySend has no Packet!\n" );
    	exit(1);
    }
    hdr_cmn *cmnh = hdr_cmn::access(pkt_);
	cmnh->gaInfo.RorCorP=2;

    a_->printSeqTimeline(pkt_);
	a_->send(pkt_, 0);//Original
}
///CG add ends