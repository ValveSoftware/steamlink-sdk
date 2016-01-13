/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
#include <qtest.h>
#include <QSignalSpy>
#include <QTimer>
#include <QHostAddress>
#include <QDebug>
#include <QThread>

#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecontext.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <QtDeclarative/qdeclarativeexpression.h>
#include <QtDeclarative/qdeclarativeproperty.h>

#include <private/qdeclarativebinding_p.h>
#include <private/qdeclarativeenginedebug_p.h>
#include <private/qdeclarativedebugclient_p.h>
#include <private/qdeclarativedebugservice_p.h>
#include <private/qdeclarativerectangle_p.h>
#include <private/qdeclarativemetatype_p.h>
#include <private/qdeclarativeproperty_p.h>

#include "../shared/debugutil_p.h"

Q_DECLARE_METATYPE(QDeclarativeDebugWatch::State)

class tst_QDeclarativeDebug : public QObject
{
    Q_OBJECT

private:
    QDeclarativeDebugObjectReference findRootObject(int context = 0, bool recursive = false);
    QDeclarativeDebugPropertyReference findProperty(const QList<QDeclarativeDebugPropertyReference> &props, const QString &name) const;
    void waitForQuery(QDeclarativeDebugQuery *query);

    void recursiveObjectTest(QObject *o, const QDeclarativeDebugObjectReference &oref, bool recursive) const;

    void recursiveCompareObjects(const QDeclarativeDebugObjectReference &a, const QDeclarativeDebugObjectReference &b) const;
    void recursiveCompareContexts(const QDeclarativeDebugContextReference &a, const QDeclarativeDebugContextReference &b) const;
    void compareProperties(const QDeclarativeDebugPropertyReference &a, const QDeclarativeDebugPropertyReference &b) const;

    QDeclarativeDebugConnection *m_conn;
    QDeclarativeEngineDebug *m_dbg;
    QDeclarativeEngine *m_engine;
    QDeclarativeItem *m_rootItem;

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
    void queryExpressionResult();
    void queryExpressionResult_data();

    void tst_QDeclarativeDebugFileReference();
    void tst_QDeclarativeDebugEngineReference();
    void tst_QDeclarativeDebugObjectReference();
    void tst_QDeclarativeDebugContextReference();
    void tst_QDeclarativeDebugPropertyReference();

    void setBindingForObject();
    void setMethodBody();
    void queryObjectTree();
    void setBindingInStates();
};

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


QDeclarativeDebugObjectReference tst_QDeclarativeDebug::findRootObject(int context, bool recursive)
{
    QDeclarativeDebugEnginesQuery *q_engines = m_dbg->queryAvailableEngines(this);
    waitForQuery(q_engines);

    if (q_engines->engines().count() == 0)
        return QDeclarativeDebugObjectReference();
    QDeclarativeDebugRootContextQuery *q_context = m_dbg->queryRootContexts(q_engines->engines()[0].debugId(), this);
    waitForQuery(q_context);

    if (q_context->rootContext().objects().count() == 0)
        return QDeclarativeDebugObjectReference();
    QDeclarativeDebugObjectQuery *q_obj = recursive ?
                m_dbg->queryObjectRecursive(q_context->rootContext().objects()[context], this) :
                m_dbg->queryObject(q_context->rootContext().objects()[context], this);
    waitForQuery(q_obj);

    QDeclarativeDebugObjectReference result = q_obj->object();

    delete q_engines;
    delete q_context;
    delete q_obj;

    return result;
}

QDeclarativeDebugPropertyReference tst_QDeclarativeDebug::findProperty(const QList<QDeclarativeDebugPropertyReference> &props, const QString &name) const
{
    foreach(const QDeclarativeDebugPropertyReference &p, props) {
        if (p.name() == name)
            return p;
    }
    return QDeclarativeDebugPropertyReference();
}

void tst_QDeclarativeDebug::waitForQuery(QDeclarativeDebugQuery *query)
{
    QVERIFY(query);
    QCOMPARE(query->parent(), qobject_cast<QObject*>(this));
    QVERIFY(query->state() == QDeclarativeDebugQuery::Waiting);
    if (!QDeclarativeDebugTest::waitForSignal(query, SIGNAL(stateChanged(QDeclarativeDebugQuery::State))))
        QFAIL("query timed out");
}

void tst_QDeclarativeDebug::recursiveObjectTest(QObject *o, const QDeclarativeDebugObjectReference &oref, bool recursive) const
{
    const QMetaObject *meta = o->metaObject();

    QDeclarativeType *type = QDeclarativeMetaType::qmlType(meta);
    QString className = type ? QString(type->qmlTypeName()) : QString(meta->className());
    className = className.mid(className.lastIndexOf(QLatin1Char('/'))+1);

    QCOMPARE(oref.debugId(), QDeclarativeDebugService::idForObject(o));
    QCOMPARE(oref.name(), o->objectName());
    QCOMPARE(oref.className(), className);
    QCOMPARE(oref.contextDebugId(), QDeclarativeDebugService::idForObject(qmlContext(o)));

    const QObjectList &children = o->children();
    for (int i=0; i<children.count(); i++) {
        QObject *child = children[i];
        if (!qmlContext(child))
            continue;
        int debugId = QDeclarativeDebugService::idForObject(child);
        QVERIFY(debugId >= 0);

        QDeclarativeDebugObjectReference cref;
        foreach (const QDeclarativeDebugObjectReference &ref, oref.children()) {
            if (ref.debugId() == debugId) {
                cref = ref;
                break;
            }
        }
        QVERIFY(cref.debugId() >= 0);

        if (recursive)
            recursiveObjectTest(child, cref, true);
    }

    foreach (const QDeclarativeDebugPropertyReference &p, oref.properties()) {
        QCOMPARE(p.objectDebugId(), QDeclarativeDebugService::idForObject(o));

        // signal properties are fake - they are generated from QDeclarativeBoundSignal children
        if (p.name().startsWith("on") && p.name().length() > 2 && p.name()[2].isUpper()) {
            QVERIFY(p.value().toString().startsWith('{') && p.value().toString().endsWith('}'));
            QVERIFY(p.valueTypeName().isEmpty());
            QVERIFY(p.binding().isEmpty());
            QVERIFY(!p.hasNotifySignal());
            continue;
        }

        QMetaProperty pmeta = meta->property(meta->indexOfProperty(p.name().toUtf8().constData()));

        QCOMPARE(p.name(), QString::fromUtf8(pmeta.name()));

        if (pmeta.type() > 0 && pmeta.type() < QVariant::UserType && pmeta.userType() != QMetaType::QVariant) // TODO test complex types
            QCOMPARE(p.value(), pmeta.read(o));

        if (p.name() == "parent")
            QVERIFY(p.valueTypeName() == "QGraphicsObject*" || p.valueTypeName() == "QDeclarativeItem*");
        else
            QCOMPARE(p.valueTypeName(), QString::fromUtf8(pmeta.typeName()));

        QDeclarativeAbstractBinding *binding =
            QDeclarativePropertyPrivate::binding(QDeclarativeProperty(o, p.name()));
        if (binding)
            QCOMPARE(binding->expression(), p.binding());

        QCOMPARE(p.hasNotifySignal(), pmeta.hasNotifySignal());

        QVERIFY(pmeta.isValid());
    }
}

void tst_QDeclarativeDebug::recursiveCompareObjects(const QDeclarativeDebugObjectReference &a, const QDeclarativeDebugObjectReference &b) const
{
    QCOMPARE(a.debugId(), b.debugId());
    QCOMPARE(a.className(), b.className());
    QCOMPARE(a.name(), b.name());
    QCOMPARE(a.contextDebugId(), b.contextDebugId());

    QCOMPARE(a.source().url(), b.source().url());
    QCOMPARE(a.source().lineNumber(), b.source().lineNumber());
    QCOMPARE(a.source().columnNumber(), b.source().columnNumber());

    QCOMPARE(a.properties().count(), b.properties().count());
    QCOMPARE(a.children().count(), b.children().count());

    QList<QDeclarativeDebugPropertyReference> aprops = a.properties();
    QList<QDeclarativeDebugPropertyReference> bprops = b.properties();

    for (int i=0; i<aprops.count(); i++)
        compareProperties(aprops[i], bprops[i]);

    for (int i=0; i<a.children().count(); i++)
        recursiveCompareObjects(a.children()[i], b.children()[i]);
}

void tst_QDeclarativeDebug::recursiveCompareContexts(const QDeclarativeDebugContextReference &a, const QDeclarativeDebugContextReference &b) const
{
    QCOMPARE(a.debugId(), b.debugId());
    QCOMPARE(a.name(), b.name());
    QCOMPARE(a.objects().count(), b.objects().count());
    QCOMPARE(a.contexts().count(), b.contexts().count());

    for (int i=0; i<a.objects().count(); i++)
        recursiveCompareObjects(a.objects()[i], b.objects()[i]);

    for (int i=0; i<a.contexts().count(); i++)
        recursiveCompareContexts(a.contexts()[i], b.contexts()[i]);
}

void tst_QDeclarativeDebug::compareProperties(const QDeclarativeDebugPropertyReference &a, const QDeclarativeDebugPropertyReference &b) const
{
    QCOMPARE(a.objectDebugId(), b.objectDebugId());
    QCOMPARE(a.name(), b.name());
    QCOMPARE(a.value(), b.value());
    QCOMPARE(a.valueTypeName(), b.valueTypeName());
    QCOMPARE(a.binding(), b.binding());
    QCOMPARE(a.hasNotifySignal(), b.hasNotifySignal());
}

void tst_QDeclarativeDebug::initTestCase()
{
    qRegisterMetaType<QDeclarativeDebugWatch::State>();
    qmlRegisterType<NonScriptProperty>("Test", 1, 0, "NonScriptPropertyElement");

    QTest::ignoreMessage(QtDebugMsg, "QDeclarativeDebugServer: Waiting for connection on port 3768...");
    m_engine = new QDeclarativeEngine(this);

    QList<QByteArray> qml;
    qml << "import QtQuick 1.0\n"
           "import Test 1.0\n"
           "Item {"
                "id: root\n"
                "width: 10; height: 20; scale: blueRect.scale;"
                "Rectangle { id: blueRect; width: 500; height: 600; color: \"blue\"; }"
                "Text { color: blueRect.color; }"
                "MouseArea {"
                    "onEntered: { console.log('hello') }"
                "}"
                "property variant varObj\n"
                "property variant varObjList: []\n"
                "Component.onCompleted: {\n"
                    "varObj = blueRect;\n"
                    "var list = varObjList;\n"
                    "list[0] = blueRect;\n"
                    "varObjList = list;\n"
                "}\n"
                "NonScriptPropertyElement {\n"
                "}\n"
            "}";

    // add second component to test multiple root contexts
    qml << "import QtQuick 1.0\n"
            "Item {}";

    // and a third to test methods
    qml << "import QtQuick 1.0\n"
            "Item {"
                "function myMethodNoArgs() { return 3; }\n"
                "function myMethod(a) { return a + 9; }\n"
                "function myMethodIndirect() { myMethod(3); }\n"
            "}";

    // and a fourth to test states
    qml << "import QtQuick 1.0\n"
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

    for (int i=0; i<qml.count(); i++) {
        QDeclarativeComponent component(m_engine);
        component.setData(qml[i], QUrl::fromLocalFile(""));
        QVERIFY(component.isReady());  // fails if bad syntax
        m_components << qobject_cast<QDeclarativeItem*>(component.create());
    }
    m_rootItem = qobject_cast<QDeclarativeItem*>(m_components.first());

    // add an extra context to test for multiple contexts
    QDeclarativeContext *context = new QDeclarativeContext(m_engine->rootContext(), this);
    context->setObjectName("tst_QDeclarativeDebug_childContext");

    m_conn = new QDeclarativeDebugConnection(this);
    m_conn->connectToHost("127.0.0.1", 3768);

    QTest::ignoreMessage(QtDebugMsg, "QDeclarativeDebugServer: Connection established");
    bool ok = m_conn->waitForConnected();
    QVERIFY(ok);
    QTRY_VERIFY(QDeclarativeDebugService::hasDebuggingClient());
    m_dbg = new QDeclarativeEngineDebug(m_conn, this);
    QTRY_VERIFY(m_dbg->status() == QDeclarativeEngineDebug::Enabled);
}

void tst_QDeclarativeDebug::cleanupTestCase()
{
    delete m_dbg;
    delete m_conn;
    qDeleteAll(m_components);
    delete m_engine;
}

void tst_QDeclarativeDebug::setMethodBody()
{
    QDeclarativeDebugObjectReference obj = findRootObject(2);

    QObject *root = m_components.at(2);
    // Without args
    {
    QVariant rv;
    QVERIFY(QMetaObject::invokeMethod(root, "myMethodNoArgs", Qt::DirectConnection,
                                      Q_RETURN_ARG(QVariant, rv)));
    QVERIFY(rv == QVariant(qreal(3)));


    QVERIFY(m_dbg->setMethodBody(obj.debugId(), "myMethodNoArgs", "return 7"));
    QTest::qWait(100);

    QVERIFY(QMetaObject::invokeMethod(root, "myMethodNoArgs", Qt::DirectConnection,
                                      Q_RETURN_ARG(QVariant, rv)));
    QVERIFY(rv == QVariant(qreal(7)));
    }

    // With args
    {
    QVariant rv;
    QVERIFY(QMetaObject::invokeMethod(root, "myMethod", Qt::DirectConnection,
                                      Q_RETURN_ARG(QVariant, rv), Q_ARG(QVariant, QVariant(19))));
    QVERIFY(rv == QVariant(qreal(28)));

    QVERIFY(m_dbg->setMethodBody(obj.debugId(), "myMethod", "return a + 7"));
    QTest::qWait(100);

    QVERIFY(QMetaObject::invokeMethod(root, "myMethod", Qt::DirectConnection,
                                      Q_RETURN_ARG(QVariant, rv), Q_ARG(QVariant, QVariant(19))));
    QVERIFY(rv == QVariant(qreal(26)));
    }
}

void tst_QDeclarativeDebug::watch_property()
{
    QDeclarativeDebugObjectReference obj = findRootObject();
    QDeclarativeDebugPropertyReference prop = findProperty(obj.properties(), "width");

    QDeclarativeDebugPropertyWatch *watch;

    QDeclarativeEngineDebug *unconnected = new QDeclarativeEngineDebug(0);
    watch = unconnected->addWatch(prop, this);
    QCOMPARE(watch->state(), QDeclarativeDebugWatch::Dead);
    delete watch;
    delete unconnected;

    watch = m_dbg->addWatch(QDeclarativeDebugPropertyReference(), this);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(watch, SIGNAL(stateChanged(QDeclarativeDebugWatch::State))));
    QCOMPARE(watch->state(), QDeclarativeDebugWatch::Inactive);
    delete watch;

    watch = m_dbg->addWatch(prop, this);
    QCOMPARE(watch->state(), QDeclarativeDebugWatch::Waiting);
    QCOMPARE(watch->objectDebugId(), obj.debugId());
    QCOMPARE(watch->name(), prop.name());

    QSignalSpy spy(watch, SIGNAL(valueChanged(QByteArray,QVariant)));

    int origWidth = m_rootItem->property("width").toInt();
    m_rootItem->setProperty("width", origWidth*2);

    // stateChanged() is received before valueChanged()
    QVERIFY(QDeclarativeDebugTest::waitForSignal(watch, SIGNAL(stateChanged(QDeclarativeDebugWatch::State))));
    QCOMPARE(watch->state(), QDeclarativeDebugWatch::Active);
    QCOMPARE(spy.count(), 1);

    m_dbg->removeWatch(watch);
    delete watch;

    // restore original value and verify spy doesn't get additional signal since watch has been removed
    m_rootItem->setProperty("width", origWidth);
    QTest::qWait(100);
    QCOMPARE(spy.count(), 1);

    QCOMPARE(spy.at(0).at(0).value<QByteArray>(), prop.name().toUtf8());
    QCOMPARE(spy.at(0).at(1).value<QVariant>(), qVariantFromValue(origWidth*2));
}

void tst_QDeclarativeDebug::watch_object()
{
    QDeclarativeDebugEnginesQuery *q_engines = m_dbg->queryAvailableEngines(this);
    waitForQuery(q_engines);

    QVERIFY(q_engines->engines().count() > 0);
    QDeclarativeDebugRootContextQuery *q_context = m_dbg->queryRootContexts(q_engines->engines()[0].debugId(), this);
    waitForQuery(q_context);

    QVERIFY(q_context->rootContext().objects().count() > 0);
    QDeclarativeDebugObjectQuery *q_obj = m_dbg->queryObject(q_context->rootContext().objects()[0], this);
    waitForQuery(q_obj);

    QDeclarativeDebugObjectReference obj = q_obj->object();

    delete q_engines;
    delete q_context;
    delete q_obj;

    QDeclarativeDebugWatch *watch;

    QDeclarativeEngineDebug *unconnected = new QDeclarativeEngineDebug(0);
    watch = unconnected->addWatch(obj, this);
    QCOMPARE(watch->state(), QDeclarativeDebugWatch::Dead);
    delete watch;
    delete unconnected;

    watch = m_dbg->addWatch(QDeclarativeDebugObjectReference(), this);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(watch, SIGNAL(stateChanged(QDeclarativeDebugWatch::State))));
    QCOMPARE(watch->state(), QDeclarativeDebugWatch::Inactive);
    delete watch;

    watch = m_dbg->addWatch(obj, this);
    QCOMPARE(watch->state(), QDeclarativeDebugWatch::Waiting);
    QCOMPARE(watch->objectDebugId(), obj.debugId());

    QSignalSpy spy(watch, SIGNAL(valueChanged(QByteArray,QVariant)));

    int origWidth = m_rootItem->property("width").toInt();
    int origHeight = m_rootItem->property("height").toInt();
    m_rootItem->setProperty("width", origWidth*2);
    m_rootItem->setProperty("height", origHeight*2);

    // stateChanged() is received before any valueChanged() signals
    QVERIFY(QDeclarativeDebugTest::waitForSignal(watch, SIGNAL(stateChanged(QDeclarativeDebugWatch::State))));
    QCOMPARE(watch->state(), QDeclarativeDebugWatch::Active);
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

    m_dbg->removeWatch(watch);
    delete watch;

    // since watch has been removed, restoring the original values should not trigger a valueChanged()
    spy.clear();
    m_rootItem->setProperty("width", origWidth);
    m_rootItem->setProperty("height", origHeight);
    QTest::qWait(100);
    QCOMPARE(spy.count(), 0);

    QCOMPARE(newWidth, origWidth * 2);
    QCOMPARE(newHeight, origHeight * 2);
}

void tst_QDeclarativeDebug::watch_expression()
{
    QFETCH(QString, expr);
    QFETCH(int, increment);
    QFETCH(int, incrementCount);

    int origWidth = m_rootItem->property("width").toInt();

    QDeclarativeDebugObjectReference obj = findRootObject();

    QDeclarativeDebugObjectExpressionWatch *watch;

    QDeclarativeEngineDebug *unconnected = new QDeclarativeEngineDebug(0);
    watch = unconnected->addWatch(obj, expr, this);
    QCOMPARE(watch->state(), QDeclarativeDebugWatch::Dead);
    delete watch;
    delete unconnected;

    watch = m_dbg->addWatch(QDeclarativeDebugObjectReference(), expr, this);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(watch, SIGNAL(stateChanged(QDeclarativeDebugWatch::State))));
    QCOMPARE(watch->state(), QDeclarativeDebugWatch::Inactive);
    delete watch;

    watch = m_dbg->addWatch(obj, expr, this);
    QCOMPARE(watch->state(), QDeclarativeDebugWatch::Waiting);
    QCOMPARE(watch->objectDebugId(), obj.debugId());
    QCOMPARE(watch->expression(), expr);

    QSignalSpy spyState(watch, SIGNAL(stateChanged(QDeclarativeDebugWatch::State)));

    QSignalSpy spy(watch, SIGNAL(valueChanged(QByteArray,QVariant)));
    int expectedSpyCount = incrementCount + 1;  // should also get signal with expression's initial value

    int width = origWidth;
    for (int i=0; i<incrementCount+1; i++) {
        if (i > 0) {
            width += increment;
            m_rootItem->setProperty("width", width);
        }
        if (!QDeclarativeDebugTest::waitForSignal(watch, SIGNAL(valueChanged(QByteArray,QVariant))))
            QFAIL("Did not receive valueChanged() for expression");
    }

    if (spyState.count() == 0)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(watch, SIGNAL(stateChanged(QDeclarativeDebugWatch::State))));
    QCOMPARE(spyState.count(), 1);
    QCOMPARE(watch->state(), QDeclarativeDebugWatch::Active);

    m_dbg->removeWatch(watch);
    delete watch;

    // restore original value and verify spy doesn't get a signal since watch has been removed
    m_rootItem->setProperty("width", origWidth);
    QTest::qWait(100);
    QCOMPARE(spy.count(), expectedSpyCount);

    width = origWidth + increment;
    for (int i=0; i<spy.count(); i++) {
        QCOMPARE(spy.at(i).at(1).value<QVariant>().toInt(), width);
        width += increment;
    }
}

void tst_QDeclarativeDebug::watch_expression_data()
{
    QTest::addColumn<QString>("expr");
    QTest::addColumn<int>("increment");
    QTest::addColumn<int>("incrementCount");

    QTest::newRow("width") << "width" << 0 << 0;
    QTest::newRow("width+10") << "width + 10" << 10 << 5;
}

void tst_QDeclarativeDebug::watch_context()
{
    QDeclarativeDebugContextReference c;
    QTest::ignoreMessage(QtWarningMsg, "QDeclarativeEngineDebug::addWatch(): Not implemented");
    QVERIFY(!m_dbg->addWatch(c, QString(), this));
}

void tst_QDeclarativeDebug::watch_file()
{
    QDeclarativeDebugFileReference f;
    QTest::ignoreMessage(QtWarningMsg, "QDeclarativeEngineDebug::addWatch(): Not implemented");
    QVERIFY(!m_dbg->addWatch(f, this));
}

void tst_QDeclarativeDebug::queryAvailableEngines()
{
    QDeclarativeDebugEnginesQuery *q_engines;

    QDeclarativeEngineDebug *unconnected = new QDeclarativeEngineDebug(0);
    q_engines = unconnected->queryAvailableEngines(0);
    QCOMPARE(q_engines->state(), QDeclarativeDebugQuery::Error);
    delete q_engines;
    delete unconnected;

    q_engines = m_dbg->queryAvailableEngines(this);
    delete q_engines;

    q_engines = m_dbg->queryAvailableEngines(this);
    QVERIFY(q_engines->engines().isEmpty());
    waitForQuery(q_engines);

    // TODO test multiple engines
    QList<QDeclarativeDebugEngineReference> engines = q_engines->engines();
    QCOMPARE(engines.count(), 1);

    foreach(const QDeclarativeDebugEngineReference &e, engines) {
        QCOMPARE(e.debugId(), QDeclarativeDebugService::idForObject(m_engine));
        QCOMPARE(e.name(), m_engine->objectName());
    }

    // Make query invalid by deleting client
    q_engines = m_dbg->queryAvailableEngines(this);
    QCOMPARE(q_engines->state(), QDeclarativeDebugQuery::Waiting);
    delete m_dbg;
    QCOMPARE(q_engines->state(), QDeclarativeDebugQuery::Error);
    delete q_engines;
    m_dbg = new QDeclarativeEngineDebug(m_conn, this);
}

void tst_QDeclarativeDebug::queryRootContexts()
{
    QDeclarativeDebugEnginesQuery *q_engines = m_dbg->queryAvailableEngines(this);
    waitForQuery(q_engines);
    int engineId = q_engines->engines()[0].debugId();
    delete q_engines;

    QDeclarativeDebugRootContextQuery *q_context;

    QDeclarativeEngineDebug *unconnected = new QDeclarativeEngineDebug(0);
    q_context = unconnected->queryRootContexts(engineId, this);
    QCOMPARE(q_context->state(), QDeclarativeDebugQuery::Error);
    delete q_context;
    delete unconnected;

    q_context = m_dbg->queryRootContexts(engineId, this);
    delete q_context;

    q_context = m_dbg->queryRootContexts(engineId, this);
    waitForQuery(q_context);

    QDeclarativeContext *actualContext = m_engine->rootContext();
    QDeclarativeDebugContextReference context = q_context->rootContext();
    QCOMPARE(context.debugId(), QDeclarativeDebugService::idForObject(actualContext));
    QCOMPARE(context.name(), actualContext->objectName());

    QCOMPARE(context.objects().count(), 4); // 4 qml component objects created for context in main()

    // root context query sends only root object data - it doesn't fill in
    // the children or property info
    QCOMPARE(context.objects()[0].properties().count(), 0);
    QCOMPARE(context.objects()[0].children().count(), 0);

    QCOMPARE(context.contexts().count(), 5);
    QVERIFY(context.contexts()[0].debugId() >= 0);
    QCOMPARE(context.contexts()[0].name(), QString("tst_QDeclarativeDebug_childContext"));

    // Make query invalid by deleting client
    q_context = m_dbg->queryRootContexts(engineId, this);
    QCOMPARE(q_context->state(), QDeclarativeDebugQuery::Waiting);
    delete m_dbg;
    QCOMPARE(q_context->state(), QDeclarativeDebugQuery::Error);
    delete q_context;
    m_dbg = new QDeclarativeEngineDebug(m_conn, this);
}

void tst_QDeclarativeDebug::queryObject()
{
    QFETCH(bool, recursive);

    QDeclarativeDebugEnginesQuery *q_engines = m_dbg->queryAvailableEngines(this);
    waitForQuery(q_engines);

    QDeclarativeDebugRootContextQuery *q_context = m_dbg->queryRootContexts(q_engines->engines()[0].debugId(), this);
    waitForQuery(q_context);
    QDeclarativeDebugObjectReference rootObject = q_context->rootContext().objects()[0];

    QDeclarativeDebugObjectQuery *q_obj = 0;

    QDeclarativeEngineDebug *unconnected = new QDeclarativeEngineDebug(0);
    q_obj = recursive ? unconnected->queryObjectRecursive(rootObject, this) : unconnected->queryObject(rootObject, this);
    QCOMPARE(q_obj->state(), QDeclarativeDebugQuery::Error);
    delete q_obj;
    delete unconnected;

    q_obj = recursive ? m_dbg->queryObjectRecursive(rootObject, this) : m_dbg->queryObject(rootObject, this);
    delete q_obj;

    q_obj = recursive ? m_dbg->queryObjectRecursive(rootObject, this) : m_dbg->queryObject(rootObject, this);
    waitForQuery(q_obj);

    QDeclarativeDebugObjectReference obj = q_obj->object();

    delete q_engines;
    delete q_context;

    // Make query invalid by deleting client
    q_obj = recursive ? m_dbg->queryObjectRecursive(rootObject, this) : m_dbg->queryObject(rootObject, this);
    QCOMPARE(q_obj->state(), QDeclarativeDebugQuery::Waiting);
    delete m_dbg;
    QCOMPARE(q_obj->state(), QDeclarativeDebugQuery::Error);
    delete q_obj;
    m_dbg = new QDeclarativeEngineDebug(m_conn, this);

    // check source as defined in main()
    QDeclarativeDebugFileReference source = obj.source();
    QCOMPARE(source.url(), QUrl::fromLocalFile(""));
    QCOMPARE(source.lineNumber(), 3);
    QCOMPARE(source.columnNumber(), 1);

    // generically test all properties, children and childrens' properties
    recursiveObjectTest(m_rootItem, obj, recursive);

    if (recursive) {
        foreach(const QDeclarativeDebugObjectReference &child, obj.children())
            QVERIFY(child.properties().count() > 0);

        QDeclarativeDebugObjectReference rect;
        QDeclarativeDebugObjectReference text;
        foreach (const QDeclarativeDebugObjectReference &child, obj.children()) {
            if (child.className() == "Rectangle")
                rect = child;
            else if (child.className() == "Text")
                text = child;
        }

        // test specific property values
        QCOMPARE(findProperty(rect.properties(), "width").value(), qVariantFromValue(500));
        QCOMPARE(findProperty(rect.properties(), "height").value(), qVariantFromValue(600));
        QCOMPARE(findProperty(rect.properties(), "color").value(), qVariantFromValue(QColor("blue")));

        QCOMPARE(findProperty(text.properties(), "color").value(), qVariantFromValue(QColor("blue")));
    } else {
        foreach(const QDeclarativeDebugObjectReference &child, obj.children())
            QCOMPARE(child.properties().count(), 0);
    }
}

void tst_QDeclarativeDebug::queryObject_data()
{
    QTest::addColumn<bool>("recursive");

    QTest::newRow("non-recursive") << false;
    QTest::newRow("recursive") << true;
}

void tst_QDeclarativeDebug::queryExpressionResult()
{
    QFETCH(QString, expr);
    QFETCH(QVariant, result);

    QDeclarativeDebugEnginesQuery *q_engines = m_dbg->queryAvailableEngines(this);
    waitForQuery(q_engines);    // check immediate deletion is ok

    QDeclarativeDebugRootContextQuery *q_context = m_dbg->queryRootContexts(q_engines->engines()[0].debugId(), this);
    waitForQuery(q_context);
    int objectId = q_context->rootContext().objects()[0].debugId();

    QDeclarativeDebugExpressionQuery *q_expr;

    QDeclarativeEngineDebug *unconnected = new QDeclarativeEngineDebug(0);
    q_expr = unconnected->queryExpressionResult(objectId, expr, this);
    QCOMPARE(q_expr->state(), QDeclarativeDebugQuery::Error);
    delete q_expr;
    delete unconnected;

    q_expr = m_dbg->queryExpressionResult(objectId, expr, this);
    delete q_expr;

    q_expr = m_dbg->queryExpressionResult(objectId, expr, this);
    QCOMPARE(q_expr->expression().toString(), expr);
    waitForQuery(q_expr);

    QCOMPARE(q_expr->result(), result);

    delete q_engines;
    delete q_context;

    // Make query invalid by deleting client
    q_expr = m_dbg->queryExpressionResult(objectId, expr, this);
    QCOMPARE(q_expr->state(), QDeclarativeDebugQuery::Waiting);
    delete m_dbg;
    QCOMPARE(q_expr->state(), QDeclarativeDebugQuery::Error);
    delete q_expr;
    m_dbg = new QDeclarativeEngineDebug(m_conn, this);
}

void tst_QDeclarativeDebug::queryExpressionResult_data()
{
    QTest::addColumn<QString>("expr");
    QTest::addColumn<QVariant>("result");

    QTest::newRow("width + 50") << "width + 50" << qVariantFromValue(60);
    QTest::newRow("blueRect.width") << "blueRect.width" << qVariantFromValue(500);
    QTest::newRow("bad expr") << "aeaef" << qVariantFromValue(QString("<undefined>"));
    QTest::newRow("QObject*") << "varObj" << qVariantFromValue(QString("<unnamed object>"));
    QTest::newRow("list of QObject*") << "varObjList" << qVariantFromValue(QString("<unknown value>"));
}

void tst_QDeclarativeDebug::tst_QDeclarativeDebugFileReference()
{
    QDeclarativeDebugFileReference ref;
    QVERIFY(ref.url().isEmpty());
    QCOMPARE(ref.lineNumber(), -1);
    QCOMPARE(ref.columnNumber(), -1);

    ref.setUrl(QUrl("http://test"));
    QCOMPARE(ref.url(), QUrl("http://test"));
    ref.setLineNumber(1);
    QCOMPARE(ref.lineNumber(), 1);
    ref.setColumnNumber(1);
    QCOMPARE(ref.columnNumber(), 1);

    QDeclarativeDebugFileReference copy(ref);
    QDeclarativeDebugFileReference copyAssign;
    copyAssign = ref;
    foreach (const QDeclarativeDebugFileReference &r, (QList<QDeclarativeDebugFileReference>() << copy << copyAssign)) {
        QCOMPARE(r.url(), ref.url());
        QCOMPARE(r.lineNumber(), ref.lineNumber());
        QCOMPARE(r.columnNumber(), ref.columnNumber());
    }
}

void tst_QDeclarativeDebug::tst_QDeclarativeDebugEngineReference()
{
    QDeclarativeDebugEngineReference ref;
    QCOMPARE(ref.debugId(), -1);
    QVERIFY(ref.name().isEmpty());

    ref = QDeclarativeDebugEngineReference(1);
    QCOMPARE(ref.debugId(), 1);
    QVERIFY(ref.name().isEmpty());

    QDeclarativeDebugEnginesQuery *q_engines = m_dbg->queryAvailableEngines(this);
    waitForQuery(q_engines);
    ref = q_engines->engines()[0];
    delete q_engines;

    QDeclarativeDebugEngineReference copy(ref);
    QDeclarativeDebugEngineReference copyAssign;
    copyAssign = ref;
    foreach (const QDeclarativeDebugEngineReference &r, (QList<QDeclarativeDebugEngineReference>() << copy << copyAssign)) {
        QCOMPARE(r.debugId(), ref.debugId());
        QCOMPARE(r.name(), ref.name());
    }
}

void tst_QDeclarativeDebug::tst_QDeclarativeDebugObjectReference()
{
    QDeclarativeDebugObjectReference ref;
    QCOMPARE(ref.debugId(), -1);
    QCOMPARE(ref.className(), QString());
    QCOMPARE(ref.name(), QString());
    QCOMPARE(ref.contextDebugId(), -1);
    QVERIFY(ref.properties().isEmpty());
    QVERIFY(ref.children().isEmpty());

    QDeclarativeDebugFileReference source = ref.source();
    QVERIFY(source.url().isEmpty());
    QVERIFY(source.lineNumber() < 0);
    QVERIFY(source.columnNumber() < 0);

    ref = QDeclarativeDebugObjectReference(1);
    QCOMPARE(ref.debugId(), 1);

    QDeclarativeDebugObjectReference rootObject = findRootObject();
    QDeclarativeDebugObjectQuery *query = m_dbg->queryObjectRecursive(rootObject, this);
    waitForQuery(query);
    ref = query->object();
    delete query;

    QVERIFY(ref.debugId() >= 0);

    QDeclarativeDebugObjectReference copy(ref);
    QDeclarativeDebugObjectReference copyAssign;
    copyAssign = ref;
    foreach (const QDeclarativeDebugObjectReference &r, (QList<QDeclarativeDebugObjectReference>() << copy << copyAssign))
        recursiveCompareObjects(r, ref);
}

void tst_QDeclarativeDebug::tst_QDeclarativeDebugContextReference()
{
    QDeclarativeDebugContextReference ref;
    QCOMPARE(ref.debugId(), -1);
    QVERIFY(ref.name().isEmpty());
    QVERIFY(ref.objects().isEmpty());
    QVERIFY(ref.contexts().isEmpty());

    QDeclarativeDebugEnginesQuery *q_engines = m_dbg->queryAvailableEngines(this);
    waitForQuery(q_engines);
    QDeclarativeDebugRootContextQuery *q_context = m_dbg->queryRootContexts(q_engines->engines()[0].debugId(), this);
    waitForQuery(q_context);

    ref = q_context->rootContext();
    delete q_engines;
    delete q_context;
    QVERIFY(ref.debugId() >= 0);

    QDeclarativeDebugContextReference copy(ref);
    QDeclarativeDebugContextReference copyAssign;
    copyAssign = ref;
    foreach (const QDeclarativeDebugContextReference &r, (QList<QDeclarativeDebugContextReference>() << copy << copyAssign))
        recursiveCompareContexts(r, ref);
}

void tst_QDeclarativeDebug::tst_QDeclarativeDebugPropertyReference()
{
    QDeclarativeDebugObjectReference rootObject = findRootObject();
    QDeclarativeDebugObjectQuery *query = m_dbg->queryObject(rootObject, this);
    waitForQuery(query);
    QDeclarativeDebugObjectReference obj = query->object();
    delete query;

    QDeclarativeDebugPropertyReference ref = findProperty(obj.properties(), "scale");
    QVERIFY(ref.objectDebugId() > 0);
    QVERIFY(!ref.name().isEmpty());
    QVERIFY(!ref.value().isNull());
    QVERIFY(!ref.valueTypeName().isEmpty());
    QVERIFY(!ref.binding().isEmpty());
    QVERIFY(ref.hasNotifySignal());

    QDeclarativeDebugPropertyReference copy(ref);
    QDeclarativeDebugPropertyReference copyAssign;
    copyAssign = ref;
    foreach (const QDeclarativeDebugPropertyReference &r, (QList<QDeclarativeDebugPropertyReference>() << copy << copyAssign))
        compareProperties(r, ref);
}

void tst_QDeclarativeDebug::setBindingForObject()
{
    QDeclarativeDebugObjectReference rootObject = findRootObject();
    QVERIFY(rootObject.debugId() != -1);
    QDeclarativeDebugPropertyReference widthPropertyRef = findProperty(rootObject.properties(), "width");

    QCOMPARE(widthPropertyRef.value(), QVariant(10));
    QCOMPARE(widthPropertyRef.binding(), QString());

    //
    // set literal
    //
    m_dbg->setBindingForObject(rootObject.debugId(), "width", "15", true);

    rootObject = findRootObject();
    widthPropertyRef =  findProperty(rootObject.properties(), "width");

    QCOMPARE(widthPropertyRef.value(), QVariant(15));
    QCOMPARE(widthPropertyRef.binding(), QString());

    //
    // set expression
    //
    m_dbg->setBindingForObject(rootObject.debugId(), "width", "height", false);

    rootObject = findRootObject();
    widthPropertyRef =  findProperty(rootObject.properties(), "width");

    QCOMPARE(widthPropertyRef.value(), QVariant(20));
    QCOMPARE(widthPropertyRef.binding(), QString("height"));

    //
    // reset
    //
    m_dbg->resetBindingForObject(rootObject.debugId(), "width");

    rootObject = findRootObject();
    widthPropertyRef =  findProperty(rootObject.properties(), "width");

   // QCOMPARE(widthPropertyRef.value(), QVariant(0)); // TODO: Shouldn't this work?
    QCOMPARE(widthPropertyRef.binding(), QString());

    //
    // set handler
    //
    rootObject = findRootObject();
    QCOMPARE(rootObject.children().size(), 5); // Rectangle, Text, MouseArea, Component.onCompleted, NonScriptPropertyElement
    QDeclarativeDebugObjectReference mouseAreaObject = rootObject.children().at(2);
    QDeclarativeDebugObjectQuery *q_obj = m_dbg->queryObjectRecursive(mouseAreaObject, this);
    waitForQuery(q_obj);
    mouseAreaObject = q_obj->object();

    QCOMPARE(mouseAreaObject.className(), QString("MouseArea"));

    QDeclarativeDebugPropertyReference onEnteredRef = findProperty(mouseAreaObject.properties(), "onEntered");

    QCOMPARE(onEnteredRef.name(), QString("onEntered"));
    QCOMPARE(onEnteredRef.value(),  QVariant("{ console.log('hello') }"));

    m_dbg->setBindingForObject(mouseAreaObject.debugId(), "onEntered", "{console.log('hello, world') }", false) ;

    rootObject = findRootObject();
    mouseAreaObject = rootObject.children().at(2);
    q_obj = m_dbg->queryObjectRecursive(mouseAreaObject, this);
    waitForQuery(q_obj);
    mouseAreaObject = q_obj->object();
    onEnteredRef = findProperty(mouseAreaObject.properties(), "onEntered");
    QCOMPARE(onEnteredRef.name(), QString("onEntered"));
    QCOMPARE(onEnteredRef.value(),  QVariant("{console.log('hello, world') }"));
}

void tst_QDeclarativeDebug::setBindingInStates()
{
    // Check if changing bindings of propertychanges works

    const int sourceIndex = 3;

    QDeclarativeDebugObjectReference obj = findRootObject(sourceIndex);

    QVERIFY(obj.debugId() != -1);
    QVERIFY(obj.children().count() >= 2);

    // We are going to switch state a couple of times, we need to get rid of the transition before
    QDeclarativeDebugExpressionQuery *q_deleteTransition = m_dbg->queryExpressionResult(obj.debugId(),QString("transitions = []"),this);
    waitForQuery(q_deleteTransition);
    delete q_deleteTransition;


    // check initial value of the property that is changing
    QDeclarativeDebugExpressionQuery *q_setState;
    q_setState = m_dbg->queryExpressionResult(obj.debugId(),QString("state=\"state1\""),this);
    waitForQuery(q_setState);
    delete q_setState;

    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties(),"width").value().toInt(),200);


    q_setState = m_dbg->queryExpressionResult(obj.debugId(),QString("state=\"\""),this);
    waitForQuery(q_setState);
    delete q_setState;


    obj = findRootObject(sourceIndex, true);
    QCOMPARE(findProperty(obj.properties(),"width").value().toInt(),100);


    // change the binding
    QDeclarativeDebugObjectReference state = obj.children()[0];
    QCOMPARE(state.className(), QString("State"));
    QVERIFY(state.children().count() > 0);

    QDeclarativeDebugObjectReference propertyChange = state.children()[0];
    QVERIFY(propertyChange.debugId() != -1);

    QVERIFY( m_dbg->setBindingForObject(propertyChange.debugId(), "width",QVariant(300),true) );

    // check properties changed in state
    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties(),"width").value().toInt(),100);


    q_setState = m_dbg->queryExpressionResult(obj.debugId(),QString("state=\"state1\""),this);
    waitForQuery(q_setState);
    delete q_setState;

    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties(),"width").value().toInt(),300);

    // check changing properties of base state from within a state
    QVERIFY(m_dbg->setBindingForObject(obj.debugId(),"width","height*2",false));
    QVERIFY(m_dbg->setBindingForObject(obj.debugId(),"height","200",true));

    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties(),"width").value().toInt(),300);

    q_setState = m_dbg->queryExpressionResult(obj.debugId(),QString("state=\"\""),this);
    waitForQuery(q_setState);
    delete q_setState;

    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties(),"width").value().toInt(), 400);

    //  reset binding while in a state
    q_setState = m_dbg->queryExpressionResult(obj.debugId(),QString("state=\"state1\""),this);
    waitForQuery(q_setState);
    delete q_setState;

    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties(),"width").value().toInt(), 300);

    m_dbg->resetBindingForObject(propertyChange.debugId(), "width");

    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties(),"width").value().toInt(), 400);

    // re-add binding
    m_dbg->setBindingForObject(propertyChange.debugId(), "width", "300", true);

    obj = findRootObject(sourceIndex);
    QCOMPARE(findProperty(obj.properties(),"width").value().toInt(), 300);
}

void tst_QDeclarativeDebug::queryObjectTree()
{
    const int sourceIndex = 3;

    // Check if states/transitions are initialized when fetching root item
    QDeclarativeDebugEnginesQuery *q_engines = m_dbg->queryAvailableEngines(this);
    waitForQuery(q_engines);

    QDeclarativeDebugRootContextQuery *q_context = m_dbg->queryRootContexts(q_engines->engines()[0].debugId(), this);
    waitForQuery(q_context);

    QVERIFY(q_context->rootContext().objects().count() > sourceIndex);
    QDeclarativeDebugObjectReference rootObject = q_context->rootContext().objects()[sourceIndex];

    QDeclarativeDebugObjectQuery *q_obj = m_dbg->queryObjectRecursive(rootObject, this);
    waitForQuery(q_obj);

    QDeclarativeDebugObjectReference obj = q_obj->object();

    delete q_engines;
    delete q_context;
    delete q_obj;

    QVERIFY(obj.debugId() != -1);
    QVERIFY(obj.children().count() >= 2);



    // check state
    QDeclarativeDebugObjectReference state = obj.findChildByClassName(QString("State"));
    QVERIFY(state.debugId() != -1);
    QCOMPARE(state.className(), QString("State"));
    QVERIFY(state.children().count() > 0);

    QDeclarativeDebugObjectReference propertyChange = state.children()[0];
    QVERIFY(propertyChange.debugId() != -1);

    QDeclarativeDebugPropertyReference propertyChangeTarget = findProperty(propertyChange.properties(),"target");
    QCOMPARE(propertyChangeTarget.objectDebugId(), propertyChange.debugId());

    QDeclarativeDebugObjectReference targetReference = qvariant_cast<QDeclarativeDebugObjectReference>(propertyChangeTarget.value());
    QVERIFY(targetReference.debugId() != -1);

    QDeclarativeDebugObjectReference nextState = obj.findChildByClassName(QString("State"), state);
    QVERIFY(nextState.debugId() == -1);


    // check transition
    QDeclarativeDebugObjectReference transition = obj.findChildByClassName(QString("Transition"));
    QVERIFY(transition.debugId() != -1);
    QCOMPARE(transition.className(), QString("Transition"));
    QCOMPARE(findProperty(transition.properties(),"from").value().toString(), QString("*"));
    QCOMPARE(findProperty(transition.properties(),"to").value(), findProperty(state.properties(),"name").value());
    QVERIFY(transition.children().count() > 0);

    QDeclarativeDebugObjectReference animation = transition.children()[0];
    QVERIFY(animation.debugId() != -1);

    QDeclarativeDebugPropertyReference animationTarget = findProperty(animation.properties(),"target");
    QCOMPARE(animationTarget.objectDebugId(), animation.debugId());

    targetReference = qvariant_cast<QDeclarativeDebugObjectReference>(animationTarget.value());
    QVERIFY(targetReference.debugId() != -1);

    QCOMPARE(findProperty(animation.properties(),"property").value().toString(), QString("width"));
    QCOMPARE(findProperty(animation.properties(),"duration").value().toInt(), 100);

    QDeclarativeDebugObjectReference nextTransition = obj.findChildByClassName(QString("Transition"), transition);
    QVERIFY(nextTransition.debugId() == -1);
}

int main(int argc, char *argv[])
{
    int _argc = argc + 1;
    char **_argv = new char*[_argc];
    for (int i = 0; i < argc; ++i)
        _argv[i] = argv[i];
    _argv[_argc - 1] = const_cast<char *>("-qmljsdebugger=port:3768");

    QApplication app(_argc, _argv);
    tst_QDeclarativeDebug tc;
    return QTest::qExec(&tc, _argc, _argv);
    delete _argv;
}

#include "tst_qdeclarativedebug.moc"
