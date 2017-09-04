#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the QtWebEngine module of the Qt Toolkit.
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

import re
import sys
import os

mocables = set()
includedMocs = set()
dir = sys.argv[1]
files = sys.argv[2:]

for f in filter(os.path.isfile, [os.path.join(dir, f) for f in files]):
    inBlockComment = False
    for line in open(f).readlines():
        # Block comments handling
        if "/*" in line:
            inBlockComment = True
        if inBlockComment and "*/" in line:
            inBlockComment = False
            if line.find("*/") != len(line) - 3:
                line = line[line.find("*/")+2:]
            else:
                continue
        if inBlockComment:
            continue
        #simple comments handling
        if "//" in line:
            line = line.partition("//")[0]
        if re.match(".*Q_OBJECT", line):
            mocables.add(f)
        im = re.search('#include "(moc_\w+.cpp)"', line)
        if im:
            includedMocs.add(im.group(1))

for mocable in includedMocs:
    print "Found included moc: " + mocable

assert len(includedMocs) == 0 , "Included mocs are not supported !"

for mocable in mocables:
    print mocable
sys.exit(0)
