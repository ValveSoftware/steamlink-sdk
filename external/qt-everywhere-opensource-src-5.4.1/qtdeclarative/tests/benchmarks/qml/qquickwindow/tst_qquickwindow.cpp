/***************************************************************************
**
** Copyright (C) 2011 - 2012 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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
