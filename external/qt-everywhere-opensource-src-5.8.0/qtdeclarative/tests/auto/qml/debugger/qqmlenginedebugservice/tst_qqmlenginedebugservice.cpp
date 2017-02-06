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

#include "qqmlenginedebugclient.h"
#include "debugutil_p.h"
#include "../../../shared/util.h"

#include <private/qqmlbinding_p.h>
#include <private/qqmlboundsignal_p.h>
#include <private/qqmldebugservice_p.h>
#include <private/qqmlmetatype_p.h>
#include <private/qqmlproperty_p.h>
#include <private/qqmldebugconnection_p.h>

#include <QtTest/qtest.h>
#include <QtTest/qsignalspy.h>

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlexpression.h>
#include <QtQml/qqmlproperty.h>
#include <QtQuick/qquickitem.h>

#include <QtNetwork/qhostaddress.h>
#include <QtCore/qtimer.h>
#include <QtCore/qdebug.h>
#include <QtCore/qthread.h>
#include <QtCore/qabstractitemmodel.h>

#define QVERIFYOBJECT(statement) \
    do {\
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__)) {\
    return QmlDebugObjectReference();\
    }\
    } while (0)

class NonScriptProperty : public QObject {
    Q_OBJECT
    Q_PROPERTY(int nonScriptProp READ nonScriptProp WRITE setNonScriptProp NOTIFY nonScriptPropChanged SCRIPTABLE false)
public:
    int nonScriptProp() const { return 0; }
    void setNonScriptProp(int) {}
signals:
    void nonScriptPropChanged();
};
QML_DECLARE_TYPE(NonScriptProperty)

class CustomTypes : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QModelIndex modelIndex READ modelIndex)
public:
    CustomTypes(QObject *parent = 0) : QObject(parent) {}

    QModelIndex modelIndex() { return QModelIndex(); }
};

class tst_QQmlEngineDebugService : public QObject
{
    Q_OBJECT
public:
    tst_QQmlEngineDebugService() : m_conn(0), m_dbg(0), m_engine(0), m_rootItem(0) {}

private:
    QmlDebugObjectReference findRootObject(int context = 0,
                                           bool recursive = false);
    QmlDebugPropertyReference findProperty(
            const QList<QmlDebugPropertyReference> &props,
            const QString &name) const;

    void recursiveObjectTest(QObject *o,
                             const QmlDebugObjectReference &oref,
                             bool recursive) const;

    QQmlDebugConnection *m_conn;
    QQmlEngineDebugClient *m_dbg;
    QQmlEngine *m_engine;
    QQuickItem *m_rootItem;

    QObjectList m_components;

private slots:
    void initTestCase();
    void cleanupTestCase();

    void watch_property();
    void watch_object();
    void watch_expression();
    void watch_expression_data();
    void watch_context();
    void watch_file();

    void queryAvailableEngines();
    void queryRootContexts();
    void queryObject();
    void queryObject_data();
    void queryObjectsForLocation();
    void queryObjectsForLocation_data();
    void queryExpressionResult();
    void queryExpressionResult_data();
    void queryExpressionResultInRootContext();
    void queryExpressionResultBC();
    void queryExpressionResultBC_data();

    void setBindingForObject();
    void resetBindingForObject();
    void setMethodBody();
    void queryObjectTree();
    void setBindingInStates();

    void regression_QTCREATORBUG_7451();
    void queryObjectWithNonStreamableTypes();
};

QmlDebugObjectReference tst_QQmlEngineDebugService::findRootObject(
        int context, bool recursive)
{
    bool success = false;
    m_dbg->queryAvailableEngines(&success);
    QVERIFYOBJECT(success);
    QVERIFYOBJECT(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    QVERIFYOBJECT(m_dbg->engines().count());
    m_dbg->queryRootContexts(m_dbg->engines()[0].debugId, &success);
    QVERIFYOBJECT(success);
    QVERIFYOBJECT(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    QVERIFYOBJECT(m_dbg->rootContext().contexts.count());
    QVERIFYOBJECT(m_dbg->rootContext().contexts.last().objects.count());
    int count = m_dbg->rootContext().contexts.count();
    recursive ? m_dbg->queryObjectRecursive(m_dbg->rootContext().contexts[count - context - 1].objects[0],
                                            &success) :
                m_dbg->queryObject(m_dbg->rootContext().contexts[count - context - 1].objects[0], &success);
    QVERIFYOBJECT(success);
    QVERIFYOBJECT(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    return m_dbg->object();
}

QmlDebugPropertyReference tst_QQmlEngineDebugService::findProperty(
        const QList<QmlDebugPropertyReference> &props, const QString &name) const
{
    foreach (const QmlDebugPropertyReference &p, props) {
        if (p.name == name)
            return p;
    }
    return QmlDebugPropertyReference();
}

void tst_QQmlEngineDebugService::recursiveObjectTest(
        QObject *o, const QmlDebugObjectReference &oref, bool recursive) const
{
    const QMetaObject *meta = o->metaObject();

    QQmlType *type = QQmlMetaType::qmlType(meta);
    QString className = type ? QString(type->qmlTypeName())
                             : QString(meta->className());
    className = className.mid(className.lastIndexOf(QLatin1Char('/'))+1);

    QCOMPARE(oref.debugId, QQmlDebugService::idForObject(o));
    QCOMPARE(oref.name, o->objectName());
    QCOMPARE(oref.className, className);
    QCOMPARE(oref.contextDebugId, QQmlDebugService::idForObject(
                 qmlContext(o)));

    const QObjectList &children = o->children();
    for (int i=0; i<children.count(); i++) {
        QObject *child = children[i];
        if (!qmlContext(child))
            continue;
        int debugId = QQmlDebugService::idForObject(child);
        QVERIFY(debugId >= 0);

        QmlDebugObjectReference cref;
        foreach (const QmlDebugObjectReference &ref, oref.children) {
            if (ref.debugId == debugId) {
                cref = ref;
                break;
            }
        }
        QVERIFY(cref.debugId >= 0);

        if (recursive)
            recursiveObjectTest(child, cref, true);
    }

    foreach (const QmlDebugPropertyReference &p, oref.properties) {
        QCOMPARE(p.objectDebugId, QQmlDebugService::idForObject(o));

        // signal properties are fake - they are generated from QQmlAbstractBoundSignal children
        if (p.name.startsWith("on") && p.name.length() > 2 && p.name[2].isUpper()) {
            QString signal = p.value.toString();
            QQmlBoundSignalExpression *expr = QQmlPropertyPrivate::signalExpression(QQmlProperty(o, p.name));
            QVERIFY(expr && expr->expression() == signal);
            QVERIFY(p.valueTypeName.isEmpty());
            QVERIFY(p.binding.isEmpty());
            QVERIFY(!p.hasNotifySignal);
            continue;
        }

        QMetaProperty pmeta = meta->property(meta->indexOfProperty(p.name.toUtf8().constData()));

        QCOMPARE(p.name, QString::fromUtf8(pmeta.name()));

        if (pmeta.type() < QVariant::UserType && pmeta.userType() !=
                QMetaType::QVariant) // TODO test complex types
            QCOMPARE(p.value , pmeta.read(o));

        if (p.name == "parent")
            QVERIFY(p.valueTypeName == "QGraphicsObject*" ||
                    p.valueTypeName == "QQuickItem*");
        else
            QCOMPARE(p.valueTypeName, QString::fromUtf8(pmeta.typeName()));

        QQmlAbstractBinding *binding =
                QQmlPropertyPrivate::binding(
                    QQmlProperty(o, p.name));
        if (binding)
            QCOMPARE(binding->expression(), p.binding);

        QCOMPARE(p.hasNotifySignal, pmeta.hasNotifySignal());

        QVERIFY(pmeta.isValid());
    }
}

void tst_QQmlEngineDebugService::initTestCase()
{
    qmlRegisterType<NonScriptProperty>("Test", 1, 0, "NonScriptPropertyElement");

    QTest::ignoreMessage(QtDebugMsg, "QML Debugger: Waiting for connection on port 3768...");
    m_engine = new QQmlEngine(this);

    QList<QByteArray> qml;
    qml << "import QtQuick 2.0\n"
           "import Test 1.0\n"
           "Item {"
                "id: root\n"
                "width: 10; height: 20; scale: blueRect.scale;"
                "Rectangle { id: blueRect; width: 500; height: 600; color: \"blue\"; }"
                "Text { font.bold: true; color: blueRect.color; }"
                "MouseArea {"
                    "onEntered: { console.log('hello') }"
                "}"
                "property variant varObj\n"
                "property variant varObjList: []\n"
                "property variant varObjMap\n"
                "property variant simpleVar: 10.05\n"
                "Component.onCompleted: {\n"
                    "varObj = blueRect;\n"
                    "var list = varObjList;\n"
                    "list[0] = blueRect;\n"
                    "varObjList = list;\n"
                    "var map = new Object;\n"
                    "map.rect = blueRect;\n"
                    "varObjMap = map;\n"
                "}\n"
                "NonScriptPropertyElement {\n"
                "}\n"
            "}";

    // add second component to test multiple root contexts
    qml << "import QtQuick 2.0\n"
            "Item {}";

    // and a third to test methods
    qml << "import QtQuick 2.0\n"
            "Item {"
                "function myMethodNoArgs() { return 3; }\n"
                "function myMethod(a) { return a + 9; }\n"
                "function myMethodIndirect() { myMethod(3); }\n"
            "}";

    // and a fourth to test states
    qml << "import QtQuick 2.0\n"
           "Rectangle {\n"
                "id:rootRect\n"
                "width:100\n"
                "states: [\n"
                    "State {\n"
                        "name:\"state1\"\n"
                        "PropertyChanges {\n"
                            "target:rootRect\n"
                            "width:200\n"
                        "}\n"
                    "}\n"
                "]\n"
                "transitions: [\n"
                    "Transition {\n"
                        "from:\"*\"\n"
                        "to:\"state1\"\n"
                        "PropertyAnimation {\n"
                            "target:rootRect\n"
                            "property:\"width\"\n"
                            "duration:100\n"
                        "}\n"
                    "}\n"
                "]\n"
           "}\n"
           ;

    // test non-streamable properties
    qmlRegisterType<CustomTypes>("Backend", 1, 0, "CustomTypes");
    qml << "import Backend 1.0\n"
           "CustomTypes {}"
           ;

    for (int i=0; i<qml.count(); i++) {
        QQmlComponent component(m_engine);
        component.setData(qml[i], QUrl::fromLocalFile(""));
        QVERIFY(component.isReady());  // fails if bad syntax
        m_components << qobject_cast<QQuickItem*>(component.create());
    }
    m_rootItem = qobject_cast<QQuickItem*>(m_components.first());

    // add an extra context to test for multiple contexts
    QQmlContext *context = new QQmlContext(m_engine->rootContext(), this);
    context->setObjectName("tst_QQmlDebug_childContext");

    m_conn = new QQmlDebugConnection(this);
    m_conn->connectToHost("127.0.0.1", 3768);

    bool ok = m_conn->waitForConnected();
    QVERIFY(ok);
    m_dbg = new QQmlEngineDebugClient(m_conn);
    QList<QQmlDebugClient *> others = QQmlDebugTest::createOtherClients(m_conn);
    QTRY_COMPARE(m_dbg->state(), QQmlEngineDebugClient::Enabled);
    foreach (QQmlDebugClient *other, others)
        QCOMPARE(other->state(), QQmlDebugClient::Unavailable);
    qDeleteAll(others);
}

void tst_QQmlEngineDebugService::cleanupTestCase()
{
    delete m_conn;
    qDeleteAll(m_components);
    delete m_engine;
}

void tst_QQmlEngineDebugService::setMethodBody()
{
    bool success;
    QmlDebugObjectReference obj = findRootObject(2);

    QObject *root = m_components.at(2);
    // Without args
    {
    QVariant rv;
    QVERIFY(QMetaObject::invokeMethod(root, "myMethodNoArgs", Qt::DirectConnection,
                                      Q_RETURN_ARG(QVariant, rv)));
    QCOMPARE(rv, QVariant(qreal(3)));


    QVERIFY(m_dbg->setMethodBody(obj.debugId, "myMethodNoArgs", "return 7",
                                 &success));
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    QVERIFY(QMetaObject::invokeMethod(root, "myMethodNoArgs", Qt::DirectConnection,
                                      Q_RETURN_ARG(QVariant, rv)));
    QCOMPARE(rv, QVariant(qreal(7)));
    }

    // With args
    {
    QVariant rv;
    QVERIFY(QMetaObject::invokeMethod(root, "myMethod", Qt::DirectConnection,
                                      Q_RETURN_ARG(QVariant, rv), Q_ARG(QVariant, QVariant(19))));
    QCOMPARE(rv, QVariant(qreal(28)));

    QVERIFY(m_dbg->setMethodBody(obj.debugId, "myMethod", "return a + 7",
                                 &success));
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    QVERIFY(QMetaObject::invokeMethod(root, "myMethod", Qt::DirectConnection,
                                      Q_RETURN_ARG(QVariant, rv), Q_ARG(QVariant, QVariant(19))));
    QCOMPARE(rv, QVariant(qreal(26)));
    }
}

void tst_QQmlEngineDebugService::watch_property()
{
    QmlDebugObjectReference obj = findRootObject();
    QmlDebugPropertyReference prop = findProperty(obj.properties, "width");

    bool success;

    QQmlEngineDebugClient *unconnected = new QQmlEngineDebugClient(0);
    unconnected->addWatch(prop, &success);
    QVERIFY(!success);
    delete unconnected;

    m_dbg->addWatch(QmlDebugPropertyReference(), &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), false);

    quint32 id = m_dbg->addWatch(prop, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), true);

    QSignalSpy spy(m_dbg, SIGNAL(valueChanged(QByteArray,QVariant)));

    int origWidth = m_rootItem->property("width").toInt();
    m_rootItem->setProperty("width", origWidth*2);

    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(valueChanged(QByteArray,QVariant))));
    QCOMPARE(spy.count(), 1);

    m_dbg->removeWatch(id, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), true);

    // restore original value and verify spy doesn't get additional signal since watch has been removed
    m_rootItem->setProperty("width", origWidth);
    QTest::qWait(100);
    QCOMPARE(spy.count(), 1);

    QCOMPARE(spy.at(0).at(0).value<QByteArray>(), prop.name.toUtf8());
    QCOMPARE(spy.at(0).at(1).value<QVariant>(), qVariantFromValue(origWidth*2));
}

void tst_QQmlEngineDebugService::watch_object()
{
    QmlDebugObjectReference obj = findRootObject();

    bool success;

    QQmlEngineDebugClient *unconnected = new QQmlEngineDebugClient(0);
    unconnected->addWatch(obj, &success);
    QVERIFY(!success);
    delete unconnected;

    m_dbg->addWatch(QmlDebugObjectReference(), &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), false);

    quint32 id = m_dbg->addWatch(obj, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), true);

    QSignalSpy spy(m_dbg, SIGNAL(valueChanged(QByteArray,QVariant)));

    int origWidth = m_rootItem->property("width").toInt();
    int origHeight = m_rootItem->property("height").toInt();
    m_rootItem->setProperty("width", origWidth*2);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(valueChanged(QByteArray,QVariant))));
    m_rootItem->setProperty("height", origHeight*2);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(valueChanged(QByteArray,QVariant))));

    QVERIFY(spy.count() > 0);

    int newWidth = -1;
    int newHeight = -1;
    for (int i=0; i<spy.count(); i++) {
        const QVariantList &values = spy[i];
        if (values[0].value<QByteArray>() == "width")
            newWidth = values[1].value<QVariant>().toInt();
        else if (values[0].value<QByteArray>() == "height")
            newHeight = values[1].value<QVariant>().toInt();

    }

    m_dbg->removeWatch(id, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), true);

    // since watch has been removed, restoring the original values should not trigger a valueChanged()
    spy.clear();
    m_rootItem->setProperty("width", origWidth);
    m_rootItem->setProperty("height", origHeight);
    QTest::qWait(100);
    QCOMPARE(spy.count(), 0);

    QCOMPARE(newWidth, origWidth * 2);
    QCOMPARE(newHeight, origHeight * 2);
}

void tst_QQmlEngineDebugService::watch_expression()
{
    QFETCH(QString, expr);
    QFETCH(int, increment);
    QFETCH(int, incrementCount);

    int origWidth = m_rootItem->property("width").toInt();

    QmlDebugObjectReference obj = findRootObject();

    bool success;

    QQmlEngineDebugClient *unconnected = new QQmlEngineDebugClient(0);
    unconnected->addWatch(obj, expr, &success);
    QVERIFY(!success);
    delete unconnected;

    m_dbg->addWatch(QmlDebugObjectReference(), expr, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), false);

    quint32 id = m_dbg->addWatch(obj, expr, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), true);

    QSignalSpy spy(m_dbg, SIGNAL(valueChanged(QByteArray,QVariant)));

    int width = origWidth;
    for (int i=0; i<incrementCount+1; i++) {
        if (i > 0) {
            width += increment;
            m_rootItem->setProperty("width", width);
            QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(valueChanged(QByteArray,QVariant))));
        }
    }

    m_dbg->removeWatch(id, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), true);

    // restore original value and verify spy doesn't get a signal since watch has been removed
    m_rootItem->setProperty("width", origWidth);
    QTest::qWait(100);
    QCOMPARE(spy.count(), incrementCount);

    width = origWidth + increment;
    for (int i=0; i<spy.count(); i++) {
        width += increment;
        QCOMPARE(spy.at(i).at(1).value<QVariant>().toInt(), width);
    }
}

void tst_QQmlEngineDebugService::watch_expression_data()
{
    QTest::addColumn<QString>("expr");
    QTest::addColumn<int>("increment");
    QTest::addColumn<int>("incrementCount");

    QTest::newRow("width") << "width" << 0 << 0;
    QTest::newRow("width+10") << "width + 10" << 10 << 5;
}

void tst_QQmlEngineDebugService::watch_context()
{
    QmlDebugContextReference c;
    QTest::ignoreMessage(QtWarningMsg, "QQmlEngineDebugClient::addWatch(): Not implemented");
    bool success;
    m_dbg->addWatch(c, QString(), &success);
    QVERIFY(!success);
}

void tst_QQmlEngineDebugService::watch_file()
{
    QmlDebugFileReference f;
    QTest::ignoreMessage(QtWarningMsg, "QQmlEngineDebugClient::addWatch(): Not implemented");
    bool success;
    m_dbg->addWatch(f, &success);
    QVERIFY(!success);
}

void tst_QQmlEngineDebugService::queryAvailableEngines()
{
    bool success;

    QQmlEngineDebugClient *unconnected = new QQmlEngineDebugClient(0);
    unconnected->queryAvailableEngines(&success);
    QVERIFY(!success);
    delete unconnected;

    m_dbg->queryAvailableEngines(&success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    // TODO test multiple engines
    QList<QmlDebugEngineReference> engines = m_dbg->engines();
    QCOMPARE(engines.count(), 1);

    foreach (const QmlDebugEngineReference &e, engines) {
        QCOMPARE(e.debugId, QQmlDebugService::idForObject(m_engine));
        QCOMPARE(e.name, m_engine->objectName());
    }
}

void tst_QQmlEngineDebugService::queryRootContexts()
{
    bool success;
    m_dbg->queryAvailableEngines(&success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QVERIFY(m_dbg->engines().count());
    int engineId =  m_dbg->engines()[0].debugId;

    QQmlEngineDebugClient *unconnected = new QQmlEngineDebugClient(0);
    unconnected->queryRootContexts(engineId, &success);
    QVERIFY(!success);
    delete unconnected;

    m_dbg->queryRootContexts(engineId, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    QQmlContext *actualContext = m_engine->rootContext();
    QmlDebugContextReference context = m_dbg->rootContext();
    QCOMPARE(context.debugId, QQmlDebugService::idForObject(actualContext));
    QCOMPARE(context.name, actualContext->objectName());

    // root context query sends only root object data - it doesn't fill in
    // the children or property info
    QCOMPARE(context.objects.count(), 0);
    QCOMPARE(context.contexts.count(), 6);
    QVERIFY(context.contexts[0].debugId >= 0);
    QCOMPARE(context.contexts[0].name, QString("tst_QQmlDebug_childContext"));
}

void tst_QQmlEngineDebugService::queryObject()
{
    QFETCH(bool, recursive);

    bool success;

    QmlDebugObjectReference rootObject = findRootObject();

    QQmlEngineDebugClient *unconnected = new QQmlEngineDebugClient(0);
    recursive ? unconnected->queryObjectRecursive(rootObject, &success) : unconnected->queryObject(rootObject, &success);
    QVERIFY(!success);
    delete unconnected;

    recursive ? m_dbg->queryObjectRecursive(rootObject, &success) : m_dbg->queryObject(rootObject, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    QmlDebugObjectReference obj = m_dbg->object();

    // check source as defined in main()
    QmlDebugFileReference source = obj.source;
    QCOMPARE(source.url, QUrl::fromLocalFile(""));
    QCOMPARE(source.lineNumber, 3);
    QCOMPARE(source.columnNumber, 1);

    // generically test all properties, children and childrens' properties
    recursiveObjectTest(m_rootItem, obj, recursive);

    if (recursive) {
        foreach (const QmlDebugObjectReference &child, obj.children)
            QVERIFY(child.properties.count() > 0);

        QmlDebugObjectReference rect;
        QmlDebugObjectReference text;
        foreach (const QmlDebugObjectReference &child, obj.children) {
            if (child.className == "Rectangle")
                rect = child;
            else if (child.className == "Text")
                text = child;
        }

        // test specific property values
        QCOMPARE(findProperty(rect.properties, "width").value, qVariantFromValue(500));
        QCOMPARE(findProperty(rect.properties, "height").value, qVariantFromValue(600));
        QCOMPARE(findProperty(rect.properties, "color").value, qVariantFromValue(QColor("blue")));

        QCOMPARE(findProperty(text.properties, "color").value, qVariantFromValue(QColor("blue")));
    } else {
        foreach (const QmlDebugObjectReference &child, obj.children)
            QCOMPARE(child.properties.count(), 0);
    }
}

void tst_QQmlEngineDebugService::queryObject_data()
{
    QTest::addColumn<bool>("recursive");

    QTest::newRow("non-recursive") << false;
    QTest::newRow("recursive") << true;
}

void tst_QQmlEngineDebugService::queryObjectsForLocation()
{
    QFETCH(bool, recursive);

    bool success;

    QmlDebugObjectReference rootObject = findRootObject();

    const QString fileName = QFileInfo(rootObject.source.url.toString()).fileName();
    int lineNumber = rootObject.source.lineNumber;
    int columnNumber = rootObject.source.columnNumber;

    QQmlEngineDebugClient *unconnected = new QQmlEngineDebugClient(0);
    recursive ? unconnected->queryObjectsForLocationRecursive(fileName, lineNumber,
                                                              columnNumber, &success)
              : unconnected->queryObjectsForLocation(fileName, lineNumber,
                                                     columnNumber, &success);
    QVERIFY(!success);
    delete unconnected;

    recursive ? m_dbg->queryObjectsForLocationRecursive(fileName, lineNumber,
                                                      columnNumber, &success)
              : m_dbg->queryObjectsForLocation(fileName, lineNumber,
                                             columnNumber, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    QCOMPARE(m_dbg->objects().count(), 1);
    QmlDebugObjectReference obj = m_dbg->objects().first();

    // check source as defined in main()
    QmlDebugFileReference source = obj.source;
    QCOMPARE(source.url, QUrl(fileName));
    QCOMPARE(source.lineNumber, lineNumber);
    QCOMPARE(source.columnNumber, columnNumber);

    // generically test all properties, children and childrens' properties
    recursiveObjectTest(m_rootItem, obj, recursive);

    if (recursive) {
        foreach (const QmlDebugObjectReference &child, obj.children)
            QVERIFY(child.properties.count() > 0);

        QmlDebugObjectReference rect;
        QmlDebugObjectReference text;
        foreach (const QmlDebugObjectReference &child, obj.children) {
            if (child.className == "Rectangle")
                rect = child;
            else if (child.className == "Text")
                text = child;
        }

        // test specific property values
        QCOMPARE(findProperty(rect.properties, "width").value, qVariantFromValue(500));
        QCOMPARE(findProperty(rect.properties, "height").value, qVariantFromValue(600));
        QCOMPARE(findProperty(rect.properties, "color").value, qVariantFromValue(QColor("blue")));

        QCOMPARE(findProperty(text.properties, "color").value, qVariantFromValue(QColor("blue")));
    } else {
        foreach (const QmlDebugObjectReference &child, obj.children)
            QCOMPARE(child.properties.count(), 0);
    }
}

void tst_QQmlEngineDebugService::queryObjectsForLocation_data()
{
    QTest::addColumn<bool>("recursive");

    QTest::newRow("non-recursive") << false;
    QTest::newRow("recursive") << true;
}

void tst_QQmlEngineDebugService::regression_QTCREATORBUG_7451()
{
    QmlDebugObjectReference rootObject = findRootObject();
    int contextId = rootObject.contextDebugId;
    QQmlContext *context = qobject_cast<QQmlContext *>(QQmlDebugService::objectForId(contextId));
    QQmlComponent component(context->engine());
    QByteArray content;
    content.append("import QtQuick 2.0\n"
               "Text {"
               "y: 10\n"
               "text: \"test\"\n"
               "}");
    component.setData(content, rootObject.source.url);
    QObject *object = component.create(context);
    QVERIFY(object);
    int idNew = QQmlDebugService::idForObject(object);
    QVERIFY(idNew >= 0);

    const QString fileName = QFileInfo(rootObject.source.url.toString()).fileName();
    int lineNumber = rootObject.source.lineNumber;
    int columnNumber = rootObject.source.columnNumber;
    bool success = false;

    m_dbg->queryObjectsForLocation(fileName, lineNumber,
                                        columnNumber, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    foreach (QmlDebugObjectReference child, rootObject.children) {
        success = false;
        lineNumber = child.source.lineNumber;
        columnNumber = child.source.columnNumber;
        m_dbg->queryObjectsForLocation(fileName, lineNumber,
                                       columnNumber, &success);
        QVERIFY(success);
        QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    }

    delete object;
    QObject *deleted = QQmlDebugService::objectForId(idNew);
    QVERIFY(!deleted);

    lineNumber = rootObject.source.lineNumber;
    columnNumber = rootObject.source.columnNumber;
    success = false;
    m_dbg->queryObjectsForLocation(fileName, lineNumber,
                                   columnNumber, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    foreach (QmlDebugObjectReference child, rootObject.children) {
        success = false;
        lineNumber = child.source.lineNumber;
        columnNumber = child.source.columnNumber;
        m_dbg->queryObjectsForLocation(fileName, lineNumber,
                                       columnNumber, &success);
        QVERIFY(success);
        QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    }
}

void tst_QQmlEngineDebugService::queryObjectWithNonStreamableTypes()
{
    bool success;

    QmlDebugObjectReference rootObject = findRootObject(4, true);

    QQmlEngineDebugClient *unconnected = new QQmlEngineDebugClient(0);
    unconnected->queryObject(rootObject, &success);
    QVERIFY(!success);
    delete unconnected;

    m_dbg->queryObject(rootObject, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    QmlDebugObjectReference obj = m_dbg->object();

    QCOMPARE(findProperty(obj.properties, "modelIndex").value, QVariant());
}


void tst_QQmlEngineDebugService::queryExpressionResult()
{
    QFETCH(QString, expr);
    QFETCH(QVariant, result);

    int objectId = findRootObject().debugId;

    bool success;

    QQmlEngineDebugClient *unconnected = new QQmlEngineDebugClient(0);
    unconnected->queryExpressionResult(objectId, expr, &success);
    QVERIFY(!success);
    delete unconnected;

    m_dbg->queryExpressionResult(objectId, expr, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    QCOMPARE(m_dbg->resultExpr(), result);
}

void tst_QQmlEngineDebugService::queryExpressionResult_data()
{
    QTest::addColumn<QString>("expr");
    QTest::addColumn<QVariant>("result");

    QTest::newRow("width + 50") << "width + 50" << qVariantFromValue(60);
    QTest::newRow("blueRect.width") << "blueRect.width" << qVariantFromValue(500);
    QTest::newRow("bad expr") << "aeaef" << qVariantFromValue(QString("<undefined>"));
    QTest::newRow("QObject*") << "varObj" << qVariantFromValue(QString("<unnamed object>"));
    QTest::newRow("list of QObject*") << "varObjList" << qVariantFromValue(QVariantList() << QVariant(QString("<unnamed object>")));
    QVariantMap map;
    map.insert(QLatin1String("rect"), QVariant(QLatin1String("<unnamed object>")));
    QTest::newRow("varObjMap") << "varObjMap" << qVariantFromValue(map);
    QTest::newRow("simpleVar") << "simpleVar" << qVariantFromValue(10.05);
}

void tst_QQmlEngineDebugService::queryExpressionResultInRootContext()
{
    bool success;
    const QString exp = QLatin1String("1");
    m_dbg->queryExpressionResult(-1, exp, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    QCOMPARE(m_dbg->resultExpr().toString(), exp);
}

void tst_QQmlEngineDebugService::queryExpressionResultBC()
{
    QFETCH(QString, expr);
    QFETCH(QVariant, result);

    int objectId = findRootObject().debugId;

    bool success;

    QQmlEngineDebugClient *unconnected = new QQmlEngineDebugClient(0);
    unconnected->queryExpressionResultBC(objectId, expr, &success);
    QVERIFY(!success);
    delete unconnected;

    m_dbg->queryExpressionResultBC(objectId, expr, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    QCOMPARE(m_dbg->resultExpr(), result);
}

void tst_QQmlEngineDebugService::queryExpressionResultBC_data()
{
    QTest::addColumn<QString>("expr");
    QTest::addColumn<QVariant>("result");

    QTest::newRow("width + 50") << "width + 50" << qVariantFromValue(60);
    QTest::newRow("blueRect.width") << "blueRect.width" << qVariantFromValue(500);
    QTest::newRow("bad expr") << "aeaef" << qVariantFromValue(QString("<undefined>"));
    QTest::newRow("QObject*") << "varObj" << qVariantFromValue(QString("<unnamed object>"));
    QTest::newRow("list of QObject*") << "varObjList" << qVariantFromValue(QVariantList() << QVariant(QString("<unnamed object>")));
    QVariantMap map;
    map.insert(QLatin1String("rect"), QVariant(QLatin1String("<unnamed object>")));
    QTest::newRow("varObjMap") << "varObjMap" << qVariantFromValue(map);
    QTest::newRow("simpleVar") << "simpleVar" << qVariantFromValue(10.05);
}

void tst_QQmlEngineDebugService::setBindingForObject()
{
    QmlDebugObjectReference rootObject = findRootObject();
    QVERIFY(rootObject.debugId != -1);
    QmlDebugPropertyReference widthPropertyRef = findProperty(rootObject.properties, "width");

    QCOMPARE(widthPropertyRef.value, QVariant(10));
    QCOMPARE(widthPropertyRef.binding, QString());

    bool success;
    //
    // set literal
    //
    m_dbg->setBindingForObject(rootObject.debugId, "width", "15", true,
                               QString(), -1, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), true);

    rootObject = findRootObject();
    widthPropertyRef =  findProperty(rootObject.properties, "width");

    QCOMPARE(widthPropertyRef.value, QVariant(15));
    QCOMPARE(widthPropertyRef.binding, QString());

    //
    // set expression
    //
    m_dbg->setBindingForObject(rootObject.debugId, "width", "height", false,
                               QString(), -1, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), true);

    rootObject = findRootObject();
    widthPropertyRef =  findProperty(rootObject.properties, "width");

    QCOMPARE(widthPropertyRef.value, QVariant(20));
    QEXPECT_FAIL("", "Cannot retrieve text for a binding (QTBUG-37273)", Continue);
    QCOMPARE(widthPropertyRef.binding, QString("height"));

    //
    // set handler
    //
    rootObject = findRootObject();
    QCOMPARE(rootObject.children.size(), 5); // Rectangle, Text, MouseArea, Component.onCompleted, NonScriptPropertyElement
    QmlDebugObjectReference mouseAreaObject = rootObject.children.at(2);
    m_dbg->queryObjectRecursive(mouseAreaObject, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    mouseAreaObject = m_dbg->object();

    QCOMPARE(mouseAreaObject.className, QString("MouseArea"));

    QmlDebugPropertyReference onEnteredRef = findProperty(mouseAreaObject.properties, "onEntered");

    QCOMPARE(onEnteredRef.name, QString("onEntered"));
    // Sorry, can't do that anymore: QCOMPARE(onEnteredRef.value,  QVariant("{ console.log('hello') }"));
    QCOMPARE(onEnteredRef.value,  QVariant("function() { [code] }"));

    m_dbg->setBindingForObject(mouseAreaObject.debugId, "onEntered",
                               "{console.log('hello, world') }", false,
                               QString(), -1, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), true);

    rootObject = findRootObject();
    mouseAreaObject = rootObject.children.at(2);
    m_dbg->queryObjectRecursive(mouseAreaObject, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    mouseAreaObject = m_dbg->object();
    onEnteredRef = findProperty(mouseAreaObject.properties, "onEntered");
    QCOMPARE(onEnteredRef.name, QString("onEntered"));
    QCOMPARE(onEnteredRef.value, QVariant("function() { [code] }"));
}

void tst_QQmlEngineDebugService::resetBindingForObject()
{
    QmlDebugObjectReference rootObject = findRootObject();
    QVERIFY(rootObject.debugId != -1);
    QmlDebugPropertyReference widthPropertyRef = findProperty(rootObject.properties, "width");

    bool success = false;

    m_dbg->setBindingForObject(rootObject.debugId, "width", "15", true,
                               QString(), -1, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), true);

    //
    // reset
    //
    m_dbg->resetBindingForObject(rootObject.debugId, "width", &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), true);

    rootObject = findRootObject();
    widthPropertyRef =  findProperty(rootObject.properties, "width");

    QCOMPARE(widthPropertyRef.value, QVariant(0));
    QCOMPARE(widthPropertyRef.binding, QString());

    //
    // reset nested property
    //
    success = false;
    m_dbg->resetBindingForObject(rootObject.debugId, "font.bold", &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), true);

    rootObject = findRootObject();
    QmlDebugPropertyReference boldPropertyRef =  findProperty(rootObject.properties, "font.bold");

    QCOMPARE(boldPropertyRef.value.toBool(), false);
    QCOMPARE(boldPropertyRef.binding, QString());
}

void tst_QQmlEngineDebugService::setBindingInStates()
{
    // Check if changing bindings of propertychanges works

    const int sourceIndex = 3;

    QmlDebugObjectReference obj = findRootObject(sourceIndex);

    QVERIFY(obj.debugId != -1);
    QVERIFY(obj.children.count() >= 2);
    bool success;
    // We are going to switch state a couple of times, we need to get rid of the transition before
    m_dbg->queryExpressionResult(obj.debugId,QString("transitions = []"), &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));


    // check initial value of the property that is changing
    m_dbg->queryExpressionResult(obj.debugId,QString("state=\"state1\""), &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties,"width").value.toInt(),200);


    m_dbg->queryExpressionResult(obj.debugId,QString("state=\"\""),
                                              &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));


    obj = findRootObject(sourceIndex, true);
    QCOMPARE(findProperty(obj.properties,"width").value.toInt(),100);


    // change the binding
    QmlDebugObjectReference state = obj.children[1];
    QCOMPARE(state.className, QString("State"));
    QVERIFY(state.children.count() > 0);

    QmlDebugObjectReference propertyChange = state.children[0];
    QVERIFY(propertyChange.debugId != -1);

    m_dbg->setBindingForObject(propertyChange.debugId, "width",QVariant(300),true,
                               QString(), -1, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    // check properties changed in state
    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties,"width").value.toInt(),100);


    m_dbg->queryExpressionResult(obj.debugId,QString("state=\"state1\""), &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties,"width").value.toInt(),300);

    // check changing properties of base state from within a state
    m_dbg->setBindingForObject(obj.debugId,"width","height*2",false,
                               QString(), -1, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    m_dbg->setBindingForObject(obj.debugId,"height","200",true,
                               QString(), -1, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties,"width").value.toInt(),300);

    m_dbg->queryExpressionResult(obj.debugId,QString("state=\"\""), &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties,"width").value.toInt(), 400);

    //  reset binding while in a state
    m_dbg->queryExpressionResult(obj.debugId,QString("state=\"state1\""), &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));

    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties,"width").value.toInt(), 300);

    m_dbg->resetBindingForObject(propertyChange.debugId, "width", &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), true);

    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties,"width").value.toInt(), 400);

    // re-add binding
    m_dbg->setBindingForObject(propertyChange.debugId, "width", "300", true,
                               QString(), -1, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_dbg, SIGNAL(result())));
    QCOMPARE(m_dbg->valid(), true);

    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties,"width").value.toInt(), 300);
}

void tst_QQmlEngineDebugService::queryObjectTree()
{
    const int sourceIndex = 3;

    QmlDebugObjectReference obj = findRootObject(sourceIndex, true);

    QVERIFY(obj.debugId != -1);
    QVERIFY(obj.children.count() >= 2);

    // check state
    QmlDebugObjectReference state = obj.children[1];
    QCOMPARE(state.className, QString("State"));
    QVERIFY(state.children.count() > 0);

    QmlDebugObjectReference propertyChange = state.children[0];
    QVERIFY(propertyChange.debugId != -1);

    QmlDebugPropertyReference propertyChangeTarget = findProperty(propertyChange.properties,"target");
    QCOMPARE(propertyChangeTarget.objectDebugId, propertyChange.debugId);

    QmlDebugObjectReference targetReference = qvariant_cast<QmlDebugObjectReference>(propertyChangeTarget.value);
    QVERIFY(targetReference.debugId != -1);



    // check transition
    QmlDebugObjectReference transition = obj.children[0];
    QCOMPARE(transition.className, QString("Transition"));
    QCOMPARE(findProperty(transition.properties,"from").value.toString(), QString("*"));
    QCOMPARE(findProperty(transition.properties,"to").value, findProperty(state.properties,"name").value);
    QVERIFY(transition.children.count() > 0);

    QmlDebugObjectReference animation = transition.children[0];
    QVERIFY(animation.debugId != -1);

    QmlDebugPropertyReference animationTarget = findProperty(animation.properties,"target");
    QCOMPARE(animationTarget.objectDebugId, animation.debugId);

    targetReference = qvariant_cast<QmlDebugObjectReference>(animationTarget.value);
    QVERIFY(targetReference.debugId != -1);

    QCOMPARE(findProperty(animation.properties,"property").value.toString(), QString("width"));
    QCOMPARE(findProperty(animation.properties,"duration").value.toInt(), 100);
}

int main(int argc, char *argv[])
{
    int _argc = argc + 1;
    QScopedArrayPointer<char *>_argv(new char*[_argc]);
    for (int i = 0; i < argc; ++i)
        _argv[i] = argv[i];
    char arg[] = "-qmljsdebugger=port:3768,services:QmlDebugger";
    _argv[_argc - 1] = arg;

    QGuiApplication app(_argc, _argv.data());
    tst_QQmlEngineDebugService tc;
    return QTest::qExec(&tc, _argc, _argv.data());
}

#include "tst_qqmlenginedebugservice.moc"
