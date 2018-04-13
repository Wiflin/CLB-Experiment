#ifndef ns_rmptcp_h
#define ns_rmptcp_h

#define IF_PRINT_DEBUG 0

#define DEVISION_SCALER ((unsigned int)0xFFFF)

struct parityInfo
{
	int ifRecovery;
	int subFlowID;
	int srcAddr;
	int srcPort;
	int dstAddr;
	int dstPort;

	int RorC; ////1 for R. 2 for C

	int genID;
	int pIndex;
	int startSeq;
	int stopSeq;

	int ifRestored;

	parityInfo& operator=(const parityInfo& s)//重载运算符  
	{  
	    this->ifRecovery=s.ifRecovery;
	    this->subFlowID=s.subFlowID;
	    this->genID=s.genID;
	    this->pIndex=s.pIndex;
	    this->startSeq=s.startSeq;
	    this->stopSeq=s.stopSeq;
	    this->ifRestored=s.ifRestored;
	    this->srcAddr=s.srcAddr;
	    this->srcPort=s.srcPort;
	    this->dstAddr=s.dstAddr;
	    this->dstPort=s.dstPort;

	    this->RorC=s.RorC;
	    return *this;
	}  
};

struct parity_seq_mapping
{
	int seqno;
	parityInfo pInfo;

	parity_seq_mapping& operator=(const parity_seq_mapping& s)//重载运算符  
	{  
	    this->seqno=s.seqno;
	    this->pInfo=s.pInfo;
	    return *this;
	} 
};

struct flowCodingInfo
{
	int currentGenID;
	int currentPIndex;
	int currentStartSeq;

	int currentReTranSeq;
	int ifRecoverFull;////1 means all doubtful packets has been sent
	///Otherwise 0
};

struct rmt_info_mapping
{
  int seqno;
  int ifParity;
  int ifProtected;
};



struct decodeBufferInfo
{
	int ifValid;
	int genID;
	int receivedParityNumber;
	int *parityBitMap;
	int startSeq;
	int stopSeq;
};


#endif