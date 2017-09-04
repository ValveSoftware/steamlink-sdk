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
import Qt.test 1.0

CppObject {
    id: obj

    property Settings settings: Settings {
        property alias intProperty: obj.intProperty
        property alias boolProperty: obj.boolProperty
        property alias realProperty: obj.realProperty
        property alias doubleProperty: obj.doubleProperty
        property alias stringProperty: obj.stringProperty
        property alias urlProperty: obj.urlProperty
        property alias objectProperty: obj.objectProperty
        property alias intListProperty: obj.intListProperty
        property alias stringListProperty: obj.stringListProperty
        property alias objectListProperty: obj.objectListProperty
        property alias dateProperty: obj.dateProperty
        // QTBUG-32295: Expected property type
        //property alias timeProperty: obj.timeProperty
        property alias sizeProperty: obj.sizeProperty
        property alias pointProperty: obj.pointProperty
        property alias rectProperty: obj.rectProperty
        property alias colorProperty: obj.colorProperty
        property alias fontProperty: obj.fontProperty
    }
}
