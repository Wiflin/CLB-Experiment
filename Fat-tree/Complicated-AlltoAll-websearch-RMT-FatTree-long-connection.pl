#!/usr/bin/perl -w
use strict;
use POSIX;

my $num_args = $#ARGV + 1;
if ($num_args != 1 && $num_args != 2) {
    print "Usage: perl $0 [load (0-1)] [rand seed (optional)]\n";
    exit(1);
}

my $linkDelay=0.02;
my $linkRate='40G';
my $switchPortNumber=4;
my $switchDownPortNumber=8;
my $linkAccessRate='10G';
my $switchLayer=3;

my $failureNumber=3;
my $flowPerHost=1000;

my $subFlowNum=8;

my $load="$ARGV[0]";

print "linkDelay=$linkDelay(ms), linkRate=$linkRate, switchPortNumber=$switchPortNumber, switchDownPortNumber=$switchDownPortNumber, linkAccessRate=$linkAccessRate, failureNumber=$failureNumber, flowPerHost=$flowPerHost, subFlowNum=$subFlowNum\n";
print "randseed=$ARGV[1]\n";
srand("$ARGV[1]");


my $hostInTotal=int($switchPortNumber*pow($switchPortNumber/2,$switchLayer-2))*$switchDownPortNumber;

if(int($hostInTotal/2) != $hostInTotal/2)
{
	print "hostInTotal=$hostInTotal, is not even!\n";
	exit;
}
my $serverPerBiNetwork=$hostInTotal/2;

my $linkRateNumber=1E10;

my $flowNumber=$flowPerHost*$serverPerBiNetwork;

my @startTime=(2)x$serverPerBiNetwork;

my $TimeToRun=10;

my $DirName="Long-connection-websearch-$subFlowNum"."SF-$failureNumber-$load-$switchDownPortNumber-$linkRate";

my $curdir = `pwd`;
chomp $curdir;
# my @methods=("REPFLOW");
# @methods=("RMPTCP-RETRAN","RMPTCP-CODE","RTCP-RETRAN","RTCP-CODE","ECMP","MPTCP","REACTIVE","CORRECTIVE","PROACTIVE","MPTCP-1","REPFLOW","Ideal");

my @methods=("RMPTCP-RETRAN","ECMP","MPTCP","REACTIVE","CORRECTIVE","REPFLOW","PROACTIVE");

my $numberOfMethods=@methods;

####[9][2]///Data mining
# my @WorkLoad=(
# 	[1,0],
# 	[1,0.5],
# 	[2,0.6],
# 	[3,0.7],
# 	[7,0.8],
# 	[267,0.9],
# 	[2107,0.95],
# 	[66667,0.99],
# 	[666667,1],
# );

####[12][2]///Web search
my @WorkLoad=(
	[6,0],
	[6,0.15],
	[13,0.2],
	[19,0.3],
	[33,0.4],
	[53,0.53],
	[133,0.6],
	[667,0.7],
	[1333,0.8],
	[3333,0.9],
	[6667,0.97],
	[20000,1],
);


if(!(-e "$DirName"))
{
	`mkdir $DirName`;
}
else
{
	`rm -r $DirName`;
	`mkdir $DirName`;
}
for(my $i=0;$i<$numberOfMethods;$i++)
{
	my $str=$methods[$i];
	if(!(-e "$DirName/$str"))
	{
		`mkdir $DirName/$str`;
		
	}
	`yes|cp RMT-FatTree-long-connection.tcl $DirName/$str`;
	`yes|cp InputFailureCDF.tr $DirName/$str`;
}
`yes|cp cdf_my.pl $DirName`;
`yes|cp InputFailureCDF.tr $DirName`;

chdir("$curdir/$DirName") or die "$!";
open(fResult,">$DirName-Result.tr");

open(fError,">$DirName-error.tr");

open(fResultEachNormalFCT,">$DirName-Result-Normal-FCT.tr");
my $fResultEachNormalFCTStr="$curdir/$DirName/$DirName-Result-Normal-FCT.tr";
print fResultEachNormalFCT "Rnd-FlowID\tFlowSize\n";

for (my $runTime = 0; $runTime < $TimeToRun; $runTime++) 
{
	my @size;
	my @avgSize=(0) x $serverPerBiNetwork;
	for(my $i=0;$i<$serverPerBiNetwork;$i++)
	{
		for(my $z=0;$z<$flowPerHost;$z++)
		{
			my $rnd = rand;
			my $startSize=0;
			my $stopSize=0;
			my $startCDF=0;
			my $stopCDF=0;
			for(my $j=0;$j<scalar @WorkLoad;$j++)
			{
				if($rnd<$WorkLoad[$j][1])
				{
					$startSize=$WorkLoad[$j-1][0];
					$startCDF=$WorkLoad[$j-1][1];
					$stopSize=$WorkLoad[$j][0];
					$stopCDF=$WorkLoad[$j][1];
					last;
				}
			}
			my $flowSize=int(($rnd-$startCDF)/($stopCDF-$startCDF)*($stopSize-$startSize))+$startSize;
			$size[$i][$z]=$flowSize*1460;
			$avgSize[$i]=$avgSize[$i]+$size[$i][$z];
			# print "flow: $i size: $size[$i]\n";
		}

		$avgSize[$i]=$avgSize[$i]/$flowPerHost;
	}

	my @dst;
	open(fTraffic,">$curdir/$DirName/InputTraffic-$hostInTotal-$flowNumber.tr");
	for(my $i=0;$i<$serverPerBiNetwork;$i++)
	{
		# $dst[$i]=$serverPerBiNetwork;
		for(my $z=0;$z<$flowPerHost;$z++)
		{
			$dst[$i][$z]=$serverPerBiNetwork+ int(rand($serverPerBiNetwork));
			my $avgInterval=$avgSize[$i]/($load*$linkRateNumber/8);

			my $interval = (0-$avgInterval) * log(rand());
			$startTime[$i]=$startTime[$i]+$interval;
			fTraffic->print("$i $dst[$i][$z] $startTime[$i] $size[$i][$z]\n");
			# print "$src $dst $startTime $size[$i]\n";
			my $flowID=$i*$flowPerHost+$z;
			print fResultEachNormalFCT "$runTime-$flowID\t$size[$i][$z]\n";
		}
	}
	close(fTraffic);

	my $rnd_tmp=rand(0xEFFF);
	for(my $i=0;$i<$numberOfMethods;$i++)
	{
		my $str=$methods[$i];
		if(!(-e "$curdir/$DirName/$str/$str-$runTime"))
		{
			`mkdir $curdir/$DirName/$str/$str-$runTime`;
		}
		`yes|cp $curdir/$DirName/$str/RMT-FatTree-long-connection.tcl $curdir/$DirName/$str/$str-$runTime`;
		`yes|cp $curdir/$DirName/$str/InputFailureCDF.tr $curdir/$DirName/$str/$str-$runTime`;
		

		if($str ne "Ideal")
		{
			`yes|cp $curdir/$DirName/InputTraffic-$hostInTotal-$flowNumber.tr $curdir/$DirName/$str/$str-$runTime`;
		}
		else ####Ideal 
		{
			open(fTraffic,">$curdir/$DirName/$str/$str-$runTime/InputTraffic-$hostInTotal-$flowNumber.tr");
			my $startTime=2;
			for(my $i=0;$i<$serverPerBiNetwork;$i++)
			{
				for(my $z=0;$z<$flowPerHost;$z++)
				{
					$startTime=$startTime+5;
					fTraffic->print("$i $dst[$i][$z] $startTime $size[$i][$z]\n");
				}
			}
			close(fTraffic);
		}
		
		defined(my $pid=fork) or die "Can not fork: $!\n";
	    unless ($pid) {
	    	chdir("$curdir/$DirName/$str/$str-$runTime") or die "$!";
	        # system("ns RMT-FatTree.tcl Sack 400 0.005 $flowNumber 2 40G 2 8 10G $str 0 1>debug.tr 2>stderr.tr") == 0
	        if($str ne "Ideal")
	        {
		    	print "ns RMT-FatTree-long-connection.tcl Sack 400 $linkDelay $flowNumber $switchPortNumber $linkRate $switchDownPortNumber $linkAccessRate $switchLayer $str $failureNumber $subFlowNum $rnd_tmp\n";
		    	system("ns RMT-FatTree-long-connection.tcl Sack 400 $linkDelay $flowNumber $switchPortNumber $linkRate $switchDownPortNumber $linkAccessRate $switchLayer $str $failureNumber $subFlowNum $rnd_tmp 1>debug.tr 2>stderr.tr") == 0
				or do {
					print "ERROR: $str runTime=$runTime failed\n";
					print fError "$str runTime=$runTime failed\n";
				};
			}
			else
			{
				print "ns RMT-FatTree-long-connection.tcl Sack 400 $linkDelay $flowNumber $switchPortNumber $linkRate $switchDownPortNumber $linkAccessRate $switchLayer ECMP 0 1 $rnd_tmp\n";
		    	system("ns RMT-FatTree-long-connection.tcl Sack 400 $linkDelay $flowNumber $switchPortNumber $linkRate $switchDownPortNumber $linkAccessRate $switchLayer ECMP 0 1 $rnd_tmp 1>debug.tr 2>stderr.tr") == 0
				or do {
					print "ERROR: $str runTime=$runTime failed\n";
					print fError "$str runTime=$runTime failed\n";
				};
			}
			print "$str runTime=$runTime Done!\n";
	    	exit();
	    }
	}	
	1 while wait() >= 0;

	for(my $i=0;$i<$numberOfMethods;$i++)
	{
		my $str=$methods[$i];

		`sed '/-1*/d' $curdir/$DirName/$str/$str-$runTime/Sack-400-fct.tr > tmp.tr`;
		`mv tmp.tr $curdir/$DirName/$str/$str-$runTime/Sack-400-fct.tr`;
		`sort -k 1 -n $curdir/$DirName/$str/$str-$runTime/Sack-400-fct.tr > tmp.tr`;
		`cp tmp.tr $curdir/$DirName/$str/$str-$runTime/Sack-400-flow-results-raw.tr`;
		if($str eq "REPFLOW")
		{
			open(fFCT,">$curdir/$DirName/$str/$str-$runTime/Sack-400-fct.tr");
			open(fTmp,"tmp.tr");
			chomp(my @flowRaw=<fTmp>);
			close(fTmp);
			`rm tmp.tr`;
			my $currMinFCT=1E8;
			my $currFlowID=0;
			foreach my $flowEntry (@flowRaw){
			    my @flowEntries=split(' ',$flowEntry);
			    if($flowEntries[0]==$currFlowID)
			    {
			    	if($flowEntries[2]<$currMinFCT)
			    	{
			    		$currMinFCT=$flowEntries[2];
			    	}
			    }
			    else
			    {
			    	if($flowEntries[0]<$currFlowID)
			    	{
			    		print("ERROR: REPFLOW fct results file wrongly sorted!\n");
			    		exit(1);
			    	}
			    	print fFCT "$currMinFCT\n";
			    	$currFlowID=$flowEntries[0];
			    	$currMinFCT=$flowEntries[2];
			    }
			}
			if(@flowRaw>0)
			{
				print fFCT "$currMinFCT\n";
			}
			close(fFCT);
		}
		else
		{
			`cut -f 3- tmp.tr > $curdir/$DirName/$str/$str-$runTime/Sack-400-fct.tr`;
			`rm tmp.tr`;
		}		
		
		open(fFCT,"$curdir/$DirName/$str/$str-$runTime/Sack-400-fct.tr");
		open(fTraffic,"$curdir/$DirName/InputTraffic-$hostInTotal-$flowNumber.tr");
	
		my $countFCT = `wc -l < $curdir/$DirName/$str/$str-$runTime/Sack-400-fct.tr`;
		my $countFlow = `wc -l < $curdir/$DirName/InputTraffic-$hostInTotal-$flowNumber.tr`;

		my $timeoutFileStr="$curdir/$DirName/$str/$str-$runTime/Sack-400-timeout.tr";
		if(!(-z $timeoutFileStr) && (-e $timeoutFileStr))
		{
			open(fTimeoutInput,$timeoutFileStr);
			my $countTimeout = `wc -l < $timeoutFileStr`;
			open(fTimeoutOutput,">>$curdir/$DirName/$str/Record-Timeout-All-$runTime.tr");
			for(my $j=0;$j<$countTimeout;$j++)
			{		
				my $line=<fTimeoutInput>;
				chomp $line;
				print fTimeoutOutput "$runTime\t$line\n";
			}
			close(fTimeoutInput);
			close(fTimeoutOutput);
		}

		if($countFCT!=$countFlow)
		{
			print fError "$str load=$load, runTime=$runTime not all flows finished!\n";
			print "$str runTime=$runTime not all flows finished!\n";
			next;
		}

		chomp(my @FCT=<fFCT>);

		open(fFCTAll,">$curdir/$DirName/$str/FCT-All-$runTime.tr");
		for(my $j=0;$j<$flowNumber;$j++)
		{		
			chomp $FCT[$j];
			print fFCTAll "$FCT[$j]\n";
			# if($i==0)
			# {
			# 	print fResultEachNormalFCT "$size[$j]";
			# }
			# print fResultEachNormalFCT " $FCT[$j]\n"
		}
		# print fResultEachNormalFCT "\n";
		close(fFCT);
		close(fTraffic);
		close(fFCTAll);
		

		chdir("$curdir/$DirName/$str/$str-$runTime") or die "$!";
		`grep -o -P "totalPacketDropped=[0-9]*" debug.tr|sed "s/totalPacketDropped=//" > ../Packet_Loss-$runTime.tr`;
		`grep -o -P "totalRetransTimes=[0-9]*" debug.tr|sed "s/totalRetransTimes=//" > ../Retrans-$runTime.tr`;
		`grep -o -P "totalTimeoutTimes=[0-9]*" debug.tr|sed "s/totalTimeoutTimes=//" > ../Timeout-$runTime.tr`;
		`grep -o -P "totalParityDropped=[0-9]*" debug.tr|sed "s/totalParityDropped=//" > ../Packet_LossParity-$runTime.tr`;
		`grep -o -P "totalPacketGenerated=[0-9]*" debug.tr|sed "s/totalPacketGenerated=//" > ../TotalPacket-$runTime.tr`;
		`grep -o -P "lossyPacketDropped=[0-9]*" debug.tr|sed "s/lossyPacketDropped=//" > ../Packet_Loss_Failure-$runTime.tr`;

		if($str eq "RMPTCP-RETRAN"
    		|| $str eq "RMPTCP-CODE"
    		|| $str eq "RTCP-RETRAN"
    		|| $str eq "RTCP-CODE"
    		|| $str eq "CORRECTIVE"
    		|| $str eq "PROACTIVE"
    		|| $str eq "REPFLOW")
		{
			`grep -o -P "ParityNumber=[0-9]*" debug.tr|sed "s/ParityNumber=//" > ../ParityNum-$runTime.tr`;
			`grep -o -P "DecodeTimes=[0-9]*" debug.tr|sed "s/DecodeTimes=//" > ../DecodeTimes-$runTime.tr`;
		}
	}	

	print "runTime=$runTime\n";
}

print fResult "$load fctAllAvg 50th 99th packetLoss retransTime timeoutTime parityNum decodeTime lossParity totalPacket packetLossFailure timeoutFlowRate\n";

for(my $i=0;$i<$numberOfMethods;$i++)
{
	my $str=$methods[$i];

	`cat $curdir/$DirName/$str/FCT-All-*.tr > $curdir/$DirName/$str/FCT-All.tr`;
	
	open(fTmp,">>$curdir/$DirName/$str/tmp-FCT-All.tr");
	print fTmp "$str\n";
	close(fTmp);
	for (my $runTime = 0; $runTime < $TimeToRun; $runTime++) 
	{
		`cat $curdir/$DirName/$str/FCT-All-$runTime.tr >> $curdir/$DirName/$str/tmp-FCT-All.tr`;
		if(-e "$curdir/$DirName/$str/Record-Timeout-All-$runTime.tr")
		{
			`cat $curdir/$DirName/$str/Record-Timeout-All-$runTime.tr >> $curdir/$DirName/$str/Record-Timeout-All.tr`;
			`rm $curdir/$DirName/$str/Record-Timeout-All-$runTime.tr`;
		}
	}

	my $timeoutFileStr="$curdir/$DirName/$str/Record-Timeout-All.tr";
	my $countTimeout=0;
	if(!(-z $timeoutFileStr) && (-e $timeoutFileStr))
	{
		$countTimeout = `wc -l < $timeoutFileStr`;
	}

	my $fResultTmpStr="$curdir/$DirName/$str/tmp-FCT-All.tr";
	`paste $fResultEachNormalFCTStr $fResultTmpStr | column -s \$'\t' -t > tmp.tr`;
	`mv tmp.tr $fResultEachNormalFCTStr`;

	# open(fFCTAll,"$curdir/$DirName/$str/FCT-All.tr");
	# my $FCT_All_avg=0;
	# my $countAll=0;
	
	# while(my $line=<fFCTAll>)
	# {
	# 	$FCT_All_avg=$FCT_All_avg+$line;
	# 	$countAll++;
	# }
	# $FCT_All_avg=$FCT_All_avg/$countAll;
	# close(fFCTAll);

	

	# chdir("$curdir/$DirName") or die "$!";

	# `perl cdf_my.pl $curdir/$DirName/$str/FCT-All.tr 99 > tmp.tr`;
	# open(fTmp,"tmp.tr");
	# my $FCT_All_99=<fTmp>;
	# chomp $FCT_All_99;
	# `rm tmp.tr`;
	# close(fTmp);

	# `perl cdf_my.pl $curdir/$DirName/$str/FCT-All.tr 50 > tmp.tr`;
	# open(fTmp,"tmp.tr");
	# my $FCT_All_50=<fTmp>;
	# chomp $FCT_All_50;
	# `rm tmp.tr`;
	# close(fTmp);

	# `cat $curdir/$DirName/$str/Packet_Loss-*.tr > $curdir/$DirName/$str/Packet_Loss.tr`;
	# `cat $curdir/$DirName/$str/Retrans-*.tr > $curdir/$DirName/$str/Retrans.tr`;
	# `cat $curdir/$DirName/$str/Timeout-*.tr > $curdir/$DirName/$str/Timeout.tr`;
	# `cat $curdir/$DirName/$str/Packet_LossParity-*.tr > $curdir/$DirName/$str/Packet_LossParity.tr`;
	# `cat $curdir/$DirName/$str/TotalPacket-*.tr > $curdir/$DirName/$str/TotalPacket.tr`;
	# `cat $curdir/$DirName/$str/Packet_Loss_Failure-*.tr > $curdir/$DirName/$str/Packet_Loss_Failure.tr`;

	# my $ParityNum=0;
	# my $DecodeTimes=0;	
	# if($str eq "RMPTCP-RETRAN"
	# 	|| $str eq "RMPTCP-CODE"
	# 	|| $str eq "RTCP-RETRAN"
 #    	|| $str eq "RTCP-CODE"
	# 	|| $str eq "CORRECTIVE"
	# 	|| $str eq "PROACTIVE"
	# 	|| $str eq "REPFLOW")
	# {
	# 	`cat $curdir/$DirName/$str/ParityNum-*.tr > $curdir/$DirName/$str/ParityNum.tr`;
	# 	`cat $curdir/$DirName/$str/DecodeTimes-*.tr > $curdir/$DirName/$str/DecodeTimes.tr`;
	# 	open(fPN,"$curdir/$DirName/$str/ParityNum.tr");
	# 	open(fDT,"$curdir/$DirName/$str/DecodeTimes.tr");	
	# 	for (my $runTime = 0; $runTime < $TimeToRun; $runTime++) 
	# 	{
	# 		my $line=<fPN>;
	# 		$ParityNum=$ParityNum+$line;
	# 		$line=<fDT>;
	# 		$DecodeTimes=$DecodeTimes+$line;
	# 	}
	# 	close(fPN);
	# 	close(fDT);

	# 	$ParityNum=$ParityNum/($TimeToRun*$flowNumber);
	# 	$DecodeTimes=$DecodeTimes/($TimeToRun*$flowNumber);

	# 	`rm $curdir/$DirName/$str/ParityNum-*.tr`;
	# 	`rm $curdir/$DirName/$str/DecodeTimes-*.tr`;
	# }

	# open(fPL,"$curdir/$DirName/$str/Packet_Loss.tr");
	# open(fRT,"$curdir/$DirName/$str/Retrans.tr");
	# open(fTO,"$curdir/$DirName/$str/Timeout.tr");
	# open(fPLP,"$curdir/$DirName/$str/Packet_LossParity.tr");
	# open(fPTL,"$curdir/$DirName/$str/TotalPacket.tr");
	# open(fPLF,"$curdir/$DirName/$str/Packet_Loss_Failure.tr");
	# my $Packet_Loss=0;
	# my $Retrans=0;
	# my $Timeout=0;
	# my $Packet_LossParity=0;
	# my $TotalPacket=0;
	# my $Packet_Loss_Failure=0;

	# for (my $runTime = 0; $runTime < $TimeToRun; $runTime++) 
	# {
	# 	my $line=<fPL>;
	# 	$Packet_Loss=$Packet_Loss+$line;
	# 	$line=<fRT>;
	# 	$Retrans=$Retrans+$line;
	# 	$line=<fTO>;
	# 	$Timeout=$Timeout+$line;
	# 	$line=<fPLP>;
	# 	$Packet_LossParity=$Packet_LossParity+$line;
	# 	$line=<fPTL>;
	# 	$TotalPacket=$TotalPacket+$line;

	# 	if(defined($line=<fPLF>))
	# 	{
	# 		$Packet_Loss_Failure=$Packet_Loss_Failure+$line;
	# 	}			
	# }
	# close(fPL);
	# close(fRT);
	# close(fTO);
	# close(fPLP);
	# close(fPTL);
	# close(fPLF);

	# $Packet_Loss=$Packet_Loss/($TimeToRun*$flowNumber);
	# $Retrans=$Retrans/($TimeToRun*$flowNumber);
	# $Timeout=$Timeout/($TimeToRun*$flowNumber);	
	# $Packet_LossParity=$Packet_LossParity/($TimeToRun*$flowNumber);	
	# $TotalPacket=$TotalPacket/($TimeToRun*$flowNumber);	
	# $Packet_Loss_Failure=$Packet_Loss_Failure/($TimeToRun*$flowNumber);

	# my $timeoutFlowRate=$countTimeout/($TimeToRun*$flowNumber);
	# print fResult "$str $FCT_All_avg $FCT_All_50 $FCT_All_99 $Packet_Loss $Retrans $Timeout $ParityNum $DecodeTimes $Packet_LossParity $TotalPacket $Packet_Loss_Failure $timeoutFlowRate\n";

	`rm $curdir/$DirName/$str/FCT-All-*.tr`;
	`rm $curdir/$DirName/$str/Packet_Loss-*.tr`;
	`rm $curdir/$DirName/$str/Retrans-*.tr`;
	`rm $curdir/$DirName/$str/Timeout-*.tr`;
	`rm $curdir/$DirName/$str/Packet_LossParity-*.tr`;
	`rm $curdir/$DirName/$str/TotalPacket-*.tr`;
	`rm $curdir/$DirName/$str/Packet_Loss_Failure-*.tr`;
}
