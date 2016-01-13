#!/bin/sh
#############################################################################
##
## Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
## Contact: http://www.qt-project.org/legal
##
## This file is part of the QtQml module of the Qt Toolkit.
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

# printProperty(): prints a key-value pair from given key and cmd list.
# If running cmd fails, or does not produce any stdout, nothing is printed.
# Arguments: $1: key, $2: cmd, $3: optional, field specification as to cut(1) -f
printProperty ()
{
    key=$1
    val=`{ eval $2 ; } 2>/dev/null`
    [ -n "$3" ] && val=`echo $val | tr -s '[:blank:]' '\t' | cut -f$3`
    [ -n "$val" ] && echo $key: $val
}

# printEnvVar(): prints a key-value pair from given environment variable name.
# key is printed as "Env_<varname>".
# If the variable is undefined, value is printed as UNDEFINED.
# Arguments: $1: varname

printEnvVar ()
{
    key=Env_$1
    val=`eval 'echo $'$1`
    [ -z "$val" ] && val='[undefined]'
    echo $key: $val
}


# printOnOff(): prints a key-value pair from given environment variable name.
# If variable is defined, value is printed as "<key>-On"; otherwise "<key>-Off".
# Arguments: $1: key $2: varname

printOnOff ()
{
    key=$1
    val=`eval 'echo $'$2`
    if [ -z "$val" ] ; then
        val=Off
    else
        val=On
    fi
    echo $key: $key-$val
}

# ------------

printProperty Uname              "uname -a"

printProperty WlanMAC            "ifconfig wlan0 | grep HWaddr" 5

printEnvVar QMLSCENE_DEVICE
