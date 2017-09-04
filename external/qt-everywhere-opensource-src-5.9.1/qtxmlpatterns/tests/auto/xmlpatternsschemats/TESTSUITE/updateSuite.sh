#!/bin/bash
#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the test suite of the Qt Toolkit.
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

# This script updates the suite from W3C's server.
#
# NOTE: the files checked out CANNOT be added to Qt's
# repository at the moment, due to legal complications.
#
# To run the script, Saxon package version 9 and above shall be installed
#

DIRECTORY_NAME="xmlschema2006-11-06"
ARCHIVE_NAME="xsts-2007-06-20.tar.gz"

rm -Rf $DIRECTORY_NAME

wget http://www.w3.org/XML/2004/xml-schema-test-suite/xmlschema2006-11-06/$ARCHIVE_NAME
tar -xzf $ARCHIVE_NAME
rm $ARCHIVE_NAME

# cvs script is used to retrieve newer version of test suite.
#CVSROOT=:pserver:anonymous@dev.w3.org:/sources/public cvs login
#CVSROOT=:pserver:anonymous@dev.w3.org:/sources/public cvs checkout -d xmlschema2006-11-06-new XML/xml-schema-test-suite/2004-01-14/xmlschema2006-11-06

#Saxon need to be installed before the following command works.
java -jar /usr/share/java/saxon.jar -xsl:unifyCatalog.xsl -s:$DIRECTORY_NAME/suite.xml > testSuites.xml
