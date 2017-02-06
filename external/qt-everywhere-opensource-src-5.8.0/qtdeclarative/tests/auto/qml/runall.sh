#!/bin/bash
#
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
            QQmlDebugServer:*Waiting*) ;;
            QQmlDebugServer:*Connection*) ;;
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

