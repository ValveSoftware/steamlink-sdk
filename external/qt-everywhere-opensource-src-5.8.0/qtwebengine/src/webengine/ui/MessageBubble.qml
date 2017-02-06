/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.5

Item {
    id: bubble

    width: 1
    height: 1

    property int maxWidth: 0
    property string mainText: ""
    property string subText: ""

    property int border: 1

    property int arrowWidth: 18
    property int arrowHeight: 18
    property int arrowOffset: 18

    property int marginLeft: border + 8
    property int marginTop: border + arrowHeight + 6
    property int marginRight: border + 8
    property int marginBottom: border + 6

    Column {
        id: messageColumn

        x: bubble.marginLeft
        y: bubble.marginTop
        z: 1

        spacing: 5

        Text {
            id: message
            width: bubble.maxWidth

            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            clip: true

            font.pointSize: subMessage.font.pointSize + 4

            text: bubble.mainText
        }

        Text {
            id: subMessage
            width: bubble.maxWidth

            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            clip: true

            text: bubble.subText
        }
    }

    Canvas {
        id: bubbleCanvas

        property int textWidth: Math.min(bubble.maxWidth, Math.max(message.paintedWidth, subMessage.paintedWidth))
        property int textHeight: message.paintedHeight + (subMessage.paintedWidth > 0 ? (messageColumn.spacing + subMessage.paintedHeight) : 0)

        width: textWidth + bubble.marginLeft + bubble.marginRight
        height: textHeight + bubble.marginTop + bubble.marginBottom

        property int cornerRadius: 7

        property int messageBoxLeft: 0
        property int messageBoxTop: bubble.arrowHeight
        property int messageBoxRight: width - border
        property int messageBoxBottom: height - border

        onPaint: {
            var ctx = getContext("2d");

            ctx.lineWidth = bubble.border;
            ctx.strokeStyle = "#555";
            ctx.fillStyle = "#ffffe1";

            ctx.beginPath();

            ctx.moveTo(messageBoxLeft + cornerRadius, messageBoxTop);

            // Arrow
            ctx.lineTo(messageBoxLeft + bubble.arrowOffset, messageBoxTop);
            ctx.lineTo(messageBoxLeft + bubble.arrowOffset, messageBoxTop - bubble.arrowHeight);
            ctx.lineTo(messageBoxLeft + bubble.arrowOffset + bubble.arrowWidth, messageBoxTop);

            // Message Box
            ctx.lineTo(messageBoxRight - cornerRadius, messageBoxTop);
            ctx.quadraticCurveTo(messageBoxRight, messageBoxTop, messageBoxRight, messageBoxTop + cornerRadius);
            ctx.lineTo(messageBoxRight, messageBoxBottom - cornerRadius);
            ctx.quadraticCurveTo(messageBoxRight, messageBoxBottom, messageBoxRight - cornerRadius, messageBoxBottom);
            ctx.lineTo(messageBoxLeft + cornerRadius, messageBoxBottom);
            ctx.quadraticCurveTo(messageBoxLeft, messageBoxBottom, messageBoxLeft, messageBoxBottom - cornerRadius);
            ctx.lineTo(messageBoxLeft, messageBoxTop + cornerRadius);
            ctx.quadraticCurveTo(messageBoxLeft, messageBoxTop, messageBoxLeft + cornerRadius, messageBoxTop);

            ctx.closePath();

            ctx.fill();
            ctx.stroke();
        }

        onPainted: {
            bubble.width = bubbleCanvas.width;
            bubble.height = bubbleCanvas.height;
        }
    }
}
