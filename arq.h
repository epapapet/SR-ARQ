#include "connector.h"
#include "ranvar.h"
class ARQSRTx;
enum ARQSRStatus {IDLE,SENT,ACKED,RTX,DROP};

class ARQSRHandler : public Handler {
 public:
	ARQSRHandler(ARQSRTx& arq) : arq_tx_(arq) {};
	void handle(Event*);
 private:
	ARQSRTx& arq_tx_;
};

class ARQSRTx : public Connector {
 public:
	ARQSRTx();
	void recv(Packet*, Handler*);
	void nack(int rcv_sn, int rcv_uid);
	void ack(int rcv_sn, int rcv_uid);
	void resume();
	int command(int argc, const char*const* argv);
 protected:
	ARQSRHandler arqh_;
	Handler* handler_;

	Packet *pending; //used for storing a packet from queue that finds the channel blocked_

	int wnd_;  //window size
	int sn_cnt; //the total count of used sequence numbers
	int retry_limit_; //maximum number of retransmissions allowed for each frame

	Packet **pkt_buf; //buffer used for storing frames under transmission (maximum size of wnd_)
	ARQSRStatus *status; //the status of each frame under transmission
	int *num_rtxs; //number of retransmisions for each frame under transmission
	int *pkt_uids; //used for debugging purposes

	int blocked_; //switch set to 1 when Tx engaged in transmiting a frame, 0 otherwise
	int last_acked_sq_; //sequence number of last acked frame
	int most_recent_sq_; //sequence number of most recent frame to be sent
	int num_pending_retrans_; //number of frames needed to be retransmitted (after the first attempt)

	int findpos_retrans();
	void reset_lastacked();

};

class ARQSRRx : public Connector {
 public:
	ARQSRRx();
	int command(int argc, const char*const* argv);
	virtual void recv(Packet*, Handler*) = 0;
 protected:
	ARQSRTx* arq_tx_;
	double delay_; //delay for returning feedback
};

class ARQSRAcker : public ARQSRRx {
 public:
	ARQSRAcker();
	virtual void handle(Event*);
	void recv(Packet*, Handler*);
	int command(int argc, const char*const* argv);
	void print_stats();
 protected:
	int wnd_;  //window size
	int sn_cnt; //the total count of used sequence numbers
	Packet **pkt_buf; //buffer used for storing packets arriving out of order
	int last_fwd_sn_; //sequence number of the last frame forwarded to the upper layer
	int most_recent_acked; //sequence number of the last frame for which an ack has been sent
	int delivered_pkts; //the total number of pkts delivered to the receiver's upper layer

	RandomVariable *ranvar_; //a random variable for generating errors in ACK delivery
	double err_rate; //the rate of errors in ACK delivery

	void deliver_frames(int steps, bool mindgaps, Handler *h);
};

class ARQSRNacker : public ARQSRRx {
 public:
	ARQSRNacker() {};
	virtual void handle(Event*);
	void recv(Packet*, Handler*);
};

class ACKEvent : public Event {
 public:
	int ACK_sn;
	int ACK_uid;
};