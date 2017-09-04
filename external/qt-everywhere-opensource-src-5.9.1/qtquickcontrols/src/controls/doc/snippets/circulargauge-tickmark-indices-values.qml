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

//! [tickmarks]
import QtQuick 2.0
import QtQuick.Controls.Styles 1.4
import QtQuick.Extras 1.4

Rectangle {
    width: 400
    height: 400

    CircularGauge {
        id: gauge
        anchors.fill: parent
        style: CircularGaugeStyle {
            labelInset: outerRadius * 0.2

            tickmarkLabel: null

            tickmark: Text {
                text: styleData.value

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.bottom
                    text: styleData.index
                    color: "blue"
                }
            }

            minorTickmark: Text {
                text: styleData.value
                font.pixelSize: 8

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.bottom
                    text: styleData.index
                    font.pixelSize: 8
                    color: "blue"
                }
            }
        }

        Text {
            id: indexText
            text: "Major and minor indices"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: valueText.top
            color: "blue"
        }
        Text {
            id: valueText
            text: "Major and minor values"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
        }
    }
}
//! [tickmarks]
