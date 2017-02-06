#! /usr/bin/env python
#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the build configuration tools of the Qt Toolkit.
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


import sys;
import re;
import os;
import subprocess;
import errno;

instructions = "This script can be used as follows:\n\
    a) if run from tests/auto without any arguments it runs unit tests and then integration tests\n\
    b) if run from tests/auto/unit, it runs unit tests\n\
    c) if run from tests/auto/integration, it runs integration tests\n\
    d) if run from tests/auto with \"unit\" it runs unit tests, and correspondingly for \"integration\""


# Colors
red="\033[41;37m";
redfg="\033[31m";
norm="\033[0m";
green="\033[32m";
grey="\033[37m";
yellow="\033[33m";

# Variables
curtest = "";
numpasses = [0];
numfails = [0];
numcrashes = 0;
numx = [0];
runTests = []
notRunTests = []

# Do not run the tests in these directories.
exclusionList = ["qdeclarativevideo", "qmultimedia_common"]

# Helper function for replacing stuffs
def print_color_string(string, color, match, index):
    if index > 0:
        print string[:match.start(index)] + color + string[match.start(index):match.end(index)] + norm + string[match.end(index):],
    else:
        print color + string[:-1] + norm

# AWK translation
awkfoo = [
    (re.compile("\*\*\*\*\*\*\*\*\* Start testing of (\S+)"), yellow, 1, curtest),
    (re.compile("^(PASS) "), green, 1, numpasses),
    (re.compile("^(FAIL!) "), red, 0, numfails),
    (re.compile("^(XFAIL) "), redfg, 1, numx),
    (re.compile("^(XPASS) "), redfg, 1, numx),
    (re.compile("^(QFATAL) "), red, 0, numx),
    (re.compile("^(QDEBUG) "), grey, 0, None),
    (re.compile("^(QWARN) "), yellow, 1, None),
    (re.compile("\*\*\*\*\*\*\*\*\* Finished testing of (\S+)"), yellow, 1, curtest),
    ]


#
# This method runs the test cases, color codes the output from the test cases and adds up the passes,
# fails etc.
#
def resultSummary(arg):
    try:
        pp = subprocess.Popen(arg, shell=False,stderr=subprocess.STDOUT,stdout=subprocess.PIPE);
        p = pp.stdout;
        try:
            while True:
                line = p.readline()
                if len(line) == 0:
                    break
                for (re, color, index, var) in awkfoo:
                    m = re.match(line)
                    if m:
                        break
                if m:
                    print_color_string(line, color, m, index)
                    if isinstance(var, list):
                        var[0] = var[0] + 1;
                    else:
                        var = m.groups(index)
                else:
                    print line,
        finally:
            rc = p.close();
            pp.wait();
            if pp.returncode < 0:
                print red + "Error: '%s' exited with signal %d" % (arg, -pp.returncode) + norm
                numcrashes = numcrashes + 1
    except OSError, e:
        if e.errno == errno.ENOENT:
            print red + "Test '%s' not found." % arg + norm;
        else:
            print red + "Got an exception running '%s': %s " % (arg, e.strerror) + norm
        numcrashes = numcrashes + 1

#
# This method finds the test cases that should be run and runs them.
#
def runAllTests(test):
    for filename in os.listdir(test):
        if(re.search("^q", filename)):

            #Skip the dir if it is in the exclusion list.
            exclude = False
            for dir in exclusionList:
                if(re.search(dir, filename)):
                    exclude = True
            if(not(exclude)):
                #Set path to this if on Windows
                if(os.name=="nt"):
                    exePath = test+"\\"+filename+"\\debug\\tst_"+filename+".exe"
                #Set path on OS X
                if(sys.platform=="darwin"):
                    exePath = test +"/"+filename+"/tst_"+filename
                    if not (os.path.exists(exePath)):
                        exePath = test + "/"+filename+"/tst_"+filename+".app/Contents/MacOS/tst_"+filename
                #Set path to this if on Unix
                else:
                    exePath = test +"/"+filename+"/tst_"+filename

                if(os.path.exists(exePath)):
                    runTests.append(filename)
                    resultSummary(exePath);
                else:
                    notRunTests.append(filename)

arguments = sys.argv[1:]
count = len(arguments)

# Find the current working directory.
cwd = os.getcwd()

if(count == 0):
    if re.search("auto$", cwd):
        x = 0
        runAllTests("unit")
        runAllTests("integration")
    elif re.search("unit$", cwd):
        runAllTests(cwd)
    elif re.search("integration$", cwd):
        runAllTests(cwd)
    else:
        print "You are running this script from the wrong directory! " + instructions
        exit()
elif(count == 1):
    if os.path.exists(sys.argv[1]):
        runAllTests(sys.argv[1])
    else:
        print sys.argv[1] + " test cases do not exist! " + instructions
        exit()
else:
    print "You have passed too many arguments! " + instructions
    exit()

print "Total of all tests: %d passes, %d failures, %d unexpected, %d badnesses." % (numpasses[0], numfails[0], numx[0], numcrashes);


if runTests:
    print "The following test cases were run: "
    for testCase in runTests:
        print testCase
else:
    print "No test cases were run!"


if notRunTests:
    print "The following test cases could not be run: "
    for testCase in notRunTests:
        print testCase
else:
    print "All test cases were run."
