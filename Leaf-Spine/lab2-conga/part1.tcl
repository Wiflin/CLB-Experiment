set ns [new Simulator]




set serverNumber 		30
set accessSwitchNumber 	2
set topSwitchNumber 	2

set leafUpPortNumber 2
set leafDownPortNumber 15

set flowNumber 15

set fTraffic [open InputFlow-$serverNumber-$flowNumber.tr r]

set fInputTrafficBindFlowID [open InputFlow-BindFlowID-$serverNumber-$flowNumber.tr w]

set fFCT "InputFlow-FCT-$serverNumber-$flowNumber.tr"

set intialCwnd 100
Agent/TCP/FullTcp/Sack set sack_rtx_cthresh_ 1000;
Agent/TCP/FullTcp set tcprexmtthresh_ 1000
Agent/TCP set IF_PRINT_SEQTIMELINE 1
Node set loadBalancePerPacket_ 1


Node set multiPath_ 1
$ns set staticRoute_ 1



# server nodes
for {set i 0} {$i<$serverNumber} {incr i} {
	set n($i) [$ns node]
	$n($i) enable-salt
}

# 1*access layer + [expr $switchLayer-2]*distribution layer
for {set i 0} {$i<$accessSwitchNumber} {incr i} {
	set sLeaf($i) [$ns node]
	$sLeaf($i) enable-salt
	$sLeaf($i) enable-conga $topSwitchNumber $leafDownPortNumber
}

# core layer
for {set i 0} {$i<$topSwitchNumber} {incr i} {
	set sSpine($i) [$ns node]
	$sSpine($i) enable-salt
}



set rwndSize 4000
set queueSpineSwitch 10000
set queueLeafSwitch 10000

set queueManage "DropTail"
set ecnThresholdPortion 0.3

# # set sendBufferSize [expr 10*$rwndSize]
set sendBufferSize	4000
set ecnThresholdPortionLeaf 0.3

#leaf <-> node
puts "initializing links between servers and leaf(i) switches......"
for {set i 0} {$i<$accessSwitchNumber} {incr i} {
	for {set j 0} {$j<$leafDownPortNumber} {incr j} {
		set k [expr $i*$leafDownPortNumber+$j]
		$ns simplex-link $n($k) $sLeaf($i) 1G 0.00002s $queueManage
		$ns queue-limit $n($k) $sLeaf($i) $sendBufferSize
		# $ns ecn-threshold $n($k) $sLeaf($i) [expr int($queueLeafSwitch*$ecnThresholdPortionLeaf)]
		$ns simplex-link $sLeaf($i) $n($k) 1G 0.00002s $queueManage
		$ns queue-limit $sLeaf($i) $n($k) $queueLeafSwitch
		# $ns ecn-threshold $sLeaf($i) $n($k) [expr int($queueLeafSwitch*$ecnThresholdPortionLeaf)]
		puts "node([$n($k) set address_]) <-> leaf($i)([$sLeaf($i) set address_]) "

		set queue1 [[$ns link $sLeaf($i) $n($k)] queue]
		# $queue1 monitor-E2EDelay
		# $queue1 monitor-QueueLen
		# $queue1 monitor-Drop
		# $queue1 monitor-FlowPath
		# $queue1 monitor-PathTrace

		set queue2 [[$ns link $n($k) $sLeaf($i)] queue]
		# $queue2 monitor-QueueLen
		# $queue2 monitor-Drop
		# $queue2 monitor-FlowPath
		# $queue2 monitor-PathTrace
	}
}



# 0 -> 0
$ns simplex-link $sLeaf(0) $sSpine(0) 10G 0.00002s $queueManage	
$ns queue-limit $sLeaf(0) $sSpine(0) $queueLeafSwitch

$ns simplex-link $sSpine(0) $sLeaf(0) 10G 0.00002s $queueManage
$ns queue-limit $sSpine(0) $sLeaf(0) $queueSpineSwitch



# 0 -> 1
$ns simplex-link $sLeaf(0) $sSpine(1) 10G 0.00002s $queueManage	
$ns queue-limit $sLeaf(0) $sSpine(1) $queueLeafSwitch

$ns simplex-link $sSpine(1) $sLeaf(0) 10G 0.00002s $queueManage
$ns queue-limit $sSpine(1) $sLeaf(0) $queueSpineSwitch



# 1 -> 0
$ns simplex-link $sLeaf(1) $sSpine(0) 10G 0.00002s $queueManage	
$ns queue-limit $sLeaf(1) $sSpine(0) $queueLeafSwitch

$ns simplex-link $sSpine(0) $sLeaf(1) 10G 0.00002s $queueManage
$ns queue-limit $sSpine(0) $sLeaf(1) $queueSpineSwitch



# 1 -> 1
$ns simplex-link $sLeaf(1) $sSpine(1) 5G 0.00002s $queueManage	
$ns queue-limit $sLeaf(1) $sSpine(1) $queueLeafSwitch

$ns simplex-link $sSpine(1) $sLeaf(1) 5G 0.00002s $queueManage
$ns queue-limit $sSpine(1) $sLeaf(1) $queueSpineSwitch


# initialize route-table
for {set i 0} {$i<$serverNumber} {incr i} {
	$n($i) init-routes-forExplicitRoute $serverNumber
}

for {set i 0} {$i<$accessSwitchNumber} {incr i} {
	$sLeaf($i) init-routes-forExplicitRoute $serverNumber
}

for {set i 0} {$i<$topSwitchNumber} {incr i} {
	$sSpine($i) init-routes-forExplicitRoute $serverNumber
}

set addRoutesStartTime [clock seconds]
puts "Start adding routes..............."

# node(x) <-> leaf
for {set i [expr $serverNumber-1]} {$i>=0} {incr i -1} {
	# set startTime [clock seconds]
	set belongToWhichLeaf [expr int($i/$leafDownPortNumber)]
	for {set j 0} {$j<$serverNumber} {incr j} {
		if {$j != $i} {
			puts "addExplicitRoute n($i) n($j) sLeaf($belongToWhichLeaf)"
			$ns addExplicitRoute $n($i) $n($j) $sLeaf($belongToWhichLeaf)
		}	
	}
	puts "addExplicitRoute sLeaf($belongToWhichLeaf) n($i) n($i)"
	$ns addExplicitRoute $sLeaf($belongToWhichLeaf) $n($i) $n($i) 
}
puts "Adding routes for nodes completed!"


#spine <-> leaf(x)
# puts "initializing routes between leaf([expr $switchLayer-2],x) and spine switches...."
for {set i 0} {$i<$accessSwitchNumber} {incr i} {

	# $n($h) is target for a flow		
	for {set h 0} {$h<$serverNumber} {incr h} {
		set belongToWhichLeaf [expr int($h/$leafDownPortNumber)]
		if {$belongToWhichLeaf == $i} {
			# so if the flow want to get the way, it must lead the flow come to this access switch
			for {set j 0} {$j<$leafUpPortNumber} {incr j} {
				# which top switch has been connected with this access switch 
				set k [expr ($i*$leafUpPortNumber+$j)%$topSwitchNumber]
				puts "addExplicitRoute sSpine($k) n($h) sLeaf($i)"
				$ns addExplicitRoute $sSpine($k) $n($h) $sLeaf($i) 
			}
		} else {
			# so if the flow want to get the way, it must lead the flow go to the top switch
			for {set j 0} {$j<$leafUpPortNumber} {incr j} {
				# which top switch has been connected with this access switch 
				set k [expr ($i*$leafUpPortNumber+$j)%$topSwitchNumber]
				puts "addExplicitRoute sLeaf($i) n($h) sSpine($k)"
				$ns addExplicitRoute $sLeaf($i) $n($h) $sSpine($k)
			}
		}
	}
}	

set addRoutesEndTime [clock seconds]
set runTime [expr $addRoutesEndTime-$addRoutesStartTime]
set hour [expr int($runTime/3600)]
set minute [expr int(($runTime-$hour*3600)/60)]
set second [expr int($runTime)%60]
puts [format "Adding routes for leaf and spine completed! Run for %dh:%dm:%ds" $hour $minute $second]

################################################################

Agent/TCP set window_ 4000
Agent/TCP set maxcwnd_ 4000
Agent/TCP set minrto_ 5e-3
Agent/TCP set tcpTick_ 1e-5
Agent/TCP set rtxcur_init_ 5e-3
Agent/TCP set windowInit_ 16

Agent/TCP set maxRto_	3

Agent/TCP set max_ssthresh_ 20000

# Agent/TCPSink set ecn_syn_ true
# Agent/TCP set ecn_ 1
# Agent/TCP set old_ecn_ 1

set STARTTIMEOFFSET 2000
set inputFlowStartTime [clock seconds]

set random_generator [ new RNG ]
$random_generator seed 1500156051
set rnd [new RandomVariable/Uniform]
$rnd use-rng $random_generator
$rnd set min_ 0
$rnd set max_ 1

set latestFlowStartTime 0
set longFlowNum 0
set smallFlowNum 0

set totalConnNum 0
set longConnNum 0
set normalConnNum 0

# bind flow with exact link 
for {set k 0} {$k<$flowNumber} {incr k} {

	if { [expr $k % 100] == 0 } {
		set inputFlowEndTime [clock seconds]
		set runTime [expr $inputFlowEndTime-$inputFlowStartTime]
		set hour [expr int($runTime/3600)]
		set minute [expr int(($runTime-$hour*3600)/60)]
		set second [expr int($runTime)%60]
		puts [format "Input flows(%d) completed! Run for %dh:%dm:%ds" $k $hour $minute $second]
		# puts [format "longFlowNum(%d) smallFlowNum(%d) totalConnNum(%d) normalConnNum(%d) longConnNum(%d)" $longFlowNum $smallFlowNum $totalConnNum $normalConnNum $longConnNum]
	} 

	gets $fTraffic line
	set fields [split $line]
	
	lassign $fields srcNode dstNode startTime flowSize
	if {$dstNode == $serverNumber || $dstNode==$srcNode} {
		puts "dstNode=$dstNode error!"
	}
	
	set startTime [expr $startTime+$STARTTIMEOFFSET]

	if {$startTime > $latestFlowStartTime} {
		set latestFlowStartTime $startTime
	}

	set currentInsertingConnID -1
	set ifEstablishNewConn 0

	# if {$flowSize>$NonLongConnFlowThreshold} {
		set currentInsertingConnID $normalConnNum

		incr longFlowNum
		incr normalConnNum
		incr totalConnNum

		set tcp($currentInsertingConnID) [new Agent/TCP/FullTcp/Sack]
		set tcpsink($currentInsertingConnID) [new Agent/TCP/FullTcp/Sack]

		$tcp($currentInsertingConnID) ecmp-hash-key [expr abs(int([$rnd value]*0x10000))]

		# $tcp($currentInsertingConnID) monitor-spare-window

		$tcp($currentInsertingConnID) monitor-Sequence
		# $tcpsink($currentInsertingConnID) monitor-Sequence

		$tcp($currentInsertingConnID) no-syn sender
		$tcpsink($currentInsertingConnID) no-syn receiver

		$tcp($currentInsertingConnID) set window_ $rwndSize
		$tcp($currentInsertingConnID) set maxcwnd_ $rwndSize

		$tcp($currentInsertingConnID) set segsize_ 1460
		
		$tcpsink($currentInsertingConnID) set segsize_ [$tcp($currentInsertingConnID) set segsize_]

		$ns attach-agent $n($srcNode) $tcp($currentInsertingConnID)
		$ns attach-agent $n($dstNode) $tcpsink($currentInsertingConnID)
		$ns connect $tcp($currentInsertingConnID) $tcpsink($currentInsertingConnID)

		$tcp($currentInsertingConnID) flow-id [expr $totalConnNum-1]
		$tcpsink($currentInsertingConnID) flow-id [expr $totalConnNum-1]

		$tcpsink($currentInsertingConnID) listen

		set ifEstablishNewConn 1
	# } 

	# if {$flowSize>$NonLongConnFlowThreshold} {
		set ftp($currentInsertingConnID) [new Application/FTP]
		$ftp($currentInsertingConnID) attach-agent $tcp($currentInsertingConnID)
		set ftpsink($currentInsertingConnID) [new Application/FTPSink]
		$ftpsink($currentInsertingConnID) attach-agent $tcpsink($currentInsertingConnID)
		$ftpsink($currentInsertingConnID) flow-size $flowSize
		$ftpsink($currentInsertingConnID) flow-id $k
		$ftpsink($currentInsertingConnID) flow-start-time $startTime
		puts $fInputTrafficBindFlowID "flow\[$k\] conn\[[$tcp($currentInsertingConnID) set flow_ID_]\]($srcNode-$dstNode) $startTime $flowSize"
		$ns at $startTime "$ftp($currentInsertingConnID) send $flowSize"
	
	# }
}


$tcpsink(0) monitor-Sequence

set inputFlowEndTime [clock seconds]
set runTime [expr $inputFlowEndTime-$inputFlowStartTime]
set hour [expr int($runTime/3600)]
set minute [expr int(($runTime-$hour*3600)/60)]
set second [expr int($runTime)%60]
puts [format "Input flows(%d) completed! Run for %dh:%dm:%ds" $flowNumber $hour $minute $second]
puts [format "longFlowNum(%d) smallFlowNum(%d) totalConnNum(%d) normalConnNum(%d) longConnNum(%d)" $longFlowNum $smallFlowNum $totalConnNum $normalConnNum $longConnNum]
flush $fInputTrafficBindFlowID

proc finish {} {
	global normalConnNum ftpsink fFCT

	for {set i 0} {$i<$normalConnNum} {incr i} {
		$ftpsink($i) print-result-file $fFCT
	}

	exit
}

set timeLength [expr $latestFlowStartTime+10]
$ns at $timeLength "finish"
$ns run