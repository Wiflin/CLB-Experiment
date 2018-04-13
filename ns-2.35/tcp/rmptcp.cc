parityInfo MptcpAgent::setParityPInfo(int subFlowID)
{
  int maxseq = subflows_[subFlowID].tcp_->mptcp_get_maxseq () - subflows_[subFlowID].tcp_->size();
  int highest_ack = subflows_[subFlowID].tcp_->mptcp_get_highest_ack ();

  if(highest_ack<fCodingInfo[subFlowID].currentStartSeq)
  {
    fprintf(stderr, "ERROR setPInfo: highest_ack<fCodingInfo[%d].currentStartSeq",subFlowID);
    exit(0);
  }
  else if(highest_ack==fCodingInfo[subFlowID].currentStartSeq)
  {
    fCodingInfo[subFlowID].currentPIndex=fCodingInfo[subFlowID].currentPIndex+1;
  }
  else if(highest_ack>fCodingInfo[subFlowID].currentStartSeq)
  {
    fCodingInfo[subFlowID].currentGenID=fCodingInfo[subFlowID].currentGenID+1;
    fCodingInfo[subFlowID].currentPIndex=0;
    fCodingInfo[subFlowID].currentStartSeq=highest_ack;
  }

  parityInfo pInfo;
  memset(&pInfo,0,sizeof(pInfo));
  pInfo.ifRecovery=1;
  pInfo.subFlowID=subFlowID;
  pInfo.srcAddr=subflows_[subFlowID].tcp_->addr();
  pInfo.srcPort=subflows_[subFlowID].tcp_->port();
  pInfo.dstAddr=subflows_[subFlowID].tcp_->daddr();
  pInfo.dstPort=subflows_[subFlowID].tcp_->dport();

  if(subflows_[subFlowID].tcp_->partiyStartSeq!=-1)
  {
    maxseq=subflows_[subFlowID].tcp_->partiyStartSeq - subflows_[subFlowID].tcp_->size();
  }
  if(subflows_[subFlowID].tcp_->partiyStartSeq!=-1
    && highest_ack>=subflows_[subFlowID].tcp_->partiyStartSeq)
  {
    highest_ack=subflows_[subFlowID].tcp_->partiyStartSeq - subflows_[subFlowID].tcp_->size();
  }

  if(subflows_[subFlowID].tcp_->partiyStartSeq==1 || maxseq<1 || highest_ack<1)
  {
    fprintf(stderr, "ERROR setPInfo: partiyStartSeq=%d,maxseq=%d,highest_ack=%d\n"
      ,subflows_[subFlowID].tcp_->partiyStartSeq
      ,maxseq
      ,highest_ack);
    exit(0);
  }
   
  // printf("%lf---Node[%d](%d.%d-%d.%d): partiyStartSeq=%d,maxseq=%d,highest_ack=%d\n"
  //   ,Scheduler::instance().clock()
  //   ,here_.addr_
  //   ,pInfo.srcAddr
  //   ,pInfo.srcPort
  //   ,pInfo.dstAddr
  //   ,pInfo.dstPort
  //   ,subflows_[subFlowID].tcp_->partiyStartSeq
  //   ,maxseq
  //   ,highest_ack);

  pInfo.startSeq=highest_ack;
  pInfo.stopSeq=maxseq;
  pInfo.genID=fCodingInfo[subFlowID].currentGenID;
  pInfo.pIndex=fCodingInfo[subFlowID].currentPIndex;

  int sackedNumber=subflows_[subFlowID].tcp_->mptcp_get_sacked_packet_number(pInfo.startSeq);

  int unAckedNumber=(pInfo.stopSeq-pInfo.startSeq)/subflows_[subFlowID].tcp_->size()+1;
  int max_parity_number=unAckedNumber-sackedNumber;

  // if(max_parity_number>MAX_PARITY_NUMBER)
  // {
  //   printf("ERROR: max_parity_number=%d, MAX_PARITY_NUMBER=%d\n",max_parity_number,MAX_PARITY_NUMBER);
  //   exit(1);
  // }

  if(pInfo.pIndex+1>=max_parity_number || pInfo.pIndex+1>=MAX_PARITY_NUMBER)
  {
    fCodingInfo[subFlowID].ifRecoverFull=1;
  }
  else
  {
    fCodingInfo[subFlowID].ifRecoverFull=0;
  }

  if(pInfo.pIndex>=max_parity_number || pInfo.pIndex>=MAX_PARITY_NUMBER) 
  ////Has already generated number of loss recover packets
    ///Set ifRecovery=0 to notify out loop not generate recover packets anymore
  {
    pInfo.ifRecovery=0;    
    return pInfo;
  }


  pInfo.RorC=2; ///// Stands for coding packet

   // printf("partiyStartSeq=%d,maxseq=%d,highest_ack=%d\n"
   //  ,subflows_[subFlowID].tcp_->partiyStartSeq
   //  ,maxseq
   //  ,highest_ack);

  // printf("%lf---Node[%d](%d.%d-%d.%d)Set a pInfo: RorC=%d,genID=%d,pIndex=%d,max_parity_number=%d\n"
  //   ,Scheduler::instance().clock()
  //   ,here_.addr_
  //   ,pInfo.srcAddr
  //   ,pInfo.srcPort
  //   ,pInfo.dstAddr
  //   ,pInfo.dstPort
  //   ,pInfo.RorC
  //   ,pInfo.genID
  //   ,pInfo.pIndex
  //   ,max_parity_number
  //   );

  return pInfo;
}

parityInfo MptcpAgent::setRetranPInfo(int subFlowID)
{
  parityInfo pInfo;
  memset(&pInfo,0,sizeof(pInfo));
  pInfo.ifRecovery=1;
  pInfo.subFlowID=subFlowID;
  pInfo.srcAddr=subflows_[subFlowID].tcp_->addr();
  pInfo.srcPort=subflows_[subFlowID].tcp_->port();
  pInfo.dstAddr=subflows_[subFlowID].tcp_->daddr();
  pInfo.dstPort=subflows_[subFlowID].tcp_->dport();
  int maxseq = subflows_[subFlowID].tcp_->mptcp_get_maxseq ();
  int highest_ack = subflows_[subFlowID].tcp_->mptcp_get_highest_ack ();

  if(maxseq<1 || highest_ack<1)
  {
    fprintf(stderr, "ERROR setPInfo: maxseq=%d,highest_ack=%d\n"
      ,maxseq
      ,highest_ack);
    exit(0);
  }
   
  // if(pInfo.srcAddr==9 && pInfo.srcPort==39 && pInfo.dstAddr==37 && pInfo.dstPort==12) 
  // {
  //   printf("Before protection\n");
  //   printf("%lf---Node[%d](%d.%d-%d.%d): maxseq=%d, highest_ack=%d, currentReTranSeq=%d, ifAllProtected=%d\n"
  //     ,Scheduler::instance().clock()
  //     ,here_.addr_
  //     ,pInfo.srcAddr
  //     ,pInfo.srcPort
  //     ,pInfo.dstAddr
  //     ,pInfo.dstPort
  //     ,maxseq
  //     ,highest_ack
  //     ,fCodingInfo[subFlowID].currentReTranSeq
  //     ,subflows_[subFlowID].tcp_->ifAllProtected);
  // }

  pInfo.RorC=1; ///Stands for retrans packet

  pInfo.stopSeq=-1;////States for unused
  pInfo.genID=-1;////States for unused
  pInfo.pIndex=-1;////States for unused
  
  int start=0;
  int end=0;
  if(fCodingInfo[subFlowID].currentReTranSeq<highest_ack)
  {
    fCodingInfo[subFlowID].currentReTranSeq=highest_ack;
  }
  subflows_[subFlowID].tcp_->mptcp_get_next_hole_range(fCodingInfo[subFlowID].currentReTranSeq,start,end);

  if(start==0)
  {
    start=fCodingInfo[subFlowID].currentReTranSeq; ////Nothing in sq_
  }

  pInfo.startSeq=start;
  while(pInfo.startSeq<maxseq)
  {
    if(subflows_[subFlowID].tcp_->mptcp_check_rmt_mapping_ifParityorProtected(pInfo.startSeq))
      pInfo.startSeq=pInfo.startSeq+subflows_[subFlowID].tcp_->size();
    else
      break;
  }

  if(pInfo.startSeq>=maxseq)
  {
    pInfo.ifRecovery=0;
    fCodingInfo[subFlowID].currentReTranSeq=maxseq;
  }
  else
  {
    subflows_[subFlowID].tcp_->mptcp_set_mapping_rmt_info(pInfo.startSeq,0,1);
    int mss=subflows_[subFlowID].tcp_->size();
    for(fCodingInfo[subFlowID].currentReTranSeq=pInfo.startSeq+mss; fCodingInfo[subFlowID].currentReTranSeq<maxseq; )
    {
      if(!subflows_[subFlowID].tcp_->mptcp_check_rmt_mapping_ifParityorProtected(fCodingInfo[subFlowID].currentReTranSeq))
        break;
      else
        fCodingInfo[subFlowID].currentReTranSeq+=mss;
    }
  }
  

  if(fCodingInfo[subFlowID].currentReTranSeq>=maxseq)
    subflows_[subFlowID].tcp_->ifAllProtected=1;
  else
    subflows_[subFlowID].tcp_->ifAllProtected=0;

  // if(pInfo.srcAddr==9 && pInfo.srcPort==39 && pInfo.dstAddr==37 && pInfo.dstPort==12) 
  // {
  // printf("After protection\n");
  //   printf("%lf---Node[%d](%d.%d-%d.%d): maxseq=%d, highest_ack=%d, currentReTranSeq=%d, ifAllProtected=%d\n"
  //   ,Scheduler::instance().clock()
  //   ,here_.addr_
  //   ,pInfo.srcAddr
  //   ,pInfo.srcPort
  //   ,pInfo.dstAddr
  //   ,pInfo.dstPort
  //   ,maxseq
  //   ,highest_ack
  //   ,fCodingInfo[subFlowID].currentReTranSeq
  //   ,subflows_[subFlowID].tcp_->ifAllProtected);
  // }

  // printf("%lf--- Set a pInfo: RorC=%d,genID=%d,pIndex=%d,startSeq=%d,stopSeq=%d\n"
  //   ,Scheduler::instance().clock()
  //   ,pInfo.RorC
  //   ,pInfo.genID
  //   ,pInfo.pIndex
  //   ,pInfo.startSeq
  //   ,pInfo.stopSeq
  //   );

  return pInfo;
}

void
MptcpAgent::sendRecoveryPackets ()
{
  // if(here_.addr_==1)
  // {
  //   if(here_.port_==40 ||here_.port_==41||here_.port_==42||here_.port_==43)
  //   {
  //     printf("%lf---Node(%d.%d): sendRecoveryPackets\n"
  //       ,Scheduler::instance().clock()
  //       ,here_.addr_
  //       ,here_.port_);
  //   }
  // }

  while(1)
  {
    int worstSubFlow=findWorstSubflow();
    if(worstSubFlow==-1)
    {
      break;
    }

    int bestSubFlow=findBestSubflow();
    if(bestSubFlow==-1)
    {
      break;
    }

    // if(worstSubFlow==bestSubFlow && sub_num_>1)
    // {
    //   break;
    // }

  /*  printf("%lf---Node(%d.%d): sendRecoveryPackets\n"
        ,Scheduler::instance().clock()
        ,here_.addr_
        ,here_.port_);*/

    int mss = subflows_[bestSubFlow].tcp_->size ();
    int curseq = subflows_[bestSubFlow].tcp_->mptcp_get_curseq();
    int curSubSeqToSend=curseq+1;
    if(!subflows_[bestSubFlow].tcp_->if_can_send(curSubSeqToSend))
    {
      fprintf(stderr, "ERROR: sendAParity through sub-flow[%d], but is not allowed by window!\n",bestSubFlow);
      exit(0);
    }

    parityInfo pInfo;

    if(MAX_PARITY_NUMBER>0)//Generate parity packet
    {
      pInfo=setParityPInfo(worstSubFlow);
    }
    else///Generate retrans packet
    {
      pInfo=setRetranPInfo(worstSubFlow);
    }

    if(pInfo.ifRecovery==0)
    {
      fprintf(stderr, "%lf---Node[%d](%d.%d-%d.%d): error worst subflow found, but no protectable un-ACKed packet!\n"
        ,Scheduler::instance().clock()
        ,here_.addr_
        ,pInfo.srcAddr
        ,pInfo.srcPort
        ,pInfo.dstAddr
        ,pInfo.dstPort);
      break;
    }

    ////For print
    // subflows_[worstSubFlow].tcp_->mptcp_sq_dumlist();
      
    Tcl& tcl = Tcl::instance();
    tcl.evalf("[Simulator instance] clb-parity-number-add");
    subflows_[bestSubFlow].tcp_->incrementRecoveryNumber();
    subflows_[bestSubFlow].tcp_->printRecoveryNumber();
      
    // subflows_[bestSubFlow].tcp_->mptcp_add_mapping (mcurseq_, mss, 1, 0);
    subflows_[bestSubFlow].tcp_->mptcp_add_rmt_mapping (1, 0); //CG add

    subflows_[bestSubFlow].tcp_->parity_sendmsg (mss,pInfo);
    // mcurseq_ += mss;
  }
}

/*Old cost of draining speed estimation one*/
// int MptcpAgent::findBestSubflow ()
// {
//   int k=rand();
//   int flowID=-1;
//   double minCost=100;
//   for (int i = 0; i < sub_num_; i++) {  ///Original
//     int j=(k+i)%sub_num_;
//     // if(!subflows_[j].tcp_->mptcp_if_in_recover())
//     // {
//     //   continue;
//     // }


//     int mss = subflows_[j].tcp_->size ();
//     int curseq = subflows_[j].tcp_->mptcp_get_curseq();
//     int curSubSeqToSend=curseq+1;
//     if(!subflows_[j].tcp_->if_can_send(curSubSeqToSend))
//     {
//       continue;
//     }
//     if(subflows_[j].tcp_->mptcp_if_in_recover() && sub_num_==1)
//     {
//       continue;
//     }

//     double drainSpeed=0;
//     if(subflows_[j].tcp_->mptcp_get_srtt ()!=0)
//     {
//       drainSpeed= subflows_[j].tcp_->mptcp_get_window()/subflows_[j].tcp_->mptcp_get_srtt ();
//     }
//     int maxseq = subflows_[j].tcp_->mptcp_get_maxseq ();
//     int highest_ack = subflows_[j].tcp_->mptcp_get_highest_ack ();
//     int packetsInFlight=(maxseq-highest_ack)/mss;  ///////Naive logic now

//     if(packetsInFlight<0 || maxseq<1 || highest_ack<1)
//     {
//       printf("ERROR findBestSubflow: packetsInFlight=%d,maxseq=%d,highest_ack=%d"
//         ,packetsInFlight
//         ,maxseq
//         ,highest_ack);
//       exit(0);
//     }

//     ////Check me!
//     double cost;
//     if(subflows_[j].tcp_->mptcp_get_window()<=packetsInFlight)
//     {
//       cost=99;
//     }
//     else
//     {
//       cost=1/(subflows_[j].tcp_->mptcp_get_window()-packetsInFlight);
//     }
//     ////   if no drainspeed estimation yet, let cost=1/remain_window

//     if(drainSpeed>0)
//     {
//       cost=(packetsInFlight+1)/drainSpeed;
//     }

//     // printf("subflows_[%d]: bestCost=%f, packetsInFlight=%d, drainSpeed=%f\n"
//     //   ,j,cost,packetsInFlight,drainSpeed);

//     if(cost<minCost)
//     {
//       minCost=cost;
//       flowID=j;
//     }

//   }
//   // printf("flowID=%d\n",flowID);
//   // printf("%lf---NODE[%d]: findBestSubflow=%d\n"
//   //         ,Scheduler::instance().clock()
//   //         ,here_.addr_
//   //         ,flowID);
//   return flowID;
// }

/*New lossrate one*/
int MptcpAgent::findBestSubflow ()
{
  int k=rand();
  int bestFlowID=-1;
  long long int bestcost=0;
  long long int cost;
  int in_flight,total_retrans,total_trans,packet_sent_from_last_retran;
  for (int i = 0; i < sub_num_; i++) {  ///Original
    int j=(k+i)%sub_num_;
    
    int mss = subflows_[j].tcp_->size ();
    int curseq = subflows_[j].tcp_->mptcp_get_curseq();
    int curSubSeqToSend=curseq+1;
    int maxseq = subflows_[j].tcp_->mptcp_get_maxseq ();
    int highest_ack = subflows_[j].tcp_->mptcp_get_highest_ack ();

    if(!subflows_[j].tcp_->if_can_send(curSubSeqToSend))
    {
      continue;
    }
    if(subflows_[j].tcp_->mptcp_if_in_recover() && sub_num_==1)
    {
      continue;
    }

    in_flight=(maxseq-highest_ack)/mss;  ///////Naive logic now
    total_retrans = subflows_[j].tcp_->mptcp_get_nrexmitpack();
    total_trans = subflows_[j].tcp_->mptcp_get_ndatapack();
    packet_sent_from_last_retran  = subflows_[j].tcp_->mptcp_get_ndatapack_from_last_retran();

    if(in_flight<0 || maxseq<1 || highest_ack<1)
    {
      fprintf(stderr,"ERROR findBestSubflow: in_flight=%d,maxseq=%d,highest_ack=%d"
        ,in_flight
        ,maxseq
        ,highest_ack);
      exit(0);
    }


    // printf("subflows_[%d]: flight = %d cwnd = %f total_retrans = %d total_trans = %d packet_sent_from_last_retran = %d rtt =%f\n"
    //   ,j,in_flight,subflows_[j].tcp_->mptcp_get_cwnd(),total_retrans,total_trans,packet_sent_from_last_retran,subflows_[j].tcp_->mptcp_get_srtt ());

    if(packet_sent_from_last_retran==0 || total_trans==0 || subflows_[j].tcp_->mptcp_get_srtt ()==0)
      cost=DEVISION_SCALER;
    else if(total_retrans==0)
      cost=0-1/(subflows_[j].tcp_->mptcp_get_srtt ());
    else
      cost = ((long long int)total_retrans*DEVISION_SCALER/(long long int)total_trans)+(1*DEVISION_SCALER/(long long int)packet_sent_from_last_retran);
    
    if(bestcost ==0){
      bestcost = cost;
      bestFlowID = j;
    }
    if(cost<bestcost){
      bestcost = cost;
      bestFlowID = j;
    }

    // printf("subflows_[%d]: cost =%lli bestcost=%lli\n"
    //   ,j,cost,bestcost);
  }
  // printf("bestFlowID=%d\n",bestFlowID);
  // printf("%lf---NODE[%d]: findBestSubflow=%d\n"
  //         ,Scheduler::instance().clock()
  //         ,here_.addr_
  //         ,bestFlowID);
  return bestFlowID;
}

/*Old cost of draining speed estimation one*/
// int MptcpAgent::findWorstSubflow ()
// {
//   // printf("%lf---NODE[%d]: findWorstSubflow\n"
//   //         ,Scheduler::instance().clock()
//   //         ,here_.addr_);
//   int k=rand();
//   int flowID=-1;
//   double maxCost=0;
//   for (int i = 0; i < sub_num_; i++) {  ///Original
//     int j=(k+i)%sub_num_;
//     int mss = subflows_[j].tcp_->size ();

//     if(subflows_[j].tcp_->partiyStartSeq==1)
//     {
//       continue;
//     }

//     if(fCodingInfo[j].ifRecoverFull==1)
//     {
//       continue;
//     }

//     int maxseq = subflows_[j].tcp_->mptcp_get_maxseq ();
//     int highest_ack = subflows_[j].tcp_->mptcp_get_highest_ack ();
//     if(subflows_[j].tcp_->partiyStartSeq!=-1)
//     {
//       maxseq=subflows_[j].tcp_->partiyStartSeq;
//     }
//     if(subflows_[j].tcp_->partiyStartSeq!=-1
//       && highest_ack>=subflows_[j].tcp_->partiyStartSeq)
//     {
//       highest_ack=subflows_[j].tcp_->partiyStartSeq;
//     }

//     if(subflows_[j].tcp_->partiyStartSeq==1 || maxseq<1 || highest_ack<1)
//     {
//       printf("ERROR findWorstSubflow: partiyStartSeq=%d,maxseq=%d,highest_ack=%d"
//         ,subflows_[j].tcp_->partiyStartSeq
//         ,maxseq
//         ,highest_ack);
//       exit(0);
//     }
    
//     if(highest_ack>=maxseq)
//     {
//       continue;
//     }

//     // printf("partiyStartSeq=%d,maxseq=%d,highest_ack=%d\n"
//     // ,subflows_[j].tcp_->partiyStartSeq
//     // ,maxseq
//     // ,highest_ack);

//     double drainSpeed=0;
//     if(subflows_[j].tcp_->mptcp_get_srtt ()!=0)
//     {
//       drainSpeed= subflows_[j].tcp_->mptcp_get_window()/subflows_[j].tcp_->mptcp_get_srtt ();
//     }
//     maxseq = subflows_[j].tcp_->mptcp_get_maxseq ();
//     highest_ack = subflows_[j].tcp_->mptcp_get_highest_ack ();
//     int packetsInFlight=(maxseq-highest_ack)/mss;  ///////Naive logic now
//     ////Check me!
//     double cost=packetsInFlight/subflows_[j].tcp_->mptcp_get_window();////   if no drainspeed estimation yet

//     if(drainSpeed>0)
//     {
//       cost=packetsInFlight/drainSpeed;
//     }

//     // printf("subflows_[%d]: worstCost=%f, packetsInFlight=%d, drainSpeed=%f\n"
//     //   ,j,cost,packetsInFlight,drainSpeed);

//     if(cost>maxCost)
//     {
//       maxCost=cost;
//       flowID=j;
//     }

//   }
//   // printf("flowID=%d\n",flowID);
//   // printf("%lf---NODE[%d]: findWorstSubflow=%d\n"
//   //         ,Scheduler::instance().clock()
//   //         ,here_.addr_
//   //         ,flowID);
//   return flowID;
// }

/*New lossrate one*/
int MptcpAgent::findWorstSubflow ()
{
  // printf("%lf---NODE[%d]: findWorstSubflow\n"
  //         ,Scheduler::instance().clock()
  //         ,here_.addr_);
  int k=rand();
  int worstFlowID=-1;
  int in_flight,total_retrans,total_trans,packet_sent_from_last_retran;
  long long int worstcost = 0;
  long long int cost;
  for (int i = 0; i < sub_num_; i++) {  ///Original
    int j=(k+i)%sub_num_;
    int mss = subflows_[j].tcp_->size ();

    if(subflows_[j].tcp_->ifAllProtected==1)
    {
      continue;
    }

    int maxseq = subflows_[j].tcp_->mptcp_get_maxseq ();
    int highest_ack = subflows_[j].tcp_->mptcp_get_highest_ack ();
 
    if(highest_ack>=maxseq)
    {
      continue;
    }

    in_flight=(maxseq-highest_ack)/mss;  ///////Naive logic now
    total_retrans = subflows_[j].tcp_->mptcp_get_nrexmitpack();
    total_trans = subflows_[j].tcp_->mptcp_get_ndatapack();
    packet_sent_from_last_retran  = subflows_[j].tcp_->mptcp_get_ndatapack_from_last_retran();

    // printf("subflows_[%d]: flight = %d cwnd = %f total_retrans = %d total_trans = %d packet_sent_from_last_retran = %d rtt =%f\n"
    //   ,j,in_flight,subflows_[j].tcp_->mptcp_get_cwnd(),total_retrans,total_trans,packet_sent_from_last_retran,subflows_[j].tcp_->mptcp_get_srtt ());

    if(subflows_[j].tcp_->mptcp_get_srtt ()==0)
      cost=DEVISION_SCALER;
    else if(total_retrans==0)
      cost=0-1/subflows_[j].tcp_->mptcp_get_srtt();
    else
      cost = ((long long int)total_retrans*DEVISION_SCALER/(long long int)total_trans)+(1*DEVISION_SCALER/(long long int)packet_sent_from_last_retran);
    
    if(worstcost ==0){
      worstcost = cost;
      worstFlowID = j;
    }
    if(cost>worstcost){
      worstcost = cost;
      worstFlowID = j;
    }

     // printf("subflows_[%d]: cost =%lli worstcost=%lli\n"
     //  ,j,cost,worstcost);

  }
   // printf("worstFlowID=%d\n",worstFlowID);
   // printf("%lf---NODE[%d]: findWorstSubflow=%d\n"
   //         ,Scheduler::instance().clock()
   //         ,here_.addr_
   //         ,worstFlowID);
  return worstFlowID;
}
