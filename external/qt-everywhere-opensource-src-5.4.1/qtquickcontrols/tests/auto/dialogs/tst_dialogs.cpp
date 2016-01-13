/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#include <qtest.h>
#include "../shared/util.h"
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QSignalSpy>

class tst_dialogs : public QQmlDataTest
{
    Q_OBJECT
public:

private slots:
    void initTestCase()
    {
        QQmlDataTest::initTestCase();
    }

    // FileDialog
    void fileDialogDefaultModality();
    void fileDialogNonModal();
    void fileDialogNameFilters();

private:
};

void tst_dialogs::fileDialogDefaultModality()
{
    QQuickView *window = new QQuickView;
    QScopedPointer<QQuickWindow> cleanup(window);

    window->setSource(testFileUrl("RectWithFileDialog.qml"));
    window->setGeometry(240,240,1024,320);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(window->rootObject());

    // Click to show
    QObject *dlg = qvariant_cast<QObject *>(window->rootObject()->property("fileDialog"));
    QSignalSpy spyVisibilityChanged(dlg, SIGNAL(visibilityChanged()));
    QTest::mouseClick(window, Qt::LeftButton, 0, QPoint(1000, 100));  // show
    QTRY_VERIFY(spyVisibilityChanged.count() > 0);
    int visibilityChangedCount = spyVisibilityChanged.count();
    // Can't hide by clicking the main window, because dialog is modal.
    QTest::mouseClick(window, Qt::LeftButton, 0, QPoint(1000, 100));
    /*
        On OS X, if you send an event directly to a window, the modal dialog
        doesn't block the event, so the window will process it normally. This
        is a different code path compared to having a user click the mouse and
        generate a native event; in that case the OS does the filtering itself,
        and Qt will not even see the event. But simulating real events in the
        test framework is generally unstable. So there isn't a good way to test
        modality on OS X.
        This test sometimes fails on other platforms too.  Maybe it's not reliable
        to try to click the main window in a location which is outside the
        dialog, without checking or guaranteeing it somehow.
    */
    QSKIP("Modality test is flaky in general and doesn't work at all on OS X");
    // So we expect no change in visibility.
    QCOMPARE(spyVisibilityChanged.count(), visibilityChangedCount);

    QCOMPARE(dlg->property("visible").toBool(), true);
    QMetaObject::invokeMethod(dlg, "close");
    QTRY_VERIFY(spyVisibilityChanged.count() > visibilityChangedCount);
    visibilityChangedCount = spyVisibilityChanged.count();
    QCOMPARE(dlg->property("visible").toBool(), false);
    QMetaObject::invokeMethod(dlg, "open");
    QTRY_VERIFY(spyVisibilityChanged.count() > visibilityChangedCount);
    QCOMPARE(dlg->property("visible").toBool(), true);
    QCOMPARE(dlg->property("modality").toInt(), (int)Qt::WindowModal);
}

void tst_dialogs::fileDialogNonModal()
{
    QQuickView *window = new QQuickView;
    QScopedPointer<QQuickWindow> cleanup(window);

    window->setSource(testFileUrl("RectWithFileDialog.qml"));
    window->setGeometry(240,240,1024,320);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(window->rootObject());

    // Click to toggle visibility
    QObject *dlg = qvariant_cast<QObject *>(window->rootObject()->property("fileDialog"));
    dlg->setProperty("modality", QVariant((int)Qt::NonModal));
    QSignalSpy spyVisibilityChanged(dlg, SIGNAL(visibilityChanged()));
    QTest::mouseClick(window, Qt::LeftButton, 0, QPoint(1000, 100));  // show
    QTRY_VERIFY(spyVisibilityChanged.count() > 0);
    int visibilityChangedCount = spyVisibilityChanged.count();
    QCOMPARE(dlg->property("visible").toBool(), true);
    QTest::mouseClick(window, Qt::LeftButton, 0, QPoint(1000, 100));  // hide
    QTRY_VERIFY(spyVisibilityChanged.count() > visibilityChangedCount);
    QCOMPARE(dlg->property("visible").toBool(), false);
#ifdef Q_OS_WIN
    QCOMPARE(dlg->property("modality").toInt(), (int)Qt::ApplicationModal);
#else
    QCOMPARE(dlg->property("modality").toInt(), (int)Qt::NonModal);
#endif
}

void tst_dialogs::fileDialogNameFilters()
{
    QQuickView *window = new QQuickView;
    QScopedPointer<QQuickWindow> cleanup(window);

    window->setSource(testFileUrl("RectWithFileDialog.qml"));
    window->setGeometry(240,240,1024,320);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(window->rootObject());

    QObject *dlg = qvariant_cast<QObject *>(window->rootObject()->property("fileDialog"));
    QStringList filters;
    filters << "QML files (*.qml)";
    filters << "Image files (*.jpg, *.png, *.gif)";
    filters << "All files (*)";
    dlg->setProperty("nameFilters", QVariant(filters));
    QCOMPARE(dlg->property("selectedNameFilter").toString(), filters.first());
}

QTEST_MAIN(tst_dialogs)

#include "tst_dialogs.moc"
