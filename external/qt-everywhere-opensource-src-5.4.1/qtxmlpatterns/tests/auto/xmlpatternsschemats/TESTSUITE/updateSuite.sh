#!/bin/bash
#############################################################################
##
## Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
## Contact: http://www.qt-project.org/legal
##
## This file is part of the test suite of the Qt Toolkit.
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
