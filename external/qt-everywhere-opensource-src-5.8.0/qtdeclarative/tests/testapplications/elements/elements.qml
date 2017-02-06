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
import "content/elements.js" as Elements
import "content"

Item {
    id: elementsapp; height: 640; width: 360

    property string qmlfiletoload: ""
    property string helptext: ""

    GradientElement { anchors.fill: parent }

    GridViewElement { height: parent.height * .95; width: parent.width * .95; anchors.centerIn: parent; }

    HelpDesk { width: parent.width; height: 200; anchors { bottom: parent.bottom; right: parent.right; bottomMargin: 3; rightMargin: 3 } }

    // Start or remove an .qml when the qmlfiletoload property changes
    onQmlfiletoloadChanged: {
        if (qmlfiletoload == "") {
            Elements.removeApp();
        } else {
            Elements.setapp(qmlfiletoload,elementsapp);
        }
    }

    // Set the qmlfiletoload property with a script function
    function runapp(qmlfile) {
        console.log("Starting ",qmlfile);
        qmlfiletoload = qmlfile;
    }
}
