/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

import QtQuick 2.9
import QtQuick.Controls 2.2

//! [1]
StackView {
    id: stackView
    property real offset: 10
    width: 100; height: 100

    initialItem: Component {
        id: page
        Rectangle {
            property real pos: StackView.index * stackView.offset
            property real hue: Math.random()
            color: Qt.hsla(hue, 0.5, 0.8, 0.6)
            border.color: Qt.hsla(hue, 0.5, 0.5, 0.9)
            StackView.visible: true
        }
    }

    pushEnter: Transition {
        id: pushEnter
        ParallelAnimation {
            PropertyAction { property: "x"; value: pushEnter.ViewTransition.item.pos }
            NumberAnimation { properties: "y"; from: pushEnter.ViewTransition.item.pos + stackView.offset; to: pushEnter.ViewTransition.item.pos; duration: 400; easing.type: Easing.OutCubic }
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 400; easing.type: Easing.OutCubic }
        }
    }
    popExit: Transition {
        id: popExit
        ParallelAnimation {
            PropertyAction { property: "x"; value: popExit.ViewTransition.item.pos }
            NumberAnimation { properties: "y"; from: popExit.ViewTransition.item.pos; to: popExit.ViewTransition.item.pos + stackView.offset; duration: 400; easing.type: Easing.OutCubic }
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 400; easing.type: Easing.OutCubic }
        }
    }

    pushExit: Transition {
        id: pushExit
        PropertyAction { property: "x"; value: pushExit.ViewTransition.item.pos }
        PropertyAction { property: "y"; value: pushExit.ViewTransition.item.pos }
    }
    popEnter: Transition {
        id: popEnter
        PropertyAction { property: "x"; value: popEnter.ViewTransition.item.pos }
        PropertyAction { property: "y"; value: popEnter.ViewTransition.item.pos }
    }
}
//! [1]
