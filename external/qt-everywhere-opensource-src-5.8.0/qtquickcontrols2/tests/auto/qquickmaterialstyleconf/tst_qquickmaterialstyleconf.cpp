/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qtest.h>
#include <QtQuick/private/qquickitem_p.h>
#include "../shared/util.h"
#include "../shared/visualtestutil.h"

using namespace QQuickVisualTestUtil;

class tst_qquickmaterialstyleconf : public QQmlDataTest
{
    Q_OBJECT

public:

private slots:
    void conf();
};

void tst_qquickmaterialstyleconf::conf()
{
    QQuickApplicationHelper helper(this, QLatin1String("applicationwindow.qml"));

    QQuickApplicationWindow *window = helper.appWindow;
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    // We specified a custom background color, so the window should have it.
    QCOMPARE(window->property("color").value<QColor>(), QColor("#444444"));

    // We specified a custom foreground color, so the label should have it.
    QQuickItem *label = window->property("label").value<QQuickItem*>();
    QVERIFY(label);
    QCOMPARE(label->property("color").value<QColor>(), QColor("#F44336"));
}

QTEST_MAIN(tst_qquickmaterialstyleconf)

#include "tst_qquickmaterialstyleconf.moc"
