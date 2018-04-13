if { $argc !=13 } {
       puts "Arguments: tcpversion rwnd_size linkDelay(ms) 
       flowNumber switchPortNumber linkRate switchDownPortNumber linkAccessRate switchLayer 
       methodName(ECMP|MPTCP|RMPTCP-RETRAN|RMPTCP-CODE|RTCP-RETRAN|RTCP-CODE|REACTIVE|CORRECTIVE|PROACTIVE|MPTCP-1|REPFLOW|RMPTCP-CODE-REACTIVE|RMPTCP-RETRAN-REACTIVE) 
       failureNumber subFlowNumber randSeed"
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
##   1 for 2%lossy,    2 for 95%lossy,    3 for %4 loss rate, and 5% lossy links obeyed to the real measurement resutls


#### lossy link distribution
set LossyLinkDistribution(0) 0.22;  ### server to ToR
set LossyLinkDistribution(1) 0.24;  ### ToR to Agg
set LossyLinkDistribution(2) 0.54;  ### Agg to Core

set LossyLinkPortion 0.05; ### 5% links are lossy

set MaxLossRate 0.15;

#### loss rate distribution  
# loss rate [0.01~0.1], devided into 9 descrete bin:
# [0.01,0.02,0.03,....,0.1]
# set LossyRateDistribution(0) 0;  ##[0.01]
# set LossyRateDistribution(1) [expr 0.372*1000/670];  ##[0.02]
# set LossyRateDistribution(2) [expr 0.467*1000/670];  ##[0.03]
# set LossyRateDistribution(3) [expr 0.531*1000/670];  ##[0.04]
# set LossyRateDistribution(4) [expr 0.584*1000/670];  ##[0.05]
# set LossyRateDistribution(5) [expr 0.615*1000/670];  ##[0.06]
# set LossyRateDistribution(6) [expr 0.626*1000/670];  ##[0.07]
# set LossyRateDistribution(7) [expr 0.635*1000/670];  ##[0.08]
# set LossyRateDistribution(8) [expr 0.644*1000/670];  ##[0.09]
# set LossyRateDistribution(9) [expr 0.666*1000/670];  ##[0.10]
# set LossyRateDistribution(10) [expr 0.670*1000/670];  ##[0.11]

set MAX_CONNECTION_NUM 50


if { $methodName=="RTCP-RETRAN"	
	|| $methodName=="RTCP-CODE"
	|| $methodName=="MPTCP-1" } {
	set subFlowNumber 1
} elseif {$methodName=="REPFLOW"} {
	set subFlowNumber 2
} else {
	set subFlowNumber [lindex $argv 11]
}

# set randSeed [clock seconds]
set randSeed [lindex $argv 12]


##### For each method, above the following thresholds are normal TCP long connection flows
set RMTThreshold 2E9
set GAThreshold 2E9
set RepFlowThreshold 2E9

##### For each method, above the following threshold are normal TCP single connection flows
if {$methodName=="MPTCP" 
	|| $methodName=="RTCP-RETRAN"
	|| $methodName=="RTCP-CODE"
	|| $methodName=="MPTCP-1" 
	|| $methodName=="RMPTCP-RETRAN"
	|| $methodName=="RMPTCP-CODE"
	|| $methodName=="RMPTCP-CODE-REACTIVE" 
	|| $methodName=="RMPTCP-RETRAN-REACTIVE" } {

	set NonLongConnFlowThreshold $RMTThreshold

} elseif {$methodName=="REPFLOW"} { 

	set NonLongConnFlowThreshold $RepFlowThreshold

} else {
	
	set NonLongConnFlowThreshold $GAThreshold
}

set STARTTIMEOFFSET 2000
set WARMUPFLOWSIZE [expr 1E3*1460]

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

set totalLinkNum [expr $serverNumber+$nonTopSwitchNumber*($switchLayer-1)*int($switchPortNumber/2)]


if { $failureNumber==1 } {
	set totalLossyLinkNum 1
} elseif { $failureNumber==2 } {
	set totalLossyLinkNum 1
} elseif { $failureNumber==3 } {
	set totalLossyLinkNum [expr round($totalLinkNum*$LossyLinkPortion)]
} else {
	set totalLossyLinkNum 0
}

puts "serverNumber:$serverNumber topSwitchNumber:$topSwitchNumber nonTopSwitchNumber:$nonTopSwitchNumber totalLinkNum:$totalLinkNum totalLossyLinkNum:$totalLossyLinkNum"

set ns [new Simulator] 

# $ns set flowNumber_ $flowNumber

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
Node set loadBalancePerPacket_ 1

set nd [open outAllTrace-$tcpVersion-$rwndSize.tr w]
# $ns trace-all $nd

set fTraffic [open InputTraffic-$serverNumber-[expr $flowNumber].tr r]
set fFCT "$tcpVersion-$rwndSize-fct.tr"

file delete -force $fFCT

set fTimeout [open $tcpVersion-$rwndSize-timeout.tr w]
set fInputTrafficBindFlowID [open InputFlow-BindFlowID-$serverNumber-[expr $flowNumber].tr w]

puts "randSeed is [$ns set randSeed_]"

proc finish {} {
	global n sLeaf sSpine switchLayer topSwitchNumber switchPortNumber switchDownPortNumber leafDownPortNumber nonTopSwitchNumber leafUpPortNumber
	global ns nd timeLength serverNumber flowNumber fTraffic fFlowThroughput linkDelay fFCT
	global programStartTime methodName subFlowNumber em fTimeout failureNumber
	global tcp tcpsink ftp ftpsink tcpLongConn tcpLongConnSink ftpLongConn ftpLongConnSink
	global longFlowNum smallFlowNum normalConnNum longConnNum totalConnNum MAX_CONNECTION_NUM totalLossyLinkNum

	####  set output result file for each app 
	for {set i 0} {$i<$normalConnNum} {incr i} {
		$ftpsink($i) print-result-file $fFCT
	}

	for {set i 0} {$i<$serverNumber} {incr i} {
		for {set j 0} {$j<$serverNumber} {incr j} {
			for {set k 0} {$k<$MAX_CONNECTION_NUM} {incr k} {
				
				for {set p 0} {$p<$subFlowNumber} {incr p} {
					if {[info exists ftpLongConnSink($i,$j,$k,$p)]} {
						$ftpLongConnSink($i,$j,$k,$p) print-result-file $fFCT
					}
				}
			
				if {[info exists ftpLongConnSink($i,$j,$k)]} {
					$ftpLongConnSink($i,$j,$k) print-result-file $fFCT
				}
			}
		}
	}

	set totalRetransTimes 0
	set totalTimeoutTimes 0
	set totalParityRetransTimes 0

	for {set i 0} {$i<$totalConnNum} {incr i} {
		set flowTimeoutTimes($i) 0
	}

	for {set i 0} {$i<$normalConnNum} {incr i} {

		set totalRetransTimes [expr $totalRetransTimes+[$tcp($i) set retransTimes]+[$tcpsink($i) set retransTimes]]
		set totalTimeoutTimes [expr $totalTimeoutTimes+[$tcp($i) set timeOutTimes]+[$tcpsink($i) set timeOutTimes]]
		set flowTimeoutTimes([$tcp($i) set flow_ID_]) [expr [$tcp($i) set timeOutTimes]+[$tcpsink($i) set timeOutTimes]]

	}

	for {set i 0} {$i<$serverNumber} {incr i} {
		for {set j 0} {$j<$serverNumber} {incr j} {
			for {set k 0} {$k<$MAX_CONNECTION_NUM} {incr k} {
				if {![info exists tcpLongConn($i,$j,$k)]} {
					continue
				}

				if {[info exists tcpLongConn($i,$j,$k,0)]} {
					for {set p 0} {$p<$subFlowNumber} {incr p} {
						set totalRetransTimes [expr $totalRetransTimes+[$tcpLongConn($i,$j,$k,$p) set retransTimes]+[$tcpLongConnSink($i,$j,$k,$p) set retransTimes]]
						set totalTimeoutTimes [expr $totalTimeoutTimes+[$tcpLongConn($i,$j,$k,$p) set timeOutTimes]+[$tcpLongConnSink($i,$j,$k,$p) set timeOutTimes]]

						set flowTimeoutTimes([$tcpLongConn($i,$j,$k) set flow_ID_]) [expr $flowTimeoutTimes([$tcpLongConn($i,$j,$k) set flow_ID_]) + [$tcpLongConn($i,$j,$k,$p) set timeOutTimes]+[$tcpLongConnSink($i,$j,$k,$p) set timeOutTimes]]
					}
				} else {
					set totalRetransTimes [expr $totalRetransTimes+[$tcpLongConn($i,$j,$k) set retransTimes]+[$tcpLongConnSink($i,$j,$k) set retransTimes]]
					set totalTimeoutTimes [expr $totalTimeoutTimes+[$tcpLongConn($i,$j,$k) set timeOutTimes]+[$tcpLongConnSink($i,$j,$k) set timeOutTimes]]

					set flowTimeoutTimes([$tcpLongConn($i,$j,$k) set flow_ID_]) [expr [$tcpLongConn($i,$j,$k) set timeOutTimes]+[$tcpLongConnSink($i,$j,$k) set timeOutTimes]]
				}

				if { $methodName=="RMPTCP-RETRAN" 
				|| $methodName=="RMPTCP-CODE"
				|| $methodName=="RTCP-RETRAN" 
				|| $methodName=="RTCP-CODE" 
				|| $methodName=="RMPTCP-CODE-REACTIVE"
				|| $methodName=="RMPTCP-RETRAN-REACTIVE"
				|| $methodName=="REPFLOW"} {
					if {[info exists tcpLongConn($i,$j,$k,0)]} {
						for {set p 0} {$p<$subFlowNumber} {incr p} {
							set totalParityRetransTimes  [expr $totalParityRetransTimes+[$tcpLongConn($i,$j,$k,$p) set parityRetransTimes]+[$tcpLongConnSink($i,$j,$k,$p) set parityRetransTimes]]
						}
					}
				}
			}
		}
	}

	for {set i 0} {$i<$totalConnNum} {incr i} {
		if {$flowTimeoutTimes($i) > 0} {
			puts $fTimeout "$i $flowTimeoutTimes($i)"
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
	set totalFCT 0

	set totalSmallFlowThroughput 0
	set totalSmallFlowFCT 0

	for {set i 0} {$i<$serverNumber} {incr i} {
		for {set j 0} {$j<$serverNumber} {incr j} {
			for {set k 0} {$k<$MAX_CONNECTION_NUM} {incr k} {
				if {![info exists ftpLongConnSink($i,$j,$k)]} {
					continue
				}
				$ftpLongConnSink($i,$j,$k) get-statistic
				set totalSmallFlowThroughput [expr $totalSmallFlowThroughput + [$ftpLongConnSink($i,$j,$k) set avg_throughput_]]
				set totalSmallFlowFCT [expr $totalSmallFlowFCT + [$ftpLongConnSink($i,$j,$k) set avg_fct_]]
			}
		}
	}

	puts "---------------------------------------------------------"
	puts "randSeed is [$ns set randSeed_]"
	puts "now :[$ns now]"

	if {$smallFlowNum == 0} {
		set avgSmallFCT 0
		set avgSmallTP 0
	} else {
		set avgSmallFCT [expr $totalSmallFlowFCT/$smallFlowNum]
		set avgSmallTP [expr $totalSmallFlowThroughput/$smallFlowNum/1E6]
	}

	puts "small flow: average FCT is $avgSmallFCT s, average throughput is $avgSmallTP Mbps"
	puts "totalPacketDropped=$totalPacketDropped (totalParityDropped=$totalParityDropped), packetDroppedAt_IO=$packetDroppedAt_IO ($parityDroppedAt_IO parity), packetDroppedAt_O=$packetDroppedAt_O ($parityDroppedAt_O parity)"
	if { $failureNumber>0 } {
		set totalLossyDropped_ 0
		if {$failureNumber==3} {
			for {set i 0} {$i<$totalLossyLinkNum} {incr i} { 
				set totalLossyDropped_ [expr $totalLossyDropped_+[$em($i,0) set packetDropped_]]
				set totalLossyDropped_ [expr $totalLossyDropped_+[$em($i,1) set packetDropped_]]
			}
		} else {
			set totalLossyDropped_ [expr [$em set packetDropped_]]
		}
		puts "Packets dropped in lossy link is: lossyPacketDropped=$totalLossyDropped_"
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

set queueSpineSwitch 600
#set queueLeafSwitch 48
set queueLeafSwitch 600

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
		$ns ecn-threshold $n($k) $sLeaf(0,$i) [expr int($queueLeafSwitch*$ecnThresholdPortionLeaf)]
		$ns simplex-link $sLeaf(0,$i) $n($k) $linkAccessRate [expr $linkDelay*1E-3]s $queueManage
		$ns queue-limit $sLeaf(0,$i) $n($k) $queueLeafSwitch
		$ns ecn-threshold $sLeaf(0,$i) $n($k) [expr int($queueLeafSwitch*$ecnThresholdPortionLeaf)]
		# puts "node([$n($k) set address_]) <-> leaf(0,$i)([$sLeaf(0,$i) set address_]) "

		set queue1 [[$ns link $sLeaf(0,$i) $n($k)] queue]
		# $queue1 monitor-E2EDelay
		# $queue1 monitor-QueueLen
		# $queue1 monitor-Drop
		# $queue1 monitor-FlowPath

		set queue2 [[$ns link $n($k) $sLeaf(0,$i)] queue]
		# $queue2 monitor-QueueLen
		# $queue2 monitor-Drop
		# $queue2 monitor-FlowPath
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
				# $queue1 monitor-FlowPath

				set queue2 [[$ns link  $sLeaf($i,$j) $sLeaf([expr $i+1],$m)] queue]
				# $queue2 tag-timestamp
				# $queue2 monitor-QueueLen
				# $queue2 monitor-FlowPath
				# $queue2 monitor-Drop
		}
	}
}

# set em1 [new ErrorModel]
# $em1 set rate_ 0
# $em1 unit pkt
# $em1 ranvar $rnd
# $em1 drop-target [new Agent/Null]
# $ns link-lossmodel $em1 $sLeaf(0) $sSpine(0)

####Lossy link!
if { $failureNumber>0 } {
	if { $failureNumber==3 } {
		set fFailure [open InputFailureCDF.tr r]
		set failure_data [read $fFailure]
		close $fFailure
		set failure_data_cdf_by_line [split $failure_data "\n"]
		set lossRateMin [regexp -inline -all -- {\S+} [lindex $failure_data_cdf_by_line 0]]
		puts "lossRateMin=[lindex $lossRateMin 0] cumProbMin=[lindex $lossRateMin 1]"

		for {set i 0} {$i<$totalLossyLinkNum} {incr i} { 
			#### for two direction lossy module, each link has two lossy module
			set em($i,0) [new ErrorModel]
			set em($i,1) [new ErrorModel]

			set rnd_tmp [$rnd value]
			if {$rnd_tmp<0 || $rnd_tmp>1} {
				puts stderr "ERROR: setting loss rate error. rnd=$rnd_tmp!"
				exit
			}
			set lossRate 0
			set x 0
			while 1 {
				set lossRateLow [regexp -inline -all -- {\S+} [lindex $failure_data_cdf_by_line $x]]
				if {[expr $x+1] < [llength $failure_data_cdf_by_line]} {
					set lossRateUp [regexp -inline -all -- {\S+} [lindex $failure_data_cdf_by_line [expr $x+1]]]
				} else {
					set lossRate [lindex $lossRateLow 0] 
				}

				if {$rnd_tmp<=[lindex $lossRateMin 1]} {
					set lossRate [lindex $lossRateMin 0]
					break
				}

				if {$rnd_tmp>[lindex $lossRateLow 1] && $rnd_tmp<=[lindex $lossRateUp 1]} {
					set lossRate $lossRateUp
					break
				}
				incr x
			}
			
			if { $lossRate>$MaxLossRate } {
				set lossRate $MaxLossRate
			}
			if {$lossRate<[lindex $lossRateMin 0] || $lossRate>$MaxLossRate} {
				puts stderr "ERROR: setting loss rate error. lossRate=$lossRate range=[[lindex $lossRateMin 0],$MaxLossRate]!"
				exit
			}

			$em($i,0) set rate_ $lossRate
			$em($i,0) unit pkt
			$em($i,0) ranvar $rnd
			$em($i,0) drop-target [new Agent/Null]
			$em($i,1) set rate_ $lossRate
			$em($i,1) unit pkt
			$em($i,1) ranvar $rnd
			$em($i,1) drop-target [new Agent/Null]

			# $em($i,0) monitor-Drop
			# $em($i,1) monitor-Drop
		}
	} else {
		set em [new ErrorModel]
		if { $failureNumber==1 } {
			$em set rate_ 0.02
		} elseif { $failureNumber==2 } {
			$em set rate_ 0.95
		} else {
			puts stderr "ERROR: unkown failure type $failureNumber!"
			exit 1;
		}
		$em unit pkt
		$em ranvar $rnd
		$em drop-target [new Agent/Null]

		# $em monitor-Drop
	}	
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
			# $queue1 monitor-FlowPath

			set queue2 [[$ns link  $sLeaf([expr $switchLayer-2],$i) $sSpine($k)] queue]
			# $queue2 tag-timestamp
			# $queue2 monitor-QueueLen
			# $queue2 monitor-FlowPath
			# $queue2 monitor-Drop

			if {$k==0 && $failureNumber>0 && $failureNumber<3} {
				$ns link-lossmodel $em $sLeaf([expr $switchLayer-2],$i) $sSpine($k)
				# $queue2 monitor-FlowPath
			}
	}
}


###set the lossy link
if { $failureNumber==3 } {

	puts "---------- lossy link: ----------"
	### set each lossy link
	for {set i 0} {$i<$totalLossyLinkNum} {incr i} {
		set failedLinkLayer -1

		set rnd_tmp [$rnd value]
		if { $rnd_tmp<$LossyLinkDistribution(0) && $rnd_tmp>=0 } {
			set failedLinkLayer 0
		} elseif { $rnd_tmp<[expr $LossyLinkDistribution(0)+$LossyLinkDistribution(1)] && $rnd_tmp>=0 } {
			set failedLinkLayer 1
		} elseif { $rnd_tmp<[expr $LossyLinkDistribution(0)+$LossyLinkDistribution(1)+$LossyLinkDistribution(2)] && $rnd_tmp>=0 } {
			set failedLinkLayer 2
		} else {
			puts stderr "ERROR: setting lossy link error, wrong failedLinkLayer. rnd=$rnd_tmp"
			exit
		}

		if { $failedLinkLayer==0 } {
			set lossyServer [expr int([$rnd value]*$serverNumber)]
			set lossyLink 0
			set belongToWhichLeaf [expr int($lossyServer/$leafDownPortNumber)]

			while {[info exists NodeToLeaf($lossyServer,$belongToWhichLeaf)]} {
				set lossyServer [expr int([$rnd value]*$serverNumber)]
				set lossyLink 0
				set belongToWhichLeaf [expr int($lossyServer/$leafDownPortNumber)]
				puts "try n($lossyServer)<--->sLeaf(0,$belongToWhichLeaf)"
			}
			set NodeToLeaf($lossyServer,$belongToWhichLeaf) 1
			puts "n($lossyServer)<--->sLeaf(0,$belongToWhichLeaf)  (loss rate [$em($i,0) set rate_])"
			$ns link-lossmodel $em($i,0) $n($lossyServer) $sLeaf(0,$belongToWhichLeaf)
			$ns link-lossmodel $em($i,1) $sLeaf(0,$belongToWhichLeaf) $n($lossyServer) 
		} elseif {$failedLinkLayer==1} {
			set switchesPerUpPod [expr int(pow($leafUpPortNumber,$failedLinkLayer))]
			set switchesPerThisPod [expr int(pow($leafUpPortNumber,$failedLinkLayer-1))]

			set lossySwitch [expr int([$rnd value]*$nonTopSwitchNumber)]
			set lossyLink [expr int([$rnd value]*$leafUpPortNumber)]			
			set upPod [expr int($lossySwitch/$switchesPerUpPod)] 
			set m [expr $upPod*$switchesPerUpPod+($lossySwitch%$switchesPerThisPod)*$leafUpPortNumber+$lossyLink]

			while {[info exists LeafToLeaf($lossySwitch,$m)]} {
				set lossySwitch [expr int([$rnd value]*$nonTopSwitchNumber)]
				set lossyLink [expr int([$rnd value]*$leafUpPortNumber)]			
				set upPod [expr int($lossySwitch/$switchesPerUpPod)] 
				set m [expr $upPod*$switchesPerUpPod+($lossySwitch%$switchesPerThisPod)*$leafUpPortNumber+$lossyLink]
				puts "try sLeaf([expr $failedLinkLayer-1],$lossySwitch)<--->sLeaf($failedLinkLayer,$m)"
			}
			set LeafToLeaf($lossySwitch,$m) 1
			puts "sLeaf([expr $failedLinkLayer-1],$lossySwitch)<--->sLeaf($failedLinkLayer,$m)  (loss rate [$em($i,0) set rate_])"
			$ns link-lossmodel $em($i,0) $sLeaf([expr $failedLinkLayer-1],$lossySwitch) $sLeaf($failedLinkLayer,$m)
			$ns link-lossmodel $em($i,1) $sLeaf($failedLinkLayer,$m) $sLeaf([expr $failedLinkLayer-1],$lossySwitch)
		} elseif {$failedLinkLayer==2} {
			set lossySwitch [expr int([$rnd value]*$nonTopSwitchNumber)]
			set lossyLink [expr int([$rnd value]*$leafUpPortNumber)]
			set k [expr ($lossySwitch*$leafUpPortNumber+$lossyLink)%$topSwitchNumber]

			while {[info exists LeafToSpine($lossySwitch,$k)]} {
				set lossySwitch [expr int([$rnd value]*$nonTopSwitchNumber)]
				set lossyLink [expr int([$rnd value]*$leafUpPortNumber)]
				set k [expr ($lossySwitch*$leafUpPortNumber+$lossyLink)%$topSwitchNumber]
				puts "try sLeaf([expr $switchLayer-2],$lossySwitch)<--->sSpine($k)"
			}
			set LeafToSpine($lossySwitch,$k) 1
			puts "sLeaf([expr $switchLayer-2],$lossySwitch)<--->sSpine($k)  (loss rate [$em($i,0) set rate_])"
			$ns link-lossmodel $em($i,0) $sLeaf([expr $switchLayer-2],$lossySwitch) $sSpine($k)
			$ns link-lossmodel $em($i,1) $sSpine($k) $sLeaf([expr $switchLayer-2],$lossySwitch)
		} else {
			puts stderr "ERROR: no failedLinkLayer set!"
			exit
		}
	}
}
###set the lossy link


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

set inputFlowStartTime [clock seconds]

set latestFlowStartTime 0
set longFlowNum 0
set smallFlowNum 0

set totalConnNum 0
set longConnNum 0
set normalConnNum 0

for {set i 0} {$i<$serverNumber} {incr i} {
	for {set j 0} {$j<$serverNumber} {incr j} {
		set nodePairLongConnCount($i,$j) 0
	}
}

# tcp(k)/tcpsink(k) for normal connection, each flow has one connection: for long flow > NonLongConnFlowThreshold 
# ftp(k)/ftpsink(k)

# tcpLongConn(i,j,k)/tcpLongConnSink(i,j,k)  for long connection, multiple flow share one connection according to their arriving time: for small flow <= NonLongConnFlowThreshold 
# src i, dst j, the kth connection
# ftpLongConn(i,j,k)/ftpLongConnSink(i,j,k)

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

	if {$flowSize>$NonLongConnFlowThreshold} {
		set currentInsertingConnID $normalConnNum

		incr longFlowNum
		incr normalConnNum
		incr totalConnNum

		set tcp($currentInsertingConnID) [new Agent/TCP/FullTcp/Sack]
		set tcpsink($currentInsertingConnID) [new Agent/TCP/FullTcp/Sack]

		$tcp($currentInsertingConnID) ecmp-hash-key [expr int([$rnd value]*0x100000000)]

		# $tcp($currentInsertingConnID) monitor-spare-window

		# $tcp($currentInsertingConnID) monitor-Sequence
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
	} else {
		incr smallFlowNum

		if {$nodePairLongConnCount($srcNode,$dstNode)>$MAX_CONNECTION_NUM} {
			puts stderr "error: nodePairLongConnCount($srcNode,$dstNode)=$nodePairLongConnCount($srcNode,$dstNode) > $MAX_CONNECTION_NUM"
			exit;		
		} else {
			if {$nodePairLongConnCount($srcNode,$dstNode)==$MAX_CONNECTION_NUM} {
				#### find an existing conn
				set currentInsertingConnID [expr int([$rnd value]*0x100000000)%$MAX_CONNECTION_NUM]
			} else {
				####  establish a new conn
				set ifEstablishNewConn 1

				set currentInsertingConnID $nodePairLongConnCount($srcNode,$dstNode)

				incr totalConnNum
				incr longConnNum
				incr nodePairLongConnCount($srcNode,$dstNode)

				if { ($methodName=="MPTCP" 
				|| $methodName=="RTCP-RETRAN"
				|| $methodName=="RTCP-CODE"
				|| $methodName=="MPTCP-1" 
				|| $methodName=="RMPTCP-RETRAN"
				|| $methodName=="RMPTCP-CODE"
				|| $methodName=="RMPTCP-CODE-REACTIVE" 
				|| $methodName=="RMPTCP-RETRAN-REACTIVE") && $flowSize<=$RMTThreshold } {
					set mptcp($srcNode,$dstNode,$currentInsertingConnID) [new Agent/MPTCP]
					set mptcpsink($srcNode,$dstNode,$currentInsertingConnID) [new Agent/MPTCP]
					for {set j 0} {$j<$subFlowNumber} {incr j} {
						set tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) [new Agent/TCP/FullTcp/Sack/Multipath]
						$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) set window_ $rwndSize
						$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) set maxcwnd_ $rwndSize
						
						$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) set segsize_ 1460


						$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) ecmp-hash-key [expr int([$rnd value]*0x100000000)]
						
						$ns attach-agent $n($srcNode) $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j)
						$mptcp($srcNode,$dstNode,$currentInsertingConnID) attach-tcp $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) $n($srcNode)

						set tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) [new Agent/TCP/FullTcp/Sack/Multipath]
						$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) set segsize_ [$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) set segsize_]
						$ns attach-agent $n($dstNode) $tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j)

						$mptcpsink($srcNode,$dstNode,$currentInsertingConnID) attach-tcp $tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) $n($dstNode)

						$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) set windowInit_ [expr int($intialCwnd/$subFlowNumber)]

						$mptcp($srcNode,$dstNode,$currentInsertingConnID) add-multihome-destination [$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) set agent_addr_] [$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) set agent_port_]
						$mptcpsink($srcNode,$dstNode,$currentInsertingConnID) add-multihome-destination [$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) set agent_addr_] [$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) set agent_port_]

						if {$methodName=="RMPTCP-CODE-REACTIVE" || $methodName=="RMPTCP-RETRAN-REACTIVE"} {
							$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) reactive
						}

						# $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) monitor-Sequence
						# $tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) monitor-Sequence

						# $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) monitor-spare-window

						$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) no-syn sender
						$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) no-syn receiver

						$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) flow-id [expr $totalConnNum-1]
						$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) flow-id [expr $totalConnNum-1]
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
						# $ns connect $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) $tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$subFlowID)
						$ns connect $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) $tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j)
					}

					if {$methodName=="RMPTCP-RETRAN" || $methodName=="RTCP-RETRAN" || $methodName=="RMPTCP-RETRAN-REACTIVE"} {
						$mptcp($srcNode,$dstNode,$currentInsertingConnID) rmptcp 0
						$mptcpsink($srcNode,$dstNode,$currentInsertingConnID) rmptcp-sink 0
					}
					if {$methodName=="RMPTCP-CODE" || $methodName=="RTCP-CODE" || $methodName=="RMPTCP-CODE-REACTIVE"} {
						$mptcp($srcNode,$dstNode,$currentInsertingConnID) rmptcp 3
						$mptcpsink($srcNode,$dstNode,$currentInsertingConnID) rmptcp-sink 3
					}

					$mptcpsink($srcNode,$dstNode,$currentInsertingConnID) listen
				} elseif {$methodName=="REPFLOW" && $flowSize<=$RepFlowThreshold} {
					for {set j 0} {$j<$subFlowNumber} {incr j} {
						set tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) [new Agent/TCP/FullTcp/Sack]
						set tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) [new Agent/TCP/FullTcp/Sack]

						$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) ecmp-hash-key [expr int([$rnd value]*0x100000000)]

						# $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) monitor-spare-window

						# $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) monitor-Sequence
						# $tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) monitor-Sequence
						# $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) set windowInit_ [expr [$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) set windowInit_]*$subFlowNumber]

						$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) no-syn sender
						$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) no-syn receiver

						$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) set window_ $rwndSize
						$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) set maxcwnd_ $rwndSize

						$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) set segsize_ 1460
						
						$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) set segsize_ [$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) set segsize_]

						$ns attach-agent $n($srcNode) $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j)
						$ns attach-agent $n($dstNode) $tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j)
						$ns connect $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) $tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j)

						$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) flow-id [expr $totalConnNum-1]
						$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) flow-id [expr $totalConnNum-1]

						$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) listen
					}
				} else {
					if { ($methodName=="REACTIVE"
						|| $methodName=="CORRECTIVE"
						|| $methodName=="PROACTIVE") && $flowSize <= $GAThreshold } {
						set tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) [new Agent/TCP/FullTcp/Sack/GA]
						set tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) [new Agent/TCP/FullTcp/Sack/GA]

						if {$methodName=="REACTIVE"} {
							$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) reactive
						}
						if {$methodName=="CORRECTIVE"} {
							$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) corrective
							$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) corrective-sink
						}
						if {$methodName=="PROACTIVE"} {
							$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) proactive
							$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) proactive-sink
						}
					} else {
						set tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) [new Agent/TCP/FullTcp/Sack]
						set tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) [new Agent/TCP/FullTcp/Sack]
					}
					
					# set tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) [new Agent/TCP/Sack1]
					# set tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) [new Agent/TCPSink]

					$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) ecmp-hash-key [expr int([$rnd value]*0x100000000)]

					# $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) monitor-spare-window

					# $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) monitor-Sequence
					# $tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) monitor-Sequence
					# $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) set windowInit_ [expr [$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) set windowInit_]*$subFlowNumber]

					$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) no-syn sender
					$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) no-syn receiver

					$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) set window_ $rwndSize
					$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) set maxcwnd_ $rwndSize

					$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) set segsize_ 1460
					
					$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) set segsize_ [$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) set segsize_]

					$ns attach-agent $n($srcNode) $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID)
					$ns attach-agent $n($dstNode) $tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID)
					$ns connect $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) $tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID)

					$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) flow-id [expr $totalConnNum-1]
					$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) flow-id [expr $totalConnNum-1]

					$tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) listen
				}
			}
		}  
	}

	if {$flowSize>$NonLongConnFlowThreshold} {
		set ftp($currentInsertingConnID) [new Application/FTP]
		$ftp($currentInsertingConnID) attach-agent $tcp($currentInsertingConnID)
		set ftpsink($currentInsertingConnID) [new Application/FTPSink]
		$ftpsink($currentInsertingConnID) attach-agent $tcpsink($currentInsertingConnID)
		$ftpsink($currentInsertingConnID) flow-size $flowSize
		$ftpsink($currentInsertingConnID) flow-id $k
		$ftpsink($currentInsertingConnID) flow-start-time $startTime
		puts $fInputTrafficBindFlowID "flow\[$k\] conn\[[$tcp($currentInsertingConnID) set flow_ID_]\]($srcNode-$dstNode) $startTime $flowSize"
		$ns at $startTime "$ftp($currentInsertingConnID) send $flowSize"
	} else {
		if { ($methodName=="MPTCP" 
			|| $methodName=="RTCP-RETRAN"
			|| $methodName=="RTCP-CODE"
			|| $methodName=="MPTCP-1" 
			|| $methodName=="RMPTCP-RETRAN"
			|| $methodName=="RMPTCP-CODE"
			|| $methodName=="RMPTCP-CODE-REACTIVE" 
			|| $methodName=="RMPTCP-RETRAN-REACTIVE")  && $flowSize<=$RMTThreshold } {

			if {$ifEstablishNewConn==1} {
				set ftpLongConn($srcNode,$dstNode,$currentInsertingConnID) [new Application/FTP]
				set ftpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) [new Application/FTPSink]
			}
			$ftpLongConn($srcNode,$dstNode,$currentInsertingConnID) attach-agent $mptcp($srcNode,$dstNode,$currentInsertingConnID)
			for {set j 0} {$j<$subFlowNumber} {incr j} {
				$ftpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) attach-agent $tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j)
			}

			puts $fInputTrafficBindFlowID "flow\[$k\] conn\[[$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,0) set flow_ID_]\]($srcNode-$dstNode:$currentInsertingConnID) $startTime $flowSize"
		} elseif {$methodName=="REPFLOW" && $flowSize<=$RepFlowThreshold} {
			for {set j 0} {$j<$subFlowNumber} {incr j} {
				if {$ifEstablishNewConn==1} {
					set ftpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) [new Application/FTP]
					set ftpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) [new Application/FTPSink]
				}
				$ftpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) attach-agent $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j)
				$ftpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) attach-agent $tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j)
			}

			puts $fInputTrafficBindFlowID "flow\[$k\] conn\[[$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID,0) set flow_ID_]\]($srcNode-$dstNode:$currentInsertingConnID) $startTime $flowSize"
		} else {
			if {$ifEstablishNewConn==1} {
				set ftpLongConn($srcNode,$dstNode,$currentInsertingConnID) [new Application/FTP]
				set ftpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) [new Application/FTPSink]
			}
			
			$ftpLongConn($srcNode,$dstNode,$currentInsertingConnID) attach-agent $tcpLongConn($srcNode,$dstNode,$currentInsertingConnID)
			$ftpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) attach-agent $tcpLongConnSink($srcNode,$dstNode,$currentInsertingConnID)

			puts $fInputTrafficBindFlowID "flow\[$k\] conn\[[$tcpLongConn($srcNode,$dstNode,$currentInsertingConnID) set flow_ID_]\]($srcNode-$dstNode:$currentInsertingConnID) $startTime $flowSize"
		}

		if {$methodName=="REPFLOW" && $flowSize<=$RepFlowThreshold} {
			for {set j 0} {$j<$subFlowNumber} {incr j} {
				$ftpLongConnSink($srcNode,$dstNode,$currentInsertingConnID,$j) set-flow-info $k $startTime $flowSize
				$ns at $startTime "$ftpLongConn($srcNode,$dstNode,$currentInsertingConnID,$j) send $flowSize"
			}
		} else {
			$ftpLongConnSink($srcNode,$dstNode,$currentInsertingConnID) set-flow-info $k $startTime $flowSize
			$ns at $startTime "$ftpLongConn($srcNode,$dstNode,$currentInsertingConnID) send $flowSize"
		}
	}
}

set connCount 0
#### generate warm-up flows
if { ($methodName=="RMPTCP-RETRAN"
	|| $methodName=="RMPTCP-CODE"
	|| $methodName=="RMPTCP-CODE-REACTIVE" 
	|| $methodName=="RMPTCP-RETRAN-REACTIVE") } {
	for {set i 0} {$i<$serverNumber} {incr i} {
		for {set j 0} {$j<$serverNumber} {incr j} {
			for {set k 0} {$k<$MAX_CONNECTION_NUM} {incr k} {
				if {[info exists ftpLongConnSink($i,$j,$k)]} {
					$ftpLongConnSink($i,$j,$k) set-flow-info -1 [expr 1+$connCount*0.01] $WARMUPFLOWSIZE
					$ns at [expr 1+$connCount*0.01] "$ftpLongConn($i,$j,$k) send $WARMUPFLOWSIZE"
					incr connCount
				} else {
					break
				}
			}
		}
	}
}


set inputFlowEndTime [clock seconds]
set runTime [expr $inputFlowEndTime-$inputFlowStartTime]
set hour [expr int($runTime/3600)]
set minute [expr int(($runTime-$hour*3600)/60)]
set second [expr int($runTime)%60]
puts [format "Input flows(%d) completed! Run for %dh:%dm:%ds" $flowNumber $hour $minute $second]
puts [format "longFlowNum(%d) smallFlowNum(%d) totalConnNum(%d) normalConnNum(%d) longConnNum(%d)" $longFlowNum $smallFlowNum $totalConnNum $normalConnNum $longConnNum]

puts "\n====== long connection lists statistic ======"
for {set i 0} {$i<$serverNumber} {incr i} {
	for {set j 0} {$j<$serverNumber} {incr j} {
		if {[info exists ftpLongConnSink($i,$j,0,0)]} {
			puts -nonewline "conn($i-$j)"
		}
		for {set k 0} {$k<$MAX_CONNECTION_NUM} {incr k} {
			if {[info exists ftpLongConnSink($i,$j,$k,0)]} {
				# puts "\n====== conn\[[$tcpLongConn($i,$j,$k) set flow_ID_]\]($i-$j:$k) ======"
				# $ftpLongConnSink($i,$j,$k) print-flow-list
				puts -nonewline " \[$k:[$ftpLongConnSink($i,$j,$k,0) set flow_num_]\]"
			} else {
				break
			}
		}
		if {[info exists ftpLongConnSink($i,$j,0,0)]} {
			puts ""
		}

		if {[info exists ftpLongConnSink($i,$j,0)]} {
			puts -nonewline "conn($i-$j)"
		}
		for {set k 0} {$k<$MAX_CONNECTION_NUM} {incr k} {
			if {[info exists ftpLongConnSink($i,$j,$k)]} {
				# puts "\n====== conn\[[$tcpLongConn($i,$j,$k) set flow_ID_]\]($i-$j:$k) ======"
				# $ftpLongConnSink($i,$j,$k) print-flow-list
				puts -nonewline " \[$k:[$ftpLongConnSink($i,$j,$k) set flow_num_]\]"
			} else {
				break
			}
		}
		if {[info exists ftpLongConnSink($i,$j,0)]} {
			puts ""
		}
	}
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

set timeLength [expr $latestFlowStartTime+5000]

flush $fInputTrafficBindFlowID

$ns at $timeLength "finish"
$ns run
