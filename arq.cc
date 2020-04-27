#include "arq.h"
#include "packet.h"

static class ARQSRTxClass: public TclClass {
 public:
	ARQSRTxClass() : TclClass("ARQSRTx") {}
	TclObject* create(int, const char*const*) {
		return (new ARQSRTx);
	}
} class_arq_sr_tx;

static class ARQSRAckerClass: public TclClass {
 public:
	ARQSRAckerClass() : TclClass("ARQSRAcker") {}
	TclObject* create(int, const char*const*) {
		return (new ARQSRAcker);
	}
} class_arq_sr_acker;

static class ARQSRNackerClass: public TclClass {
 public:
	ARQSRNackerClass() : TclClass("ARQSRNacker") {}
	TclObject* create(int, const char*const*) {
		return (new ARQSRNacker);
	}
} class_arq_sr_nacker;

void ARQSRHandler::handle(Event* e)
{
	arq_tx_.resume();
}


//-------------------------------------------ARQSRRx--------------------------------------------//
//--------------------------------------------------------------------------------------------//
ARQSRTx::ARQSRTx() : arqh_(*this)
{

	blocked_ = 0;
	last_acked_sq_ = -1;
	most_recent_sq_ = 0;
	num_pending_retrans_ = 0;

	pending = NULL;

	handler_ = 0;
	wnd_ = 0;
	sn_cnt = 0;
	retry_limit_ = 0;
	bind("retry_limit_", &retry_limit_);

	start_time = -1; //time when 1st packet arrived at ARQTx::recv
	packets_sent = 0; //unique packets sent
}

int ARQSRTx::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	if (argc == 3) {
		if (strcmp(argv[1], "setup-wnd") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot setup NULL wnd\n");
				return(TCL_ERROR);
			}
			wnd_ = atoi(argv[2]);
			sn_cnt = 4 * wnd_;
			pkt_buf = new Packet* [wnd_];
			status = new ARQSRStatus[wnd_];
			num_rtxs = new int[wnd_];
			pkt_uids = new int[wnd_];
			pkt_tx_start = new double[wnd_]; 
			for(int i=0; i<wnd_; i++){ pkt_buf[i] = NULL; status[i] = IDLE; num_rtxs[i] = 0; pkt_uids[i]=-1; pkt_tx_start[i]=-1; }
			return(TCL_OK);
		}
	} return Connector::command(argc, argv);
}


void ARQSRTx::recv(Packet* p, Handler* h)
{

	//This procedure is invoked by the queue_ (i.e., output queue) to deliver a message to ARQSRTx
	//The check whether the current window exceeds or not wnd_ has already be done at this point
	
	if(last_acked_sq_ == -1 && most_recent_sq_ == 0 && start_time == -1){ //first packet to be received will trigger the clock
		start_time = Scheduler::instance().clock();
	}

	//Sanity checks---------//
	if (&arqh_==0) {
		fprintf(stderr, "Error at ARQSRTx::recv, Cannot transmit when &arqh_ is Null.\n");
		abort();
	}
	if (pending) {
		fprintf(stderr, "Error at ARQSRTx::recv, Tx should not have a pending frame when recv is called.\n");
		abort();
	}
	if (status[most_recent_sq_%wnd_] != IDLE) {
		fprintf(stderr, "Error at ARQSRTx::recv, storing position should be in IDLE mode.\n");
		abort();
	}
	//---------------------//
	//Initialization-------//
	if (h != 0) handler_ = h;
	//--------------------//

	if (blocked_) { pending = p; return; }

	hdr_cmn *ch = HDR_CMN(p);
	ch-> opt_num_forwards_ = most_recent_sq_;
	pkt_buf[most_recent_sq_%wnd_] = p;
	num_rtxs[most_recent_sq_%wnd_] = 0;
	pkt_uids[most_recent_sq_%wnd_] = ch->uid();
	status[most_recent_sq_%wnd_] = SENT;

	packets_sent += 1;
	pkt_tx_start[most_recent_sq_%wnd_] = Scheduler::instance().clock(); //retransmitted pkts are not sent through recv(), so this is a new pkt

	most_recent_sq_ = (most_recent_sq_+1)%sn_cnt;

	blocked_ = 1;
	send(p,&arqh_);

}

void ARQSRTx::ack(int rcv_sn, int rcv_uid)
{

	//Sanity checks--------//
	if (status[rcv_sn%wnd_] != SENT) {
		fprintf(stderr,"Error at ARQSRTx::ack, an ACK is received when the status is not SENT.\n");
		abort();
	}
	if (handler_ == 0) {
		fprintf(stderr,"Error at ARQSRTx::ack, handler_ is null.\n");
		abort();
	}
	//--------------------//

	status[rcv_sn%wnd_] = ACKED;
	if (rcv_sn%wnd_ == ((last_acked_sq_ + 1)%sn_cnt)%wnd_) {//acked frame is next in order
		reset_lastacked();  // check whether the active window should advance
		if (!blocked_) handler_->handle(0); //if ARQSRTx is not transmiting ask queue_ to deliver next packet
	}

}

void ARQSRTx::nack(int rcv_sn, int rcv_uid)
{

	//Sanity checks--------//
	if (status[rcv_sn%wnd_] != SENT) {
		fprintf(stderr,"Error at ARQSRTx::nack, a NACK is received when the status is not SENT.\n");
		abort();
	}
	if (handler_ == 0) {
		fprintf(stderr,"Error at ARQSRTx::nack, handler_ is null\n");
		abort();
	}
	if (&arqh_==0) {
		fprintf(stderr, "Error at ARQSRTx::nack, Cannot transmit when &arqh_ is Null.\n");
		abort();
	}
	//--------------------//

	if( num_rtxs[rcv_sn%wnd_] < retry_limit_) { //packet shoud be retransmitted

		status[rcv_sn%wnd_] = RTX;
		if (!blocked_){ //if ARQSRTx is available go on with retransmision
			blocked_ = 1;
			num_rtxs[rcv_sn%wnd_]++;
			status[rcv_sn%wnd_] = SENT;
			//TO DO: here we should add a dignostic before using pkt_buf[rcv_sn%wnd_] in order to avoid using a null pointer
			send(pkt_buf[rcv_sn%wnd_],&arqh_);
		} else {
			num_pending_retrans_++;
		}

	} else {//packet should be dropped

		status[rcv_sn%wnd_] = DROP;
		drop(pkt_buf[rcv_sn%wnd_]);
		if (rcv_sn%wnd_ == ((last_acked_sq_ + 1)%sn_cnt)%wnd_){
			reset_lastacked(); //droped frame is next in order so check whether the active window should advance
			if(!blocked_) handler_->handle(0); //if ARQSRTx is not transmiting ask queue_ to deliver next packet
		}
	}
}

void ARQSRTx::resume()
{
	//This is the procedure invoked by link_ when a transmission is completed, i.e., it is invoked T secs after ARQSRTx executes send() where T equals transmission_delay.
	blocked_ = 0;

	if (pending){
		Packet *fwpkt = pending;
		pending = NULL;
		recv(fwpkt, handler_);

	} else if (num_pending_retrans_ > 0) {//if there exist packets not ACKed that need to be retransmitted

		int runner_ = findpos_retrans();
		num_rtxs[runner_]++;
		status[runner_] = SENT;
		num_pending_retrans_--;
		blocked_ = 1;
		send(pkt_buf[runner_],&arqh_);

    } else {//there are no pending retransmision, check whether it is possible to send a new packet

		//TO DO: here we should add a diagnostic to confirm that current_wnd_ never exceeds wnd_ or alternatively check that always last_acked_sq_ < most_recent_sq_
		//int current_wnd_ = (most_recent_sq_ > last_acked_sq_) ? (most_recent_sq_ - 1 - last_acked_sq_) : (sn_cnt - last_acked_sq_ + most_recent_sq_ - 1);
		int current_wnd_ = (most_recent_sq_ - last_acked_sq_ + sn_cnt)%sn_cnt;
		if (current_wnd_ <= wnd_) {
			handler_->handle(0); //ask queue_ to deliver next packet
		}

	}
}

int ARQSRTx::findpos_retrans()
{
	//----------DEBUG------------------//
	if((last_acked_sq_+1)%sn_cnt == most_recent_sq_) {
		fprintf(stderr, "Error at ARQSRTx::findpos_retrans, no packet is waiting (stored) for transmission.\n");
		abort();
	}
	//---------------------------------//

	bool found = FALSE;
	int runner_ = ((last_acked_sq_+1)%sn_cnt)%wnd_;

	do {
		if (status[runner_] == RTX) {
			found = TRUE;
			break;
		}
		runner_ = (runner_+1)%wnd_;
	} while (runner_ != (most_recent_sq_%wnd_));
	//----------DEBUG------------------//
	if (!found){
		fprintf(stderr, "Error at ARQSRTx::findpos_retrans, packet with RTX status NOT FOUND.\n");
		abort();
	}
	//---------------------------------//
	return runner_;

}

void ARQSRTx::reset_lastacked()
{

	if((last_acked_sq_+1)%sn_cnt == most_recent_sq_) return; //no need to reset last_ack because there is no packet stored (MOST RECENT - LAST ACKED = 1)

	int runner_ = ((last_acked_sq_+1)%sn_cnt)%wnd_;
	do {

		if ((status[runner_] == RTX) || (status[runner_] == SENT)) break;

		if ((pkt_buf[runner_]) && (status[runner_] != DROP)){
			//delete pkt_buf[runner_];
			Packet::free(pkt_buf[runner_]);
			pkt_buf[runner_] = NULL;
		}

		status[runner_] = IDLE;
		num_rtxs[runner_] = 0;

		pkt_uids[runner_] = -1;
		pkt_tx_start[runner_] = -1;
		last_acked_sq_ = (last_acked_sq_ + 1)%sn_cnt;

		runner_ = (runner_ + 1)%wnd_;

	} while (runner_ != (most_recent_sq_%wnd_));
}


//--------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------//





//-------------------------------------------ARQSRRx--------------------------------------------//
//--------------------------------------------------------------------------------------------//
ARQSRRx::ARQSRRx()
{
	arq_tx_=0;
	delay_ = 0;
	bind("delay_", &delay_);
}


int ARQSRRx::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	if (argc == 3) {
		if (strcmp(argv[1], "attach-ARQSRTx") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot attach NULL ARQSRTx\n");
				return(TCL_ERROR);
			}
			arq_tx_ = (ARQSRTx*)TclObject::lookup(argv[2]);
			return(TCL_OK);
		}
	} return Connector::command(argc, argv);
}


ARQSRAcker::ARQSRAcker()
{
	wnd_ = 0;
	sn_cnt = 0;
	last_fwd_sn_ = -1;
	most_recent_acked = 0;
	ranvar_ = NULL;
	err_rate = 0.0;

	finish_time = 0; //time when the last pkt was delivered to the receiver's upper layer, used to calculate throughput
	delivered_pkts = 0; //the total number of pkts delivered to the receiver's upper layer
	delivered_data = 0; //the total number of bytes delivered to the receiver's upper layer
	sum_of_delay = 0; //sum of delays for every packet delivered, used to calculate average delay

}



int ARQSRAcker::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	if (argc == 2) {
		if (strcmp(argv[1], "print-stats") == 0) {
			print_stats(); //used for collecting statistics, along with the corresponding tcl command
			return(TCL_OK);
		}
	} else if (argc == 3) {
		if (strcmp(argv[1], "setup-wnd") == 0) {
			if (*argv[2] == '0') {
				tcl.resultf("Cannot setup NULL wnd\n");
				return(TCL_ERROR);
			}
			wnd_ = atoi(argv[2]);
			sn_cnt = 4 * wnd_;
			pkt_buf = new Packet* [wnd_];
			for(int i=0; i<wnd_; i++){ pkt_buf[i] = NULL; }
			return(TCL_OK);
		}
		if (strcmp(argv[1], "ranvar") == 0) {
			ranvar_ = (RandomVariable*) TclObject::lookup(argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1], "set-err") == 0) {
			if (atof(argv[2]) > 1) {
				tcl.resultf("Cannot set error more than 1.\n");
				return(TCL_ERROR);
			}
			if (atof(argv[2]) < 0) {
				tcl.resultf("Cannot set error less than 0.\n");
				return(TCL_ERROR);
			}
			err_rate = atof(argv[2]);
			return(TCL_OK);
		}
	} return ARQSRRx::command(argc, argv);
}



void ARQSRAcker::recv(Packet* p, Handler* h)
{

	SRACKEvent *new_ACKEvent = new SRACKEvent();
	hdr_cmn *ch = HDR_CMN(p);
	int seq_num = ch->opt_num_forwards_;
	int pkt_uid = ch->uid();


	int fw_dis = (seq_num - last_fwd_sn_ + sn_cnt)%sn_cnt;
	int fw_width = (most_recent_acked - last_fwd_sn_ + sn_cnt)%sn_cnt;
	bool within_fww = ((fw_dis <= wnd_) && (fw_dis > 0)) ? (true) : (false);

	int bw_dis = (most_recent_acked - seq_num + sn_cnt)%sn_cnt;
	bool within_bww = (bw_dis < wnd_) ? (true) : (false);


	int nxt_seq = (last_fwd_sn_ + 1)%sn_cnt;

	//There are two reasons for a frame arriving not in the forward window:
	//1) Corresponding ACK was lost,
	//2) One or more frames are dropped by ARQSRTx (exceeds retrans limit) so it moves on to transmit new frames. ARQSRRx is not informed to advance its window and receives brand new frames that may consider as old ones
	//The problem is that there is a probability that a new frame (tranmitted after ARQSRTx drops an old frame) is mistaken as a retransmission. Monitoring time of arrival or more SNs can help minimize the probability.

	if (within_fww){//frame belongs to the forward window

		if(seq_num == nxt_seq){//frame arrives in order, ack and send frame and finally check if other frames are now in order

			pkt_buf[nxt_seq%wnd_] = NULL;
			last_fwd_sn_ = (last_fwd_sn_+1)%sn_cnt;

			Packet *pnew = p->copy();
			finish_time = Scheduler::instance().clock();
			delivered_data += ch->size_;
			delivered_pkts++;
			sum_of_delay = sum_of_delay + Scheduler::instance().clock() - arq_tx_->get_pkt_tx_start(nxt_seq);

			send(pnew,h);
			deliver_frames((wnd_-1), true, h); //check whether other frames are now in order and should be delivered

		} else {//a new frame arrives out of order, should be ACKEd and stored in the appropriate position

			if (pkt_buf[seq_num%wnd_] == NULL){

				pkt_buf[seq_num%wnd_] = p;

			} else { // the frame has already been received, thus the ACK was lost and nothing needs to be done beyond ACKing the frame

				//Sanity-------------------------//
				hdr_cmn *chold = HDR_CMN(pkt_buf[seq_num%wnd_]);
				hdr_cmn *chnew = HDR_CMN(p);
				if (chold->uid() != chnew->uid()){
					fprintf(stderr, "Error at ARQSRRx::handle, received retransmission has not the same uid.\n");
					abort();
				}
				//-------------------------------//

			}

		}

		if (fw_dis > fw_width) most_recent_acked = seq_num;

	} else if (within_bww && !within_fww) {//frame belongs to the backward window, so it is a retransmitted frame (due to loss of ACK)

		//ignore the packet and acknowledge
		//delete p;
		//ATTTENTION: we should provide enough separation between bww and fww so that we can identify new frames, i.e., being able to enter the else clause

	} else {//frame arrives out of order because ARQSRTx has exceeded the retransmission limit and as a result has dropped one or more frames

		int first_st = ((seq_num - wnd_ + sn_cnt)%sn_cnt - last_fwd_sn_ + sn_cnt)%wnd_;

		deliver_frames(first_st, false, h);

		pkt_buf[seq_num%wnd_] = p;
		deliver_frames(wnd_, true, h);
		most_recent_acked = seq_num;

	}



	//-----Schedule ACK----------------//
	Event *ack_e;
	new_ACKEvent->ACK_sn = seq_num;
	new_ACKEvent->ACK_uid = pkt_uid;
	ack_e = (Event *)new_ACKEvent;

	if (delay_ > 0)
		Scheduler::instance().schedule(this, ack_e, delay_);
	else
		handle(ack_e);
    //---------------------------------//
}


void ARQSRAcker::deliver_frames(int steps, bool mindgaps, Handler *h)
{

	int count = 0;

	while (count < steps){

		if ((pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_] == NULL)&&(mindgaps)) break;

		if(pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_]) {
			Packet *pnew = pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_]->copy();
			finish_time = Scheduler::instance().clock();
			delivered_data += (HDR_CMN(pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_]))->size_;
			delivered_pkts++;
			sum_of_delay = sum_of_delay + Scheduler::instance().clock() - arq_tx_->get_pkt_tx_start((HDR_CMN(pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_]))->opt_num_forwards_);


			send(pnew,h);
		}
		pkt_buf[((last_fwd_sn_+1)%sn_cnt)%wnd_] = NULL;
		last_fwd_sn_ = (last_fwd_sn_+1)%sn_cnt;

		count++;

	}

}

void ARQSRAcker::print_stats()
{
	printf("//--------------- STATISTICS ---------------//\n");
	printf("Start time (sec):\t\t%f\n", arq_tx_->get_start_time());
	printf("Finish time (sec):\t\t%f\n", finish_time);

	printf("Total number of delivered pkts:\t%d\n", delivered_pkts);
	printf("Delivered data (in bytes):\t%d\n", delivered_data);
	double throughput = (delivered_data * 8) / (double) (finish_time - arq_tx_->get_start_time());
	printf("Total throughput (Mbps):\t%f\n", throughput * 1.0e-6);

	double mean = sum_of_delay / delivered_pkts;
	printf("Mean delay (msec):\t\t%f\n", mean * 1.0e+3);

	printf("Unique packets sent:\t\t%d\n", arq_tx_->get_total_packets_sent());
	printf("Packet loss rate:\t\t%f\n", 1 - (delivered_pkts / (double) arq_tx_->get_total_packets_sent()));
}

void ARQSRAcker::handle(Event* e)
{

	SRACKEvent *rcv_ack = (SRACKEvent *)e;
	int rcv_sn = rcv_ack->ACK_sn;
	int rcv_uid = rcv_ack->ACK_uid;
	delete e;

	if ( ranvar_->value() < err_rate ){
		arq_tx_->nack(rcv_sn,rcv_uid);
	} else {
		arq_tx_->ack(rcv_sn,rcv_uid);
	}

}




void ARQSRNacker::recv(Packet* p, Handler* h)
{

	SRACKEvent *new_ACKEvent = new SRACKEvent();
	hdr_cmn *ch = HDR_CMN(p);
	Event *ack_e;
	new_ACKEvent->ACK_sn = ch->opt_num_forwards_;
	new_ACKEvent->ACK_uid = ch->uid();
	ack_e = (Event *)new_ACKEvent;

	if (delay_ > 0)
		Scheduler::instance().schedule(this, ack_e, delay_);
	else
		handle(ack_e);
}

void ARQSRNacker::handle(Event* e)
{
	SRACKEvent *rcv_ack = (SRACKEvent *)e;
	int rcv_sn = rcv_ack->ACK_sn;
	int rcv_uid = rcv_ack->ACK_uid;
	delete e;
	arq_tx_->nack(rcv_sn,rcv_uid);
}

//--------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------//
