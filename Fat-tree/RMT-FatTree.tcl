if { $argc !=12 } {
       puts "Arguments: tcpversion rwnd_size linkDelay(ms) 
       flowNumber switchPortNumber linkRate switchDownPortNumber linkAccessRate switchLayer 
       methodName(ECMP|MPTCP|RMPTCP-RETRAN|RMPTCP-CODE|RTCP-RETRAN|RTCP-CODE|REACTIVE|CORRECTIVE|PROACTIVE|MPTCP-1|REPFLOW|RMPTCP-CODE-REACTIVE|RMPTCP-RETRAN-REACTIVE) 
       failureNumber subFlowNumber"
       exit
}
 
set tcpVersion [lindex $argv 0]
#tcp version

set rwndSize [lindex $argv 1]
#rwnd size in packets

set linkDelay [lindex $argv 2]
#linkDelay in ms

set flowNumber [lindex $argv 3]
#how many flows in total generated

set switchPortNumber [lindex $argv 4]
set linkRate [lindex $argv 5]

set switchDownPortNumber [lindex $argv 6]
set linkAccessRate [lindex $argv 7]
 
set switchLayer [lindex $argv 8]

set methodName [lindex $argv 9]

set failureNumber [lindex $argv 10]
##   1 for 0.2%lossy,    2 for 95%lossy

set subFlowNumber [lindex $argv 11]


if { $methodName=="RTCP-RETRAN"	
	|| $methodName=="RTCP-CODE"
	|| $methodName=="MPTCP-1" } {
	set subFlowNumber 1
} elseif {$methodName=="REPFLOW"} {
	set subFlowNumber 2
} else {
	set subFlowNumber $subFlowNumber
}

set RMTThreshold 1E6



set intialCwnd 16

if {$methodName!="ECMP"
	&& $methodName!="MPTCP"
	&& $methodName!="RMPTCP-RETRAN"
	&& $methodName!="RMPTCP-CODE"
	&& $methodName!="RTCP-RETRAN"
	&& $methodName!="RTCP-CODE"
	&& $methodName!="REACTIVE"
	&& $methodName!="CORRECTIVE"
	&& $methodName!="PROACTIVE"
	&& $methodName!="MPTCP-1" 
	&& $methodName!="REPFLOW" 
	&& $methodName!="RMPTCP-CODE-REACTIVE" 
	&& $methodName!="RMPTCP-RETRAN-REACTIVE" } {
		puts "methodName=$methodName illegal!! Default to be ECMP"
		exit
	}

if {[expr $switchPortNumber%2]!=0} {
	puts "switchPortNumber must be even!"
	exit
}

set serverNumber [expr int($switchPortNumber*pow($switchPortNumber/2,$switchLayer-2))*$switchDownPortNumber]
set nonTopSwitchNumber [expr int($serverNumber/$switchDownPortNumber)]
set topSwitchNumber [expr int($nonTopSwitchNumber/2)]

puts "serverNumber:$serverNumber topSwitchNumber:$topSwitchNumber nonTopSwitchNumber:$nonTopSwitchNumber"

set ns [new Simulator] 

# $ns set flowNumber_ $flowNumber

$ns set randSeed_ [clock seconds]
set randSeed 1439278075

$ns set randSeed_ $randSeed
$ns enable-seed

set random_generator [ new RNG ]
$random_generator seed $randSeed
set rnd [new RandomVariable/Uniform]
$rnd use-rng $random_generator
$rnd set min_ 0
$rnd set max_ 1


set programStartTime [clock seconds]

Node set multiPath_ 1

set nd [open outAllTrace-$tcpVersion-$rwndSize.tr w]
# $ns trace-all $nd

set fTraffic [open InputTraffic-$serverNumber-[expr $flowNumber].tr r]
set fFCT [open $tcpVersion-$rwndSize-fct.tr w]
set fFlowThroughput [open $tcpVersion-$rwndSize-flowthroughput.tr w]

set fTimeout [open $tcpVersion-$rwndSize-timeout.tr w]

# set fInputTraffic [open InputFlow-$serverNumber-[expr $flowNumber].tr w]

proc finish {} {
	global n sLeaf sSpine switchLayer topSwitchNumber switchPortNumber switchDownPortNumber leafDownPortNumber nonTopSwitchNumber leafUpPortNumber
	global ns nd tcp timeLength tcpsink serverNumber flowNumber ftpsink fTraffic fFCT fFlowThroughput linkDelay
	global programStartTime methodName subFlowNumber em fTimeout failureNumber

	set totalRetransTimes 0
	set totalTimeoutTimes 0
	set totalParityRetransTimes 0

	for {set i 0} {$i<$flowNumber} {incr i} {
		set flowTimeoutTimes 0

		if { $methodName!="MPTCP" 
		&& $methodName!="RMPTCP-RETRAN" 
		&& $methodName!="RMPTCP-CODE"
		&& $methodName!="RTCP-RETRAN" 
		&& $methodName!="RTCP-CODE"
		&& $methodName!="MPTCP-1" 
		&& $methodName!="RMPTCP-CODE-REACTIVE" 
		&& $methodName!="RMPTCP-RETRAN-REACTIVE"
		&& $methodName!="REPFLOW"} {
			set totalRetransTimes [expr $totalRetransTimes+[$tcp($i) set retransTimes]+[$tcpsink($i) set retransTimes]]
			set totalTimeoutTimes [expr $totalTimeoutTimes+[$tcp($i) set timeOutTimes]+[$tcpsink($i) set timeOutTimes]]

			set flowTimeoutTimes [expr [$tcp($i) set timeOutTimes]+[$tcpsink($i) set timeOutTimes]]
		} else {
			if {[info exists tcp($i,0)]} {
				for {set p 0} {$p<$subFlowNumber} {incr p} {
					set totalRetransTimes [expr $totalRetransTimes+[$tcp($i,$p) set retransTimes]+[$tcpsink($i,$p) set retransTimes]]
					set totalTimeoutTimes [expr $totalTimeoutTimes+[$tcp($i,$p) set timeOutTimes]+[$tcpsink($i,$p) set timeOutTimes]]

					set flowTimeoutTimes [expr $flowTimeoutTimes + [$tcp($i,$p) set timeOutTimes]+[$tcpsink($i,$p) set timeOutTimes]]
				}
			} else {
				set totalRetransTimes [expr $totalRetransTimes+[$tcp($i) set retransTimes]+[$tcpsink($i) set retransTimes]]
				set totalTimeoutTimes [expr $totalTimeoutTimes+[$tcp($i) set timeOutTimes]+[$tcpsink($i) set timeOutTimes]]

				set flowTimeoutTimes [expr [$tcp($i) set timeOutTimes]+[$tcpsink($i) set timeOutTimes]]
			}
		}

		if { $methodName=="RMPTCP-RETRAN" 
		|| $methodName=="RMPTCP-CODE"
		|| $methodName=="RTCP-RETRAN" 
		|| $methodName=="RTCP-CODE" 
		|| $methodName=="RMPTCP-CODE-REACTIVE"
		|| $methodName=="RMPTCP-RETRAN-REACTIVE"
		|| $methodName=="REPFLOW"} {
			if {[info exists tcp($i,0)]} {
				for {set p 0} {$p<$subFlowNumber} {incr p} {
					set totalParityRetransTimes  [expr $totalParityRetransTimes+[$tcp($i,$p) set parityRetransTimes]+[$tcpsink($i,$p) set parityRetransTimes]]
				}
			}
		}

		if {$flowTimeoutTimes > 0} {
			puts $fTimeout "$i $flowTimeoutTimes"
		}
	}

	set totalPacketDropped 0
	set totalParityDropped 0
	#leaf(i,x) <-> leaf(i+1,x)
	for {set i 0} {$i<[expr $switchLayer-2]} {incr i} {
	#puts "initializing links between leaf($i,x) and leaf([expr $i+1],x) switches...."
		set switchesPerThisPod [expr int(pow($leafUpPortNumber,$i))]
		set switchesPerUpPod [expr int(pow($leafUpPortNumber,$i+1))]
		for {set j 0} {$j<$nonTopSwitchNumber} {incr j} {
			for {set k 0} {$k<$leafUpPortNumber} {incr k} {
				set upPod [expr int($j/$switchesPerUpPod)] 
				set m [expr $upPod*$switchesPerUpPod+($j%$switchesPerThisPod)*$leafUpPortNumber+$k]
				set queue1 [[$ns link $sLeaf($i,$j) $sLeaf([expr $i+1],$m)] queue]
				set queue2 [[$ns link $sLeaf([expr $i+1],$m) $sLeaf($i,$j)] queue]
				set totalPacketDropped [expr $totalPacketDropped+[$queue1 set packetDropped_]+[$queue2 set packetDropped_]]
				set totalParityDropped [expr $totalParityDropped+[$queue1 set parityDropped_]+[$queue2 set parityDropped_]]
			}
		}
	}


	#spine <-> leaf([expr $switchLayer-2],x)
	#puts "initializing links between leaf([expr $switchLayer-2],x) and spine switches...."
	for {set i 0} {$i<$nonTopSwitchNumber} {incr i} {
		for {set j 0} {$j<$leafUpPortNumber} {incr j} {
			set k [expr ($i*$leafUpPortNumber+$j)%$topSwitchNumber]
			set queue1 [[$ns link $sLeaf([expr $switchLayer-2],$i) $sSpine($k)] queue]
			set queue2 [[$ns link $sSpine($k) $sLeaf([expr $switchLayer-2],$i)] queue]
			set totalPacketDropped [expr $totalPacketDropped+[$queue1 set packetDropped_]+[$queue2 set packetDropped_]]
			set totalParityDropped [expr $totalParityDropped+[$queue1 set parityDropped_]+[$queue2 set parityDropped_]]
		}
	}

	set packetDroppedAt_IO 0
	set packetDroppedAt_O 0
	set parityDroppedAt_IO 0
	set parityDroppedAt_O 0
	for {set i 0} {$i<$nonTopSwitchNumber} {incr i} {
		for {set j 0} {$j<$leafDownPortNumber} {incr j} {
			set k [expr $i*$leafDownPortNumber+$j]
			set queue1 [[$ns link $sLeaf(0,$i) $n($k)] queue]
			set queue2 [[$ns link $n($k) $sLeaf(0,$i)] queue]
			set packetDroppedAt_O [expr $packetDroppedAt_O+[$queue1 set packetDropped_]]
			set packetDroppedAt_IO [expr $packetDroppedAt_IO+[$queue1 set packetDropped_]+[$queue2 set packetDropped_]]
			set totalPacketDropped [expr $totalPacketDropped+[$queue1 set packetDropped_]+[$queue2 set packetDropped_]]
			set parityDroppedAt_O [expr $parityDroppedAt_O+[$queue1 set parityDropped_]]
			set parityDroppedAt_IO [expr $parityDroppedAt_IO+[$queue1 set parityDropped_]+[$queue2 set parityDropped_]]
			set totalParityDropped [expr $totalParityDropped+[$queue1 set parityDropped_]+[$queue2 set parityDropped_]]
		}
	}


	set totalThroughput 0
	set throughput 0
	set totalFCT 0
	set activeFlowNumber 0
	set completedFlowNumber 0
	set uncompletedFlowNumber 0
	set maxFCT 0
	set maxTP 0
	if {$methodName=="REPFLOW" && [info exists ftpsink(0,0)]} {
		set minFCT [$ftpsink(0,0) set fct_]
		set minTP [expr [$ftpsink(0,0) set flowSize_]*8/[$ftpsink(0,0) set fct_]/1E6] 
	} else {
		set minFCT [$ftpsink(0) set fct_]
		set minTP [expr [$ftpsink(0) set flowSize_]*8/[$ftpsink(0) set fct_]/1E6] 
	}
	
	
	for {set i 0} {$i<$flowNumber} {incr i} {
		if {$methodName=="REPFLOW" && [info exists ftpsink($i,0)]} {
			set fctFlow($i) [$ftpsink($i,0) set fct_]
			set fctFlowSize($i) [$ftpsink($i,0) set flowSize_]
			set fctFlowStartTime($i) [$ftpsink($i,0) set flowStartTime_]
			set fctFlowReceivedBytes($i) [$ftpsink($i,0) set receivedBytes_]
			for {set p 0} {$p<$subFlowNumber} {incr p} {
				if {[$ftpsink($i,$p) set fct_] < $fctFlow($i)} {
					set fctFlow($i) [$ftpsink($i,$p) set fct_]
				}
				if {[$ftpsink($i,$p) set receivedBytes_] > $fctFlowReceivedBytes($i)} {
					set $fctFlowReceivedBytes($i) [$ftpsink($i,$p) set receivedBytes_]
				}
			}
		} else {
			set fctFlow($i) [$ftpsink($i) set fct_]
			set fctFlowSize($i) [$ftpsink($i) set flowSize_]
			set fctFlowStartTime($i) [$ftpsink($i) set flowStartTime_]
			set fctFlowReceivedBytes($i) [$ftpsink($i) set receivedBytes_]
		}

		if {$fctFlow($i)>0} {
			set activeFlowNumber [expr $activeFlowNumber+1]
			set completedFlowNumber [expr $completedFlowNumber+1]
	#				 set theoreticalFCT [expr [$ftpsink($i) set flowSize_]/1E10+12*$linkDelay/1E3]
			set totalFCT [expr {$totalFCT+$fctFlow($i)}]
			set throughput [expr $fctFlowSize($i)*8/$fctFlow($i)] 	
	#				puts [format "flow%d(%d.%d-%d.%d) complete : FCT %f s (start:%f,stop:%f), throughput %f Mbps" $i [$tcp($i) set agent_addr_] [$tcp($i) set agent_port_] [$tcp($i) set dst_addr_] [$tcp($i) set dst_port_] [$ftpsink($i) set fct_] [$ftpsink($i) set flowStartTime_] [$ftpsink($i) set flowStopTime_] [expr $throughput/1E6]]
			puts $fFCT "$fctFlow($i)"
			puts $fFlowThroughput "[expr $throughput/1E6]"
			if {$fctFlow($i) < $minFCT} {
				set minFCT $fctFlow($i)
			}
			if {$fctFlow($i) > $maxFCT} {
				set maxFCT $fctFlow($i)
			}
			if {[expr $throughput/1E6] < $minTP} {
				set minTP [expr $throughput/1E6]
			}
			if {[expr $throughput/1E6] > $maxTP} {
				set maxTP [expr $throughput/1E6]
			}
		} elseif {$fctFlowStartTime($i)<$timeLength} {
			set activeFlowNumber [expr $activeFlowNumber+1]
			set uncompletedFlowNumber [expr $uncompletedFlowNumber+1]
			set throughput [expr $fctFlowReceivedBytes($i)*8/([$ns now]-$fctFlowStartTime($i))]
			
			puts [format "flow%d uncomplete : start at %f s, transfered %d bytes yet, throughput %f Mbps" $i $fctFlowStartTime($i) $fctFlowReceivedBytes($i) [expr $throughput/1E6]]
			
			puts $fFlowThroughput "[expr $throughput/1E6]"
			puts $fFCT "[expr [$ns now]-$fctFlowStartTime($i)]"
			set totalFCT [expr {$totalFCT+[expr [$ns now]-$fctFlowStartTime($i)]}]
		} else {
			set throughput 0
		}
		set totalThroughput [expr {$totalThroughput+$throughput}]
	}

	if { $failureNumber>0 } {
		set totalPacketDropped [expr $totalPacketDropped+[$em set packetDropped_]]
	}

	puts "---------------------------------------------------------"
	puts "randSeed is [$ns set randSeed_]"
	puts "now :[$ns now]"
	puts "$activeFlowNumber valid flows generated, $completedFlowNumber completed, $uncompletedFlowNumber uncompleted"


	set avgFCT [expr $totalFCT/$completedFlowNumber]
	set avgTP [expr $totalThroughput/$activeFlowNumber/1E6]
	set imbalanceFCT 0
	set imbalanceTP 0

	if {$avgFCT>0 && $avgTP>0} {
		set imbalanceFCT [expr ($maxFCT-$minFCT)/$avgFCT]
		set imbalanceTP [expr ($maxTP-$minTP)/$avgTP]
	}

	puts "average FCT is $avgFCT s, average throughput is $avgTP Mbps, imbalanceFCT=$imbalanceFCT, imbalanceTP=$imbalanceTP"
	puts "totalPacketDropped=$totalPacketDropped (totalParityDropped=$totalParityDropped), packetDroppedAt_IO=$packetDroppedAt_IO ($parityDroppedAt_IO parity), packetDroppedAt_O=$packetDroppedAt_O ($parityDroppedAt_O parity)"
	if { $failureNumber>0 } {
		puts [format "Packets dropped in lossy link is: lossyPacketDropped=%d" \
		[expr [$em set packetDropped_]]]
	}

	puts [format "totalPacketGenerated=%d" [$ns set totalPacketsGenerated]]

	if { $methodName=="RMPTCP-RETRAN" 
	|| $methodName=="RMPTCP-CODE"
	|| $methodName=="RTCP-RETRAN"
	|| $methodName=="RTCP-CODE"
	|| $methodName=="CORRECTIVE"
	|| $methodName=="PROACTIVE"
	|| $methodName=="REPFLOW"
	|| $methodName=="RMPTCP-CODE-REACTIVE"
	|| $methodName=="RMPTCP-RETRAN-REACTIVE"} {
		puts [format "ParityNumber=%d, DecodeTimes=%d" \
		[$ns set clbParityNumber] \
		[$ns set clbDecodeTimes]]

	}

	puts "totalRetransTimes=$totalRetransTimes, totalTimeoutTimes=$totalTimeoutTimes"
	

	set programEndTime [clock seconds]
	set runTime [expr $programEndTime-$programStartTime]
	set hour [expr int($runTime/3600)]
	set minute [expr int(($runTime-$hour*3600)/60)]
	set second [expr int($runTime)%60]
	puts [format "program run for %dh:%dm:%ds" $hour $minute $second]
	$ns flush-trace
#	close $nd
	close $fTraffic 
	close $fFCT 
	close $fFlowThroughput
	exit 0
}

for {set i 0} {$i<$serverNumber} {incr i} {
	set n($i) [$ns node]
	$n($i) enable-salt
}

for {set i 0} {$i<[expr $switchLayer-1]} {incr i} {
	for {set j 0} {$j<$nonTopSwitchNumber} {incr j} {
		set sLeaf($i,$j) [$ns node]
		$sLeaf($i,$j) enable-salt
	}
}
for {set i 0} {$i<$topSwitchNumber} {incr i} {
	set sSpine($i) [$ns node]
	$sSpine($i) enable-salt
}

set queueSpineSwitch 200
#set queueLeafSwitch 48
set queueLeafSwitch 200

set queueManage "DropTail"
set ecnThresholdPortion 0.3

set sendBufferSize [expr 10*$rwndSize]
set ecnThresholdPortionLeaf 0.3

set leafUpPortNumber [expr int($switchPortNumber/2)]
set leafDownPortNumber $switchDownPortNumber
#leaf(0,x) <-> node
# puts "initializing links between servers and leaf(0,x) switches...."
for {set i 0} {$i<$nonTopSwitchNumber} {incr i} {
	for {set j 0} {$j<$leafDownPortNumber} {incr j} {
		set k [expr $i*$leafDownPortNumber+$j]
		$ns simplex-link $n($k) $sLeaf(0,$i) $linkAccessRate [expr $linkDelay*1E-3]s DropTail
		$ns queue-limit $n($k) $sLeaf(0,$i) $sendBufferSize
		$ns ecn-threshold $n($k) $sLeaf(0,$i) $sendBufferSize
		$ns simplex-link $sLeaf(0,$i) $n($k) $linkAccessRate [expr $linkDelay*1E-3]s $queueManage
		$ns queue-limit $sLeaf(0,$i) $n($k) $queueLeafSwitch
		$ns ecn-threshold $sLeaf(0,$i) $n($k) [expr int($queueLeafSwitch*$ecnThresholdPortionLeaf)]
		# puts "node([$n($k) set address_]) <-> leaf(0,$i)([$sLeaf(0,$i) set address_]) "

		set queue1 [[$ns link $sLeaf(0,$i) $n($k)] queue]
		# $queue1 monitor-E2EDelay
		# $queue1 monitor-QueueLen
		# $queue1 monitor-Drop

		set queue2 [[$ns link $n($k) $sLeaf(0,$i)] queue]
		# $queue2 monitor-QueueLen
		# $queue2 monitor-Drop
	}
}

 
#leaf(i,x) <-> leaf(i+1,x)
for {set i 0} {$i<[expr $switchLayer-2]} {incr i} {
# puts "initializing links between leaf($i,x) and leaf([expr $i+1],x) switches...."
	set switchesPerThisPod [expr int(pow($leafUpPortNumber,$i))]
	set switchesPerUpPod [expr int(pow($leafUpPortNumber,$i+1))]
	for {set j 0} {$j<$nonTopSwitchNumber} {incr j} {
		for {set k 0} {$k<$leafUpPortNumber} {incr k} {
			set upPod [expr int($j/$switchesPerUpPod)] 
				set m [expr $upPod*$switchesPerUpPod+($j%$switchesPerThisPod)*$leafUpPortNumber+$k]
				$ns simplex-link $sLeaf($i,$j) $sLeaf([expr $i+1],$m) $linkRate [expr $linkDelay*1E-3]s $queueManage
				$ns queue-limit $sLeaf($i,$j) $sLeaf([expr $i+1],$m) $queueLeafSwitch
				$ns ecn-threshold $sLeaf($i,$j) $sLeaf([expr $i+1],$m) [expr int($queueSpineSwitch*$ecnThresholdPortion)]
				$ns simplex-link $sLeaf([expr $i+1],$m) $sLeaf($i,$j) $linkRate [expr $linkDelay*1E-3]s $queueManage
				$ns queue-limit $sLeaf([expr $i+1],$m) $sLeaf($i,$j) $queueLeafSwitch
				$ns ecn-threshold $sLeaf([expr $i+1],$m) $sLeaf($i,$j) [expr int($queueSpineSwitch*$ecnThresholdPortion)]
				# puts "leaf($i,$j)([$sLeaf($i,$j) set address_]) <-> leaf([expr $i+1],$m)([$sLeaf([expr $i+1],$m) set address_])"

				set queue1 [[$ns link $sLeaf([expr $i+1],$m) $sLeaf($i,$j)] queue]
				# $queue1 monitor-QueueLen
				# $queue1 monitor-Drop

				set queue2 [[$ns link  $sLeaf($i,$j) $sLeaf([expr $i+1],$m)] queue]
				# $queue2 tag-timestamp
				# $queue2 monitor-QueueLen
				# $queue2 monitor-FlowPath
				# $queue2 monitor-Drop
		}
	}
}

set em ""

# set em1 [new ErrorModel]
# $em1 set rate_ 0
# $em1 unit pkt
# $em1 ranvar $rnd
# $em1 drop-target [new Agent/Null]
# $ns link-lossmodel $em1 $sLeaf(0) $sSpine(0)

####Lossy link!
if { $failureNumber>0 } {
	set em [new ErrorModel]
	if { $failureNumber==1 } {
		$em set rate_ 0.02
	} elseif { $failureNumber==2 } {
		$em set rate_ 0.95
	}
	$em unit pkt
	$em ranvar $rnd
	$em drop-target [new Agent/Null]
}
##########


#spine <-> leaf([expr $switchLayer-2],x)
# puts "initializing links between leaf([expr $switchLayer-2],x) and spine switches...."
for {set i 0} {$i<$nonTopSwitchNumber} {incr i} {
	for {set j 0} {$j<$leafUpPortNumber} {incr j} {
		set k [expr ($i*$leafUpPortNumber+$j)%$topSwitchNumber]
			$ns simplex-link $sLeaf([expr $switchLayer-2],$i) $sSpine($k) $linkRate [expr $linkDelay*1E-3]s $queueManage	
			$ns queue-limit $sLeaf([expr $switchLayer-2],$i) $sSpine($k) $queueLeafSwitch
			$ns ecn-threshold $sLeaf([expr $switchLayer-2],$i) $sSpine($k) [expr int($queueSpineSwitch*$ecnThresholdPortion)]
			$ns simplex-link $sSpine($k) $sLeaf([expr $switchLayer-2],$i) $linkRate [expr $linkDelay*1E-3]s $queueManage
			$ns queue-limit $sSpine($k) $sLeaf([expr $switchLayer-2],$i) $queueSpineSwitch
			$ns ecn-threshold $sSpine($k) $sLeaf([expr $switchLayer-2],$i) [expr int($queueSpineSwitch*$ecnThresholdPortion)]

			# puts "leaf([expr $switchLayer-2],$i)([$sLeaf([expr $switchLayer-2],$i) set address_]) <-> spine($k)([$sSpine($k) set address_])"

			set queue1 [[$ns link $sSpine($k) $sLeaf([expr $switchLayer-2],$i)] queue]
			# $queue1 monitor-QueueLen
			# $queue1 monitor-Drop

			set queue2 [[$ns link  $sLeaf([expr $switchLayer-2],$i) $sSpine($k)] queue]
			# $queue2 tag-timestamp
			# $queue2 monitor-QueueLen
			# $queue2 monitor-FlowPath
			# $queue2 monitor-Drop

			if {$k==0 && $failureNumber>0} {
				$ns link-lossmodel $em $sLeaf([expr $switchLayer-2],$i) $sSpine($k)
				$queue2 monitor-FlowPath
				# $em monitor-Drop
			}
	}
}


Agent/TCP set window_ $rwndSize
Agent/TCP set maxcwnd_ $rwndSize
Agent/TCP set minrto_ 5e-3
Agent/TCP set tcpTick_ 1e-5
Agent/TCP set rtxcur_init_ 5e-3
Agent/TCP set windowInit_ $intialCwnd

Agent/TCP set maxRto_	3

Agent/TCP set max_ssthresh_ 200

# Agent/TCPSink set ecn_syn_ true
Agent/TCP set ecn_ 1
Agent/TCP set old_ecn_ 1

set latestFlowStartTime 0
for {set k 0} {$k<$flowNumber} {incr k} {
	gets $fTraffic line
	set fields [split $line]
	
	lassign $fields srcNode dstNode startTime flowSize
	if {$dstNode == $serverNumber || $dstNode==$srcNode} {
		puts "dstNode=$dstNode error!"
	}
	if {$startTime > $latestFlowStartTime} {
		set latestFlowStartTime $startTime
	}


	if { ($methodName=="MPTCP" 
	|| $methodName=="RTCP-RETRAN"
	|| $methodName=="RTCP-CODE"
	|| $methodName=="MPTCP-1" 
	|| $methodName=="RMPTCP-RETRAN"
	|| $methodName=="RMPTCP-CODE"
	|| $methodName=="RMPTCP-CODE-REACTIVE" 
	|| $methodName=="RMPTCP-RETRAN-REACTIVE") && $flowSize<=$RMTThreshold } {
		set mptcp($k) [new Agent/MPTCP]
		set mptcpsink($k) [new Agent/MPTCP]
		for {set j 0} {$j<$subFlowNumber} {incr j} {
			set tcp($k,$j) [new Agent/TCP/FullTcp/Sack/Multipath]
			$tcp($k,$j) set window_ $rwndSize
			$tcp($k,$j) set maxcwnd_ $rwndSize
			
			$tcp($k,$j) set segsize_ 1460


			$tcp($k,$j) ecmp-hash-key [expr int([$rnd value]*0x100000000)]
			
			$ns attach-agent $n($srcNode) $tcp($k,$j)
			$mptcp($k) attach-tcp $tcp($k,$j) $n($srcNode)

			set tcpsink($k,$j) [new Agent/TCP/FullTcp/Sack/Multipath]
			$tcpsink($k,$j) set segsize_ [$tcp($k,$j) set segsize_]
			$ns attach-agent $n($dstNode) $tcpsink($k,$j)

			$mptcpsink($k) attach-tcp $tcpsink($k,$j) $n($dstNode)

			$tcp($k,$j) set windowInit_ [expr int($intialCwnd/$subFlowNumber)]

			$mptcp($k) add-multihome-destination [$tcpsink($k,$j) set agent_addr_] [$tcpsink($k,$j) set agent_port_]
			$mptcpsink($k) add-multihome-destination [$tcp($k,$j) set agent_addr_] [$tcp($k,$j) set agent_port_]

			if {$methodName=="RMPTCP-RETRAN" || $methodName=="RTCP-RETRAN" || $methodName=="RMPTCP-RETRAN-REACTIVE"} {
				$mptcp($k) rmptcp 0
				$mptcpsink($k) rmptcp-sink 0
			}
			if {$methodName=="RMPTCP-CODE" || $methodName=="RTCP-CODE" || $methodName=="RMPTCP-CODE-REACTIVE"} {
				$mptcp($k) rmptcp 3
				$mptcpsink($k) rmptcp-sink 3
			}

			if {$methodName=="RMPTCP-CODE-REACTIVE" || $methodName=="RMPTCP-RETRAN-REACTIVE"} {
				$tcp($k,$j) reactive
			}

			# $tcp($k,$j) monitor-Sequence
			# $tcpsink($k,$j) monitor-Sequence

			# $tcp($k,$j) monitor-spare-window

			$tcp($k,$j) no-syn sender
			$tcpsink($k,$j) no-syn receiver

			$tcp($k,$j) flow-id $k
			$tcpsink($k,$j) flow-id $k
		}

		
		set subFLowList {}

		for {set j 0} {$j<$subFlowNumber} {incr j} {
			lappend subFLowList $j
		}
		# puts $subFLowList

		for {set j 0} {$j<$subFlowNumber} {incr j} {
			# set ix [expr {int([$rnd value]*[llength $subFLowList])}]
		 #    set subFlowID [lindex $subFLowList $ix]
		 #    set subFLowList [lreplace $subFLowList $ix $ix]
			# $ns connect $tcp($k,$j) $tcpsink($k,$subFlowID)
			$ns connect $tcp($k,$j) $tcpsink($k,$j)
		}

		$mptcpsink($k) listen

		set ftp($k) [new Application/FTP]
		$ftp($k) attach-agent $mptcp($k)
		set ftpsink($k) [new Application/FTPSink]
		for {set j 0} {$j<$subFlowNumber} {incr j} {
			$ftpsink($k) attach-agent $tcpsink($k,$j)
		}
	} elseif {$methodName=="REPFLOW" && $flowSize <= $RMTThreshold} {
		for {set j 0} {$j<$subFlowNumber} {incr j} {
			set tcp($k,$j) [new Agent/TCP/FullTcp/Sack]
			set tcpsink($k,$j) [new Agent/TCP/FullTcp/Sack]

			$tcp($k,$j) ecmp-hash-key [expr int([$rnd value]*0x100000000)]

			# $tcp($k) monitor-spare-window

			# $tcp($k,$j) monitor-Sequence
			# $tcpsink($k,$j) monitor-Sequence
			# $tcp($k) set windowInit_ [expr [$tcp($k) set windowInit_]*$subFlowNumber]

			$tcp($k,$j) no-syn sender
			$tcpsink($k,$j) no-syn receiver

			$tcp($k,$j) set window_ $rwndSize
			$tcp($k,$j) set maxcwnd_ $rwndSize

			$tcp($k,$j) set segsize_ 1460
			
			$tcpsink($k,$j) set segsize_ [$tcp($k,$j) set segsize_]

			$ns attach-agent $n($srcNode) $tcp($k,$j)
			$ns attach-agent $n($dstNode) $tcpsink($k,$j)
			$ns connect $tcp($k,$j) $tcpsink($k,$j)

			$tcp($k,$j) flow-id $k
			$tcpsink($k,$j) flow-id $k

			$tcpsink($k,$j) listen

			set ftp($k,$j) [new Application/FTP]
			$ftp($k,$j) attach-agent $tcp($k,$j)
			set ftpsink($k,$j) [new Application/FTPSink]
			$ftpsink($k,$j) attach-agent $tcpsink($k,$j)

			$ftpsink($k,$j) flow-size $flowSize
			$ftpsink($k,$j) flow-id $k
			$ftpsink($k,$j) flow-start-time $startTime
			 # puts [format "%4d %d.%d %d.%d $startTime $flowSize" $k [$tcp($k) set agent_addr_] [$tcp($k) set agent_port_] [$tcp($k) set dst_addr_] [$tcp($k) set dst_port_]]
			$ns at $startTime "$ftp($k,$j) send $flowSize"
		}
		continue
	} else {
		if { ($methodName=="REACTIVE"
			|| $methodName=="CORRECTIVE"
			|| $methodName=="PROACTIVE") && $flowSize <= $RMTThreshold } {
			set tcp($k) [new Agent/TCP/FullTcp/Sack/GA]
			set tcpsink($k) [new Agent/TCP/FullTcp/Sack/GA]

			if {$methodName=="REACTIVE"} {
				$tcp($k) reactive
			}
			if {$methodName=="CORRECTIVE"} {
				$tcp($k) corrective
				$tcpsink($k) corrective-sink
			}
			if {$methodName=="PROACTIVE"} {
				$tcp($k) proactive
				$tcpsink($k) proactive-sink
			}
		} else {
			set tcp($k) [new Agent/TCP/FullTcp/Sack]
			set tcpsink($k) [new Agent/TCP/FullTcp/Sack]
		}
		
		# set tcp($k) [new Agent/TCP/Sack1]
		# set tcpsink($k) [new Agent/TCPSink]

		$tcp($k) ecmp-hash-key [expr int([$rnd value]*0x100000000)]

		# $tcp($k) monitor-spare-window

		# $tcp($k) monitor-Sequence
		# $tcpsink($k) monitor-Sequence
		# $tcp($k) set windowInit_ [expr [$tcp($k) set windowInit_]*$subFlowNumber]

		$tcp($k) no-syn sender
		$tcpsink($k) no-syn receiver

		$tcp($k) set window_ $rwndSize
		$tcp($k) set maxcwnd_ $rwndSize

		$tcp($k) set segsize_ 1460
		
		$tcpsink($k) set segsize_ [$tcp($k) set segsize_]

		$ns attach-agent $n($srcNode) $tcp($k)
		$ns attach-agent $n($dstNode) $tcpsink($k)
		$ns connect $tcp($k) $tcpsink($k)

		$tcp($k) flow-id $k
		$tcpsink($k) flow-id $k

		$tcpsink($k) listen

		set ftp($k) [new Application/FTP]
		$ftp($k) attach-agent $tcp($k)
		set ftpsink($k) [new Application/FTPSink]
		$ftpsink($k) attach-agent $tcpsink($k)
	}

	$ftpsink($k) flow-size $flowSize
	$ftpsink($k) flow-id $k
	$ftpsink($k) flow-start-time $startTime
	 # puts [format "%4d %d.%d %d.%d $startTime $flowSize" $k [$tcp($k) set agent_addr_] [$tcp($k) set agent_port_] [$tcp($k) set dst_addr_] [$tcp($k) set dst_port_]]
	$ns at $startTime "$ftp($k) send $flowSize"
	# $ns at [expr $startTime+1] "$ftp($k) send $flowSize"
}


#Agent/rtProto/DV set advertInterval 1000
#$ns rtproto DV
$ns set staticRoute_ 1

#########################################################
for {set i 0} {$i<$serverNumber} {incr i} {
	$n($i) init-routes-forExplicitRoute $serverNumber
}

for {set i 0} {$i<[expr $switchLayer-1]} {incr i} {
	for {set j 0} {$j<$nonTopSwitchNumber} {incr j} {
		$sLeaf($i,$j) init-routes-forExplicitRoute $serverNumber
	}
}
for {set i 0} {$i<$topSwitchNumber} {incr i} {
	$sSpine($i) init-routes-forExplicitRoute $serverNumber
}

set addRoutesStartTime [clock seconds]
puts "Start adding routes..............."
for {set i [expr $serverNumber-1]} {$i>=0} {incr i -1} {
	set startTime [clock seconds]
	set belongToWhichLeaf [expr int($i/$leafDownPortNumber)]
	for {set j 0} {$j<$serverNumber} {incr j} {
		if {$j != $i} {
			# puts "addExplicitRoute n($i) n($j) sLeaf(0,$belongToWhichLeaf)"
			$ns addExplicitRoute $n($i) $n($j) $sLeaf(0,$belongToWhichLeaf)
		}	
	}
	# puts "addExplicitRoute sLeaf(0,$belongToWhichLeaf) n($i) n($i)"
	$ns addExplicitRoute $sLeaf(0,$belongToWhichLeaf) $n($i) $n($i) 
}
puts "Adding routes for nodes completed!"

#leaf(i,x) <-> leaf(i+1,x)
for {set i 0} {$i<[expr $switchLayer-2]} {incr i} {
	# puts "initializing routes between leaf($i,x) and leaf([expr $i+1],x) switches...."
	set hostDownPerSwitch [expr $leafDownPortNumber*int(pow($switchPortNumber/2,$i))]
	set switchesPerThisPod [expr int(pow($leafUpPortNumber,$i))]
	set switchesPerUpPod [expr int(pow($leafUpPortNumber,$i+1))]
	for {set j 0} {$j<$nonTopSwitchNumber} {incr j} {
		set thisPodID [expr int($j/$switchesPerThisPod)]
		set minHost [expr $thisPodID*$hostDownPerSwitch]
		set maxHost [expr $thisPodID*$hostDownPerSwitch+$hostDownPerSwitch-1]
		for {set h 0} {$h<$serverNumber} {incr h} {
			if {$h <= $maxHost &&  $h >= $minHost} {
				for {set k 0} {$k<$leafUpPortNumber} {incr k} {
					set upPod [expr int($j/$switchesPerUpPod)] 
					set m [expr $upPod*$switchesPerUpPod+($j%$switchesPerThisPod)*$leafUpPortNumber+$k]
					# puts "addExplicitRoute sLeaf([expr $i+1],$m) n($h) sLeaf($i,$j)"
					$ns addExplicitRoute $sLeaf([expr $i+1],$m) $n($h) $sLeaf($i,$j)
				}		
			} else {
				for {set k 0} {$k<$leafUpPortNumber} {incr k} {
					set upPod [expr int($j/$switchesPerUpPod)] 
					set m [expr $upPod*$switchesPerUpPod+($j%$switchesPerThisPod)*$leafUpPortNumber+$k]
					# puts "addExplicitRoute sLeaf($i,$j) n($h) sLeaf([expr $i+1],$m)"
					$ns addExplicitRoute $sLeaf($i,$j) $n($h) $sLeaf([expr $i+1],$m)
				}
			}
		}
	}	
	# puts "Adding routes for leaf($i) completed!"
}



#spine <-> leaf([expr $switchLayer-2],x)
# puts "initializing routes between leaf([expr $switchLayer-2],x) and spine switches...."
set hostDownPerSwitch [expr $leafDownPortNumber*int(pow($switchPortNumber/2,$switchLayer-2))]
set switchesPerThisPod [expr int(pow($leafUpPortNumber,[expr $switchLayer-2]))]
for {set i 0} {$i<$nonTopSwitchNumber} {incr i} {
	set thisPodID [expr int($i/$switchesPerThisPod)]
	set minHost [expr $thisPodID*$hostDownPerSwitch]
	set maxHost [expr $thisPodID*$hostDownPerSwitch+$hostDownPerSwitch-1]
	for {set h 0} {$h<$serverNumber} {incr h} {
		set belongToWhichLeaf [expr int($h/$leafDownPortNumber)]
		if {$h <= $maxHost &&  $h >= $minHost} {
			for {set j 0} {$j<$leafUpPortNumber} {incr j} {
				set k [expr ($i*$leafUpPortNumber+$j)%$topSwitchNumber]
				# puts "addExplicitRoute sSpine($k) n($h) sLeaf([expr $switchLayer-2],$i)"
				$ns addExplicitRoute $sSpine($k) $n($h) $sLeaf([expr $switchLayer-2],$i) 
			}
		} else {
		for {set j 0} {$j<$leafUpPortNumber} {incr j} {
			set k [expr ($i*$leafUpPortNumber+$j)%$topSwitchNumber]
				# puts "addExplicitRoute sLeaf([expr $switchLayer-2],$i) n($h) sSpine($k)"
				$ns addExplicitRoute $sLeaf([expr $switchLayer-2],$i) $n($h) $sSpine($k)
			}
		}
	}
}	

set addRoutesEndTime [clock seconds]
set runTime [expr $addRoutesEndTime-$addRoutesStartTime]
set hour [expr int($runTime/3600)]
set minute [expr int(($runTime-$hour*3600)/60)]
set second [expr int($runTime)%60]
puts [format "Adding routes for leaf([expr $switchLayer-2]) and spine completed! Run for %dh:%dm:%ds" $hour $minute $second]

set timeLength [expr $latestFlowStartTime+10]

$ns at $timeLength "finish"
$ns run
