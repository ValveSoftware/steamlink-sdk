/****************************************************************************
**
** Copyright (C) 2017 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
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
