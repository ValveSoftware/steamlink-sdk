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
import QtQml 2.1
import QtQuick 2.2
import Qt.labs.settings 1.0

QtObject {
    id: root

    property int intProperty: 123
    property bool boolProperty: true
    property real realProperty: 1.23
    property double doubleProperty: 3.45
    property string stringProperty: "foo"
    property url urlProperty: "http://www.qt-project.org"
    property var objectProperty: {"foo":"bar"}
    property var intListProperty: [1, 2, 3]
    property var stringListProperty: ["a", "b", "c"]
    property var objectListProperty: [{"a":"b"}, {"c":"d"}]
    property date dateProperty: "2000-01-02"
    // QTBUG-32295: Expected property type
    //property time timeProperty: "12:34:56"
    property size sizeProperty: Qt.size(12, 34)
    property point pointProperty: Qt.point(12, 34)
    property rect rectProperty: Qt.rect(1, 2, 3, 4)
    property color colorProperty: "red"
    property font fontProperty

    property Settings settings: Settings {
        id: settings

        property alias intProperty: root.intProperty
        property alias boolProperty: root.boolProperty
        property alias realProperty: root.realProperty
        property alias doubleProperty: root.doubleProperty
        property alias stringProperty: root.stringProperty
        property alias urlProperty: root.urlProperty
        property alias objectProperty: root.objectProperty
        property alias intListProperty: root.intListProperty
        property alias stringListProperty: root.stringListProperty
        property alias objectListProperty: root.objectListProperty
        property alias dateProperty: root.dateProperty
        // QTBUG-32295: Expected property type
        //property alias timeProperty: root.timeProperty
        property alias sizeProperty: root.sizeProperty
        property alias pointProperty: root.pointProperty
        property alias rectProperty: root.rectProperty
        property alias colorProperty: root.colorProperty
        property alias fontProperty: root.fontProperty
    }
}
