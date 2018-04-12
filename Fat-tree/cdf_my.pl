#!/usr/bin/perl
use warnings;
use strict;

my $argc = @ARGV;
if($argc!=2)
{
	print STDOUT "usage: file percentile\n";
	exit(1);
}

my $percentile = $ARGV[1];

my $file = $ARGV[0];


open my $IN, '<', $file or die $!;

1 while <$IN>;             # Just count the number of lines.
my $line_count = $.;
seek $IN, 0, 0;            # Rewind.

# Calculate the size of the sliding window.
my $remember_count = 1 + (100 - $percentile) * $line_count / 100;

# Initialize the window with the first lines.
my @window = sort { $a <=> $b }
             map scalar <$IN>,
             1 .. $remember_count;
chomp @window;

while (<$IN>) {
    chomp;
    next if $_ < $window[0];
    shift @window;
    my $i = 0;
    $i++ while $i <= $#window and $window[$i] <= $_;
    splice @window, $i, 0, $_;
}
print "$window[0]\n";
