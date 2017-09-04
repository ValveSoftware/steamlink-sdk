/****************************************************************************
**
** Copyright (C) 2016 Research in Motion.
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

#include <qtest.h>
#include <QDebug>
#include <QQmlEngine>
#include <QQmlComponent>
#include "../../shared/util.h"

class tst_qtqmlmodules : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qtqmlmodules() {}

private slots:
    void baseTypes();
    void modelsTypes();
    void unavailableTypes();
};

void tst_qtqmlmodules::baseTypes()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("base.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);
    QVERIFY(object->property("success").toBool());

    delete object;
}

void tst_qtqmlmodules::modelsTypes()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("models.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);
    QVERIFY(object->property("success").toBool());

    delete object;
}

void tst_qtqmlmodules::unavailableTypes()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("unavailable.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);
    QVERIFY(object->property("success").toBool());

    delete object;
}

QTEST_MAIN(tst_qtqmlmodules)

#include "tst_qtqmlmodules.moc"
