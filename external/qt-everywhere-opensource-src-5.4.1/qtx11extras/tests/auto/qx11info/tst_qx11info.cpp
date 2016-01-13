/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2012 David Faure <david.faure@kdab.com>
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


#include <QtTest/QtTest>

#include <QApplication>
#include <QX11Info>

class tst_QX11Info : public QObject
{
    Q_OBJECT

private slots:
    void staticFunctionsBeforeQApplication();
    void startupId();
    void isPlatformX11();
    void appTime();
};

void tst_QX11Info::staticFunctionsBeforeQApplication()
{
    QVERIFY(!QApplication::instance());

    // none of these functions should crash if QApplication hasn't
    // been constructed

    Display *display = QX11Info::display();
    QCOMPARE(display, (Display *)0);

#if 0
    const char *appClass = QX11Info::appClass();
    QCOMPARE(appClass, (const char *)0);
#endif
    int appScreen = QX11Info::appScreen();
    QCOMPARE(appScreen, 0);
#if 0
    int appDepth = QX11Info::appDepth();
    QCOMPARE(appDepth, 32);
    int appCells = QX11Info::appCells();
    QCOMPARE(appCells, 0);
    Qt::HANDLE appColormap = QX11Info::appColormap();
    QCOMPARE(appColormap, static_cast<Qt::HANDLE>(0));
    void *appVisual = QX11Info::appVisual();
    QCOMPARE(appVisual, static_cast<void *>(0));
#endif
    unsigned long appRootWindow = QX11Info::appRootWindow();
    QCOMPARE(appRootWindow, static_cast<unsigned long>(0));

#if 0
    bool appDefaultColormap = QX11Info::appDefaultColormap();
    QCOMPARE(appDefaultColormap, true);
    bool appDefaultVisual = QX11Info::appDefaultVisual();
    QCOMPARE(appDefaultVisual, true);
#endif

    int appDpiX = QX11Info::appDpiX();
    int appDpiY = QX11Info::appDpiY();
    QCOMPARE(appDpiX, 75);
    QCOMPARE(appDpiY, 75);

    unsigned long appTime = QX11Info::appTime();
    unsigned long appUserTime = QX11Info::appUserTime();
    QCOMPARE(appTime, 0ul);
    QCOMPARE(appUserTime, 0ul);
    // setApp*Time do nothing without QApplication
    QX11Info::setAppTime(1234);
    QX11Info::setAppUserTime(5678);
    appTime = QX11Info::appTime();
    appUserTime = QX11Info::appUserTime();
    QCOMPARE(appTime, 0ul);
    QCOMPARE(appTime, 0ul);
}

static const char idFromEnv[] = "startupid_TIME123456";
void initialize()
{
    qputenv("DESKTOP_STARTUP_ID", idFromEnv);
}
Q_CONSTRUCTOR_FUNCTION(initialize)

void tst_QX11Info::startupId()
{
    int argc = 0;
    QApplication app(argc, 0);

    // This relies on the fact that no widget was shown yet,
    // so please make sure this method is always the first test.
    QCOMPARE(QString(QX11Info::nextStartupId()), QString(idFromEnv));
    QWidget w;
    w.show();
    QVERIFY(QX11Info::nextStartupId().isEmpty());

    QByteArray idSecondWindow = "startupid2_TIME234567";
    QX11Info::setNextStartupId(idSecondWindow);
    QCOMPARE(QX11Info::nextStartupId(), idSecondWindow);

    QWidget w2;
    w2.show();
    QVERIFY(QX11Info::nextStartupId().isEmpty());
}

void tst_QX11Info::isPlatformX11()
{
    int argc = 0;
    QApplication app(argc, 0);

    QVERIFY(QX11Info::isPlatformX11());
}

void tst_QX11Info::appTime()
{
    int argc = 0;
    QApplication app(argc, 0);

    // No X11 event received yet
    QCOMPARE(QX11Info::appTime(), 0ul);
    QCOMPARE(QX11Info::appUserTime(), 0ul);

    // Trigger some X11 events
    QWindow window;
    window.show();
    QTest::qWait(100);
    QVERIFY(QX11Info::appTime() > 0);

    unsigned long t0 = QX11Info::appTime();
    unsigned long t1 = t0 + 1;
    QX11Info::setAppTime(t1);
    QCOMPARE(QX11Info::appTime(), t1);

    unsigned long u0 = QX11Info::appUserTime();
    unsigned long u1 = u0 + 1;
    QX11Info::setAppUserTime(u1);
    QCOMPARE(QX11Info::appUserTime(), u1);
}

QTEST_APPLESS_MAIN(tst_QX11Info)

#include "tst_qx11info.moc"
