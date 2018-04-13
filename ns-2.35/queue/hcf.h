//
// Author:    Jae Chung
// File:      dtrr-queue.h
// Written:   07/19/99 (for ns-2.1b4a)
// Modifed:   10/14/01 (for ns-2.1b8a)
//

#ifndef ns_hcf_h
#define ns_hcf_h


#include <string.h>
#include "queue.h"
#include "address.h"


class HCFQueue : public Queue {
 public:
         HCFQueue() {
		bind("limit_", &qlim_);
		bind("srcID", &last_hop);
		bind("dstID", &next_hop);
		bind("hash_bin_",&hash_bin);
		bind("credits_",&credits_);
		HQ=new PacketQueue;
		LQ=new PacketQueue;
		ifInitiate=0;
	}
	int length();

 protected:
 	int command(int argc, const char*const* argv); 
	void enque(Packet*);
	Packet* deque();
	int q_hcf_limit_;
	PacketQueue * HQ;
	PacketQueue * LQ;   // First  FIFO queue
	int qlim_;
	int last_hop;
	int next_hop;

	int ifInitiate;
	time_t seed;
	void init();
	int hash_bin;
	int * c;
	int credits_;
	void queueSwap();
	int hashFunc(long int srcIP,long int dstIP,long int srcPort,long int dstPort);

	int ecn_threshold_;///CG add
};

#endif
