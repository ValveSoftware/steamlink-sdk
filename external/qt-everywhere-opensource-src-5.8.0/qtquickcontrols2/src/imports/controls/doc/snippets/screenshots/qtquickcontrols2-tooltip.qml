/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:FDL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Free Documentation License Usage
** Alternatively, this file may be used under the terms of the GNU Free
** Documentation License version 1.3 as published by the Free Software
** Foundation and appearing in the file included in the packaging of
** this file. Please review the following information to ensure
** the GNU Free Documentation License version 1.3 requirements
** will be met: http://www.gnu.org/copyleft/fdl.html.
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtQuick.Window 2.2
import QtQuick.Controls 2.1

Item {
    id: root
    width: 360
    height: button.height * 2

    property Button button: children[0]

    Binding { target: button; property: "down"; value: root.Window.active }
    Binding { target: button.anchors; property: "bottom"; value: root.bottom }
    Binding { target: button.anchors; property: "horizontalCenter"; value: root.horizontalCenter }

    //! [1]
    Button {
        text: qsTr("Save")

        ToolTip.visible: down
        ToolTip.text: qsTr("Save the active project")
    }
    //! [1]
}
