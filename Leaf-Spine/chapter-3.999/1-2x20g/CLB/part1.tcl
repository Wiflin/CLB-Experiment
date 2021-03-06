if { $argc != 3 } {
       puts "Arguments: method load flowNumbers"
       puts "Method : ECMP CONGA CLOVE CLB"
       exit
}
 

set method [lindex $argv 0]
set load [lindex $argv 1]

set flowNumber [lindex $argv 2]


set ns [new Simulator]


set serverNumber 		30
set accessSwitchNumber 	3
set topSwitchNumber 	2

set leafUpPortNumber $topSwitchNumber
set leafDownPortNumber 10

set perFlowSize "10M"

set fTraffic [open InputFlow-$serverNumber-$flowNumber-load-$load.tr r]

set fInputTrafficBindFlowID [open InputFlow-BindFlowID-$serverNumber-$flowNumber.tr w]

set fFCT "InputFlow-FCT-$serverNumber-$flowNumber.tr"

set intialCwnd 100
Agent/TCP/FullTcp/Sack set sack_rtx_cthresh_ 5000;
Agent/TCP/FullTcp set tcprexmtthresh_ 5000
Agent/TCP set IF_PRINT_SEQTIMELINE 1

if {$method == "CONGA"} {
	Node set loadBalancePerPacket_ 1
}
# Node set loadBalanceFlowlet_ 1


Node set multiPath_ 1
$ns set staticRoute_ 1


# server nodes
for {set i 0} {$i<$serverNumber} {incr i} {
	set n($i) [$ns node]
	$n($i) enable-salt

	if {$method == "CLB"} {
		[$n($i) entry] enable-clb $topSwitchNumber
	}

	if {$method == "CLOVE"} {
		[$n($i) entry] enable-clove $topSwitchNumber
	}

	if {$method == "HERMES"} {
		[$n($i) entry] enable-hermes $topSwitchNumber
	}
	# puts [$n($i) entry]
}

# [$n(0) entry] enable-clb
# [$n(40) entry] enable-clb

# 1*access layer + [expr $switchLayer-2]*distribution layer
for {set i 0} {$i<$accessSwitchNumber} {incr i} {
	set sLeaf($i) [$ns node]
	$sLeaf($i) enable-salt
	if {$method == "CONGA"} {
		$sLeaf($i) enable-conga $topSwitchNumber $leafDownPortNumber
	}
}

# core layer
for {set i 0} {$i<$topSwitchNumber} {incr i} {
	set sSpine($i) [$ns node]
	$sSpine($i) enable-salt
}


# 10G => 400 queue length


set queueSpineSwitch 400
set queueLeafSwitch 400

set queueManage "DropTail"
set ecnThresholdPortion 0.3

set sendBufferSize	800
set ecnThresholdPortionLeaf 0.3

set linkRate	10G
set linkAccessRate 	20G
# 0.02 ms
set linkDelay 	0.02 
# 

#leaf <-> node
puts "initializing links between servers and leaf(i) switches......"
for {set i 0} {$i<$accessSwitchNumber} {incr i} {
	for {set j 0} {$j<$leafDownPortNumber} {incr j} {
		set k [expr $i*$leafDownPortNumber+$j]
		$ns simplex-link $n($k) $sLeaf($i) $linkAccessRate 0.00002s $queueManage
		$ns queue-limit $n($k) $sLeaf($i) $sendBufferSize
		$ns ecn-threshold $n($k) $sLeaf($i) [expr int($sendBufferSize*$ecnThresholdPortionLeaf)]

		$ns simplex-link $sLeaf($i) $n($k) $linkAccessRate 0.00002s $queueManage
		$ns queue-limit $sLeaf($i) $n($k) $sendBufferSize
		$ns ecn-threshold $sLeaf($i) $n($k) [expr int($sendBufferSize*$ecnThresholdPortionLeaf)]
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





#spine <-> leaf
puts "initializing links between leaf(i) and spine switches......"
# for {set i 0} {$i<$accessSwitchNumber} {incr i} {
# 	for {set j 0} {$j<$leafUpPortNumber} {incr j} {
# 		set k [expr ($i*$leafUpPortNumber+$j)%$topSwitchNumber]
# 			$ns simplex-link $sLeaf($i) $sSpine($k) $linkRate [expr $linkDelay*1E-3]s $queueManage	
# 			$ns queue-limit $sLeaf($i) $sSpine($k) $queueLeafSwitch
# 			$ns ecn-threshold $sLeaf($i) $sSpine($k) [expr int($queueSpineSwitch*$ecnThresholdPortion)]
# 			$ns simplex-link $sSpine($k) $sLeaf($i) $linkRate [expr $linkDelay*1E-3]s $queueManage
# 			$ns queue-limit $sSpine($k) $sLeaf($i) $queueSpineSwitch
# 			$ns ecn-threshold $sSpine($k) $sLeaf($i) [expr int($queueSpineSwitch*$ecnThresholdPortion)]

# 			puts "leaf($i)([$sLeaf($i) set address_]) <-> spine($k)([$sSpine($k) set address_])"

# 			set queue1 [[$ns link $sSpine($k) $sLeaf($i)] queue]
# 			# $queue1 monitor-QueueLen
# 			# $queue1 monitor-Drop
# 			# $queue1 monitor-FlowPath
# 			# $queue1 monitor-PathTrace

# 			set queue2 [[$ns link  $sLeaf($i) $sSpine($k)] queue]
# 			# $queue2 tag-timestamp
# 			$queue2 monitor-QueueLen
# 			# $queue2 monitor-FlowPath
# 			# $queue2 monitor-FlowSpeed
# 			# $queue2 monitor-Drop
# 			# $queue2 monitor-PathTrace
			
# 			# what is this means?
# 			# if {$k==0 && $failureNumber>0 && $failureNumber<3} {
# 			# 	$ns link-lossmodel $em $sLeaf($i) $sSpine($k)
# 			# 	# $queue2 monitor-FlowPath
# 			# }
# 	}
# }


# 0 -> 0
$ns simplex-link $sLeaf(0) $sSpine(0) $linkRate 0.00002s $queueManage	
$ns queue-limit $sLeaf(0) $sSpine(0) $queueLeafSwitch
$ns ecn-threshold $sLeaf(0) $sSpine(0) [expr int($queueLeafSwitch*$ecnThresholdPortion)]

$ns simplex-link $sSpine(0) $sLeaf(0) $linkRate 0.00002s $queueManage
$ns queue-limit $sSpine(0) $sLeaf(0) $queueSpineSwitch
$ns ecn-threshold $sSpine(0) $sLeaf(0) [expr int($queueSpineSwitch*$ecnThresholdPortion)]

set queueA [[$ns link $sLeaf(0) $sSpine(0)] queue]
# $queueA monitor-QueueLen
$queueA monitor-FlowSpeed
$queueA monitor-FlowPath


# 0 -> 1
$ns simplex-link $sLeaf(0) $sSpine(1) $linkRate 0.00002s $queueManage	
$ns queue-limit $sLeaf(0) $sSpine(1) $queueLeafSwitch
$ns ecn-threshold $sLeaf(0) $sSpine(1) [expr int($queueLeafSwitch*$ecnThresholdPortion)]
$ns simplex-link $sSpine(1) $sLeaf(0) $linkRate 0.00002s $queueManage
$ns queue-limit $sSpine(1) $sLeaf(0) $queueSpineSwitch
$ns ecn-threshold $sSpine(1) $sLeaf(0) [expr int($queueSpineSwitch*$ecnThresholdPortion)]

set queueB [[$ns link $sLeaf(0) $sSpine(1)] queue]
# $queueB monitor-QueueLen
$queueB monitor-FlowSpeed
$queueB monitor-FlowPath

# 0 -> 2
# $ns simplex-link $sLeaf(0) $sSpine(2) 10G 0.00002s $queueManage	
# $ns queue-limit $sLeaf(0) $sSpine(2) $queueLeafSwitch
# $ns ecn-threshold $sLeaf(0) $sSpine(2) [expr int($queueLeafSwitch*$ecnThresholdPortion)]
# $ns simplex-link $sSpine(2) $sLeaf(0) 10G 0.00002s $queueManage
# $ns queue-limit $sSpine(2) $sLeaf(0) $queueSpineSwitch
# $ns ecn-threshold $sSpine(2) $sLeaf(0) [expr int($queueSpineSwitch*$ecnThresholdPortion)]
# set queueC [[$ns link $sLeaf(0) $sSpine(2)] queue]
# $queueC monitor-QueueLen

# 0 -> 3
# $ns simplex-link $sLeaf(0) $sSpine(3) 10G 0.00002s $queueManage	
# $ns queue-limit $sLeaf(0) $sSpine(3) $queueLeafSwitch
# $ns ecn-threshold $sLeaf(0) $sSpine(3) [expr int($queueLeafSwitch*$ecnThresholdPortion)]
# $ns simplex-link $sSpine(3) $sLeaf(0) 10G 0.00002s $queueManage
# $ns queue-limit $sSpine(3) $sLeaf(0) $queueSpineSwitch
# $ns ecn-threshold $sSpine(3) $sLeaf(0) [expr int($queueSpineSwitch*$ecnThresholdPortion)]
# set queueD [[$ns link $sLeaf(0) $sSpine(3)] queue]
# $queueD monitor-QueueLen

# 1 -> 0
$ns simplex-link $sLeaf(1) $sSpine(0) 5G 0.00002s $queueManage	
$ns queue-limit $sLeaf(1) $sSpine(0) 200
$ns ecn-threshold $sLeaf(1) $sSpine(0) [expr int(200*$ecnThresholdPortion)]
$ns simplex-link $sSpine(0) $sLeaf(1) 5G 0.00002s $queueManage
$ns queue-limit $sSpine(0) $sLeaf(1) 200
$ns ecn-threshold $sSpine(0) $sLeaf(1) [expr int(200*$ecnThresholdPortion)]
set queueC [[$ns link $sLeaf(1) $sSpine(0)] queue]
# $queueC monitor-QueueLen
$queueC monitor-FlowSpeed
$queueC monitor-FlowPath

# 1 -> 1
$ns simplex-link $sLeaf(1) $sSpine(1) $linkRate 0.00002s $queueManage	
$ns queue-limit $sLeaf(1) $sSpine(1) $queueLeafSwitch
$ns ecn-threshold $sLeaf(1) $sSpine(1) [expr int($queueLeafSwitch*$ecnThresholdPortion)]
$ns simplex-link $sSpine(1) $sLeaf(1) $linkRate 0.00002s $queueManage
$ns queue-limit $sSpine(1) $sLeaf(1) $queueSpineSwitch
$ns ecn-threshold $sSpine(1) $sLeaf(1) [expr int($queueSpineSwitch*$ecnThresholdPortion)]
set queueD [[$ns link $sLeaf(1) $sSpine(1)] queue]
# $queueD monitor-QueueLen
$queueD monitor-FlowSpeed
$queueD monitor-FlowPath

# 1 -> 2
# $ns simplex-link $sLeaf(1) $sSpine(2) 10G 0.00002s $queueManage	
# $ns queue-limit $sLeaf(1) $sSpine(2) $queueLeafSwitch
# $ns ecn-threshold $sLeaf(1) $sSpine(2) [expr int($queueLeafSwitch*$ecnThresholdPortion)]
# $ns simplex-link $sSpine(2) $sLeaf(1) 10G 0.00002s $queueManage
# $ns queue-limit $sSpine(2) $sLeaf(1) $queueSpineSwitch
# $ns ecn-threshold $sSpine(2) $sLeaf(1) [expr int($queueSpineSwitch*$ecnThresholdPortion)]

# 1 -> 3
# $ns simplex-link $sLeaf(1) $sSpine(3) 10G 0.00002s $queueManage	
# $ns queue-limit $sLeaf(1) $sSpine(3) $queueLeafSwitch
# $ns ecn-threshold $sLeaf(1) $sSpine(3) [expr int($queueLeafSwitch*$ecnThresholdPortion)]
# $ns simplex-link $sSpine(3) $sLeaf(1) 10G 0.00002s $queueManage
# $ns queue-limit $sSpine(3) $sLeaf(1) $queueSpineSwitch
# $ns ecn-threshold $sSpine(3) $sLeaf(1) [expr int($queueSpineSwitch*$ecnThresholdPortion)]


# 2 -> 0
$ns simplex-link $sLeaf(2) $sSpine(0) $linkRate 0.00002s $queueManage	
$ns queue-limit $sLeaf(2) $sSpine(0) $queueLeafSwitch
$ns ecn-threshold $sLeaf(2) $sSpine(0) [expr int($queueLeafSwitch*$ecnThresholdPortion)]
$ns simplex-link $sSpine(0) $sLeaf(2) $linkRate 0.00002s $queueManage
$ns queue-limit $sSpine(0) $sLeaf(2) $queueSpineSwitch
$ns ecn-threshold $sSpine(0) $sLeaf(2) [expr int($queueSpineSwitch*$ecnThresholdPortion)]

set queueH [[$ns link $sSpine(0) $sLeaf(2)] queue]
# $queueH monitor-QueueLen
$queueH monitor-FlowSpeed
$queueH monitor-FlowPath

# 2 -> 1
$ns simplex-link $sLeaf(2) $sSpine(1) $linkRate 0.00002s $queueManage	
$ns queue-limit $sLeaf(2) $sSpine(1) $queueLeafSwitch
$ns ecn-threshold $sLeaf(2) $sSpine(1) [expr int($queueLeafSwitch*$ecnThresholdPortion)]
$ns simplex-link $sSpine(1) $sLeaf(2) $linkRate 0.00002s $queueManage
$ns queue-limit $sSpine(1) $sLeaf(2) $queueSpineSwitch
$ns ecn-threshold $sSpine(1) $sLeaf(2) [expr int($queueSpineSwitch*$ecnThresholdPortion)]

set queueI [[$ns link $sSpine(1) $sLeaf(2)] queue]
# $queueI monitor-QueueLen
$queueI monitor-FlowSpeed
$queueI monitor-FlowPath

# $sLeaf(0) conga-queue "0" $queueA
# $sLeaf(0) conga-queue "1" $queueB
# $sLeaf(0) conga-queue "2" $queueC
# $sLeaf(0) conga-queue "3" $queueD

# $sLeaf(0) conga-route-debug 1
# $sLeaf(0) conga-packet-debug 1

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

set rwndSize 4000
Agent/TCP set window_ 4000
Agent/TCP set maxcwnd_ 4000
Agent/TCP set minrto_ 5e-3
Agent/TCP set tcpTick_ 1e-5
Agent/TCP set rtxcur_init_ 5e-3
Agent/TCP set windowInit_ 16

Agent/TCP set maxRto_	3

Agent/TCP set max_ssthresh_ 20000

Agent/TCPSink set ecn_syn_ true
Agent/TCP set ecn_ 1
Agent/TCP set old_ecn_ 1

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

		# $tcp($currentInsertingConnID) ecmp-hash-key [expr abs(int([$rnd value]*0x10000))]

		$tcp($currentInsertingConnID) ecmp-hash-key 12345

		# $tcp($currentInsertingConnID) monitor-spare-window

		# $tcp($currentInsertingConnID) monitor-Sequence
		$tcp($currentInsertingConnID) monitor-Speed
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


set limitStartTime [expr $latestFlowStartTime + 0.035]
set limitEndTime [expr $latestFlowStartTime + 0.05]
puts "$limitStartTime,$limitEndTime"


# $ns at $limitStartTime "$ns bandwidth $sLeaf(0) $sSpine(0) 100M simplex"
# $ns at $limitEndTime "$ns bandwidth $sLeaf(0) $sSpine(0) 250M simplex"

# set limitLink1 [$ns link $sLeaf(0) $sSpine(1)]
# $ns at $limitStartTime "$limitLink1 set bandwidth_ 100M"
# $ns at $limitEndTime "$limitLink1 set bandwidth_ 250M"
# set limitLink2 [$ns link $sLeaf(0) $sSpine(2)]
# $ns at $limitStartTime "$limitLink2 set bandwidth_ 100M"
# $ns at $limitEndTime "$limitLink2 set bandwidth_ 250M"
# set limitLink3 [$ns link $sLeaf(0) $sSpine(3)]
# $ns at $limitStartTime "$limitLink3 set bandwidth_ 100M"
# $ns at $limitEndTime "$limitLink3 set bandwidth_ 250M"



set timeLength [expr $latestFlowStartTime+20]


$ns at $timeLength "finish"
$ns run