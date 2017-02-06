/***************************************************************************
**
** Copyright (C) 2016 - 2012 Research In Motion
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include <QtQuick/QQuickWindow>
#include <QtQuick/private/qquickrectangle_p.h>
#include <QtQuick/private/qquickwindow_p.h>

#include <qtest.h>
#include <QtTest/QtTest>

class tst_qquickwindow : public QObject
{
    Q_OBJECT
public:
    tst_qquickwindow();

private slots:
    void tst_updateCursor();
    void cleanupTestCase();
private:
    QQuickWindow* window;
};

tst_qquickwindow::tst_qquickwindow()
{
    window = new QQuickWindow;
    window->resize(250, 250);
    window->setPosition(100, 100);
    for ( int i=0; i<8000; i++ ) {
        QQuickRectangle *r =new QQuickRectangle(window->contentItem());
        for ( int j=0; j<10; ++j ) {
            new QQuickRectangle(r);
        }
    }
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
}

void tst_qquickwindow::cleanupTestCase()
{
    delete window;
}

void tst_qquickwindow::tst_updateCursor()
{
    QBENCHMARK {
        QQuickWindowPrivate::get(window)->updateCursor(QPoint(100,100));
    }
}

QTEST_MAIN(tst_qquickwindow);

#include "tst_qquickwindow.moc"
