/****************************************************************************
**
** Copyright (C) 2014 BlackBerry Ltd.
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
    view.setSource(QUrl::fromLocalFile("data/basic.qml"));

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
