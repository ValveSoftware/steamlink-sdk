/****************************************************************************
**
** Copyright (C) 2016 BlackBerry Ltd.
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

#include <QtTest/qtest.h>
#include <QtTest/qsignalspy.h>

#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickview.h>

#include <QtGui/qopenglcontext.h>
#include <QtGui/qsurfaceformat.h>

#include "../../shared/util.h"

class tst_QQuickOpenGLInfo : public QQmlDataTest
{
    Q_OBJECT

private slots:
    void testProperties();
};

void tst_QQuickOpenGLInfo::testProperties()
{
    QQuickView view;
    view.setSource(testFileUrl("basic.qml"));

    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QSignalSpy spy(&view, SIGNAL(sceneGraphInitialized()));
    spy.wait();

    QVERIFY(view.openglContext());
    QSurfaceFormat format = view.openglContext()->format();

    QObject* obj = view.rootObject();
    QVERIFY(obj);
    QCOMPARE(obj->property("majorVersion").toInt(), format.majorVersion());
    QCOMPARE(obj->property("minorVersion").toInt(), format.minorVersion());
    QCOMPARE(obj->property("profile").toInt(), static_cast<int>(format.profile()));
    QCOMPARE(obj->property("renderableType").toInt(), static_cast<int>(format.renderableType()));
}

QTEST_MAIN(tst_QQuickOpenGLInfo)

#include "tst_qquickopenglinfo.moc"
