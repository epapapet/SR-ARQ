ARQSRTx set retry_limit_ 100
ARQSRTx set debug_ NULL
ARQSRAcker set debug_ NULL
ARQSRAcker set delay_ 30ms
ARQSRNacker set debug_ NULL
ARQSRNacker set delay_ 30ms

# usage: ns <scriptfile> <bandwidth> <propagation_delay> <window_size> <cbr_rate> <pkt_size> <err_rate> <ack_err_rate> <num_rtx> <seed>
# <bandwidth> : in bps, example: set to 5Mbps -> 5M or 5000000
# <propagation_delay> : in secs, example: set to 30ms -> 30ms or 0.03
# <window_size> : aqr window size in pkts
# <cbr_rate> : the rate of the cbr applications, in bps, example: set to 3Mbps -> 3M or 3000000
# <pkt_size> : the size of udp pkt (including udp and ip headers)
# <err_rate> : the error rate in the forward channel (error rate for frames)
# <ack_rate> : the error rate in the return channel (error rate for ACKs)
# <num_rtx> : the number of retransmissions allowed for a native pkt
# <seed> : seed used to produce randomness
SimpleLink instproc link-arq { limit wndsize vgseed ackerr} {
    $self instvar link_ link_errmodule_ queue_ drophead_ head_
    $self instvar tARQ_ acker_ nacker_
 
    set tARQ_ [new ARQSRTx]
    set acker_ [new ARQSRAcker]
    set nacker_ [new ARQSRNacker]
    $tARQ_ set retry_limit_ $limit

    $tARQ_ setup-wnd $wndsize
    $acker_ attach-ARQSRTx $tARQ_
    $nacker_ attach-ARQSRTx $tARQ_
    $acker_ setup-wnd $wndsize


    $tARQ_ target [$queue_ target]
    $queue_ target $tARQ_
    $acker_ target [$link_errmodule_ target]
    $link_errmodule_ target $acker_
    $link_errmodule_ drop-target $nacker_
    $tARQ_ drop-target $drophead_

    $acker_ set delay_ [$self delay]
    $nacker_ set delay_ [$self delay]

    set vagrngn2 [new RNG]
    $vagrngn2 seed [expr {$vgseed + 1}]
    set vagranvarn2 [new RandomVariable/Uniform]
    $vagranvarn2 use-rng $vagrngn2

    $acker_ ranvar $vagranvarn2
    
    $acker_ set-err $ackerr

    return $acker_
    
}

Simulator instproc link-arq {wndsize limit from to vgseed ackerr} {
    set link [$self link $from $to]
    $link link-arq $limit $wndsize $vgseed $ackerr
}

proc print_stats {} {
	global receiver
	$receiver print-stats
}

#=== Create the Simulator, Nodes, and Links ===
set ns [new Simulator]
set n1 [$ns node]
set n2 [$ns node]
set n3 [$ns node]

set link_bwd [lindex $argv 0]
set link_delay [lindex $argv 1]

$ns duplex-link $n1 $n2 $link_bwd $link_delay DropTail
$ns duplex-link $n2 $n3 $link_bwd $link_delay DropTail
$ns duplex-link $n1 $n3 $link_bwd $link_delay DropTail

#=== Create error and ARQ module ===
set window [lindex $argv 2]
set em [new ErrorModel]
$em set rate_ [lindex $argv 5]

$em set enable_ 1
$em unit pkt
$em set bandwidth_ $link_bwd

set vagrng [new RNG]
$vagrng seed [lindex $argv 8]
set vagranvar [new RandomVariable/Uniform]
$vagranvar use-rng $vagrng

$em ranvar $vagranvar
$em drop-target [new Agent/Null]

$ns link-lossmodel $em $n1 $n3

set num_rtx [lindex $argv 7]
set receiver [$ns link-arq $window $num_rtx $n1 $n3 [lindex $argv 8] [lindex $argv 6]]

#=== Set up a UDP connection ===
set udp [new Agent/UDP]
set sink [new Agent/Null]
set cbr [new Application/Traffic/CBR]

$cbr set type_ CBR
$cbr set packet_size_ [lindex $argv 4]
$cbr set rate_ [lindex $argv 3]
$cbr set random_ false

$ns attach-agent $n1 $udp
$ns attach-agent $n3 $sink
$cbr attach-agent $udp
$ns connect $udp $sink

$ns at 0.0 "$cbr start"
$ns at 100.0 print_stats
$ns at 100.1 "exit 0"
$ns run
