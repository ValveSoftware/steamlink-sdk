#! /usr/bin/perl -w
#############################################################################
##
## Copyright (C) 2017 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the translations module of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################


use strict;

my @groups = (
    "qtbase", "qtdeclarative", "qtxmlpatterns", "qtmultimedia", "qtscript", "qtquick1",
    "qtquickcontrols", "qtquickcontrols2",
    "qtlocation", "qtconnectivity", "qtwebsockets", "qtserialport", "qtwebengine",
#    "qtdocgallery", "qtpim", "qtsystems",
    "assistant", "designer", "linguist", "qt_help", "qtconfig", "qmlviewer"
);

my %scores = ();
my %langs = ();

my $files = join("\n", <*_??.ts>);
my $res = `xmlpatterns -param files=\"$files\" check-ts.xq`;
for my $i (split(/ /, $res)) {
  $i =~ /^([^.]+)\.ts:(.*)$/;
  my ($fn, $pc) = ($1, $2);
  for my $g (@groups) {
    if ($fn =~ /^${g}_((.._)?..)$/) {
      my $lang = $1;
      $scores{$g}{$lang} = $pc;
      $langs{$lang} = 1;
      last;
    }
  }
}

my $code = "";

print "L10n  ";
for my $g (@groups) {
  print " ".$g." ";
}
print "\n";
for my $lang (sort(keys(%langs))) {
  printf "%-5s ", $lang;
  my $qt = 1;
  my $rest = 1;
  my $line = "";
  for my $g (@groups) {
    my $pc = $scores{$g}{$lang};
    $pc = "0" if !defined($pc);
    if (int($pc) < 98 or !$qt) {
      if ($g eq "qt") {
        $qt = 0;
      } else {
        $rest = 0;
      }
    } else {
      $line .= " ".$g."_".$lang.".ts";
    }
    printf " %-".(length($g)+1)."s", $pc;
  }
  if ($qt) {
    $code .= " \\\n   ".$line;
    if (!$rest) {
      print "   (partial)";
    }
  } else {
    print "   (excluded)";
  }
  print "\n";
}

my $fn = "translations.pro";
my $nfn = $fn."new";
open IN, $fn or die;
open OUT, ">".$nfn or die;
while (1) {
  $_ = <IN>;
  last if (/^TRANSLATIONS /);
  print OUT $_;
}
while ($_ =~ /\\\n$/) {
  $_ = <IN>;
}
print OUT "TRANSLATIONS =".$code."\n";
while (<IN>) {
  print OUT $_;
}
close OUT;
close IN;
rename $nfn, $fn;
