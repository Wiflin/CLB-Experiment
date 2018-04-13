#ifndef ns_tcp_ga_full_h
#define ns_tcp_ga_full_h

#include "tcp-full.h"

class GAFullTcpAgent;

class GAReactiveTimer : public TimerHandler {
public:
  GAReactiveTimer(GAFullTcpAgent *a) : TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  GAFullTcpAgent *a_;
};

class GACorrectiveTimer : public TimerHandler {
public:
  GACorrectiveTimer(GAFullTcpAgent *a) : TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  GAFullTcpAgent *a_;
};

class GADelaySendTimer : public TimerHandler {
public:
  GADelaySendTimer(GAFullTcpAgent *a) : 
  TimerHandler(), a_(a),pkt_(NULL) { }
  void setPacket(Packet* pkt)
  {
    pkt_=pkt;
  }

protected:
  virtual void expire(Event *);
  GAFullTcpAgent *a_;
  Packet* pkt_;
};

class GAFullTcpAgent:public SackFullTcpAgent
{
public:
  GAFullTcpAgent ():ga_pto_timer_(this)
  ,ga_encode_timer(this),sendTimer_(this)
  {
    ecmpHashKey=rand();//CG add
    RepFlowHashKey_=rand();
    GA_init(); 
  }
  virtual int command(int argc, const char*const* argv);
  void pto_timeout()///CG add for GA reactive
  {
    handle_pto();///CG add
  } 
  void ga_encode_timeout();///CG add for GA corrective


protected:
  void sendpacket(int seqno, int ackno, int pflags, int datalen, int reason, Packet *p);
  virtual void recv(Packet *pkt, Handler *hdl);
  void cancel_timers();   // cancel all timers

  ///GA Reactive TCP
  void GA_init(); 

  int ifGAReactive_;
  GAReactiveTimer ga_pto_timer_;
  void schedule_pto();
  void handle_pto();
  int consecutivePTO;

  int ifGACorrective_;
  int ifGACorrectiveSink_;
  int MaxBLockNumber;
  int numberInCurrentBlock;
  GACorrectiveTimer ga_encode_timer;
  void ga_encode(int seqno, int datalen);
  CodingINFO encodeBuff_;
  void generate_GA_Parity();
  GADelaySendTimer sendTimer_;
  int process_coding_packet(CodingINFO cInfo);
  ///1 means decodable, 0 means not
  int ifFinshed;///HACK

  int ifGAProactive_;
  int ifGAProactiveSink_;

  virtual int ifEarlyRetransmit();///CG add
};



#endif
