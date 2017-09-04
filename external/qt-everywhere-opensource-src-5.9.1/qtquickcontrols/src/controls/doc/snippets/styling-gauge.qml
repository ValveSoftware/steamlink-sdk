/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:FDL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Free Documentation License Usage
** Alternatively, this file may be used under the terms of the GNU Free
** Documentation License version 1.3 as published by the Free Software
** Foundation and appearing in the file included in the packaging of
** this file. Please review the following information to ensure
** the GNU Free Documentation License version 1.3 requirements
** will be met: https://www.gnu.org/licenses/fdl-1.3.html.
** $QT_END_LICENSE$
**
****************************************************************************/

//! [all]
import QtQuick 2.2
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Extras 1.4

Rectangle {
    width: 100
    height: 220
    color: "#494d53"

    Gauge {
        value: 50
        tickmarkStepSize: 20
        minorTickmarkCount: 1
        //! [font-size]
        font.pixelSize: 15
        //! [font-size]
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: -4

        style: GaugeStyle {
            //! [valueBar]
            valueBar: Rectangle {
                color: "#e34c22"
                implicitWidth: 28
            }
            //! [valueBar]

            //! [foreground]
            foreground: null
            //! [foreground]

            //! [tickmark]
            tickmark: Item {
                implicitWidth: 8
                implicitHeight: 4

                Rectangle {
                    x: control.tickmarkAlignment === Qt.AlignLeft
                        || control.tickmarkAlignment === Qt.AlignTop ? parent.implicitWidth : -28
                    width: 28
                    height: parent.height
                    color: "#ffffff"
                }
            }
            //! [tickmark]

            //! [minorTickmark]
            minorTickmark: Item {
                implicitWidth: 8
                implicitHeight: 2

                Rectangle {
                    x: control.tickmarkAlignment === Qt.AlignLeft
                        || control.tickmarkAlignment === Qt.AlignTop ? parent.implicitWidth : -28
                    width: 28
                    height: parent.height
                    color: "#ffffff"
                }
            }
            //! [minorTickmark]
        }
    }
}
//! [all]
