#!/usr/bin/perl -w
use strict;
use POSIX;

my $workload="websearch";
my $smallSize=100*1024;
my $largeSize=10240*1024;

my $linkDelay=0.02;
my $linkRate='40G';
my $switchPortNumber=4;
my $switchDownPortNumber=8;
my $linkAccessRate='10G';
my $switchLayer=3;

my $failureNumber=3;
my $flowPerHost=1000;

my $subFlowNum=8;

print "linkDelay=$linkDelay(ms), linkRate=$linkRate, switchPortNumber=$switchPortNumber, switchDownPortNumber=$switchDownPortNumber, linkAccessRate=$linkAccessRate, failureNumber=$failureNumber, flowPerHost=$flowPerHost, subFlowNum=$subFlowNum\n";


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

open(fResultSmall,">$workload.result.small");
open(fResultMiddle,">$workload.result.middle");
open(fResultLarge,">$workload.result.large");

my @loadstr=("0.1",".2",".3",".4",".5",".6",".7",".8",".9");
my @methods=("RMPTCP-RETRAN","ECMP","MPTCP","REACTIVE","CORRECTIVE","REPFLOW","PROACTIVE");

## 2-D arrary
# smallFlowAvg[i][j]   i=load, j=methods
my @smallFlowAvg;
my @smallFlow99;
my @smallFlow999;
my @middleFlowAvg;
my @middleFlow99;
my @middleFlow999;
my @largeFlowAvg;
my @largeFlow99;
my @largeFlow999;

my $loadID=0;
foreach my $load (@loadstr)
{
	my $methodID=0;
	foreach my $method (@methods)
	{
		my @smallFCT;
		my @middleFCT;
		my @largeFCT;

		my $smallFCTtotal=0;
		my $middleFCTtotal=0;
		my $largeFCTtotal=0;

		for (my $runTime = 0; $runTime < $TimeToRun; $runTime++) 
		{
			my $DirName="Long-connection-$workload-$subFlowNum"."SF-$failureNumber-$load-$switchDownPortNumber-$linkRate";
			# print "$DirName\n";
			if(!(-e "$DirName"))
			{
				print "No such result folder";
				exit(1);
			}
			$DirName="$DirName/$method/$method-$runTime";

			open(fTraffic,"$DirName/InputTraffic-$hostInTotal-$flowNumber.tr");
			chomp(my @flowRaw=<fTraffic>);
			close(fTraffic);
			my $countFlow = `wc -l < $DirName/InputTraffic-$hostInTotal-$flowNumber.tr`;
			chomp $countFlow;

			open(fFCT,"$DirName/Sack-400-fct.tr");
			chomp(my @fctRaw=<fFCT>);
			close(fFCT);
			my $countFCT = `wc -l < $DirName/Sack-400-fct.tr`;
			chomp $countFCT;
			
			if($countFCT==0 || $countFlow!=$countFCT)
			{
				print "$DirName/Sack-400-fct.tr countFCT=$countFCT countFlow=$countFlow!\n";
				next;
			}

			for (my $i=0;$i<$countFCT;$i++)
			{
				my @flowEntries=split(' ',$flowRaw[$i]);
				my $flowSize=$flowEntries[3];
				if($flowSize<$smallSize)
				{
					push @smallFCT,	$fctRaw[$i];
					$smallFCTtotal=$smallFCTtotal+$fctRaw[$i];
					# if($i<10)
					# {
					# 	print "$DirName flow\[$i\]:\tflowSize=$flowSize\tfct=$fctRaw[$i]\n";
					# }
				}

				if($flowSize>=$smallSize && $flowSize<$largeSize)
				{
					push @middleFCT, $fctRaw[$i];
					$middleFCTtotal=$middleFCTtotal+$fctRaw[$i];
				}

				if($flowSize>=$largeSize)
				{
					push @largeFCT, $fctRaw[$i];
					$largeFCTtotal=$largeFCTtotal+$fctRaw[$i];
				}
			}
		}

		my $smallFlowNum=@smallFCT;
		my $middleFlowNum=@middleFCT;
		my $largeFlowNum=@largeFCT;
		print "$load\t $method:\t smallFlowNum=$smallFlowNum\t middleFlowNum=$middleFlowNum\t largeFlowNum=$largeFlowNum\n";

		@smallFCT = sort { $a <=> $b } @smallFCT;
		@middleFCT = sort { $a <=> $b } @middleFCT;
		@largeFCT = sort { $a <=> $b } @largeFCT;

		$smallFlowAvg[$loadID][$methodID]=$smallFCTtotal/$smallFlowNum;
		$smallFlow99[$loadID][$methodID]=$smallFCT[int($smallFlowNum*0.99)];
		$smallFlow999[$loadID][$methodID]=$smallFCT[int($smallFlowNum*0.999)];
		print "$smallFlowAvg[$loadID][$methodID]\t$smallFlow99[$loadID][$methodID]\t$smallFlow999[$loadID][$methodID]\n";

		$middleFlowAvg[$loadID][$methodID]=$middleFCTtotal/$middleFlowNum;
		$middleFlow99[$loadID][$methodID]=$middleFCT[int($middleFlowNum*0.99)];
		$middleFlow999[$loadID][$methodID]=$middleFCT[int($middleFlowNum*0.999)];
		print "$middleFlowAvg[$loadID][$methodID]\t$middleFlow99[$loadID][$methodID]\t$middleFlow999[$loadID][$methodID]\n";

		$largeFlowAvg[$loadID][$methodID]=$largeFCTtotal/$largeFlowNum;
		$largeFlow99[$loadID][$methodID]=$largeFCT[int($largeFlowNum*0.99)];
		$largeFlow999[$loadID][$methodID]=$largeFCT[int($largeFlowNum*0.999)];
		print "$largeFlowAvg[$loadID][$methodID]\t$largeFlow99[$loadID][$methodID]\t$largeFlow999[$loadID][$methodID]\n";

		$methodID++;
	}
	$loadID++;
}

print fResultSmall "LOAD";
foreach my $load (@loadstr)
{
	print fResultSmall "\t$load\t\t";
}
print fResultSmall "\n";
foreach my $load (@loadstr)
{
	print fResultSmall "\tAvg\t99th\t99.9th";
}
my $methodID=0;
foreach my $method (@methods)
{
	print fResultSmall "\n$method";
	my $loadID=0;
	foreach my $load (@loadstr)
	{
		print fResultSmall "\t$smallFlowAvg[$loadID][$methodID]\t$smallFlow99[$loadID][$methodID]\t$smallFlow999[$loadID][$methodID]";
		$loadID++;
	}
	$methodID++;
}

print fResultMiddle "LOAD";
foreach my $load (@loadstr)
{
	print fResultMiddle "\t$load\t\t";
}
print fResultMiddle "\n";
foreach my $load (@loadstr)
{
	print fResultMiddle "\tAvg\t99th\t99.9th";
}
my $methodID=0;
foreach my $method (@methods)
{
	print fResultMiddle "\n$method";
	my $loadID=0;
	foreach my $load (@loadstr)
	{
		print fResultMiddle "\t$middleFlowAvg[$loadID][$methodID]\t$middleFlow99[$loadID][$methodID]\t$middleFlow999[$loadID][$methodID]";
		$loadID++;
	}
	$methodID++;
}

print fResultLarge "LOAD";
foreach my $load (@loadstr)
{
	print fResultLarge "\t$load\t\t";
}
print fResultLarge "\n";
foreach my $load (@loadstr)
{
	print fResultLarge "\tAvg\t99th\t99.9th";
}
my $methodID=0;
foreach my $method (@methods)
{
	print fResultLarge "\n$method";
	my $loadID=0;
	foreach my $load (@loadstr)
	{
		print fResultLarge "\t$largeFlowAvg[$loadID][$methodID]\t$largeFlow99[$loadID][$methodID]\t$largeFlow999[$loadID][$methodID]";
		$loadID++;
	}
	$methodID++;
}
