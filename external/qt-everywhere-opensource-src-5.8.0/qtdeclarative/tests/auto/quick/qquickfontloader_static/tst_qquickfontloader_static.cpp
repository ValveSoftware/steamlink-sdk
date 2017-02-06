/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt-project.org/legal
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

#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickView>
#include <QtQml/QQmlComponent>
#include "../../shared/util.h"
#include <qtest.h>

QByteArray qmltemplate("import QtQuick 2.0\n"
"\n"
"Rectangle {\n"
"    width: 400\n"
"    height: 400\n"
"    color: \"red\"\n"
"    FontLoader { id: fixedFont; source: \"%1\" }\n"
"    Text {\n"
"        text: \"hello world\"\n"
"        anchors.centerIn: parent\n"
"        font.family: fixedFont.name\n"
"    }\n"
"}\n");

int main(int argc, char **argv)
{
    for (int i = 0; i < 3; i++) {
        QGuiApplication app(argc, argv);
        QQmlDataTest dataTest;
        dataTest.initTestCase();
        QQuickView window;
        QQmlComponent component (window.engine());
        QUrl current = QUrl::fromLocalFile("");
        qmltemplate.replace("%1", dataTest.testFileUrl("font.ttf").toString().toLocal8Bit());
        component.setData(qmltemplate, current);
        window.setContent(current, &component, component.create());
        window.show();
        QTest::qWaitForWindowActive(&window);
    }
}
