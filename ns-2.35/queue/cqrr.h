//
// Author:    Jae Chung
// File:      dtrr-queue.h
// Written:   07/19/99 (for ns-2.1b4a)
// Modifed:   10/14/01 (for ns-2.1b8a)
//

#ifndef ns_cqrr_h
#define ns_cqrr_h


#include <string.h>
#include "queue.h"
#include "address.h"


class CqRrQueue : public Queue {
 public:
         CqRrQueue() { 
		deq_turn_ = 0;
//		ifInitiate = 0;
		bind("portnumber_", &portnumber_);
		bind("limit_", &qlim_);
		bind("srcID", &last_hop);
		bind("dstID", &next_hop);
		bind_bool("drop_rand_", &drop_rand_); 
		bind_bool("drop_front_", &drop_front_);
		bind_bool("drop_tail_", &drop_tail_);
		ifInitiate=0;
	}
	int length();

 protected:
 	int portnumber_; /////input port number
 	int command(int argc, const char*const* argv); 
     	void enque(Packet*);
	 Packet* deque();
	 void CqRrQueueInit();

	 int q_cq_limit_;
	 PacketQueue * q_;   // First  FIFO queue
	 int deq_turn_;      // 
	int qlim_;
	 nsaddr_t * inputState;////store information of the corressponding relation from input address to q_[x]
	int last_hop;
	int next_hop;

	int ifInitiate;

	int drop_rand_;
	int drop_front_;
	int drop_tail_;
	Packet* pickPacketToDrop(PacketQueue qTmp);

	int ecn_threshold_;///CG add
};

#endif
