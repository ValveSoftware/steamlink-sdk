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

import random
"""stupid generator of a large scxml state machine"""
nStatesMax=10
depth=5
breath=10
nTransitions=8
nEvents=100

nStates = 0
tTotal = 0
depthMax = 0
f = open("out.scxml", "w")
f.write("""<scxml xmlns="http://www.w3.org/2005/07/scxml"
    initial="s_1"
    version="1.0">""")
knownStates=[]
sIndex=[1]
depthLevel=0
breathLevel=[nStatesMax]
while True:
    sName=reduce(lambda x,y:x+"_"+y, map(str,sIndex), "s")
    knownStates.append(sName)
    f.write("<state id=\"%s\" >\n" % sName)
    nStates += 1
    if nStates < nStatesMax and depthLevel < depth and random.random() < 0.5:
        # go deeper
        sIndex.append(1)
        breathLevel.append(random.randint(1,breath))
        depthLevel += 1
        if depthMax < depthLevel:
            depthMax = depthLevel
        continue
    while True:
        for iTransition in range(random.randint(1,nTransitions)):
            tTotal += 1
            target = random.choice(knownStates)
            event = ("E%d" % random.randint(1,nEvents))
            f.write("""<transition event="%s" target="%s" />\n""" % (event, target))
        f.write("</state>\n")
        sIndex[depthLevel] += 1
        if (nStates < nStatesMax and breathLevel[depthLevel] > sIndex[depthLevel]):
            break
        depthLevel -= 1
        if depthLevel < 0:
            break
        sIndex.pop()
        breathLevel.pop()
    if depthLevel < 0:
        break
f.write("</scxml>\n")
f.close()
print "totalStates: ", nStates
print "totalTransitions: ", tTotal
print "depthMax: ", depthMax + 1
