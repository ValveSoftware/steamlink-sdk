/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

import QtQuick 2.0

Rectangle {
    id: gradientelement
    property real gradstop: 0.25
    gradient: Gradient {
         GradientStop { position: 0.0; color: "red" }
         GradientStop { position: gradstop; color: "yellow" }
         GradientStop { position: 1.0; color: "green" }
    }
    MouseArea { anchors.fill: parent; enabled: qmlfiletoload == ""; hoverEnabled: true
        onEntered: { helptext = "The gradient should show a trio of colors - red, yellow and green"+
        " - with a slow movement of the yellow up and down the view" }
        onExited: { helptext = "" }
    }
    // Animate the background gradient
    SequentialAnimation { id: gradanim; running: true; loops: Animation.Infinite
        NumberAnimation { target: gradientelement; property: "gradstop"; to: 0.88; duration: 10000; easing.type: Easing.InOutQuad }
        NumberAnimation { target: gradientelement; property: "gradstop"; to: 0.22; duration: 10000; easing.type: Easing.InOutQuad }
    }
}
