/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include <QtTest/QtTest>
#include <QtTest/QSignalSpy>
#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickview.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlexpression.h>

#include <qpa/qplatformdrag.h>
#include <qpa/qwindowsysteminterface.h>

template <typename T> static T evaluate(QObject *scope, const QString &expression)
{
    QQmlExpression expr(qmlContext(scope), scope, expression);
    QVariant result = expr.evaluate();
    if (expr.hasError())
        qWarning() << expr.error().toString();
    return result.value<T>();
}

template <> void evaluate<void>(QObject *scope, const QString &expression)
{
    QQmlExpression expr(qmlContext(scope), scope, expression);
    expr.evaluate();
    if (expr.hasError())
        qWarning() << expr.error().toString();
}

class tst_QQuickDropArea: public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void containsDrag_internal();
    void containsDrag_external();
    void keys_internal();
    void keys_external();
    void source_internal();
//    void source_external();
    void position_internal();
    void position_external();
    void drop_internal();
//    void drop_external();
    void competingDrags();
    void simultaneousDrags();
    void dropStuff();

private:
    QQmlEngine engine;
};

void tst_QQuickDropArea::initTestCase()
{

}

void tst_QQuickDropArea::cleanupTestCase()
{

}

void tst_QQuickDropArea::containsDrag_internal()
{
    QQuickWindow window;
    QQmlComponent component(&engine);
    component.setData(
            "import QtQuick 2.0\n"
            "DropArea {\n"
                "property bool hasDrag: containsDrag\n"
                "property int enterEvents: 0\n"
                "property int exitEvents: 0\n"
                "width: 100; height: 100\n"
                "onEntered: {++enterEvents}\n"
                "onExited: {++exitEvents}\n"
                "Item {\n"
                    "objectName: \"dragItem\"\n"
                    "x: 50; y: 50\n"
                    "width: 10; height: 10\n"
                "}\n"
            "}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickItem *dropArea = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(dropArea);
    dropArea->setParentItem(window.contentItem());

    QQuickItem *dragItem = dropArea->findChild<QQuickItem *>("dragItem");
    QVERIFY(dragItem);

    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), false);
    QCOMPARE(evaluate<bool>(dropArea, "hasDrag"), false);

    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<bool>(dropArea, "hasDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "exitEvents"), 0);

    evaluate<void>(dropArea, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem, "Drag.active = false");
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), false);
    QCOMPARE(evaluate<bool>(dropArea, "hasDrag"), false);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea, "exitEvents"), 1);

    evaluate<void>(dropArea, "{ enterEvents = 0; exitEvents = 0 }");

    dragItem->setPosition(QPointF(150, 50));
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), false);
    QCOMPARE(evaluate<bool>(dropArea, "hasDrag"), false);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea, "exitEvents"), 0);

    dragItem->setPosition(QPointF(50, 50));
    QCoreApplication::processEvents();
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<bool>(dropArea, "hasDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "exitEvents"), 0);

    evaluate<void>(dropArea, "{ enterEvents = 0; exitEvents = 0 }");
    dragItem->setPosition(QPointF(150, 50));
    QCoreApplication::processEvents();

    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), false);
    QCOMPARE(evaluate<bool>(dropArea, "hasDrag"), false);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea, "exitEvents"), 1);

    evaluate<void>(dragItem, "Drag.active = false");
}

void tst_QQuickDropArea::containsDrag_external()
{
    QQuickWindow window;
    QQmlComponent component(&engine);
    component.setData(
            "import QtQuick 2.0\n"
            "DropArea {\n"
                "property bool hasDrag: containsDrag\n"
                "property int enterEvents: 0\n"
                "property int exitEvents: 0\n"
                "width: 100; height: 100\n"
                "onEntered: {++enterEvents}\n"
                "onExited: {++exitEvents}\n"
            "}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickItem *dropArea = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(dropArea);
    dropArea->setParentItem(window.contentItem());

    QMimeData data;
    QQuickWindow alternateWindow;

    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), false);
    QCOMPARE(evaluate<bool>(dropArea, "hasDrag"), false);

    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<bool>(dropArea, "hasDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "exitEvents"), 0);

    evaluate<void>(dropArea, "{ enterEvents = 0; exitEvents = 0 }");
    QWindowSystemInterface::handleDrag(&alternateWindow, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), false);
    QCOMPARE(evaluate<bool>(dropArea, "hasDrag"), false);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea, "exitEvents"), 1);

    evaluate<void>(dropArea, "{ enterEvents = 0; exitEvents = 0 }");

    QWindowSystemInterface::handleDrag(&window, &data, QPoint(150, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), false);
    QCOMPARE(evaluate<bool>(dropArea, "hasDrag"), false);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea, "exitEvents"), 0);

    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<bool>(dropArea, "hasDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "exitEvents"), 0);

    evaluate<void>(dropArea, "{ enterEvents = 0; exitEvents = 0 }");

    QWindowSystemInterface::handleDrag(&window, &data, QPoint(150, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), false);
    QCOMPARE(evaluate<bool>(dropArea, "hasDrag"), false);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea, "exitEvents"), 1);

    QWindowSystemInterface::handleDrop(&window, &data, QPoint(150, 50), Qt::CopyAction);
}

void tst_QQuickDropArea::keys_internal()
{
    QQuickWindow window;
    QQmlComponent component(&engine);
    component.setData(
            "import QtQuick 2.0\n"
            "DropArea {\n"
                "property variant dragKeys\n"
                "property variant dropKeys: keys\n"
                "property int enterEvents: 0\n"
                "width: 100; height: 100\n"
                "onEntered: {++enterEvents; dragKeys = drag.keys }\n"
                "Item {\n"
                    "objectName: \"dragItem\"\n"
                    "x: 50; y: 50\n"
                    "width: 10; height: 10\n"
                    "Drag.keys: [\"red\", \"blue\"]\n"
                "}\n"
            "}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickItem *dropArea = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(dropArea);
    dropArea->setParentItem(window.contentItem());

    QQuickItem *dragItem = dropArea->findChild<QQuickItem *>("dragItem");
    QVERIFY(dragItem);

    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), false);

    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(dropArea->property("dragKeys").toStringList(), QStringList() << "red" << "blue");

    evaluate<void>(dragItem, "Drag.active = false");
    evaluate<void>(dropArea, "keys = \"blue\"");
    QCOMPARE(dropArea->property("keys").toStringList(), QStringList() << "blue");
    QCOMPARE(dropArea->property("dropKeys").toStringList(), QStringList() << "blue");
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(dropArea->property("dragKeys").toStringList(), QStringList() << "red" << "blue");

    evaluate<void>(dragItem, "Drag.active = false");
    evaluate<void>(dropArea, "keys = \"red\"");
    QCOMPARE(dropArea->property("keys").toStringList(), QStringList() << "red");
    QCOMPARE(dropArea->property("dropKeys").toStringList(), QStringList() << "red");
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(dropArea->property("dragKeys").toStringList(), QStringList() << "red" << "blue");

    evaluate<void>(dragItem, "Drag.active = false");
    evaluate<void>(dropArea, "keys = \"green\"");
    QCOMPARE(dropArea->property("keys").toStringList(), QStringList() << "green");
    QCOMPARE(dropArea->property("dropKeys").toStringList(), QStringList() << "green");
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 0);

    evaluate<void>(dragItem, "Drag.active = false");
    evaluate<void>(dropArea, "keys = [\"red\", \"green\"]");
    QCOMPARE(dropArea->property("keys").toStringList(), QStringList() << "red" << "green");
    QCOMPARE(dropArea->property("dropKeys").toStringList(), QStringList() << "red" << "green");
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(dropArea->property("dragKeys").toStringList(), QStringList() << "red" << "blue");

    evaluate<void>(dragItem, "Drag.active = false");
    evaluate<void>(dragItem, "Drag.keys = []");
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 0);

    evaluate<void>(dragItem, "Drag.active = false");
    evaluate<void>(dropArea, "keys = []");
    QCOMPARE(dropArea->property("keys").toStringList(), QStringList());
    QCOMPARE(dropArea->property("dropKeys").toStringList(), QStringList());
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(dropArea->property("dragKeys").toStringList(), QStringList());

    evaluate<void>(dragItem, "Drag.active = false");
    evaluate<void>(dropArea, "keys = []");
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(dropArea->property("dragKeys").toStringList(), QStringList());

    evaluate<void>(dragItem, "Drag.active = false");
    evaluate<void>(dragItem, "Drag.keys = [\"red\", \"blue\"]");
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(dropArea->property("dragKeys").toStringList(), QStringList() << "red" << "blue");
}

void tst_QQuickDropArea::keys_external()
{
    QQuickWindow window;
    QQmlComponent component(&engine);
    component.setData(
            "import QtQuick 2.0\n"
            "DropArea {\n"
                "property variant dragKeys\n"
                "property variant dropKeys: keys\n"
                "property int enterEvents: 0\n"
                "width: 100; height: 100\n"
                "onEntered: {++enterEvents; dragKeys = drag.keys }\n"
            "}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickItem *dropArea = qobject_cast<QQuickItem *>(object.data());
    dropArea->setParentItem(window.contentItem());

    QMimeData data;
    QQuickWindow alternateWindow;

    data.setData("text/x-red", "red");
    data.setData("text/x-blue", "blue");

    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), false);

    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(dropArea->property("dragKeys").toStringList(), QStringList() << "text/x-red" << "text/x-blue");

    QWindowSystemInterface::handleDrag(&alternateWindow, &data, QPoint(50, 50), Qt::CopyAction);
    evaluate<void>(dropArea, "keys = \"text/x-blue\"");
    QCOMPARE(dropArea->property("keys").toStringList(), QStringList() << "text/x-blue");
    QCOMPARE(dropArea->property("dropKeys").toStringList(), QStringList() << "text/x-blue");
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(dropArea->property("dragKeys").toStringList(), QStringList() << "text/x-red" << "text/x-blue");

    QWindowSystemInterface::handleDrag(&alternateWindow, &data, QPoint(50, 50), Qt::CopyAction);
    evaluate<void>(dropArea, "keys = \"text/x-red\"");
    QCOMPARE(dropArea->property("keys").toStringList(), QStringList() << "text/x-red");
    QCOMPARE(dropArea->property("dropKeys").toStringList(), QStringList() << "text/x-red");
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(dropArea->property("dragKeys").toStringList(), QStringList() << "text/x-red" << "text/x-blue");

    QWindowSystemInterface::handleDrag(&alternateWindow, &data, QPoint(50, 50), Qt::CopyAction);
    evaluate<void>(dropArea, "keys = \"text/x-green\"");
    QCOMPARE(dropArea->property("keys").toStringList(), QStringList() << "text/x-green");
    QCOMPARE(dropArea->property("dropKeys").toStringList(), QStringList() << "text/x-green");
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 0);

    QWindowSystemInterface::handleDrag(&alternateWindow, &data, QPoint(50, 50), Qt::CopyAction);
    evaluate<void>(dropArea, "keys = [\"text/x-red\", \"text/x-green\"]");
    QCOMPARE(dropArea->property("keys").toStringList(), QStringList() << "text/x-red" << "text/x-green");
    QCOMPARE(dropArea->property("dropKeys").toStringList(), QStringList() << "text/x-red" << "text/x-green");
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(dropArea->property("dragKeys").toStringList(), QStringList() << "text/x-red" << "text/x-blue");

    QWindowSystemInterface::handleDrag(&alternateWindow, &data, QPoint(50, 50), Qt::CopyAction);
    data.removeFormat("text/x-red");
    data.removeFormat("text/x-blue");
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 0);

    QWindowSystemInterface::handleDrag(&alternateWindow, &data, QPoint(50, 50), Qt::CopyAction);
    evaluate<void>(dropArea, "keys = []");
    QCOMPARE(dropArea->property("keys").toStringList(), QStringList());
    QCOMPARE(dropArea->property("dropKeys").toStringList(), QStringList());
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(dropArea->property("dragKeys").toStringList(), QStringList());

    QWindowSystemInterface::handleDrag(&alternateWindow, &data, QPoint(50, 50), Qt::CopyAction);
    data.setData("text/x-red", "red");
    data.setData("text/x-blue", "blue");
    QCOMPARE(dropArea->property("keys").toStringList(), QStringList());
    QCOMPARE(dropArea->property("dropKeys").toStringList(), QStringList());
    evaluate<void>(dropArea, "{ enterEvents = 0; dragKeys = undefined }");
    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(dropArea->property("dragKeys").toStringList(), QStringList() << "text/x-red" << "text/x-blue");

    QWindowSystemInterface::handleDrop(&window, &data, QPoint(50, 50), Qt::CopyAction);
}

void tst_QQuickDropArea::source_internal()
{
    QQuickWindow window;
    QQmlComponent component(&engine);
    component.setData(
            "import QtQuick 2.0\n"
            "DropArea {\n"
                "property Item source: drag.source\n"
                "property Item eventSource\n"
                "width: 100; height: 100\n"
                "onEntered: {eventSource = drag.source}\n"
                "Item {\n"
                    "objectName: \"dragItem\"\n"
                    "x: 50; y: 50\n"
                    "width: 10; height: 10\n"
                "}\n"
                "Item { id: dragSource; objectName: \"dragSource\" }\n"
            "}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickItem *dropArea = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(dropArea);
    dropArea->setParentItem(window.contentItem());

    QQuickItem *dragItem = dropArea->findChild<QQuickItem *>("dragItem");
    QVERIFY(dragItem);

    QQuickItem *dragSource = dropArea->findChild<QQuickItem *>("dragSource");
    QVERIFY(dragSource);

    QCOMPARE(evaluate<QObject *>(dropArea, "source"), static_cast<QObject *>(0));
    QCOMPARE(evaluate<QObject *>(dropArea, "drag.source"), static_cast<QObject *>(0));

    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<QObject *>(dropArea, "source"), static_cast<QObject *>(dragItem));
    QCOMPARE(evaluate<QObject *>(dropArea, "drag.source"), static_cast<QObject *>(dragItem));
    QCOMPARE(evaluate<QObject *>(dropArea, "eventSource"), static_cast<QObject *>(dragItem));

    evaluate<void>(dragItem, "Drag.active = false");
    QCOMPARE(evaluate<QObject *>(dropArea, "source"), static_cast<QObject *>(0));
    QCOMPARE(evaluate<QObject *>(dropArea, "drag.source"), static_cast<QObject *>(0));


    evaluate<void>(dropArea, "{ eventSource = null }");
    evaluate<void>(dragItem, "Drag.source = dragSource");

    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<QObject *>(dropArea, "source"), static_cast<QObject *>(dragSource));
    QCOMPARE(evaluate<QObject *>(dropArea, "drag.source"), static_cast<QObject *>(dragSource));
    QCOMPARE(evaluate<QObject *>(dropArea, "eventSource"), static_cast<QObject *>(dragSource));

    evaluate<void>(dragItem, "Drag.active = false");
    QCOMPARE(evaluate<QObject *>(dropArea, "source"), static_cast<QObject *>(0));
    QCOMPARE(evaluate<QObject *>(dropArea, "drag.source"), static_cast<QObject *>(0));
}

// Setting a source can't be emulated using the QWindowSystemInterface API.

//void tst_QQuickDropArea::source_external()
//{
//}

void tst_QQuickDropArea::position_internal()
{
    QQuickWindow window;
    QQmlComponent component(&engine);
    component.setData(
            "import QtQuick 2.0\n"
            "DropArea {\n"
                "property real dragX: drag.x\n"
                "property real dragY: drag.y\n"
                "property real eventX\n"
                "property real eventY\n"
                "property int enterEvents: 0\n"
                "property int moveEvents: 0\n"
                "width: 100; height: 100\n"
                "onEntered: {++enterEvents; eventX = drag.x; eventY = drag.y}\n"
                "onPositionChanged: {++moveEvents; eventX = drag.x; eventY = drag.y}\n"
                "Item {\n"
                    "objectName: \"dragItem\"\n"
                    "x: 50; y: 50\n"
                    "width: 10; height: 10\n"
                "}\n"
            "}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickItem *dropArea = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(dropArea);
    dropArea->setParentItem(window.contentItem());

    QQuickItem *dragItem = dropArea->findChild<QQuickItem *>("dragItem");
    QVERIFY(dragItem);

    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "moveEvents"), 0);
    QCOMPARE(evaluate<qreal>(dropArea, "drag.x"), qreal(50));
    QCOMPARE(evaluate<qreal>(dropArea, "drag.y"), qreal(50));
    QCOMPARE(evaluate<qreal>(dropArea, "dragX"), qreal(50));
    QCOMPARE(evaluate<qreal>(dropArea, "dragY"), qreal(50));
    QCOMPARE(evaluate<qreal>(dropArea, "eventX"), qreal(50));
    QCOMPARE(evaluate<qreal>(dropArea, "eventY"), qreal(50));

    evaluate<void>(dropArea, "{ enterEvents = 0; moveEvents = 0; eventX = -1; eventY = -1 }");
    dragItem->setPosition(QPointF(40, 50));
    QCoreApplication::processEvents();
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea, "moveEvents"), 1);
    QCOMPARE(evaluate<qreal>(dropArea, "drag.x"), qreal(40));
    QCOMPARE(evaluate<qreal>(dropArea, "drag.y"), qreal(50));
    QCOMPARE(evaluate<qreal>(dropArea, "dragX"), qreal(40));
    QCOMPARE(evaluate<qreal>(dropArea, "dragY"), qreal(50));
    QCOMPARE(evaluate<qreal>(dropArea, "eventX"), qreal(40));
    QCOMPARE(evaluate<qreal>(dropArea, "eventY"), qreal(50));

    evaluate<void>(dropArea, "{ enterEvents = 0; moveEvents = 0; eventX = -1; eventY = -1 }");
    dragItem->setPosition(QPointF(75, 25));
    QCoreApplication::processEvents();
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea, "moveEvents"), 1);
    QCOMPARE(evaluate<qreal>(dropArea, "drag.x"), qreal(75));
    QCOMPARE(evaluate<qreal>(dropArea, "drag.y"), qreal(25));
    QCOMPARE(evaluate<qreal>(dropArea, "dragX"), qreal(75));
    QCOMPARE(evaluate<qreal>(dropArea, "dragY"), qreal(25));
    QCOMPARE(evaluate<qreal>(dropArea, "eventX"), qreal(75));
    QCOMPARE(evaluate<qreal>(dropArea, "eventY"), qreal(25));

    evaluate<void>(dragItem, "Drag.active = false");
}

void tst_QQuickDropArea::position_external()
{
    QQuickWindow window;
    QQmlComponent component(&engine);
    component.setData(
            "import QtQuick 2.0\n"
            "DropArea {\n"
                "property real dragX: drag.x\n"
                "property real dragY: drag.y\n"
                "property real eventX\n"
                "property real eventY\n"
                "property int enterEvents: 0\n"
                "property int moveEvents: 0\n"
                "width: 100; height: 100\n"
                "onEntered: {++enterEvents; eventX = drag.x; eventY = drag.y}\n"
                "onPositionChanged: {++moveEvents; eventX = drag.x; eventY = drag.y}\n"
            "}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickItem *dropArea = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(dropArea);
    dropArea->setParentItem(window.contentItem());

    QMimeData data;

    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "moveEvents"), 1);
    QCOMPARE(evaluate<qreal>(dropArea, "drag.x"), qreal(50));
    QCOMPARE(evaluate<qreal>(dropArea, "drag.y"), qreal(50));
    QCOMPARE(evaluate<qreal>(dropArea, "dragX"), qreal(50));
    QCOMPARE(evaluate<qreal>(dropArea, "dragY"), qreal(50));
    QCOMPARE(evaluate<qreal>(dropArea, "eventX"), qreal(50));
    QCOMPARE(evaluate<qreal>(dropArea, "eventY"), qreal(50));

    evaluate<void>(dropArea, "{ enterEvents = 0; moveEvents = 0; eventX = -1; eventY = -1 }");
    QWindowSystemInterface::handleDrag(&window, &data, QPoint(40, 50), Qt::CopyAction);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea, "moveEvents"), 1);
    QCOMPARE(evaluate<qreal>(dropArea, "drag.x"), qreal(40));
    QCOMPARE(evaluate<qreal>(dropArea, "drag.y"), qreal(50));
    QCOMPARE(evaluate<qreal>(dropArea, "dragX"), qreal(40));
    QCOMPARE(evaluate<qreal>(dropArea, "dragY"), qreal(50));
    QCOMPARE(evaluate<qreal>(dropArea, "eventX"), qreal(40));
    QCOMPARE(evaluate<qreal>(dropArea, "eventY"), qreal(50));

    evaluate<void>(dropArea, "{ enterEvents = 0; moveEvents = 0; eventX = -1; eventY = -1 }");
    QWindowSystemInterface::handleDrag(&window, &data, QPoint(75, 25), Qt::CopyAction);
    QCOMPARE(evaluate<int>(dropArea, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea, "moveEvents"), 1);
    QCOMPARE(evaluate<qreal>(dropArea, "drag.x"), qreal(75));
    QCOMPARE(evaluate<qreal>(dropArea, "drag.y"), qreal(25));
    QCOMPARE(evaluate<qreal>(dropArea, "dragX"), qreal(75));
    QCOMPARE(evaluate<qreal>(dropArea, "dragY"), qreal(25));
    QCOMPARE(evaluate<qreal>(dropArea, "eventX"), qreal(75));
    QCOMPARE(evaluate<qreal>(dropArea, "eventY"), qreal(25));

    QWindowSystemInterface::handleDrop(&window, &data, QPoint(75, 25), Qt::CopyAction);
}

void tst_QQuickDropArea::drop_internal()
{
    QQuickWindow window;
    QQmlComponent component(&engine);
    component.setData(
            "import QtQuick 2.0\n"
            "DropArea {\n"
                "property bool accept: false\n"
                "property bool setAccepted: false\n"
                "property bool acceptDropAction: false\n"
                "property bool setDropAction: false\n"
                "property int dropAction: Qt.IgnoreAction\n"
                "property int proposedAction: Qt.IgnoreAction\n"
                "property int supportedActions: Qt.IgnoreAction\n"
                "property int dropEvents: 0\n"
                "width: 100; height: 100\n"
                "onDropped: {\n"
                    "++dropEvents\n"
                    "supportedActions = drop.supportedActions\n"
                    "proposedAction = drop.action\n"
                    "if (setDropAction)\n"
                        "drop.action = dropAction\n"
                    "if (acceptDropAction)\n"
                        "drop.accept(dropAction)\n"
                    "else if (setAccepted)\n"
                        "drop.accepted = accept\n"
                    "else if (accept)\n"
                        "drop.accept()\n"
                "}\n"
                "Item {\n"
                    "objectName: \"dragItem\"\n"
                    "x: 50; y: 50\n"
                    "width: 10; height: 10\n"
                "}\n"
            "}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickItem *dropArea = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(dropArea);
    dropArea->setParentItem(window.contentItem());

    QQuickItem *dragItem = dropArea->findChild<QQuickItem *>("dragItem");
    QVERIFY(dragItem);

    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<int>(dragItem, "Drag.drop()"), int(Qt::IgnoreAction));
    QCOMPARE(evaluate<int>(dropArea, "dropEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "supportedActions"), int(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "proposedAction"), int(Qt::MoveAction));

    evaluate<void>(dropArea, "{ dropEvents = 0; proposedAction = Qt.IgnoreAction; supportedActions = Qt.IgnoreAction }");
    evaluate<void>(dropArea, "{ accept = true; setDropAction = true; dropAction = Qt.LinkAction }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<int>(dragItem, "Drag.drop()"), int(Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "dropEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "supportedActions"), int(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "proposedAction"), int(Qt::MoveAction));

    evaluate<void>(dropArea, "{ dropEvents = 0; proposedAction = Qt.IgnoreAction; supportedActions = Qt.IgnoreAction }");
    evaluate<void>(dropArea, "{ setAccepted = true; }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<int>(dragItem, "Drag.drop()"), int(Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "dropEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "supportedActions"), int(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "proposedAction"), int(Qt::MoveAction));

    evaluate<void>(dropArea, "{ dropEvents = 0; proposedAction = Qt.IgnoreAction; supportedActions = Qt.IgnoreAction }");
    evaluate<void>(dropArea, "{ accept = false; setAccepted = true; }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<int>(dragItem, "Drag.drop()"), int(Qt::IgnoreAction));
    QCOMPARE(evaluate<int>(dropArea, "dropEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "supportedActions"), int(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "proposedAction"), int(Qt::MoveAction));

    evaluate<void>(dropArea, "{ dropEvents = 0; proposedAction = Qt.IgnoreAction; supportedActions = Qt.IgnoreAction }");
    evaluate<void>(dropArea, "{ setAccepted = false; setDropAction = false; acceptDropAction = true; }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<int>(dragItem, "Drag.drop()"), int(Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "dropEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "supportedActions"), int(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "proposedAction"), int(Qt::MoveAction));

    evaluate<void>(dropArea, "{ dropEvents = 0; proposedAction = Qt.IgnoreAction; supportedActions = Qt.IgnoreAction }");
    evaluate<void>(dropArea, "{ acceptDropAction = false; dropAction = Qt.IgnoreAction; accept = true }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<int>(dragItem, "Drag.drop()"), int(Qt::MoveAction));
    QCOMPARE(evaluate<int>(dropArea, "dropEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "supportedActions"), int(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "proposedAction"), int(Qt::MoveAction));

    evaluate<void>(dropArea, "{ dropEvents = 0; proposedAction = Qt.IgnoreAction; supportedActions = Qt.IgnoreAction }");
    evaluate<void>(dropArea, "{ setAccepted = true }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<int>(dragItem, "Drag.drop()"), int(Qt::MoveAction));
    QCOMPARE(evaluate<int>(dropArea, "dropEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "supportedActions"), int(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "proposedAction"), int(Qt::MoveAction));

    evaluate<void>(dropArea, "{ dropEvents = 0; proposedAction = Qt.IgnoreAction; supportedActions = Qt.IgnoreAction }");
    evaluate<void>(dropArea, "{ setAccepted = false }");
    evaluate<void>(dragItem, "Drag.supportedActions = Qt.LinkAction");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<int>(dragItem, "Drag.drop()"), int(Qt::MoveAction));
    QCOMPARE(evaluate<int>(dropArea, "dropEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "supportedActions"), int(Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "proposedAction"), int(Qt::MoveAction));

    evaluate<void>(dropArea, "{ dropEvents = 0; proposedAction = Qt.IgnoreAction; supportedActions = Qt.IgnoreAction }");
    evaluate<void>(dropArea, "{ setAccepted = true }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<int>(dragItem, "Drag.drop()"), int(Qt::MoveAction));
    QCOMPARE(evaluate<int>(dropArea, "dropEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "supportedActions"), int(Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "proposedAction"), int(Qt::MoveAction));

    evaluate<void>(dropArea, "{ dropEvents = 0; proposedAction = Qt.IgnoreAction; supportedActions = Qt.IgnoreAction }");
    evaluate<void>(dropArea, "{ setAccepted = false }");
    evaluate<void>(dragItem, "Drag.proposedAction = Qt.LinkAction");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<int>(dragItem, "Drag.drop()"), int(Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "dropEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "supportedActions"), int(Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "proposedAction"), int(Qt::LinkAction));

    evaluate<void>(dropArea, "{ dropEvents = 0; proposedAction = Qt.IgnoreAction; supportedActions = Qt.IgnoreAction }");
    evaluate<void>(dropArea, "{ setAccepted = true }");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<int>(dragItem, "Drag.drop()"), int(Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "dropEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea, "supportedActions"), int(Qt::LinkAction));
    QCOMPARE(evaluate<int>(dropArea, "proposedAction"), int(Qt::LinkAction));
}

// Setting the supportedActions can't be emulated using the QWindowSystemInterface API.

//void tst_QQuickDropArea::drop_external()
//{
//}

void tst_QQuickDropArea::competingDrags()
{
    QQuickWindow window;
    QQmlComponent component(&engine);
    component.setData(
            "import QtQuick 2.0\n"
            "DropArea {\n"
                "width: 100; height: 100\n"
                "objectName: \"dropArea1\"\n"
                "property string statuslol\n"
                "onEntered: { statuslol = 'parent' }\n"
                "DropArea {\n"
                    "objectName: \"dropArea2\"\n"
                    "width: 100; height: 100\n"
                    "property bool acceptsEnters: true\n"
                    "onEntered: { parent.statuslol = 'son'; drag.accepted = acceptsEnters; }\n"
                "}\n"
                "Item {\n"
                    "objectName: \"dragItem\"\n"
                    "x: 50; y: 50\n"
                    "width: 10; height: 10\n"
                "}\n"
            "}\n", QUrl());

    QScopedPointer<QObject> object(component.create());
    QQuickItem *dropArea1 = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(dropArea1);
    dropArea1->setParentItem(window.contentItem());

    QQuickItem *dropArea2 = dropArea1->findChild<QQuickItem *>("dropArea2");
    QVERIFY(dropArea2);

    QQuickItem *dragItem = dropArea1->findChild<QQuickItem *>("dragItem");
    QVERIFY(dragItem);

    QCOMPARE(evaluate<QString>(dropArea1, "statuslol"), QStringLiteral(""));
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<QString>(dropArea1, "statuslol"), QStringLiteral("son"));
    evaluate<void>(dragItem, "Drag.active = false");
    evaluate<void>(dropArea2, "acceptsEnters = false");
    evaluate<void>(dragItem, "Drag.active = true");
    QCOMPARE(evaluate<QString>(dropArea1, "statuslol"), QStringLiteral("parent"));
}

void tst_QQuickDropArea::simultaneousDrags()
{
    QQuickWindow window;
    QQmlComponent component(&engine);
    component.setData(
            "import QtQuick 2.0\n"
            "DropArea {\n"
                "property int enterEvents: 0\n"
                "property int exitEvents: 0\n"
                "width: 100; height: 100\n"
                "objectName: \"dropArea1\"\n"
                "keys: [\"red\", \"text/x-red\"]\n"
                "onEntered: {++enterEvents}\n"
                "onExited: {++exitEvents}\n"
                "DropArea {\n"
                    "objectName: \"dropArea2\"\n"
                    "property int enterEvents: 0\n"
                    "property int exitEvents: 0\n"
                    "width: 100; height: 100\n"
                    "keys: [\"blue\", \"text/x-blue\"]\n"
                    "onEntered: {++enterEvents}\n"
                    "onExited: {++exitEvents}\n"
                "}\n"
                "Item {\n"
                    "objectName: \"dragItem1\"\n"
                    "x: 50; y: 50\n"
                    "width: 10; height: 10\n"
                    "Drag.keys: [\"red\", \"blue\"]"
                "}\n"
                "Item {\n"
                    "objectName: \"dragItem2\"\n"
                    "x: 50; y: 50\n"
                    "width: 10; height: 10\n"
                    "Drag.keys: [\"red\", \"blue\"]"
                "}\n"
            "}", QUrl());

    QScopedPointer<QObject> object(component.create());
    QQuickItem *dropArea1 = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(dropArea1);
    dropArea1->setParentItem(window.contentItem());

    QQuickItem *dropArea2 = dropArea1->findChild<QQuickItem *>("dropArea2");
    QVERIFY(dropArea2);

    QQuickItem *dragItem1 = dropArea1->findChild<QQuickItem *>("dragItem1");
    QVERIFY(dragItem1);

    QQuickItem *dragItem2 = dropArea1->findChild<QQuickItem *>("dragItem2");
    QVERIFY(dragItem2);

    QMimeData data;
    data.setData("text/x-red", "red");
    data.setData("text/x-blue", "blue");

    QQuickWindow alternateWindow;

    // Mixed internal drags.
    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem1, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem2, "Drag.active = true");
    //DropArea discards events if already contains something being dragged in
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dragItem2, "Drag.active = false");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 1);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dragItem2, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 2);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 1);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dragItem1, "Drag.active = false");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 2);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 1);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 1);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem2, "Drag.active = false");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 1);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    // internal then external.
    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem1, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    //Same as in the first case, dropArea2 already contains a drag, dropArea1 will get the event
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    QWindowSystemInterface::handleDrag(&alternateWindow, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 1);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 2);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 1);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dragItem1, "Drag.active = false");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 2);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 1);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 1);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    QWindowSystemInterface::handleDrag(&alternateWindow, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 1);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    // external then internal.
    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem2, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dragItem2, "Drag.active = false");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 1);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dragItem2, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 2);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 1);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    QWindowSystemInterface::handleDrag(&alternateWindow, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 2);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 1);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 1);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem2, "Drag.active = false");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 1);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    // Different acceptance
    evaluate<void>(dragItem1, "Drag.keys = \"red\"");
    evaluate<void>(dragItem2, "Drag.keys = \"blue\"");
    data.removeFormat("text/x-red");

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem1, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem2, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem2, "Drag.active = false");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 1);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem2, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem1, "Drag.active = false");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 1);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem2, "Drag.active = false");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 1);

    // internal then external
    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem1, "Drag.active = true");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    QWindowSystemInterface::handleDrag(&alternateWindow, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 1);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 1);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dragItem1, "Drag.active = false");
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 1);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), true);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 0);

    evaluate<void>(dropArea1, "{ enterEvents = 0; exitEvents = 0 }");
    evaluate<void>(dropArea2, "{ enterEvents = 0; exitEvents = 0 }");
    QWindowSystemInterface::handleDrag(&alternateWindow, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<bool>(dropArea1, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea1, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea1, "exitEvents"), 0);
    QCOMPARE(evaluate<bool>(dropArea2, "containsDrag"), false);
    QCOMPARE(evaluate<int>(dropArea2, "enterEvents"), 0);
    QCOMPARE(evaluate<int>(dropArea2, "exitEvents"), 1);

    QWindowSystemInterface::handleDrop(&alternateWindow, &data, QPoint(50, 50), Qt::CopyAction);
}

void tst_QQuickDropArea::dropStuff()
{
    QQuickWindow window;
    QQmlComponent component(&engine);
    component.setData(
            "import QtQuick 2.3\n"
            "DropArea {\n"
                "width: 100; height: 100\n"
                "property var array\n"
                "onDropped: { array = drop.getDataAsArrayBuffer('text/x-red'); }\n"
            "}", QUrl());

    QScopedPointer<QObject> object(component.create());
    QQuickItem *dropArea = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(dropArea);
    dropArea->setParentItem(window.contentItem());

    QMimeData data;
    data.setData("text/x-red", "red");

    QCOMPARE(evaluate<QVariant>(dropArea, "array"), QVariant());

    QWindowSystemInterface::handleDrag(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QWindowSystemInterface::handleDrop(&window, &data, QPoint(50, 50), Qt::CopyAction);
    QCOMPARE(evaluate<int>(dropArea, "array.byteLength"), 3);
    QCOMPARE(evaluate<QByteArray>(dropArea, "array"), QByteArray("red"));
}

QTEST_MAIN(tst_QQuickDropArea)

#include "tst_qquickdroparea.moc"
