#!/usr/bin/python

# Copyright (C) 2016 The Qt Company Ltd.
# Contact: https://www.qt.io/licensing/
#
# This file is part of the QtScxml module of the Qt Toolkit.
#
# $QT_BEGIN_LICENSE:GPL-EXCEPT$
# Commercial License Usage
# Licensees holding valid commercial Qt licenses may use this file in
# accordance with the commercial license agreement provided with the
# Software or, alternatively, in accordance with the terms contained in
# a written agreement between you and The Qt Company. For licensing terms
# and conditions see https://www.qt.io/terms-conditions. For further
# information use the contact form at https://www.qt.io/contact-us.
#
# GNU General Public License Usage
# Alternatively, this file may be used under the terms of the GNU
# General Public License version 3 as published by the Free Software
# Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
# included in the packaging of this file. Please review the following
# information to ensure the GNU General Public License requirements will
# be met: https://www.gnu.org/licenses/gpl-3.0.html.
#
# $QT_END_LICENSE$

from os import walk
from os.path import isfile, join, splitext

f = open("scion.qrc", "w")
f.write("<!DOCTYPE RCC><RCC version=\"1.0\">\n<qresource>\n")

g = open("scion.h","w")
g.write("const char *testBases[] = {")

first = True
mypath = "scion-tests/scxml-test-framework/test"
for root, _, filenames in walk(mypath):
    for filename in filenames:
        if filename.endswith(".scxml"):
            base = join(root,splitext(filename)[0])
            json = base+".json"
            if isfile(json):
                f.write("<file>")
                f.write(join(root,filename))
                f.write("</file>\n")
                f.write("<file>")
                f.write(json)
                f.write("</file>\n")
                if first:
                    first = False
                else:
                    g.write(",")
                g.write("\n    \"" + base + "\"")

f.write("</qresource></RCC>\n")
f.close()

g.write("\n};\n")
g.close()

