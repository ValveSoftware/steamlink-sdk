/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include <QtTest/qtest.h>
#include "../shared/visualtestutil.h"

#include <QtQuick/qquickview.h>
#include <QtQuickTemplates2/private/qquickapplicationwindow_p.h>
#include <QtQuickTemplates2/private/qquickcontrol_p.h>
#include <QtQuickTemplates2/private/qquickpageindicator_p.h>
#include <QtQuickTemplates2/private/qquickscrollbar_p.h>
#include <QtQuickTemplates2/private/qquicktextarea_p.h>

using namespace QQuickVisualTestUtil;

class tst_cursor : public QQmlDataTest
{
    Q_OBJECT

private slots:
    void controls_data();
    void controls();
    void editable();
    void pageIndicator();
    void scrollBar();
};

void tst_cursor::controls_data()
{
    QTest::addColumn<QString>("testFile");

    QTest::newRow("buttons") << "buttons.qml";
    QTest::newRow("containers") << "containers.qml";
    QTest::newRow("sliders") << "sliders.qml";
}

void tst_cursor::controls()
{
    QFETCH(QString, testFile);

    QQuickView view(testFileUrl(testFile));
    view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));

    QQuickItem *mouseArea = view.rootObject();
    QVERIFY(mouseArea);
    QCOMPARE(mouseArea->cursor().shape(), Qt::ForbiddenCursor);

    QQuickItem *column = mouseArea->childItems().value(0);
    QVERIFY(column);

    const auto controls = column->childItems();
    for (QQuickItem *control : controls) {
        QCOMPARE(control->cursor().shape(), Qt::ArrowCursor);

        QTest::mouseMove(&view, control->mapToScene(QPointF(-1, -1)).toPoint());
        QCOMPARE(view.cursor().shape(), Qt::ForbiddenCursor);

        QTest::mouseMove(&view, control->mapToScene(QPointF(0, 0)).toPoint());
        QCOMPARE(view.cursor().shape(), Qt::ArrowCursor);

        QTest::mouseMove(&view, control->mapToScene(QPointF(control->width() + 1, control->height() + 1)).toPoint());
        QCOMPARE(view.cursor().shape(), Qt::ForbiddenCursor);
    }
}

void tst_cursor::editable()
{
    QQuickView view(testFileUrl("editable.qml"));
    view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));

    QQuickItem *mouseArea = view.rootObject();
    QVERIFY(mouseArea);
    QCOMPARE(mouseArea->cursor().shape(), Qt::ForbiddenCursor);

    QQuickItem *column = mouseArea->childItems().value(0);
    QVERIFY(column);

    const auto children = column->childItems();
    for (QQuickItem *child : children) {
        QQuickControl *control = qobject_cast<QQuickControl *>(child);
        QVERIFY(control);
        QCOMPARE(control->cursor().shape(), Qt::ArrowCursor);
        QCOMPARE(control->contentItem()->cursor().shape(), Qt::IBeamCursor);

        QTest::mouseMove(&view, control->mapToScene(QPointF(-1, -1)).toPoint());
        QCOMPARE(view.cursor().shape(), Qt::ForbiddenCursor);

        QTest::mouseMove(&view, control->mapToScene(QPointF(control->width() / 2, control->height() / 2)).toPoint());
        QCOMPARE(view.cursor().shape(), Qt::IBeamCursor);

        control->setProperty("editable", false);
        QCOMPARE(control->cursor().shape(), Qt::ArrowCursor);
        QCOMPARE(control->contentItem()->cursor().shape(), Qt::ArrowCursor);
        QCOMPARE(view.cursor().shape(), Qt::ArrowCursor);

        QTest::mouseMove(&view, control->mapToScene(QPointF(control->width() + 1, control->height() + 1)).toPoint());
        QCOMPARE(view.cursor().shape(), Qt::ForbiddenCursor);
    }
}

void tst_cursor::pageIndicator()
{
    QQuickView view(testFileUrl("pageindicator.qml"));
    view.show();
    QVERIFY(QTest::qWaitForWindowActive(&view));

    QQuickItem *mouseArea = view.rootObject();
    QVERIFY(mouseArea);
    QCOMPARE(mouseArea->cursor().shape(), Qt::ForbiddenCursor);

    QQuickPageIndicator *indicator = qobject_cast<QQuickPageIndicator *>(mouseArea->childItems().value(0));
    QVERIFY(indicator);

    QTest::mouseMove(&view, indicator->mapToScene(QPointF(-1, -1)).toPoint());
    QCOMPARE(view.cursor().shape(), Qt::ForbiddenCursor);

    QTest::mouseMove(&view, indicator->mapToScene(QPointF(0, 0)).toPoint());
    QCOMPARE(view.cursor().shape(), Qt::ForbiddenCursor);

    indicator->setInteractive(true);
    QCOMPARE(view.cursor().shape(), Qt::ArrowCursor);

    QTest::mouseMove(&view, indicator->mapToScene(QPointF(indicator->width() + 1, indicator->height() + 1)).toPoint());
    QCOMPARE(view.cursor().shape(), Qt::ForbiddenCursor);
}

// QTBUG-59629
void tst_cursor::scrollBar()
{
    // Ensure that the mouse cursor has the correct shape when over a scrollbar
    // which is itself over a text area with IBeamCursor.
    QQuickApplicationHelper helper(this, QStringLiteral("scrollbar.qml"));
    QQuickApplicationWindow *window = helper.appWindow;
    window->show();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QQuickScrollBar *scrollBar = helper.appWindow->property("scrollBar").value<QQuickScrollBar*>();
    QVERIFY(scrollBar);

    QQuickTextArea *textArea = helper.appWindow->property("textArea").value<QQuickTextArea*>();
    QVERIFY(textArea);

    textArea->setText(QString("\n").repeated(100));

    const QPoint textAreaPos(window->width() / 2, window->height() / 2);
    QTest::mouseMove(window, textAreaPos);
    QCOMPARE(window->cursor().shape(), textArea->cursor().shape());
    QCOMPARE(textArea->cursor().shape(), Qt::CursorShape::IBeamCursor);

    const QPoint scrollBarPos(window->width() - scrollBar->width() / 2, window->height() / 2);
    QTest::mouseMove(window, scrollBarPos);
    QVERIFY(scrollBar->isActive());
    QCOMPARE(window->cursor().shape(), scrollBar->cursor().shape());
    QCOMPARE(scrollBar->cursor().shape(), Qt::CursorShape::ArrowCursor);

    scrollBar->setInteractive(false);
    QCOMPARE(window->cursor().shape(), textArea->cursor().shape());
}

QTEST_MAIN(tst_cursor)

#include "tst_cursor.moc"
