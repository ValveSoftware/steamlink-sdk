/****************************************************************************
**
** Copyright (C) 2015 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
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

import QtGraphicalEffects.private 1.0
import QtQuick 2.4;

Item {
    id: root

    width: 400
    height: 600

    // Text {
    //     text: "source: layered, padded\nproxy: expect-no-padding"
    //     layer.enabled: true
    //     layer.sourceRect: Qt.rect(-1, -1, width, height);
    //     font.pixelSize: 14
    //     font.bold: true
    // }

    Flickable {

        anchors.fill: parent

        contentWidth: root.width
        contentHeight: column.height

        Column {
            id: column
            width: root.width

            SourceProxyTest {
                label: "source: layered, padded\nproxy: expect-no-padding"
                sourcing: "layered"
                proxyPadding: false
                padding: true
                expectProxy: true
            }
            SourceProxyTest {
                label: "source: layered, non-padded\nproxy: expect-no-padding"
                sourcing: "layered"
                proxyPadding: false
                padding: false
                expectProxy: false
            }
            SourceProxyTest {
                label: "source: layered, padded\nproxy: expect-padding"
                sourcing: "layered"
                proxyPadding: true
                padding: true
                expectProxy: false
            }
            SourceProxyTest {
                label: "source: layered, non-padded\nproxy: expect-padding"
                sourcing: "layered"
                proxyPadding: true
                padding: false
                expectProxy: true
            }


            SourceProxyTest {
                label: "source: shadersource, padded\nproxy: expect-no-padding"
                sourcing: "shadersource"
                proxyPadding: false
                padding: true
                expectProxy: true
            }
            SourceProxyTest {
                label: "source: shadersource, non-padded\nproxy: expect-no-padding"
                sourcing: "shadersource"
                proxyPadding: false
                padding: false
                expectProxy: false
            }
            SourceProxyTest {
                label: "source: shadersource, padded\nproxy: expect-padding"
                sourcing: "shadersource"
                proxyPadding: true
                padding: true
                expectProxy: false
            }
            SourceProxyTest {
                label: "source: shadersource, non-padded\nproxy: expect-padding"
                sourcing: "shadersource"
                proxyPadding: true
                padding: false
                expectProxy: true
            }


            SourceProxyTest {
                label: "source: layered, non-smooth\nproxy: any-interpolation, "
                sourcing: "layered"
                smoothness: false
                proxyInterpolation: SourceProxy.AnyInterpolation
                expectProxy: false
            }
            SourceProxyTest {
                label: "source: layered, smooth\nproxy: any-interpolation, "
                sourcing: "layered"
                smoothness: true
                proxyInterpolation: SourceProxy.AnyInterpolation
                expectProxy: false
            }
            SourceProxyTest {
                label: "source: layered, non-smooth\nproxy: nearest-interpolation, "
                sourcing: "layered"
                smoothness: false
                proxyInterpolation: SourceProxy.NearestInterpolation
                expectProxy: false
            }
            SourceProxyTest {
                label: "source: layered, smooth\nproxy: nearest-interpolation, "
                sourcing: "layered"
                smoothness: true
                proxyInterpolation: SourceProxy.NearestInterpolation
                expectProxy: true
            }

            SourceProxyTest {
                label: "source: layered, non-smooth\nproxy: linear-interpolation, "
                sourcing: "layered"
                smoothness: false
                proxyInterpolation: SourceProxy.LinearInterpolation
                expectProxy: true
            }
            SourceProxyTest {
                label: "source: layered, smooth\nproxy: linear-interpolation, "
                sourcing: "layered"
                smoothness: true
                proxyInterpolation: SourceProxy.LinearInterpolation
                expectProxy: false
            }



            SourceProxyTest {
                label: "source: shadersource, non-smooth\nproxy: any-interpolation, "
                sourcing: "shadersource"
                smoothness: false
                proxyInterpolation: SourceProxy.AnyInterpolation
                expectProxy: false
            }
            SourceProxyTest {
                label: "source: shadersource, smooth\nproxy: any-interpolation, "
                sourcing: "shadersource"
                smoothness: true
                proxyInterpolation: SourceProxy.AnyInterpolation
                expectProxy: false
            }

            SourceProxyTest {
                label: "source: shadersource, non-smooth\nproxy: nearest-interpolation, "
                sourcing: "shadersource"
                smoothness: false
                proxyInterpolation: SourceProxy.NearestInterpolation
                expectProxy: false
            }
            SourceProxyTest {
                label: "source: shadersource, smooth\nproxy: nearest-interpolation, "
                sourcing: "shadersource"
                smoothness: true
                proxyInterpolation: SourceProxy.NearestInterpolation
                expectProxy: true
            }

            SourceProxyTest {
                label: "source: shadersource, non-smooth\nproxy: linear-interpolation, "
                sourcing: "shadersource"
                smoothness: false
                proxyInterpolation: SourceProxy.LinearInterpolation
                expectProxy: true
            }
            SourceProxyTest {
                label: "source: shadersource, smooth\nproxy: linear-interpolation, "
                sourcing: "shadersource"
                smoothness: true
                proxyInterpolation: SourceProxy.LinearInterpolation
                expectProxy: false
            }



            SourceProxyTest {
                label: "source: none\nproxy: any-interpolation"
                sourcing: "none"
                proxyInterpolation: SourceProxy.AnyInterpolation
                expectProxy: false
            }
            SourceProxyTest {
                label: "source: none\nproxy: nearest-interpolation"
                sourcing: "none"
                proxyInterpolation: SourceProxy.NearestInterpolation
                expectProxy: false
            }
            SourceProxyTest {
                label: "source: none\nproxy: linear-interpolation"
                sourcing: "none"
                proxyInterpolation: SourceProxy.LinearInterpolation
                expectProxy: false
            }


        }

    }
}
