/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
        property alias intListProperty: obj.intListProperty
        property alias stringListProperty: obj.stringListProperty
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
