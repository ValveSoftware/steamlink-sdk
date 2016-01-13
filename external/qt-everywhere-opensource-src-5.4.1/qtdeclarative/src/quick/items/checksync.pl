#!/usr/bin/perl
#############################################################################
##
## Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
## Contact: http://www.qt-project.org/legal
##
## This file is part of the Declarative module of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:LGPL21$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and Digia. For licensing terms and
## conditions see http://qt.digia.com/licensing. For further information
## use the contact form at http://qt.digia.com/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 2.1 or version 3 as published by the Free
## Software Foundation and appearing in the file LICENSE.LGPLv21 and
## LICENSE.LGPLv3 included in the packaging of this file. Please review the
## following information to ensure the GNU Lesser General Public License
## requirements will be met: https://www.gnu.org/licenses/lgpl.html and
## http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
##
## In addition, as a special exception, Digia gives you certain additional
## rights. These rights are described in the Digia Qt LGPL Exception
## version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
##
## $QT_END_LICENSE$
##
#############################################################################

use strict;
use warnings;

die "Usage: $0 <QML directory>" if (@ARGV != 1);

my @excludes;
open (SYNCEXCLUDES, "<", "syncexcludes");
while (<SYNCEXCLUDES>) {
    if (/^([a-zA-Z0-9\._]+)/) {
        my $exclude = $1;
        push (@excludes, $exclude);
    }
}

my $portdir = ".";
my $qmldir = $ARGV[0];

opendir (PORTDIR, $portdir) or die "Cannot open port directory";
opendir (QMLDIR, $qmldir) or die "Cannot open QML directory";

my @portfiles = readdir(PORTDIR);
my @qmlfiles = readdir(QMLDIR);

closedir(PORTDIR);
closedir(QMLDIR);

foreach my $qmlfile (@qmlfiles) {
    if ($qmlfile =~ /^qdeclarative.*\.cpp$/ or $qmlfile =~ /qdeclarative.*\.h$/) {

        if (grep { $_ eq $qmlfile} @excludes) {
            next;
        }

        my $portfile = $qmlfile;
        $portfile =~ s/^qdeclarative/qsg/;

        if (grep { $_ eq $portfile} @portfiles) {

            open (PORTFILE, "<", "$portdir/$portfile") or die("Cannot open $portdir/$portfile for reading");

            my $firstline = <PORTFILE>;

            close (PORTFILE);

            if ($firstline and $firstline =~ /^\/\/ Commit: ([a-z0-9]+)/) {
                my $sha1 = $1;
                my $commitSha1 = "";

                my $output = `cd $qmldir; git log $qmlfile | head -n 1`;
                if ($output =~ /commit ([a-z0-9]+)/) {
                    $commitSha1 = $1;
                }

                if ($commitSha1 eq $sha1) {
                    print ("$portfile: OK\n");
                } else {
                    print ("$portfile: OUT OF DATE\n");
                }
            } else {
                print ("$portfile: OUT OF DATE\n");
            }
        } else {
            print ("$portfile: MISSING\n");
        }
    }
}
