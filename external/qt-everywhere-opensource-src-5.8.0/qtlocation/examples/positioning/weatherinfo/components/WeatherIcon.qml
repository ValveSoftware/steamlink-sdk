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

import QtQuick 2.0

Item {
    id: container

    property string weatherIcon: "01d"

    //server icons are too small. we keep using the local images
    property bool useServerIcon: true

    Image {
        id: img
        source: {
            if (useServerIcon)
                "http://openweathermap.org/img/w/" + container.weatherIcon + ".png"
            else {
                switch (weatherIcon) {
                case "01d":
                case "01n":
                     "../icons/weather-sunny.png"
                    break;
                case "02d":
                case "02n":
                    "../icons/weather-sunny-very-few-clouds.png"
                    break;
                case "03d":
                case "03n":
                    "../icons/weather-few-clouds.png"
                    break;
                case "04d":
                case "04n":
                    "../icons/weather-overcast.png"
                    break;
                case "09d":
                case "09n":
                    "../icons/weather-showers.png"
                    break;
                case "10d":
                case "10n":
                    "../icons/weather-showers.png"
                    break;
                case "11d":
                case "11n":
                    "../icons/weather-thundershower.png"
                    break;
                case "13d":
                case "13n":
                    "../icons/weather-snow.png"
                    break;
                case "50d":
                case "50n":
                    "../icons/weather-fog.png"
                    break;
                default:
                    "../icons/weather-unknown.png"
                }
            }
        }
        smooth: true
        anchors.fill: parent
    }
}
