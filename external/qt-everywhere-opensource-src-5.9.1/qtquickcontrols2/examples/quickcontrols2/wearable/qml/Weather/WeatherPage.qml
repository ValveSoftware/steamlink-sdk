/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

import QtQuick 2.7
import QtQuick.Controls 2.0 as QQC2
import "../Style"
import "weather.js" as WeatherData

Item {
    QQC2.SwipeView {
        id: svWeatherContainer

        anchors.fill: parent

        Item {
            id: weatherPage1

            Row {
                anchors.centerIn: parent
                spacing: 2

                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    source: "images/temperature.png"
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 40

                    Text {
                        text: (wDataCntr.weatherData
                               && wDataCntr.weatherData.main
                               && wDataCntr.weatherData.main.temp) ?
                                  qsTr("Avg: ")
                                  + String(wDataCntr.weatherData.main.temp)
                                  + " °F" : "N/A"
                        font.pixelSize: UIStyle.fontSizeM
                        font.letterSpacing: 1
                        color: UIStyle.colorQtGray1
                    }

                    Text {
                        text: (wDataCntr.weatherData
                               && wDataCntr.weatherData.main
                               && wDataCntr.weatherData.main.temp_min) ?
                                  qsTr("Min: ")
                                  + String(wDataCntr.weatherData.main.temp_min)
                                  + " °F" : "N/A"
                        font.pixelSize: UIStyle.fontSizeM
                        font.letterSpacing: 1
                        color: UIStyle.colorQtGray1
                    }

                    Text {
                        text: (wDataCntr.weatherData
                               && wDataCntr.weatherData.main
                               && wDataCntr.weatherData.main.temp_max) ?
                                  qsTr("Max: ")
                                  + String(wDataCntr.weatherData.main.temp_max)
                                  + " °F " : "N/A"
                        font.pixelSize: UIStyle.fontSizeM
                        font.letterSpacing: 1
                        color: UIStyle.colorQtGray1
                    }
                }
            }
        }

        Item {
            id: weatherPage2

            Column {
                spacing: 40
                anchors.centerIn: parent

                Row {
                    spacing: 20
                    anchors.horizontalCenter: parent.horizontalCenter

                    Image {
                        id: wImg
                        anchors.verticalCenter: parent.verticalCenter
                        source: "images/wind.png"
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: (wDataCntr.weatherData
                               && wDataCntr.weatherData.wind
                               && wDataCntr.weatherData.wind.speed) ?
                                  String(wDataCntr.weatherData.wind.speed)
                                  + " mph" : "N/A"
                        font.pixelSize: UIStyle.fontSizeM
                        font.letterSpacing: 1
                        color: UIStyle.colorQtGray1
                    }
                }

                Row {
                    spacing: 20
                    anchors.horizontalCenter: parent.horizontalCenter

                    Image {
                        id: hImg
                        anchors.verticalCenter: parent.verticalCenter
                        source: "images/humidity.png"
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: (wDataCntr.weatherData
                               && wDataCntr.weatherData.main
                               && wDataCntr.weatherData.main.humidity) ?
                                  String(wDataCntr.weatherData.main.humidity)
                                  + " %" : "N/A"
                        font.pixelSize: UIStyle.fontSizeM
                        font.letterSpacing: 1
                        color: UIStyle.colorQtGray1
                    }
                }
            }
        }

        Item {
            id: weatherPage3

            Row {
                anchors.centerIn: parent
                spacing: 10

                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    source: "images/pressure.png"
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 40

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: (wDataCntr.weatherData
                               && wDataCntr.weatherData.main
                               && wDataCntr.weatherData.main.pressure) ?
                                  String(wDataCntr.weatherData.main.pressure)
                                  + " hPa" : "N/A"
                        font.pixelSize: UIStyle.fontSizeM
                        font.letterSpacing: 1
                        color: UIStyle.colorQtGray1
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: (wDataCntr.weatherData
                               && wDataCntr.weatherData.main
                               && wDataCntr.weatherData.main.sea_level) ?
                                  String(wDataCntr.weatherData.main.sea_level)
                                  + " hPa" : "N/A"
                        font.pixelSize: UIStyle.fontSizeM
                        font.letterSpacing: 1
                        color: UIStyle.colorQtGray1
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: (wDataCntr.weatherData
                               && wDataCntr.weatherData.main
                               && wDataCntr.weatherData.main.grnd_level) ?
                                  String(wDataCntr.weatherData.main.grnd_level)
                                  + " hPa" : "N/A"
                        font.pixelSize: UIStyle.fontSizeM
                        font.letterSpacing: 1
                        color: UIStyle.colorQtGray1
                    }
                }
            }
        }

        Item {
            id: weatherPage4

            Column {
                spacing: 40
                anchors.centerIn: parent

                Row {
                    spacing: 30
                    anchors.horizontalCenter: parent.horizontalCenter

                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        source: "images/sunrise.png"
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: (wDataCntr.weatherData
                               && wDataCntr.weatherData.sys
                               && wDataCntr.weatherData.sys.sunrise) ?
                                  WeatherData.getTimeHMS(wDataCntr.weatherData.sys.sunrise)
                                : "N/A"
                        font.pixelSize: UIStyle.fontSizeM
                        font.letterSpacing: 1
                        color: UIStyle.colorQtGray1
                    }
                }

                Row {
                    spacing: 30
                    anchors.horizontalCenter: parent.horizontalCenter

                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        source: "images/sunset.png"
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: (wDataCntr.weatherData
                               && wDataCntr.weatherData.sys
                               && wDataCntr.weatherData.sys.sunset) ?
                                  WeatherData.getTimeHMS(wDataCntr.weatherData.sys.sunset)
                                : "N/A"
                        font.pixelSize: UIStyle.fontSizeM
                        font.letterSpacing: 1
                        color: UIStyle.colorQtGray1
                    }
                }
            }
        }
    }

    QtObject {
        id: wDataCntr
        property var weatherData
    }

    QQC2.PageIndicator {
        count: svWeatherContainer.count
        currentIndex: svWeatherContainer.currentIndex

        anchors.bottom: svWeatherContainer.bottom
        anchors.horizontalCenter: parent.horizontalCenter
    }
    Component.onCompleted: {
        WeatherData.requestWeatherData(wDataCntr)
    }
}
