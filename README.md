# Selective Repeat ARQ implementation for ns2

This is an implementation of the Selective Repeat (SR) ARQ protocol in [ns2](https://www.isi.edu/nsnam/ns/). In contrast to the HDLC implementation included in ns2, which can only be used in a satellite channel, this implementation can be used in a typical wired link.

The code was developed in the [Department of Computer Science and Engineering](http://www.cse.uoi.gr/en/index.php?menu=m1), [University of Ioannina](http://www.uoi.gr/en), Greece by [Evangelos Papapetrou](http://cse.uoi.gr/~epap) in cooperation with undergraduate student [Foteini Karetsi](https://github.com/pkaretsi).

This implementation is based on the design of the Alternating Bit Protocol (ABP) implementation discussed in the book:

Issariyakul T., Hossain E. _"Introduction to Network Simulator 2 (NS2)"_, Springer, 2012.

## Installation

**Requirements**

The implementation has been tested successfully in ns2 v2.35. Compatibility with older (or future) versions is probable but not guaranteed.

**Steps**

Copy the files arq.h and arq.cc in a folder within the ns2 folder. Then, include the statement ``arq_folder/arq.o \`` at the end of the ``OBJ_CC`` section of the Makefile, where ``arq_folder`` is the folder in which you copied the files. After that recompile ns.

## Running a simulation

We provide a bunch of tcl files that can be used to run example simulations. You can modify those tcl files to suit your needs. To use those simulation files you can issue the following command from within the ns directory:

```
$ ./ns arq-Tx_ftp.tcl <bandwidth> <propagation_delay> <window_size> <pkt_size> <err_rate> <ack_err_rate> <num_rtx> <simulation_time> <seed>
```

where:

* ns is the ns executable
* \<bandwidth\> : the link bandwidth (in bps, example: set to 5Mbps -> 5M or 5000000)
* \<propagation_delay\> : the link propagation delay (in secs, example: set to 30ms -> 30ms or 0.03)
* \<window_size\> : the aqr window size in pkts (a value of 1 results in simulating the Alternating Bit Protocol)
* \<pkt_size\> : the size of a TCP segment (not including the TCP and IP headers)
* \<err_rate\> : the error rate in the forward channel (sender->receiver)
* \<ack_rate\> : the error rate in the return channel (receiver->sender)
* \<num_rtx\> : the number of retransmissions allowed for a packet
* \<simulation_time\> : the simulation time in secs
* \<seed\> : seed used to produce randomness

or:

```
$ ./ns arq-Tx_cbr.tcl <bandwidth> <propagation_delay> <window_size> <cbr_rate> <pkt_size> <err_rate> <ack_err_rate> <num_rtx> <simulation_time> <seed>
```

where: 

* \<cbr_rate\> : the rate of the cbr applications, in bps, example: set to 3Mbps -> 3M or 3000000
* \<pkt_size\> : the size of udp pkt (including udp and ip headers)

while the other parameters are the same as in the case of arq-Tx_ftp.tcl.

-----------------------
## How to cite this work

When you write a paper that involves the use of this code, we would appreciate it if you can cite our tool using the following entry.

* E. Papapetrou and F. Karetsi, "Selective Repeat ARQ implementation for ns2", 2020. [Online]. Available: https://github.com/epapapet/SR-ARQ.

* BibTeX entry:
>@misc{SR-ARQ-epap,  
>	author = {Papapetrou, Evangelos and Karetsi, Foteini},  
>	title = {Selective Repeat ARQ implementation for ns2},  
>	howpublished = {[Online]. Available: https://github.com/epapapet/SR-ARQ },  
>	year = {2020}  
>}
