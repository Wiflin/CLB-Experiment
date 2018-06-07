#!/usr/bin/perl -w
use strict;
use POSIX;

my $num_args = $#ARGV + 1;
if ($num_args != 1 && $num_args != 2) {
    print "Usage: perl $0 [load (0-1)] [rand seed (optional)]\n";
    exit(1);
}


my $linkRateNumber=1E10;
my $flowPerHost=10;
my $hostInTotal=24;

my $load="$ARGV[0]";

srand("$ARGV[1]");


my $serverPerBiNetwork=$hostInTotal/2;

my $flowNumber=$flowPerHost*$serverPerBiNetwork;

my @startTime=(2)x$serverPerBiNetwork;

####[9][2]///Data mining
my @WorkLoad=(
	[1,0],
	[1,0.5],
	[2,0.6],
	[3,0.7],
	[7,0.8],
	[267,0.9],
	[2107,0.95],
	[66667,0.99],
	[666667,1],
);

####[12][2]///Web search
# my @WorkLoad=(
# 	[6,0],
# 	[6,0.15],
# 	[13,0.2],
# 	[19,0.3],
# 	[33,0.4],
# 	[53,0.53],
# 	[133,0.6],
# 	[667,0.7],
# 	[1333,0.8],
# 	[3333,0.9],
# 	[6667,0.97],
# 	[20000,1],
# );




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
open(fTraffic,">InputFlow-$hostInTotal-$flowNumber.tr");

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

	}
}
close(fTraffic);