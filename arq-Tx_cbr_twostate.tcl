ARQSRTx set retry_limit_ 100
ARQSRTx set debug_ NULL
ARQSRAcker set debug_ NULL
ARQSRAcker set delay_ 30ms
ARQSRNacker set debug_ NULL
ARQSRNacker set delay_ 30ms

# usage: ns <scriptfile> <bandwidth> <propagation_delay> <window_size> <cbr_rate> <pkt_size> <err_rate> <ack_err_rate> <num_rtx> <simulation_time> <seed>
# <bandwidth> : in bps, example: set to 5Mbps -> 5M or 5000000
# <propagation_delay> : in secs, example: set to 30ms -> 30ms or 0.03
# <window_size> : aqr window size in pkts
# <cbr_rate> : the rate of the cbr applications, in bps, example: set to 3Mbps -> 3M or 3000000
# <pkt_size> : the size of udp pkt (including udp and ip headers)
# <err_rate> : the error rate in the forward channel (error rate for frames) during a burst period
# <burst_duration> : 0,..,1 -> the percentage of time that the channel is in an error burst state
# <ack_rate> : the error rate in the return channel (error rate for ACKs)
# <num_rtx> : the number of retransmissions allowed for a native pkt
# <simulation_time> : the simulation time in secs
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

#Create uniform Errormodel representing first state
set tmp [new ErrorModel]
$tmp set rate_ 0
$tmp set enable_ 1
$tmp set bandwidth_ $link_bwd
set vagrng00 [new RNG]
$vagrng00 seed [expr {[lindex $argv 10] + 10}]
set vagranvar00 [new RandomVariable/Uniform]
$vagranvar00 use-rng $vagrng00
$tmp ranvar $vagranvar00

#Create uniform Errormodel representing second state
set tmp1 [new ErrorModel]
$tmp1 set rate_ [lindex $argv 5]
$tmp1 set enable_ 1
$tmp1 set bandwidth_ $link_bwd
set vagrng01 [new RNG]
$vagrng01 seed [lindex $argv 10]
set vagranvar01 [new RandomVariable/Uniform]
$vagranvar01 use-rng $vagrng01
$tmp1 ranvar $vagranvar01

if {[lindex $argv 6] > 1 || [lindex $argv 6] < 0} {
    puts "Burst duration percentage should be in \[0, 1\]"
    exit 1;
}
if {[string first "M" [lindex $argv 0]] != -1} {
    set bwd_per_string [string map {"M" ""} [lindex $argv 0]]
    set bwdcalc [expr {double($bwd_per_string)*1000000}]
} else {
    set bwdcalc [lindex $argv 0]
}
set bduration [lindex $argv 6]
set wndduration [expr {8.0*$window*[lindex $argv 4]/$bwdcalc}]
set state1nduration [expr {(1- $bduration)*$wndduration}]
set state2nduration [expr {$bduration*$wndduration}]


# Array of states (error models)
set m_states [list $tmp $tmp1]
# Durations for each of the states, tmp, tmp1 and tmp2, respectively
set m_periods [list $state1nduration $state2nduration]
# Transition state model matrix
set m_transmx { {0 1} {1 0}}
set m_trunit pkt
 # Use time-based transition
set m_sttype time
set m_nstates 2
set m_nstart [lindex $m_states 0]
set em [new ErrorModel/MultiState $m_states $m_periods $m_transmx $m_trunit $m_sttype $m_nstates $m_nstart]


















$em drop-target [new Agent/Null]

$ns link-lossmodel $em $n1 $n3

set num_rtx [lindex $argv 8]
set receiver [$ns link-arq $window $num_rtx $n1 $n3 [lindex $argv 10] [lindex $argv 7]]

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
$ns at [lindex $argv 9] "$cbr stop"
$ns at [expr {[lindex $argv 9] + 0.5}] print_stats
$ns at [expr {[lindex $argv 9] + 1.0}] "exit 0"
$ns run
