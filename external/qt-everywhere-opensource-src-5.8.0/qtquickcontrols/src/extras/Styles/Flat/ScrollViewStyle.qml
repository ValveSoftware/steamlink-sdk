/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Extras module of the Qt Toolkit.
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
import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2 as Base
import QtQuick.Controls.Styles.Flat 1.0
import QtQuick.Controls.Private 1.0

Base.ScrollViewStyle {
    readonly property int barWidth: 2 * Math.round(FlatStyle.scaleFactor)
    readonly property int frameWidth: control.frameVisible ? Math.max(1, Math.round(FlatStyle.scaleFactor)) : 0
    padding {
        top: frameWidth
        left: frameWidth
        right: frameWidth
        bottom: frameWidth
    }
    corner: null
    transientScrollBars: true
    frame: Rectangle {
        color: control["backgroundVisible"] ? FlatStyle.backgroundColor : "transparent"
        border.color: FlatStyle.mediumFrameColor
        border.width: frameWidth
        visible: control.frameVisible
    }
    scrollBarBackground: Item {
        implicitWidth: 2 * barWidth
        implicitHeight: 2 * barWidth
    }
    handle: Item  {
        implicitWidth: barWidth
        implicitHeight: barWidth
        Rectangle {
            color: FlatStyle.darkFrameColor
            radius: barWidth / 2
            anchors.fill: parent
            anchors.topMargin: styleData.horizontal ? 0 : barWidth
            anchors.leftMargin: styleData.horizontal ? barWidth : 0
            anchors.rightMargin: styleData.horizontal ? barWidth : 0
            anchors.bottomMargin: styleData.horizontal ? 0 : barWidth
        }
    }
}
