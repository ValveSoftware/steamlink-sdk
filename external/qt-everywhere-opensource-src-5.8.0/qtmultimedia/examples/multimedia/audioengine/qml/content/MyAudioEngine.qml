/****************************************************************************
 **
 ** Copyright (C) 2015 The Qt Company Ltd.
 ** Contact: http://www.qt.io/licensing/
 **
 ** This file is part of the examples of the Qt Toolkit.
 **
 ** $QT_BEGIN_LICENSE:BSD$
 ** You may use this file under the terms of the BSD license as follows:
 **
 ** "Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions are
 ** met:
 **   * Redistributions of source code must retain the above copyright
 **     notice, this list of conditions and the following disclaimer.
 **   * Redistributions in binary form must reproduce the above copyright
 **     notice, this list of conditions and the following disclaimer in
 **     the documentation and/or other materials provided with the
 **     distribution.
 **   * Neither the name of The Qt Company Ltd nor the names of its
 **     contributors may be used to endorse or promote products derived
 **     from this software without specific prior written permission.
 **
 **
 ** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 ** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 ** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 ** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 ** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 ** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 ** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 ** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 **
 ** $QT_END_LICENSE$
 **
 ****************************************************************************/

import QtAudioEngine 1.0
import QtQuick 2.0

AudioEngine {

    AudioCategory {
        name:"sfx"
        volume: 1
    }

    AudioCategory {
        name:"music"
        volume: 1
    }


    AttenuationModelInverse {
       name:"default"
       start: 20
       end: 1000
       rolloff: 1
    }

    AttenuationModelLinear {
       name:"shipengine"
       start: 20
       end: 180
    }

    AudioSample {
        name:"engine"
        source: "engine-loop.wav"
        preloaded:true
    }

    AudioSample {
        name:"horn"
        source: "horn.wav"
    }
    AudioSample {
        name:"whistle"
        source: "whistle.wav"
    }
    Sound {
        name:"shipengine"
        attenuationModel:"shipengine"
        category:"sfx"
        PlayVariation {
            looping:true
            sample:"engine"
            maxGain:0.9
            minGain:0.8
        }
    }

    Sound {
        name:"effects"
        category:"sfx"
        PlayVariation {
            sample:"horn"
            maxGain:2.0
            minGain:0.9
        }
        PlayVariation {
            sample:"whistle"
            maxGain:1.0
            minGain:0.8
        }
    }

    dopplerFactor: 1
    speedOfSound: 343.33

    listener.up:"0,0,1"
    listener.position:"0,0,0"
    listener.velocity:"0,0,0"
    listener.direction:"0,1,0"
}
