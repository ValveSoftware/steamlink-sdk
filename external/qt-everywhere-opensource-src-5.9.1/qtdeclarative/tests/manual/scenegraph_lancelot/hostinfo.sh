#!/bin/sh
#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the QtQml module of the Qt Toolkit.
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
# If the variable is undefined, nothing is printed.
# Arguments: $1: varname

printEnvVar ()
{
    key=Env_$1
    val=`eval 'echo $'$1`
    [ -n "$val" ] && echo $key: $val
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
