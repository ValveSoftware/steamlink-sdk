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
#include <QtDeclarative/private/qdeclarativeitem_p.h>
#include <QtDeclarative/private/qdeclarativetext_p.h>
#include <QtDeclarative/private/qdeclarativeengine_p.h>
#include <QtDeclarative/private/qdeclarativelistmodel_p.h>
#include <QtDeclarative/private/qdeclarativeexpression_p.h>
#include <QDeclarativeComponent>

#include <QtCore/qtimer.h>
#include <QtCore/qdebug.h>
#include <QtCore/qtranslator.h>
#include <QSignalSpy>

Q_DECLARE_METATYPE(QList<int>)
Q_DECLARE_METATYPE(QList<QVariantHash>)

class tst_qdeclarativelistmodel : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativelistmodel() {}

private:
    int roleFromName(const QDeclarativeListModel *model, const QString &roleName);
    QScriptValue nestedListValue(QScriptEngine *eng) const;
    QDeclarativeItem *createWorkerTest(QDeclarativeEngine *eng, QDeclarativeComponent *component, QDeclarativeListModel *model);
    void waitForWorker(QDeclarativeItem *item);

private slots:
    void static_types();
    void static_types_data();
    void static_i18n();
    void static_nestedElements();
    void static_nestedElements_data();
    void dynamic_data();
    void dynamic();
    void dynamic_worker_data();
    void dynamic_worker();
    void dynamic_worker_sync_data();
    void dynamic_worker_sync();
    void convertNestedToFlat_fail();
    void convertNestedToFlat_fail_data();
    void convertNestedToFlat_ok();
    void convertNestedToFlat_ok_data();
    void enumerate();
    void error_data();
    void error();
    void syncError();
    void set();
    void get();
    void get_data();
    void get_worker();
    void get_worker_data();
    void get_nested();
    void get_nested_data();
    void crash_model_with_multiple_roles();
    void set_model_cache();
    void property_changes();
    void property_changes_data();
    void property_changes_worker();
    void property_changes_worker_data();
    void clear();
};
int tst_qdeclarativelistmodel::roleFromName(const QDeclarativeListModel *model, const QString &roleName)
{
    QList<int> roles = model->roles();
    for (int i=0; i<roles.count(); i++) {
        if (model->toString(roles[i]) == roleName)
            return roles[i];
    }
    return -1;
}

QScriptValue tst_qdeclarativelistmodel::nestedListValue(QScriptEngine *eng) const
{
    QScriptValue list = eng->newArray();
    list.setProperty(0, eng->newObject());
    list.setProperty(1, eng->newObject());
    QScriptValue sv = eng->newObject();
    sv.setProperty("foo", list);
    return sv;
}

QDeclarativeItem *tst_qdeclarativelistmodel::createWorkerTest(QDeclarativeEngine *eng, QDeclarativeComponent *component, QDeclarativeListModel *model)
{
    QDeclarativeItem *item = qobject_cast<QDeclarativeItem*>(component->create());
    QDeclarativeEngine::setContextForObject(model, eng->rootContext());
    if (item)
        item->setProperty("model", qVariantFromValue(model));
    return item;
}

void tst_qdeclarativelistmodel::waitForWorker(QDeclarativeItem *item)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));

    QDeclarativeProperty prop(item, "done");
    QVERIFY(prop.isValid());
    QVERIFY(prop.connectNotifySignal(&loop, SLOT(quit())));
    timer.start(10000);
    loop.exec();
    QVERIFY(timer.isActive());
}

void tst_qdeclarativelistmodel::static_i18n()
{
    QString expect = QString::fromUtf8("na\303\257ve");

    QString componentStr = "import QtQuick 1.0\nListModel { ListElement { prop1: \""+expect+"\"; prop2: QT_TR_NOOP(\""+expect+"\") } }";
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toUtf8(), QUrl::fromLocalFile(""));
    QDeclarativeListModel *obj = qobject_cast<QDeclarativeListModel*>(component.create());
    QVERIFY(obj != 0);
    QString prop1 = obj->get(0).property(QLatin1String("prop1")).toString();
    QCOMPARE(prop1,expect);
    QString prop2 = obj->get(0).property(QLatin1String("prop2")).toString();
    QCOMPARE(prop2,expect); // (no, not translated, QT_TR_NOOP is a no-op)
    delete obj;
}

void tst_qdeclarativelistmodel::static_nestedElements()
{
    QFETCH(int, elementCount);

    QStringList elements;
    for (int i=0; i<elementCount; i++)
        elements.append("ListElement { a: 1; b: 2 }");
    QString elementsStr = elements.join(",\n") + "\n";

    QString componentStr =
        "import QtQuick 1.0\n"
        "ListModel {\n"
        "   ListElement {\n"
        "       attributes: [\n";
    componentStr += elementsStr.toUtf8().constData();
    componentStr +=
        "       ]\n"
        "   }\n"
        "}";

    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toUtf8(), QUrl::fromLocalFile(""));

    QDeclarativeListModel *obj = qobject_cast<QDeclarativeListModel*>(component.create());
    QVERIFY(obj != 0);

    QScriptValue prop = obj->get(0).property(QLatin1String("attributes")).property(QLatin1String("count"));
    QVERIFY(prop.isNumber());
    QCOMPARE(prop.toInt32(), qint32(elementCount));

    delete obj;
}

void tst_qdeclarativelistmodel::static_nestedElements_data()
{
    QTest::addColumn<int>("elementCount");

    QTest::newRow("0 items") << 0;
    QTest::newRow("1 item") << 1;
    QTest::newRow("2 items") << 2;
    QTest::newRow("many items") << 5;
}

void tst_qdeclarativelistmodel::dynamic_data()
{
    QTest::addColumn<QString>("script");
    QTest::addColumn<int>("result");
    QTest::addColumn<QString>("warning");

    // Simple flat model

    QTest::newRow("count") << "count" << 0 << "";

    QTest::newRow("get1") << "{get(0) === undefined}" << 1 << "";
    QTest::newRow("get2") << "{get(-1) === undefined}" << 1 << "";
    QTest::newRow("get3") << "{append({'foo':123});get(0) != undefined}" << 1 << "";
    QTest::newRow("get4") << "{append({'foo':123});get(0).foo}" << 123 << "";

    QTest::newRow("get-modify1") << "{append({'foo':123,'bar':456});get(0).foo = 333;get(0).foo}" << 333 << "";
    QTest::newRow("get-modify2") << "{append({'z':1});append({'foo':123,'bar':456});get(1).bar = 999;get(1).bar}" << 999 << "";

    QTest::newRow("append1") << "{append({'foo':123});count}" << 1 << "";
    QTest::newRow("append2") << "{append({'foo':123,'bar':456});count}" << 1 << "";
    QTest::newRow("append3a") << "{append({'foo':123});append({'foo':456});get(0).foo}" << 123 << "";
    QTest::newRow("append3b") << "{append({'foo':123});append({'foo':456});get(1).foo}" << 456 << "";
    QTest::newRow("append4a") << "{append(123)}" << 0 << "<Unknown File>: QML ListModel: append: value is not an object";
    QTest::newRow("append4b") << "{append([1,2,3])}" << 0 << "<Unknown File>: QML ListModel: append: value is not an object";

    QTest::newRow("clear1") << "{append({'foo':456});clear();count}" << 0 << "";
    QTest::newRow("clear2") << "{append({'foo':123});append({'foo':456});clear();count}" << 0 << "";
    QTest::newRow("clear3") << "{append({'foo':123});clear()}" << 0 << "";

    QTest::newRow("remove1") << "{append({'foo':123});remove(0);count}" << 0 << "";
    QTest::newRow("remove2a") << "{append({'foo':123});append({'foo':456});remove(0);count}" << 1 << "";
    QTest::newRow("remove2b") << "{append({'foo':123});append({'foo':456});remove(0);get(0).foo}" << 456 << "";
    QTest::newRow("remove2c") << "{append({'foo':123});append({'foo':456});remove(1);get(0).foo}" << 123 << "";
    QTest::newRow("remove3") << "{append({'foo':123});remove(0)}" << 0 << "";
    QTest::newRow("remove3a") << "{append({'foo':123});remove(-1);count}" << 1 << "<Unknown File>: QML ListModel: remove: index -1 out of range";
    QTest::newRow("remove4a") << "{remove(0)}" << 0 << "<Unknown File>: QML ListModel: remove: index 0 out of range";
    QTest::newRow("remove4b") << "{append({'foo':123});remove(0);remove(0);count}" << 0 << "<Unknown File>: QML ListModel: remove: index 0 out of range";
    QTest::newRow("remove4c") << "{append({'foo':123});remove(1);count}" << 1 << "<Unknown File>: QML ListModel: remove: index 1 out of range";

    QTest::newRow("insert1") << "{insert(0,{'foo':123});count}" << 1 << "";
    QTest::newRow("insert2") << "{insert(1,{'foo':123});count}" << 0 << "<Unknown File>: QML ListModel: insert: index 1 out of range";
    QTest::newRow("insert3a") << "{append({'foo':123});insert(1,{'foo':456});count}" << 2 << "";
    QTest::newRow("insert3b") << "{append({'foo':123});insert(1,{'foo':456});get(0).foo}" << 123 << "";
    QTest::newRow("insert3c") << "{append({'foo':123});insert(1,{'foo':456});get(1).foo}" << 456 << "";
    QTest::newRow("insert3d") << "{append({'foo':123});insert(0,{'foo':456});get(0).foo}" << 456 << "";
    QTest::newRow("insert3e") << "{append({'foo':123});insert(0,{'foo':456});get(1).foo}" << 123 << "";
    QTest::newRow("insert4") << "{append({'foo':123});insert(-1,{'foo':456});count}" << 1 << "<Unknown File>: QML ListModel: insert: index -1 out of range";
    QTest::newRow("insert5a") << "{insert(0,123)}" << 0 << "<Unknown File>: QML ListModel: insert: value is not an object";
    QTest::newRow("insert5b") << "{insert(0,[1,2,3])}" << 0 << "<Unknown File>: QML ListModel: insert: value is not an object";

    QTest::newRow("set1") << "{append({'foo':123});set(0,{'foo':456});count}" << 1 << "";
    QTest::newRow("set2") << "{append({'foo':123});set(0,{'foo':456});get(0).foo}" << 456 << "";
    QTest::newRow("set3a") << "{append({'foo':123,'bar':456});set(0,{'foo':999});get(0).foo}" << 999 << "";
    QTest::newRow("set3b") << "{append({'foo':123,'bar':456});set(0,{'foo':999});get(0).bar}" << 456 << "";
    QTest::newRow("set4a") << "{set(0,{'foo':456});count}" << 1 << "";
    QTest::newRow("set4c") << "{set(-1,{'foo':456})}" << 0 << "<Unknown File>: QML ListModel: set: index -1 out of range";
    QTest::newRow("set5a") << "{append({'foo':123,'bar':456});set(0,123);count}" << 1 << "<Unknown File>: QML ListModel: set: value is not an object";
    QTest::newRow("set5b") << "{append({'foo':123,'bar':456});set(0,[1,2,3]);count}" << 1 << "<Unknown File>: QML ListModel: set: value is not an object";
    QTest::newRow("set6") << "{append({'foo':123});set(1,{'foo':456});count}" << 2 << "";

    QTest::newRow("setprop1") << "{append({'foo':123});setProperty(0,'foo',456);count}" << 1 << "";
    QTest::newRow("setprop2") << "{append({'foo':123});setProperty(0,'foo',456);get(0).foo}" << 456 << "";
    QTest::newRow("setprop3a") << "{append({'foo':123,'bar':456});setProperty(0,'foo',999);get(0).foo}" << 999 << "";
    QTest::newRow("setprop3b") << "{append({'foo':123,'bar':456});setProperty(0,'foo',999);get(0).bar}" << 456 << "";
    QTest::newRow("setprop4a") << "{setProperty(0,'foo',456)}" << 0 << "<Unknown File>: QML ListModel: set: index 0 out of range";
    QTest::newRow("setprop4b") << "{setProperty(-1,'foo',456)}" << 0 << "<Unknown File>: QML ListModel: set: index -1 out of range";
    QTest::newRow("setprop4c") << "{append({'foo':123,'bar':456});setProperty(1,'foo',456);count}" << 1 << "<Unknown File>: QML ListModel: set: index 1 out of range";
    QTest::newRow("setprop5") << "{append({'foo':123,'bar':456});append({'foo':111});setProperty(1,'bar',222);get(1).bar}" << 222 << "";

    QTest::newRow("move1a") << "{append({'foo':123});append({'foo':456});move(0,1,1);count}" << 2 << "";
    QTest::newRow("move1b") << "{append({'foo':123});append({'foo':456});move(0,1,1);get(0).foo}" << 456 << "";
    QTest::newRow("move1c") << "{append({'foo':123});append({'foo':456});move(0,1,1);get(1).foo}" << 123 << "";
    QTest::newRow("move1d") << "{append({'foo':123});append({'foo':456});move(1,0,1);get(0).foo}" << 456 << "";
    QTest::newRow("move1e") << "{append({'foo':123});append({'foo':456});move(1,0,1);get(1).foo}" << 123 << "";
    QTest::newRow("move2a") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(0,1,2);count}" << 3 << "";
    QTest::newRow("move2b") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(0,1,2);get(0).foo}" << 789 << "";
    QTest::newRow("move2c") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(0,1,2);get(1).foo}" << 123 << "";
    QTest::newRow("move2d") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(0,1,2);get(2).foo}" << 456 << "";
    QTest::newRow("move3a") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(1,0,3);count}" << 3 << "<Unknown File>: QML ListModel: move: out of range";
    QTest::newRow("move3b") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(1,-1,1);count}" << 3 << "<Unknown File>: QML ListModel: move: out of range";
    QTest::newRow("move3c") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(1,0,-1);count}" << 3 << "<Unknown File>: QML ListModel: move: out of range";
    QTest::newRow("move3d") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(0,3,1);count}" << 3 << "<Unknown File>: QML ListModel: move: out of range";

    // Nested models

    QTest::newRow("nested-append1") << "{append({'foo':123,'bars':[{'a':1},{'a':2},{'a':3}]});count}" << 1 << "";
    QTest::newRow("nested-append2") << "{append({'foo':123,'bars':[{'a':1},{'a':2},{'a':3}]});get(0).bars.get(1).a}" << 2 << "";
    QTest::newRow("nested-append3") << "{append({'foo':123,'bars':[{'a':1},{'a':2},{'a':3}]});get(0).bars.append({'a':4});get(0).bars.get(3).a}" << 4 << "";

    QTest::newRow("nested-insert") << "{append({'foo':123});insert(0,{'bars':[{'a':1},{'b':2},{'c':3}]});get(0).bars.get(0).a}" << 1 << "";
    QTest::newRow("nested-set") << "{append({'foo':[{'x':1}]});set(0,{'foo':[{'x':123}]});get(0).foo.get(0).x}" << 123 << "";

    QTest::newRow("nested-count") << "{append({'foo':123,'bars':[{'a':1},{'a':2},{'a':3}]}); get(0).bars.count}" << 3 << "";
    QTest::newRow("nested-clear") << "{append({'foo':123,'bars':[{'a':1},{'a':2},{'a':3}]}); get(0).bars.clear(); get(0).bars.count}" << 0 << "";
}

void tst_qdeclarativelistmodel::dynamic()
{
    QFETCH(QString, script);
    QFETCH(int, result);
    QFETCH(QString, warning);

    QDeclarativeEngine engine;
    QDeclarativeListModel model;
    QDeclarativeEngine::setContextForObject(&model,engine.rootContext());
    engine.rootContext()->setContextObject(&model);
    QDeclarativeExpression e(engine.rootContext(), &model, script);
    if (!warning.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, warning.toLatin1());

    QSignalSpy spyCount(&model, SIGNAL(countChanged()));

    int actual = e.evaluate().toInt();
    if (e.hasError())
        qDebug() << e.error(); // errors not expected

    QCOMPARE(actual,result);

    if (model.count() > 0)
        QVERIFY(spyCount.count() > 0);
}

void tst_qdeclarativelistmodel::dynamic_worker_data()
{
    dynamic_data();
}

void tst_qdeclarativelistmodel::dynamic_worker()
{
    QFETCH(QString, script);
    QFETCH(int, result);
    QFETCH(QString, warning);

    if (QByteArray(QTest::currentDataTag()).startsWith("nested"))
        return;

    // This is same as dynamic() except it applies the test to a ListModel called
    // from a WorkerScript (i.e. testing the internal FlatListModel that is created
    // by the WorkerListModelAgent)

    QDeclarativeListModel model;
    QDeclarativeEngine eng;
    QDeclarativeComponent component(&eng, QUrl::fromLocalFile(SRCDIR "/data/model.qml"));
    QDeclarativeItem *item = createWorkerTest(&eng, &component, &model);
    QVERIFY(item != 0);

    QSignalSpy spyCount(&model, SIGNAL(countChanged()));

    if (script[0] == QLatin1Char('{') && script[script.length()-1] == QLatin1Char('}'))
        script = script.mid(1, script.length() - 2);
    QVariantList operations;
    foreach (const QString &s, script.split(';')) {
        if (!s.isEmpty())
            operations << s;
    }

    if (!warning.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, warning.toLatin1());

    QVERIFY(QMetaObject::invokeMethod(item, "evalExpressionViaWorker",
            Q_ARG(QVariant, operations)));
    waitForWorker(item);
    QCOMPARE(QDeclarativeProperty(item, "result").read().toInt(), result);

    if (model.count() > 0)
        QVERIFY(spyCount.count() > 0);

    delete item;
    qApp->processEvents();
}



void tst_qdeclarativelistmodel::dynamic_worker_sync_data()
{
    dynamic_data();
}

void tst_qdeclarativelistmodel::dynamic_worker_sync()
{
    QFETCH(QString, script);
    QFETCH(int, result);
    QFETCH(QString, warning);

    // This is the same as dynamic_worker() except that it executes a set of list operations
    // from the worker script, calls sync(), and tests the changes are reflected in the
    // list in the main thread

    QDeclarativeListModel model;
    QDeclarativeEngine eng;
    QDeclarativeComponent component(&eng, QUrl::fromLocalFile(SRCDIR "/data/model.qml"));
    QDeclarativeItem *item = createWorkerTest(&eng, &component, &model);
    QVERIFY(item != 0);

    if (script[0] == QLatin1Char('{') && script[script.length()-1] == QLatin1Char('}'))
        script = script.mid(1, script.length() - 2);
    QVariantList operations;
    foreach (const QString &s, script.split(';')) {
        if (!s.isEmpty())
            operations << s;
    }

    if (!warning.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, warning.toLatin1());

    // execute a set of commands on the worker list model, then check the
    // changes are reflected in the list model in the main thread
    if (QByteArray(QTest::currentDataTag()).startsWith("nested"))
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML ListModel: Cannot add list-type data when modifying or after modification from a worker script");

    if (QByteArray(QTest::currentDataTag()).startsWith("nested-set"))
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML ListModel: Cannot add list-type data when modifying or after modification from a worker script");

    QVERIFY(QMetaObject::invokeMethod(item, "evalExpressionViaWorker",
            Q_ARG(QVariant, operations.mid(0, operations.length()-1))));
    waitForWorker(item);

    QDeclarativeExpression e(eng.rootContext(), &model, operations.last().toString());
    if (!QByteArray(QTest::currentDataTag()).startsWith("nested"))
        QCOMPARE(e.evaluate().toInt(), result);

    delete item;
    qApp->processEvents();
}

void tst_qdeclarativelistmodel::convertNestedToFlat_fail()
{
    // If a model has nested data, it cannot be used at all from a worker script

    QFETCH(QString, script);

    QDeclarativeListModel model;
    QDeclarativeEngine eng;
    QDeclarativeComponent component(&eng, QUrl::fromLocalFile(SRCDIR "/data/model.qml"));
    QDeclarativeItem *item = createWorkerTest(&eng, &component, &model);
    QVERIFY(item != 0);

    QScriptEngine s_eng;
    QScriptValue plainData = s_eng.newObject();
    plainData.setProperty("foo", QScriptValue(123));
    model.append(plainData);
    model.append(nestedListValue(&s_eng));
    QCOMPARE(model.count(), 2);

    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML ListModel: List contains list-type data and cannot be used from a worker script");
    QVERIFY(QMetaObject::invokeMethod(item, "evalExpressionViaWorker", Q_ARG(QVariant, script)));
    waitForWorker(item);

    QCOMPARE(model.count(), 2);

    delete item;
    qApp->processEvents();
}

void tst_qdeclarativelistmodel::convertNestedToFlat_fail_data()
{
    QTest::addColumn<QString>("script");

    QTest::newRow("clear") << "clear()";
    QTest::newRow("remove") << "remove(0)";
    QTest::newRow("append") << "append({'x':1})";
    QTest::newRow("insert") << "insert(0, {'x':1})";
    QTest::newRow("set") << "set(0, {'foo':1})";
    QTest::newRow("setProperty") << "setProperty(0, 'foo', 1})";
    QTest::newRow("move") << "move(0, 1, 1})";
    QTest::newRow("get") << "get(0)";
}

void tst_qdeclarativelistmodel::convertNestedToFlat_ok()
{
    // If a model only has plain data, it can be modified from a worker script. However,
    // once the model is used from a worker script, it no longer accepts nested data

    QFETCH(QString, script);

    QDeclarativeListModel model;
    QDeclarativeEngine eng;
    QDeclarativeComponent component(&eng, QUrl::fromLocalFile(SRCDIR "/data/model.qml"));
    QDeclarativeItem *item = createWorkerTest(&eng, &component, &model);
    QVERIFY(item != 0);

    QScriptEngine s_eng;
    QScriptValue plainData = s_eng.newObject();
    plainData.setProperty("foo", QScriptValue(123));
    model.append(plainData);
    QCOMPARE(model.count(), 1);

    QVERIFY(QMetaObject::invokeMethod(item, "evalExpressionViaWorker", Q_ARG(QVariant, script)));
    waitForWorker(item);

    // can still add plain data
    int count = model.count();
    model.append(plainData);
    QCOMPARE(model.count(), count+1);

    QScriptValue nested = nestedListValue(&s_eng);
    const char *warning = "<Unknown File>: QML ListModel: Cannot add list-type data when modifying or after modification from a worker script";

    QTest::ignoreMessage(QtWarningMsg, warning);
    model.append(nested);

    QTest::ignoreMessage(QtWarningMsg, warning);
    model.insert(0, nested);

    QTest::ignoreMessage(QtWarningMsg, warning);
    model.set(0, nested);

    QCOMPARE(model.count(), count+1);

    delete item;
    qApp->processEvents();
}

void tst_qdeclarativelistmodel::convertNestedToFlat_ok_data()
{
    convertNestedToFlat_fail_data();
}

void tst_qdeclarativelistmodel::static_types_data()
{
    QTest::addColumn<QString>("qml");
    QTest::addColumn<QVariant>("value");

    QTest::newRow("string")
        << "ListElement { foo: \"bar\" }"
        << QVariant(QString("bar"));

    QTest::newRow("real")
        << "ListElement { foo: 10.5 }"
        << QVariant(10.5);

    QTest::newRow("real0")
        << "ListElement { foo: 0 }"
        << QVariant(double(0));

    QTest::newRow("bool")
        << "ListElement { foo: false }"
        << QVariant(false);

    QTest::newRow("bool")
        << "ListElement { foo: true }"
        << QVariant(true);

    QTest::newRow("enum")
        << "ListElement { foo: Text.AlignHCenter }"
        << QVariant(double(QDeclarativeText::AlignHCenter));

    QTest::newRow("real11")
        << "ListElement { foo: 11 }"
        << QVariant(11.0);
}

void tst_qdeclarativelistmodel::static_types()
{
    QFETCH(QString, qml);
    QFETCH(QVariant, value);

    qml = "import QtQuick 1.0\nListModel { " + qml + " }";

    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData(qml.toUtf8(),
                      QUrl::fromLocalFile(QString("dummy.qml")));

    if (value.toString().startsWith("QTBUG-"))
        QEXPECT_FAIL("",value.toString().toLatin1(),Abort);

    QVERIFY(!component.isError());

    QDeclarativeListModel *obj = qobject_cast<QDeclarativeListModel*>(component.create());
    QVERIFY(obj != 0);

    QScriptValue actual = obj->get(0).property(QLatin1String("foo"));

    QCOMPARE(actual.isString(), value.type() == QVariant::String);
    QCOMPARE(actual.isBoolean(), value.type() == QVariant::Bool);
    QCOMPARE(actual.isNumber(), value.type() == QVariant::Double);

    QCOMPARE(actual.toString(), value.toString());

    delete obj;
}

void tst_qdeclarativelistmodel::enumerate()
{
    QDeclarativeEngine eng;
    QDeclarativeComponent component(&eng, QUrl::fromLocalFile(SRCDIR "/data/enumerate.qml"));
    QVERIFY(!component.isError());
    QDeclarativeItem *item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item != 0);
    QStringList r = item->property("result").toString().split(":");
    r.sort();
    QCOMPARE(r[1],QLatin1String("val1=1Y"));
    QCOMPARE(r[2],QLatin1String("val2=2Y"));
    QCOMPARE(r[3],QLatin1String("val3=strY"));
    QCOMPARE(r[4],QLatin1String("val4=falseN"));
    QCOMPARE(r[5],QLatin1String("val5=trueY"));
    delete item;
}


void tst_qdeclarativelistmodel::error_data()
{
    QTest::addColumn<QString>("qml");
    QTest::addColumn<QString>("error");

    QTest::newRow("id not allowed in ListElement")
        << "import QtQuick 1.0\nListModel { ListElement { id: fred } }"
        << "ListElement: cannot use reserved \"id\" property";

    QTest::newRow("id allowed in ListModel")
        << "import QtQuick 1.0\nListModel { id:model }"
        << "";

    QTest::newRow("random properties not allowed in ListModel")
        << "import QtQuick 1.0\nListModel { foo:123 }"
        << "ListModel: undefined property 'foo'";

    QTest::newRow("random properties allowed in ListElement")
        << "import QtQuick 1.0\nListModel { ListElement { foo:123 } }"
        << "";

    QTest::newRow("bindings not allowed in ListElement")
        << "import QtQuick 1.0\nRectangle { id: rect; ListModel { ListElement { foo: rect.color } } }"
        << "ListElement: cannot use script for property value";

    QTest::newRow("random object list properties allowed in ListElement")
        << "import QtQuick 1.0\nListModel { ListElement { foo: [ ListElement { bar: 123 } ] } }"
        << "";

    QTest::newRow("default properties not allowed in ListElement")
        << "import QtQuick 1.0\nListModel { ListElement { Item { } } }"
        << "ListElement: cannot contain nested elements";

    QTest::newRow("QML elements not allowed in ListElement")
        << "import QtQuick 1.0\nListModel { ListElement { a: Item { } } }"
        << "ListElement: cannot contain nested elements";

    QTest::newRow("qualified ListElement supported")
        << "import QtQuick 1.0 as Foo\nFoo.ListModel { Foo.ListElement { a: 123 } }"
        << "";

    QTest::newRow("qualified ListElement required")
        << "import QtQuick 1.0 as Foo\nFoo.ListModel { ListElement { a: 123 } }"
        << "ListElement is not a type";

    QTest::newRow("unknown qualified ListElement not allowed")
        << "import QtQuick 1.0\nListModel { Foo.ListElement { a: 123 } }"
        << "Foo.ListElement - Foo is not a namespace";
}

void tst_qdeclarativelistmodel::error()
{
    QFETCH(QString, qml);
    QFETCH(QString, error);

    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData(qml.toUtf8(),
                      QUrl::fromLocalFile(QString("dummy.qml")));
    if (error.isEmpty()) {
        QVERIFY(!component.isError());
    } else {
        QVERIFY(component.isError());
        QList<QDeclarativeError> errors = component.errors();
        QCOMPARE(errors.count(),1);
        QCOMPARE(errors.at(0).description(),error);
    }
}

void tst_qdeclarativelistmodel::syncError()
{
    QString qml = "import QtQuick 1.0\nListModel { id: lm; Component.onCompleted: lm.sync() }";
    QString error = "file:dummy.qml:2:1: QML ListModel: List sync() can only be called from a WorkerScript";

    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);
    component.setData(qml.toUtf8(),
                      QUrl::fromLocalFile(QString("dummy.qml")));
    QTest::ignoreMessage(QtWarningMsg,error.toUtf8());
    QObject *obj = component.create();
    QVERIFY(obj);
    delete obj;
}

/*
    Test model changes from set() are available to the view
*/
void tst_qdeclarativelistmodel::set()
{
    QDeclarativeEngine engine;
    QDeclarativeListModel model;
    QDeclarativeEngine::setContextForObject(&model,engine.rootContext());
    engine.rootContext()->setContextObject(&model);
    QScriptEngine *seng = QDeclarativeEnginePrivate::getScriptEngine(&engine);

    QScriptValue sv = seng->newObject();
    sv.setProperty("test", QScriptValue(false));
    model.append(sv);

    sv.setProperty("test", QScriptValue(true));
    model.set(0, sv);
    QCOMPARE(model.get(0).property("test").toBool(), true); // triggers creation of model cache
    QCOMPARE(model.data(0, model.roles()[0]), qVariantFromValue(true));

    sv.setProperty("test", QScriptValue(false));
    model.set(0, sv);
    QCOMPARE(model.get(0).property("test").toBool(), false); // tests model cache is updated
    QCOMPARE(model.data(0, model.roles()[0]), qVariantFromValue(false));
}

/*
    Test model changes on values returned by get() are available to the view
*/
void tst_qdeclarativelistmodel::get()
{
    QFETCH(QString, expression);
    QFETCH(int, index);
    QFETCH(QString, roleName);
    QFETCH(QVariant, roleValue);

    QDeclarativeEngine eng;
    QDeclarativeComponent component(&eng);
    component.setData(
        "import QtQuick 1.0\n"
        "ListModel { \n"
            "ListElement { roleA: 100 }\n"
            "ListElement { roleA: 200; roleB: 400 } \n"
            "ListElement { roleA: 200; roleB: 400 } \n"
         "}", QUrl());
    QDeclarativeListModel *model = qobject_cast<QDeclarativeListModel*>(component.create());
    int role = roleFromName(model, roleName);
    QVERIFY(role >= 0);

    QSignalSpy spy(model, SIGNAL(itemsChanged(int, int, QList<int>)));
    QDeclarativeExpression expr(eng.rootContext(), model, expression);
    expr.evaluate();
    QVERIFY(!expr.hasError());

    QCOMPARE(model->data(index, role), roleValue);
    QCOMPARE(spy.count(), 1);

    QList<QVariant> spyResult = spy.takeFirst();
    QCOMPARE(spyResult.at(0).toInt(), index);
    QCOMPARE(spyResult.at(1).toInt(), 1);  // only 1 item is modified at a time
    QCOMPARE(spyResult.at(2).value<QList<int> >(), (QList<int>() << role));
}

void tst_qdeclarativelistmodel::get_data()
{
    QTest::addColumn<QString>("expression");
    QTest::addColumn<int>("index");
    QTest::addColumn<QString>("roleName");
    QTest::addColumn<QVariant>("roleValue");

    QTest::newRow("simple value") << "get(0).roleA = 500" << 0 << "roleA" << QVariant(500);
    QTest::newRow("simple value 2") << "get(1).roleB = 500" << 1 << "roleB" << QVariant(500);

    QVariantMap map;
    map["zzz"] = 123;
    QTest::newRow("object value") << "get(1).roleB = {'zzz':123}" << 1 << "roleB" << QVariant::fromValue(map);

    QVariantList list;
    map.clear(); map["a"] = 50; map["b"] = 500;
    list << map;
    map.clear(); map["c"] = 1000;
    list << map;
    QTest::newRow("list of objects") << "get(2).roleB = [{'a': 50, 'b': 500}, {'c': 1000}]" << 2 << "roleB" << QVariant::fromValue(list);
}

void tst_qdeclarativelistmodel::get_worker()
{
    QFETCH(QString, expression);
    QFETCH(int, index);
    QFETCH(QString, roleName);
    QFETCH(QVariant, roleValue);

    QDeclarativeListModel model;
    QDeclarativeEngine eng;
    QDeclarativeComponent component(&eng, QUrl::fromLocalFile(SRCDIR "/data/model.qml"));
    QDeclarativeItem *item = createWorkerTest(&eng, &component, &model);
    QVERIFY(item != 0);
    QScriptEngine *seng = QDeclarativeEnginePrivate::getScriptEngine(&eng);

    // Add some values like get() test
    QScriptValue sv = seng->newObject();
    sv.setProperty(QLatin1String("roleA"), seng->newVariant(QVariant::fromValue(100)));
    model.append(sv);
    sv = seng->newObject();
    sv.setProperty(QLatin1String("roleA"), seng->newVariant(QVariant::fromValue(200)));
    sv.setProperty(QLatin1String("roleB"), seng->newVariant(QVariant::fromValue(400)));
    model.append(sv);
    model.append(sv);
    int role = roleFromName(&model, roleName);
    QVERIFY(role >= 0);

    const char *warning = "<Unknown File>: QML ListModel: Cannot add list-type data when modifying or after modification from a worker script";
    if (roleValue.type() == QVariant::List || roleValue.type() == QVariant::Map)
        QTest::ignoreMessage(QtWarningMsg, warning);
    QSignalSpy spy(&model, SIGNAL(itemsChanged(int, int, QList<int>)));

    // in the worker thread, change the model data and call sync()
    QVERIFY(QMetaObject::invokeMethod(item, "evalExpressionViaWorker",
            Q_ARG(QVariant, QStringList(expression))));
    waitForWorker(item);

    // see if we receive the model changes in the main thread's model
    if (roleValue.type() == QVariant::List || roleValue.type() == QVariant::Map) {
        QVERIFY(model.data(index, role) != roleValue);
        QCOMPARE(spy.count(), 0);
    } else {
        QCOMPARE(model.data(index, role), roleValue);
        QCOMPARE(spy.count(), 1);

        QList<QVariant> spyResult = spy.takeFirst();
        QCOMPARE(spyResult.at(0).toInt(), index);
        QCOMPARE(spyResult.at(1).toInt(), 1);  // only 1 item is modified at a time
        QVERIFY(spyResult.at(2).value<QList<int> >().contains(role));
    }
}

void tst_qdeclarativelistmodel::get_worker_data()
{
    get_data();
}

/*
    Test that the tests run in get() also work for nested list data
*/
void tst_qdeclarativelistmodel::get_nested()
{
    QFETCH(QString, expression);
    QFETCH(int, index);
    QFETCH(QString, roleName);
    QFETCH(QVariant, roleValue);

    QDeclarativeEngine eng;
    QDeclarativeComponent component(&eng);
    component.setData(
        "import QtQuick 1.0\n"
        "ListModel { \n"
            "ListElement {\n"
                "listRoleA: [\n"
                    "ListElement { roleA: 100 },\n"
                    "ListElement { roleA: 200; roleB: 400 },\n"
                    "ListElement { roleA: 200; roleB: 400 } \n"
                "]\n"
            "}\n"
            "ListElement {\n"
                "listRoleA: [\n"
                    "ListElement { roleA: 100 },\n"
                    "ListElement { roleA: 200; roleB: 400 },\n"
                    "ListElement { roleA: 200; roleB: 400 } \n"
                "]\n"
                "listRoleB: [\n"
                    "ListElement { roleA: 100 },\n"
                    "ListElement { roleA: 200; roleB: 400 },\n"
                    "ListElement { roleA: 200; roleB: 400 } \n"
                "]\n"
                "listRoleC: [\n"
                    "ListElement { roleA: 100 },\n"
                    "ListElement { roleA: 200; roleB: 400 },\n"
                    "ListElement { roleA: 200; roleB: 400 } \n"
                "]\n"
            "}\n"
         "}", QUrl());
    QDeclarativeListModel *model = qobject_cast<QDeclarativeListModel*>(component.create());
    QVERIFY(component.errorString().isEmpty());
    QDeclarativeListModel *childModel;

    // Test setting the inner list data for:
    //  get(0).listRoleA
    //  get(1).listRoleA
    //  get(1).listRoleB
    //  get(1).listRoleC

    QList<QPair<int, QString> > testData;
    testData << qMakePair(0, QString("listRoleA"));
    testData << qMakePair(1, QString("listRoleA"));
    testData << qMakePair(1, QString("listRoleB"));
    testData << qMakePair(1, QString("listRoleC"));

    for (int i=0; i<testData.count(); i++) {
        int outerListIndex = testData[i].first;
        QString outerListRoleName = testData[i].second;
        int outerListRole = roleFromName(model, outerListRoleName);
        QVERIFY(outerListRole >= 0);

        childModel = qobject_cast<QDeclarativeListModel*>(model->data(outerListIndex, outerListRole).value<QObject*>());
        QVERIFY(childModel);

        QString extendedExpression = QString("get(%1).%2.%3").arg(outerListIndex).arg(outerListRoleName).arg(expression);
        QDeclarativeExpression expr(eng.rootContext(), model, extendedExpression);

        QSignalSpy spy(childModel, SIGNAL(itemsChanged(int, int, QList<int>)));
        expr.evaluate();
        QVERIFY(!expr.hasError());

        int role = roleFromName(childModel, roleName);
        QVERIFY(role >= 0);
        QCOMPARE(childModel->data(index, role), roleValue);
        QCOMPARE(spy.count(), 1);

        QList<QVariant> spyResult = spy.takeFirst();
        QCOMPARE(spyResult.at(0).toInt(), index);
        QCOMPARE(spyResult.at(1).toInt(), 1);  // only 1 item is modified at a time
        QCOMPARE(spyResult.at(2).value<QList<int> >(), (QList<int>() << role));
    }
}

void tst_qdeclarativelistmodel::get_nested_data()
{
    get_data();
}

//QTBUG-13754
void tst_qdeclarativelistmodel::crash_model_with_multiple_roles()
{
    QDeclarativeEngine eng;
    QDeclarativeComponent component(&eng, QUrl::fromLocalFile(SRCDIR "/data/multipleroles.qml"));
    QObject *rootItem = component.create();
    QVERIFY(component.errorString().isEmpty());
    QVERIFY(rootItem != 0);
    QDeclarativeListModel *model = rootItem->findChild<QDeclarativeListModel*>("listModel");
    QVERIFY(model != 0);

    // used to cause a crash in QDeclarativeVisualDataModel
    model->setProperty(0, "black", true);
}

//QTBUG-15190
void tst_qdeclarativelistmodel::set_model_cache()
{
    QDeclarativeEngine eng;
    QDeclarativeComponent component(&eng, QUrl::fromLocalFile(SRCDIR "/data/setmodelcachelist.qml"));
    QObject *model = component.create();
    QVERIFY2(component.errorString().isEmpty(), QTest::toString(component.errorString()));
    QVERIFY(model != 0);
    QVERIFY(model->property("ok").toBool());
}

void tst_qdeclarativelistmodel::property_changes()
{
    QFETCH(QString, script_setup);
    QFETCH(QString, script_change);
    QFETCH(QString, roleName);
    QFETCH(int, listIndex);
    QFETCH(bool, itemsChanged);
    QFETCH(QString, testExpression);

    QDeclarativeEngine engine;
    QDeclarativeListModel model;
    QDeclarativeEngine::setContextForObject(&model, engine.rootContext());
    engine.rootContext()->setContextObject(&model);

    QDeclarativeExpression expr(engine.rootContext(), &model, script_setup);
    expr.evaluate();
    QVERIFY2(!expr.hasError(), QTest::toString(expr.error().toString()));

    QString signalHandler = "on" + QString(roleName[0].toUpper()) + roleName.mid(1, roleName.length()) + "Changed:";
    QString qml = "import QtQuick 1.0\n"
                  "Connections {\n"
                        "property bool gotSignal: false\n"
                        "target: model.get(0)\n"
                        + signalHandler + " gotSignal = true\n"
                  "}\n";
    QDeclarativeComponent component(&engine);
    component.setData(qml.toUtf8(), QUrl::fromLocalFile(""));
    engine.rootContext()->setContextProperty("model", &model);
    QObject *connectionsObject = component.create();
    QVERIFY2(component.errorString().isEmpty(), QTest::toString(component.errorString()));

    QSignalSpy spyItemsChanged(&model, SIGNAL(itemsChanged(int, int, QList<int>)));

    expr.setExpression(script_change);
    expr.evaluate();
    QVERIFY2(!expr.hasError(), QTest::toString(expr.error().toString()));

    // test the object returned by get() emits the correct signals
    QCOMPARE(connectionsObject->property("gotSignal").toBool(), itemsChanged);

    // test itemsChanged() is emitted correctly
    if (itemsChanged) {
        QCOMPARE(spyItemsChanged.count(), 1);
        QCOMPARE(spyItemsChanged.at(0).at(0).toInt(), listIndex);
        QCOMPARE(spyItemsChanged.at(0).at(1).toInt(), 1);
    } else {
        QCOMPARE(spyItemsChanged.count(), 0);
    }

    expr.setExpression(testExpression);
    QCOMPARE(expr.evaluate().toBool(), true);

    delete connectionsObject;
}

void tst_qdeclarativelistmodel::property_changes_data()
{
    QTest::addColumn<QString>("script_setup");
    QTest::addColumn<QString>("script_change");
    QTest::addColumn<QString>("roleName");
    QTest::addColumn<int>("listIndex");
    QTest::addColumn<bool>("itemsChanged");
    QTest::addColumn<QString>("testExpression");

    QTest::newRow("set: plain") << "append({'a':123, 'b':456, 'c':789});" << "set(0,{'b':123});"
            << "b" << 0 << true << "get(0).b == 123";
    QTest::newRow("setProperty: plain") << "append({'a':123, 'b':456, 'c':789});" << "setProperty(0, 'b', 123);"
            << "b" << 0 << true << "get(0).b == 123";

    QTest::newRow("set: plain, no changes") << "append({'a':123, 'b':456, 'c':789});" << "set(0,{'b':456});"
            << "b" << 0 << false << "get(0).b == 456";
    QTest::newRow("setProperty: plain, no changes") << "append({'a':123, 'b':456, 'c':789});" << "setProperty(0, 'b', 456);"
            << "b" << 0 << false << "get(0).b == 456";

    // Following tests only call set() since setProperty() only allows plain
    // values, not lists, as the argument.
    // Note that when a list is changed, itemsChanged() is currently always
    // emitted regardless of whether it actually changed or not.

    QTest::newRow("nested-set: list, new size") << "append({'a':123, 'b':[{'a':1},{'a':2},{'a':3}], 'c':789});" << "set(0,{'b':[{'a':1},{'a':2}]});"
            << "b" << 0 << true << "get(0).b.get(0).a == 1 && get(0).b.get(1).a == 2";

    QTest::newRow("nested-set: list, empty -> non-empty") << "append({'a':123, 'b':[], 'c':789});" << "set(0,{'b':[{'a':1},{'a':2},{'a':3}]});"
            << "b" << 0 << true << "get(0).b.get(0).a == 1 && get(0).b.get(1).a == 2 && get(0).b.get(2).a == 3";

    QTest::newRow("nested-set: list, non-empty -> empty") << "append({'a':123, 'b':[{'a':1},{'a':2},{'a':3}], 'c':789});" << "set(0,{'b':[]});"
            << "b" << 0 << true << "get(0).b.count == 0";

    QTest::newRow("nested-set: list, same size, different values") << "append({'a':123, 'b':[{'a':1},{'a':2},{'a':3}], 'c':789});" << "set(0,{'b':[{'a':1},{'a':222},{'a':3}]});"
            << "b" << 0 << true << "get(0).b.get(0).a == 1 && get(0).b.get(1).a == 222 && get(0).b.get(2).a == 3";

    QTest::newRow("nested-set: list, no changes") << "append({'a':123, 'b':[{'a':1},{'a':2},{'a':3}], 'c':789});" << "set(0,{'b':[{'a':1},{'a':2},{'a':3}]});"
            << "b" << 0 << true << "get(0).b.get(0).a == 1 && get(0).b.get(1).a == 2 && get(0).b.get(2).a == 3";

    QTest::newRow("nested-set: list, no changes, empty") << "append({'a':123, 'b':[], 'c':789});" << "set(0,{'b':[]});"
            << "b" << 0 << true << "get(0).b.count == 0";
}


void tst_qdeclarativelistmodel::property_changes_worker()
{
    // nested models are not supported when WorkerScript is involved
    if (QByteArray(QTest::currentDataTag()).startsWith("nested-"))
        return;

    QFETCH(QString, script_setup);
    QFETCH(QString, script_change);
    QFETCH(QString, roleName);
    QFETCH(int, listIndex);
    QFETCH(bool, itemsChanged);

    QDeclarativeListModel model;
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/model.qml"));
    QVERIFY2(component.errorString().isEmpty(), component.errorString().toUtf8());
    QDeclarativeItem *item = createWorkerTest(&engine, &component, &model);
    QVERIFY(item != 0);

    QDeclarativeExpression expr(engine.rootContext(), &model, script_setup);
    expr.evaluate();
    QVERIFY2(!expr.hasError(), QTest::toString(expr.error().toString()));

    QSignalSpy spyItemsChanged(&model, SIGNAL(itemsChanged(int, int, QList<int>)));

    QVERIFY(QMetaObject::invokeMethod(item, "evalExpressionViaWorker",
            Q_ARG(QVariant, QStringList(script_change))));
    waitForWorker(item);

    // test itemsChanged() is emitted correctly
    if (itemsChanged) {
        QCOMPARE(spyItemsChanged.count(), 1);
        QCOMPARE(spyItemsChanged.at(0).at(0).toInt(), listIndex);
        QCOMPARE(spyItemsChanged.at(0).at(1).toInt(), 1);
    } else {
        QCOMPARE(spyItemsChanged.count(), 0);
    }

    delete item;
    qApp->processEvents();
}

void tst_qdeclarativelistmodel::property_changes_worker_data()
{
    property_changes_data();
}

void tst_qdeclarativelistmodel::clear()
{
    QDeclarativeEngine engine;
    QDeclarativeListModel model;
    QDeclarativeEngine::setContextForObject(&model, engine.rootContext());
    engine.rootContext()->setContextObject(&model);

    QScriptEngine *seng = QDeclarativeEnginePrivate::getScriptEngine(&engine);
    QScriptValue sv = seng->newObject();
    QVariant result;

    model.clear();
    QCOMPARE(model.count(), 0);

    sv.setProperty("propertyA", "value a");
    sv.setProperty("propertyB", "value b");
    model.append(sv);
    QCOMPARE(model.count(), 1);

    model.clear();
    QCOMPARE(model.count(), 0);

    model.append(sv);
    model.append(sv);
    QCOMPARE(model.count(), 2);

    model.clear();
    QCOMPARE(model.count(), 0);

    // clearing does not remove the roles
    sv.setProperty("propertyC", "value c");
    model.append(sv);
    QList<int> roles = model.roles();
    model.clear();
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.roles(), roles);
    QStringList roleNames = QStringList()
            << model.toString(roles[0])
            << model.toString(roles[1])
            << model.toString(roles[2]);
    roleNames.sort();
    QCOMPARE(roleNames[0], QString("propertyA"));
    QCOMPARE(roleNames[1], QString("propertyB"));
    QCOMPARE(roleNames[2], QString("propertyC"));
}

QTEST_MAIN(tst_qdeclarativelistmodel)

#include "tst_qdeclarativelistmodel.moc"
