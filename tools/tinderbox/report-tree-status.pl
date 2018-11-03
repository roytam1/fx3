#!/usr/bin/perl
# -*- Mode: perl; indent-tabs-mode: nil -*-

#
# Graph tree-open status, via tbox graph server.
#

# Location of this file.  Make sure that hand-runs and crontab-runs
# of this script read/write the same data.
my $script_dir = "/builds/tinderbox/mozilla/tools/tinderbox";  

# Send data to graph server via HTTP.
require "$script_dir/reportdata.pl";

use Sys::Hostname;  # for ::hostname()

my $sheriff_string;

sub is_tree_open {
  my $tbox_url = "http://tinderbox.mozilla.org/showbuilds.cgi?tree=SeaMonkey";

  # Dump tbox page source into a file.
  print "HTTP...";
  system ("wget", "-q", "-O", "$script_dir/tbox.source", $tbox_url);
  print "done\n";

  my $rv = 0;

  $sheriff_string = "";

  # Scan file, looking for line that starts with <a NAME="open">
  open TBOX_FILE, "$script_dir/tbox.source";
  while (<TBOX_FILE>) {
    # Scan for open string
    if(/^<a NAME="open">/) {
      # look for "open" string
      if (/open<\/font>$/) {
        print "open\n";
        $rv = 1;
      } else {
        print "closed\n";
        $rv = 0;
      }
    }

    # Scan for sheriff string & save it off for HTTP submit later.
    if(/^<br><a NAME="sheriff"><\/a>/) {
      chomp;
      $sheriff_string = $_;

      # Strip out content to save space.

      # Strip out permanent content.
      $sheriff_string =~ s/^<br><a NAME="sheriff"><\/a>//;
      
      # Crude attempt at reducing the random html that shows up here.
      # Order is important, pick off easy tags, then make it legal cgi,
      # then shorten it up.
      $sheriff_string =~ s/<[pP]>//g;
      $sheriff_string =~ s/\015//g;   # ^M
      $sheriff_string =~ s/<br>//g;
      $sheriff_string =~ s/<\/a>//g;
      $sheriff_string =~ s/<//g;
      $sheriff_string =~ s/>//g;
      $sheriff_string =~ s/"/ /g;
      $sheriff_string =~ s/\///g;
      $sheriff_string =~ s/\\//g;
      $sheriff_string =~ s/#/[lb]/g;
      $sheriff_string =~ s/mailto://g;
      $sheriff_string =~ s/[aA][iI][mM]://g;
      $sheriff_string =~ s/[iI][rR][cC]://g;
      $sheriff_string =~ s/a href//g;
      $sheriff_string =~ s/[sS]heriff//g;
      $sheriff_string =~ s/[mM]onday//g;
      $sheriff_string =~ s/[tT]uesday//g;
      $sheriff_string =~ s/[wW]ednesday//g;
      $sheriff_string =~ s/[tT]hursday//g;
      $sheriff_string =~ s/[fF]riday//g;
      $sheriff_string =~ s/[wW]eekend//g;
      $sheriff_string =~ s/^[tT]he //g;
      $sheriff_string =~ s/\/a//g;
      $sheriff_string =~ s/netscape.com/nscp/g;
      $sheriff_string =~ s/mozilla.org/moz/g;
      $sheriff_string =~ s/ is / /g;
      $sheriff_string =~ s/ for / /g;
      $sheriff_string =~ s/ on / /g;
      $sheriff_string =~ s/ in / /g;
      $sheriff_string =~ s/ = / /g;
      $sheriff_string =~ s/ - / /g;

      $sheriff_string = substr($sheriff_string,0,60);
      print "sheriff string = $sheriff_string\n";
    }
  }
  close TBOX_FILE;

  # Clean up.
  unlink("$script_dir/tbox.source");

  return $rv;
}


# main
{
  my $time_since_open = 0;
  my $timefile = "$script_dir/treeopen_timefile";

  # Get tree status.
  if(is_tree_open()) {

    # Record tree open time if not set.
    if (not (-e "$timefile")) {
      open TIMEFILE, ">$timefile";
      print TIMEFILE time();
      close TIMEFILE;
    } else {
      # Timefile found, compute difference and report that number.
      print "found timefile: $timefile!\n";     
      
      my $time_tree_opened = 0;
      my $now = 0;
  
      open TIMEFILE, "$timefile";
      while (<TIMEFILE>) {
        chomp;
        $time_tree_opened = $_;
      }
      close TIMEFILE;
      print "time_tree_opened = $time_tree_opened\n";
      
      $now = time();
      print "now = $now\n";

      # Report time in hours.
      $time_since_open = ($now - $time_tree_opened)/3600;
      print "time_since_open (hours) = $time_since_open\n";

      # Clamp time to 20 hours so we don't get huge spikes for
      # extended open times (weekends)
      if($time_since_open > 20.0) {
        $time_since_open = 20.0;
      }
      
    }
    
  } else {
    # tree is closed, leave tree_open_time at zero.

    # Delete timefile if there is one.
    if (-e "$timefile") {
      unlink("$timefile");
    }
  }

  ReportData::send_results_to_server("tegu.mozilla.org", 
                                     "$time_since_open",
                                     "$sheriff_string",
                                     "treeopen", ::hostname());
}
