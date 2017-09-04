#!/bin/bash
#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is the build configuration utility of the Qt Toolkit.
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

# Generate some simple test data.  Uses "sox".

endian=""
endian_extn=""

for channel in 1 2; do
    if [ $channel -eq 1 ]; then
        endian="little"
        endian_extn="le"
    fi

    if [ $channel -eq 2 ]; then
        endian="big"
        endian_extn="be"
    fi
    for samplebits in 8 16 32; do
        for samplerate in 44100 8000; do
            if [ $samplebits -ne 8 ]; then
                sox -n --endian "${endian}" -c ${channel} -b ${samplebits} -r ${samplerate} isawav_${channel}_${samplebits}_${samplerate}_${endian_extn}.wav synth 0.25 sine 300-3300
            else
                sox -n -c ${channel} -b ${samplebits} -r ${samplerate} isawav_${channel}_${samplebits}_${samplerate}.wav synth 0.25 sine 300-3300
            fi
        done
     done
done

