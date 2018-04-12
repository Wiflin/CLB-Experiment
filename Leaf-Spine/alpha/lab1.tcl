set ns [new Simulator]


set rwndSize 				[lindex $argv 0]
#rwnd size in packets

set linkDelay 				[lindex $argv 1]
#linkDelay in ms

set switchPortNumber 		[lindex $argv 2]
set linkRate 				[lindex $argv 3]

set switchDownPortNumber 	[lindex $argv 4]
set linkAccessRate 			[lindex $argv 5]

set flowNumber 				[lindex $argv 6]
#how many flows in total generated




set serverNumber 20
set nonTopSwitchNumber 2
set topSwitchNumber 4

set queueSpineSwitch 600
#set queueLeafSwitch 48
set queueLeafSwitch 600

set queueManage "DropTail"
set ecnThresholdPortion 0.3


# set sendBufferSize [expr 10*$rwndSize]
set sendBufferSize	4000
set ecnThresholdPortionLeaf 0.3

set leafUpPortNumber [expr int($switchPortNumber/2)]
set leafDownPortNumber $switchDownPortNumber


# set failureNumber 3


# server nodes
for {set i 0} {$i<$serverNumber} {incr i} {
	set n($i) [$ns node]
	# $n($i) enable-salt
}

# 1*access layer + [expr $switchLayer-2]*distribution layer
for {set i 0} {$i<[expr $switchLayer-1]} {incr i} {
	for {set j 0} {$j<$nonTopSwitchNumber} {incr j} {
		set sLeaf($i,$j) [$ns node]
		# $sLeaf($i,$j) enable-salt
	}
}

# core layer
for {set i 0} {$i<$topSwitchNumber} {incr i} {
	set sSpine($i) [$ns node]
	# $sSpine($i) enable-salt
}


#leaf(0,x) <-> node
# puts "initializing links between servers and leaf(0,i) switches...."
for {set i 0} {$i<$nonTopSwitchNumber} {incr i} {
	for {set j 0} {$j<$leafDownPortNumber} {incr j} {
		set k [expr $i*$leafDownPortNumber+$j]
		$ns simplex-link $n($k) $sLeaf(0,$i) $linkAccessRate [expr $linkDelay*1E-3]s $queueManage
		$ns queue-limit $n($k) $sLeaf(0,$i) $sendBufferSize
		# $ns ecn-threshold $n($k) $sLeaf(0,$i) [expr int($queueLeafSwitch*$ecnThresholdPortionLeaf)]
		$ns simplex-link $sLeaf(0,$i) $n($k) $linkAccessRate [expr $linkDelay*1E-3]s $queueManage
		$ns queue-limit $sLeaf(0,$i) $n($k) $queueLeafSwitch
		# $ns ecn-threshold $sLeaf(0,$i) $n($k) [expr int($queueLeafSwitch*$ecnThresholdPortionLeaf)]
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
# if { $failureNumber>0 } {
# 	if { $failureNumber==3 } {
# 		set fFailure [open InputFailureCDF.tr r]
# 		set failure_data [read $fFailure]
# 		close $fFailure
# 		set failure_data_cdf_by_line [split $failure_data "\n"]
# 		set lossRateMin [regexp -inline -all -- {\S+} [lindex $failure_data_cdf_by_line 0]]
# 		puts "lossRateMin=[lindex $lossRateMin 0] cumProbMin=[lindex $lossRateMin 1]"

# 		for {set i 0} {$i<$totalLossyLinkNum} {incr i} { 
# 			#### for two direction lossy module, each link has two lossy module
# 			set em($i,0) [new ErrorModel]
# 			set em($i,1) [new ErrorModel]

# 			set rnd_tmp [$rnd value]
# 			if {$rnd_tmp<0 || $rnd_tmp>1} {
# 				puts stderr "ERROR: setting loss rate error. rnd=$rnd_tmp!"
# 				exit
# 			}
# 			set lossRate 0
# 			set x 0
# 			while 1 {
# 				set lossRateLow [regexp -inline -all -- {\S+} [lindex $failure_data_cdf_by_line $x]]
# 				if {[expr $x+1] < [llength $failure_data_cdf_by_line]} {
# 					set lossRateUp [regexp -inline -all -- {\S+} [lindex $failure_data_cdf_by_line [expr $x+1]]]
# 				} else {
# 					set lossRate [lindex $lossRateLow 0] 
# 				}

# 				if {$rnd_tmp<=[lindex $lossRateMin 1]} {
# 					set lossRate [lindex $lossRateMin 0]
# 					break
# 				}

# 				if {$rnd_tmp>[lindex $lossRateLow 1] && $rnd_tmp<=[lindex $lossRateUp 1]} {
# 					set lossRate $lossRateUp
# 					break
# 				}
# 				incr x
# 			}
			
# 			if { $lossRate>$MaxLossRate } {
# 				set lossRate $MaxLossRate
# 			}
# 			if {$lossRate<[lindex $lossRateMin 0] || $lossRate>$MaxLossRate} {
# 				puts stderr "ERROR: setting loss rate error. lossRate=$lossRate range=[[lindex $lossRateMin 0],$MaxLossRate]!"
# 				exit
# 			}

# 			$em($i,0) set rate_ $lossRate
# 			$em($i,0) unit pkt
# 			$em($i,0) ranvar $rnd
# 			$em($i,0) drop-target [new Agent/Null]
# 			$em($i,1) set rate_ $lossRate
# 			$em($i,1) unit pkt
# 			$em($i,1) ranvar $rnd
# 			$em($i,1) drop-target [new Agent/Null]

# 			# $em($i,0) monitor-Drop
# 			# $em($i,1) monitor-Drop
# 		}
# 	} else {
# 		set em [new ErrorModel]
# 		if { $failureNumber==1 } {
# 			$em set rate_ 0.02
# 		} elseif { $failureNumber==2 } {
# 			$em set rate_ 0.95
# 		} else {
# 			puts stderr "ERROR: unkown failure type $failureNumber!"
# 			exit 1;
# 		}
# 		$em unit pkt
# 		$em ranvar $rnd
# 		$em drop-target [new Agent/Null]

# 		# $em monitor-Drop
# 	}	
# }
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

			# what is this means?
			# if {$k==0 && $failureNumber>0 && $failureNumber<3} {
			# 	$ns link-lossmodel $em $sLeaf([expr $switchLayer-2],$i) $sSpine($k)
			# 	# $queue2 monitor-FlowPath
			# }
	}
}


###set the lossy link
# if { $failureNumber==3 } {

# 	puts "---------- lossy link: ----------"
# 	### set each lossy link
# 	for {set i 0} {$i<$totalLossyLinkNum} {incr i} {
# 		set failedLinkLayer -1

# 		set rnd_tmp [$rnd value]
# 		if { $rnd_tmp<$LossyLinkDistribution(0) && $rnd_tmp>=0 } {
# 			set failedLinkLayer 0
# 		} elseif { $rnd_tmp<[expr $LossyLinkDistribution(0)+$LossyLinkDistribution(1)] && $rnd_tmp>=0 } {
# 			set failedLinkLayer 1
# 		} elseif { $rnd_tmp<[expr $LossyLinkDistribution(0)+$LossyLinkDistribution(1)+$LossyLinkDistribution(2)] && $rnd_tmp>=0 } {
# 			set failedLinkLayer 2
# 		} else {
# 			puts stderr "ERROR: setting lossy link error, wrong failedLinkLayer. rnd=$rnd_tmp"
# 			exit
# 		}

# 		if { $failedLinkLayer==0 } {
# 			set lossyServer [expr int([$rnd value]*$serverNumber)]
# 			set lossyLink 0
# 			set belongToWhichLeaf [expr int($lossyServer/$leafDownPortNumber)]

# 			while {[info exists NodeToLeaf($lossyServer,$belongToWhichLeaf)]} {
# 				set lossyServer [expr int([$rnd value]*$serverNumber)]
# 				set lossyLink 0
# 				set belongToWhichLeaf [expr int($lossyServer/$leafDownPortNumber)]
# 				puts "try n($lossyServer)<--->sLeaf(0,$belongToWhichLeaf)"
# 			}
# 			set NodeToLeaf($lossyServer,$belongToWhichLeaf) 1
# 			puts "n($lossyServer)<--->sLeaf(0,$belongToWhichLeaf)  (loss rate [$em($i,0) set rate_])"
# 			$ns link-lossmodel $em($i,0) $n($lossyServer) $sLeaf(0,$belongToWhichLeaf)
# 			$ns link-lossmodel $em($i,1) $sLeaf(0,$belongToWhichLeaf) $n($lossyServer) 
# 		} elseif {$failedLinkLayer==1} {
# 			set switchesPerUpPod [expr int(pow($leafUpPortNumber,$failedLinkLayer))]
# 			set switchesPerThisPod [expr int(pow($leafUpPortNumber,$failedLinkLayer-1))]

# 			set lossySwitch [expr int([$rnd value]*$nonTopSwitchNumber)]
# 			set lossyLink [expr int([$rnd value]*$leafUpPortNumber)]			
# 			set upPod [expr int($lossySwitch/$switchesPerUpPod)] 
# 			set m [expr $upPod*$switchesPerUpPod+($lossySwitch%$switchesPerThisPod)*$leafUpPortNumber+$lossyLink]

# 			while {[info exists LeafToLeaf($lossySwitch,$m)]} {
# 				set lossySwitch [expr int([$rnd value]*$nonTopSwitchNumber)]
# 				set lossyLink [expr int([$rnd value]*$leafUpPortNumber)]			
# 				set upPod [expr int($lossySwitch/$switchesPerUpPod)] 
# 				set m [expr $upPod*$switchesPerUpPod+($lossySwitch%$switchesPerThisPod)*$leafUpPortNumber+$lossyLink]
# 				puts "try sLeaf([expr $failedLinkLayer-1],$lossySwitch)<--->sLeaf($failedLinkLayer,$m)"
# 			}
# 			set LeafToLeaf($lossySwitch,$m) 1
# 			puts "sLeaf([expr $failedLinkLayer-1],$lossySwitch)<--->sLeaf($failedLinkLayer,$m)  (loss rate [$em($i,0) set rate_])"
# 			$ns link-lossmodel $em($i,0) $sLeaf([expr $failedLinkLayer-1],$lossySwitch) $sLeaf($failedLinkLayer,$m)
# 			$ns link-lossmodel $em($i,1) $sLeaf($failedLinkLayer,$m) $sLeaf([expr $failedLinkLayer-1],$lossySwitch)
# 		} elseif {$failedLinkLayer==2} {
# 			set lossySwitch [expr int([$rnd value]*$nonTopSwitchNumber)]
# 			set lossyLink [expr int([$rnd value]*$leafUpPortNumber)]
# 			set k [expr ($lossySwitch*$leafUpPortNumber+$lossyLink)%$topSwitchNumber]

# 			while {[info exists LeafToSpine($lossySwitch,$k)]} {
# 				set lossySwitch [expr int([$rnd value]*$nonTopSwitchNumber)]
# 				set lossyLink [expr int([$rnd value]*$leafUpPortNumber)]
# 				set k [expr ($lossySwitch*$leafUpPortNumber+$lossyLink)%$topSwitchNumber]
# 				puts "try sLeaf([expr $switchLayer-2],$lossySwitch)<--->sSpine($k)"
# 			}
# 			set LeafToSpine($lossySwitch,$k) 1
# 			puts "sLeaf([expr $switchLayer-2],$lossySwitch)<--->sSpine($k)  (loss rate [$em($i,0) set rate_])"
# 			$ns link-lossmodel $em($i,0) $sLeaf([expr $switchLayer-2],$lossySwitch) $sSpine($k)
# 			$ns link-lossmodel $em($i,1) $sSpine($k) $sLeaf([expr $switchLayer-2],$lossySwitch)
# 		} else {
# 			puts stderr "ERROR: no failedLinkLayer set!"
# 			exit
# 		}
# 	}
# }
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

# set an unknown markup
for {set i 0} {$i<$serverNumber} {incr i} {
	for {set j 0} {$j<$serverNumber} {incr j} {
		set nodePairLongConnCount($i,$j) 0
	}
}

# --------------------------------------------------------------

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


# --------------------------------------------------------------
# setup static route in the network

$ns set staticRoute_ 1

# initialize $serverNumber routes at each nodes
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


# node <-> leaf(0,x)
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