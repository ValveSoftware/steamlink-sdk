#!/bin/sh
#
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
############################################################################/

if [ "$(uname)" = Linux ]
then
    Xnest :7 2>/dev/null &
    sleep 1
    trap "kill $!" EXIT
    export DISPLAY=:7
    export LANG=en_US
    kwin 2>/dev/null &
    sleep 1
fi

function filter
{
    exe=$1
    skip=0
    while read line
    do
        if [ $skip != 0 ]
        then
            let skip=skip-1
        else
            case "$line" in
            make*Error) echo "$line";;
            make*Stop) echo "$line";;
            /*/bin/make*) ;;
            make*) ;;
            install*) ;;
            QDeclarativeDebugServer:*Waiting*) ;;
            QDeclarativeDebugServer:*Connection*) ;;
            */qmake*) ;;
            */bin/moc*) ;;
            *targ.debug*) ;;
            g++*) ;;
            cd*) ;;
            XFAIL*) skip=1;;
            SKIP*) skip=1;;
            PASS*) ;;
            QDEBUG*) ;;
            Makefile*) ;;
            Config*) ;;
            Totals*) ;;
            \**) ;;
            ./*) ;;
            *tst_*) echo "$line" ;;
            *) echo "$exe: $line"
            esac
        fi
    done
}

make -k -j1 install 2>&1 | filter build
for exe in $(make install | sed -n 's/^install .* "\([^"]*qt4\/tst_[^"]*\)".*/\1/p')
do
    echo $exe
    $exe 2>&1 | filter $exe
done

