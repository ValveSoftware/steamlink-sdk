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
#include <qtest.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquicktext_p.h>
#include <QtQml/private/qqmlengine_p.h>
#include <QtQml/private/qqmllistmodel_p.h>
#include <QtQml/private/qqmlexpression_p.h>
#include <QQmlComponent>

#include <QtCore/qtimer.h>
#include <QtCore/qdebug.h>
#include <QtCore/qtranslator.h>
#include <QSignalSpy>

#include "../../shared/util.h"

Q_DECLARE_METATYPE(QList<int>)
Q_DECLARE_METATYPE(QList<QVariantHash>)

#define RUNEVAL(object, string) \
    QVERIFY(QMetaObject::invokeMethod(object, "runEval", Q_ARG(QVariant, QString(string))));

inline QVariant runexpr(QQmlEngine *engine, const QString &str)
{
    QQmlExpression expr(engine->rootContext(), 0, str);
    return expr.evaluate();
}

#define RUNEXPR(string) runexpr(&engine, QString(string))

static bool isValidErrorMessage(const QString &msg, bool dynamicRoleTest)
{
    bool valid = true;

    if (msg.isEmpty()) {
        valid = false;
    } else if (dynamicRoleTest) {
        if (msg.contains("Can't assign to existing role") || msg.contains("Can't create role for unsupported data type"))
            valid = false;
    }

    return valid;
}

class tst_qqmllistmodelworkerscript : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmllistmodelworkerscript()
    {
        qRegisterMetaType<QVector<int> >();
    }

private:
    int roleFromName(const QQmlListModel *model, const QString &roleName);
    QQuickItem *createWorkerTest(QQmlEngine *eng, QQmlComponent *component, QQmlListModel *model);
    void waitForWorker(QQuickItem *item);

    static bool compareVariantList(const QVariantList &testList, QVariant object);

private slots:
    void dynamic_data();
    void dynamic_worker_data();
    void dynamic_worker();
    void dynamic_worker_sync_data();
    void dynamic_worker_sync();
    void get_data();
    void get_worker();
    void get_worker_data();
    void property_changes_data();
    void property_changes_worker();
    void property_changes_worker_data();
    void worker_sync_data();
    void worker_sync();
    void worker_remove_element_data();
    void worker_remove_element();
    void worker_remove_list_data();
    void worker_remove_list();
    void dynamic_role_data();
    void dynamic_role();
};

bool tst_qqmllistmodelworkerscript::compareVariantList(const QVariantList &testList, QVariant object)
{
    bool allOk = true;

    QQmlListModel *model = qobject_cast<QQmlListModel *>(object.value<QObject *>());
    if (model == 0)
        return false;

    if (model->count() != testList.count())
        return false;

    for (int i=0 ; i < testList.count() ; ++i) {
        const QVariant &testVariant = testList.at(i);
        if (testVariant.type() != QVariant::Map)
            return false;
        const QVariantMap &map = testVariant.toMap();

        const QHash<int, QByteArray> roleNames = model->roleNames();

        QVariantMap::const_iterator it = map.begin();
        QVariantMap::const_iterator end = map.end();

        while (it != end) {
            const QString &testKey = it.key();
            const QVariant &testData = it.value();

            int roleIndex = roleNames.key(testKey.toUtf8(), -1);
            if (roleIndex == -1)
                return false;

            const QVariant &modelData = model->data(model->index(i, 0, QModelIndex()), roleIndex);

            if (testData.type() == QVariant::List) {
                const QVariantList &subList = testData.toList();
                allOk = allOk && compareVariantList(subList, modelData);
            } else {
                allOk = allOk && (testData == modelData);
            }

            ++it;
        }
    }

    return allOk;
}

int tst_qqmllistmodelworkerscript::roleFromName(const QQmlListModel *model, const QString &roleName)
{
    return model->roleNames().key(roleName.toUtf8(), -1);
}

QQuickItem *tst_qqmllistmodelworkerscript::createWorkerTest(QQmlEngine *eng, QQmlComponent *component, QQmlListModel *model)
{
    QQuickItem *item = qobject_cast<QQuickItem*>(component->create());
    QQmlEngine::setContextForObject(model, eng->rootContext());
    if (item)
        item->setProperty("model", qVariantFromValue(model));
    return item;
}

void tst_qqmllistmodelworkerscript::waitForWorker(QQuickItem *item)
{
    QQmlProperty prop(item, "done");
    QVERIFY(prop.isValid());
    if (prop.read().toBool())
        return; // already finished

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));

    QVERIFY(prop.connectNotifySignal(&loop, SLOT(quit())));
    timer.start(10000);
    loop.exec();
    QVERIFY(timer.isActive());
    QVERIFY(prop.read().toBool());
}

void tst_qqmllistmodelworkerscript::dynamic_data()
{
    QTest::addColumn<QString>("script");
    QTest::addColumn<int>("result");
    QTest::addColumn<QString>("warning");
    QTest::addColumn<bool>("dynamicRoles");

    for (int i=0 ; i < 2 ; ++i) {
        bool dr = (i != 0);

        // Simple flat model
        QTest::newRow("count") << "count" << 0 << "" << dr;

        QTest::newRow("get1") << "{get(0) === undefined}" << 1 << "" << dr;
        QTest::newRow("get2") << "{get(-1) === undefined}" << 1 << "" << dr;
        QTest::newRow("get3") << "{append({'foo':123});get(0) != undefined}" << 1 << "" << dr;
        QTest::newRow("get4") << "{append({'foo':123});get(0).foo}" << 123 << "" << dr;
        QTest::newRow("get-modify1") << "{append({'foo':123,'bar':456});get(0).foo = 333;get(0).foo}" << 333 << "" << dr;
        QTest::newRow("get-modify2") << "{append({'z':1});append({'foo':123,'bar':456});get(1).bar = 999;get(1).bar}" << 999 << "" << dr;

        QTest::newRow("append1") << "{append({'foo':123});count}" << 1 << "" << dr;
        QTest::newRow("append2") << "{append({'foo':123,'bar':456});count}" << 1 << "" << dr;
        QTest::newRow("append3a") << "{append({'foo':123});append({'foo':456});get(0).foo}" << 123 << "" << dr;
        QTest::newRow("append3b") << "{append({'foo':123});append({'foo':456});get(1).foo}" << 456 << "" << dr;
        QTest::newRow("append4a") << "{append(123)}" << 0 << "<Unknown File>: QML ListModel: append: value is not an object" << dr;
        QTest::newRow("append4b") << "{append([{'foo':123},{'foo':456},{'foo':789}]);count}" << 3 << "" << dr;
        QTest::newRow("append4c") << "{append([{'foo':123},{'foo':456},{'foo':789}]);get(1).foo}" << 456 << "" << dr;

        QTest::newRow("clear1") << "{append({'foo':456});clear();count}" << 0 << "" << dr;
        QTest::newRow("clear2") << "{append({'foo':123});append({'foo':456});clear();count}" << 0 << "" << dr;
        QTest::newRow("clear3") << "{append({'foo':123});clear()}" << 0 << "" << dr;

        QTest::newRow("remove1") << "{append({'foo':123});remove(0);count}" << 0 << "" << dr;
        QTest::newRow("remove2a") << "{append({'foo':123});append({'foo':456});remove(0);count}" << 1 << "" << dr;
        QTest::newRow("remove2b") << "{append({'foo':123});append({'foo':456});remove(0);get(0).foo}" << 456 << "" << dr;
        QTest::newRow("remove2c") << "{append({'foo':123});append({'foo':456});remove(1);get(0).foo}" << 123 << "" << dr;
        QTest::newRow("remove3") << "{append({'foo':123});remove(0)}" << 0 << "" << dr;
        QTest::newRow("remove3a") << "{append({'foo':123});remove(-1);count}" << 1 << "<Unknown File>: QML ListModel: remove: indices [-1 - 0] out of range [0 - 1]" << dr;
        QTest::newRow("remove4a") << "{remove(0)}" << 0 << "<Unknown File>: QML ListModel: remove: indices [0 - 1] out of range [0 - 0]" << dr;
        QTest::newRow("remove4b") << "{append({'foo':123});remove(0);remove(0);count}" << 0 << "<Unknown File>: QML ListModel: remove: indices [0 - 1] out of range [0 - 0]" << dr;
        QTest::newRow("remove4c") << "{append({'foo':123});remove(1);count}" << 1 << "<Unknown File>: QML ListModel: remove: indices [1 - 2] out of range [0 - 1]" << dr;
        QTest::newRow("remove5a") << "{append({'foo':123});append({'foo':456});remove(0,2);count}" << 0 << "" << dr;
        QTest::newRow("remove5b") << "{append({'foo':123});append({'foo':456});remove(0,1);count}" << 1 << "" << dr;
        QTest::newRow("remove5c") << "{append({'foo':123});append({'foo':456});remove(1,1);count}" << 1 << "" << dr;
        QTest::newRow("remove5d") << "{append({'foo':123});append({'foo':456});remove(0,1);get(0).foo}" << 456 << "" << dr;
        QTest::newRow("remove5e") << "{append({'foo':123});append({'foo':456});remove(1,1);get(0).foo}" << 123 << "" << dr;
        QTest::newRow("remove5f") << "{append({'foo':123});append({'foo':456});append({'foo':789});remove(0,1);remove(1,1);get(0).foo}" << 456 << "" << dr;
        QTest::newRow("remove6a") << "{remove();count}" << 0 << "<Unknown File>: QML ListModel: remove: incorrect number of arguments" << dr;
        QTest::newRow("remove6b") << "{remove(1,2,3);count}" << 0 << "<Unknown File>: QML ListModel: remove: incorrect number of arguments" << dr;
        QTest::newRow("remove7a") << "{append({'foo':123});remove(0,0);count}" << 1 << "<Unknown File>: QML ListModel: remove: indices [0 - 0] out of range [0 - 1]" << dr;
        QTest::newRow("remove7b") << "{append({'foo':123});remove(0,-1);count}" << 1 << "<Unknown File>: QML ListModel: remove: indices [0 - -1] out of range [0 - 1]" << dr;

        QTest::newRow("insert1") << "{insert(0,{'foo':123});count}" << 1 << "" << dr;
        QTest::newRow("insert2") << "{insert(1,{'foo':123});count}" << 0 << "<Unknown File>: QML ListModel: insert: index 1 out of range" << dr;
        QTest::newRow("insert3a") << "{append({'foo':123});insert(1,{'foo':456});count}" << 2 << "" << dr;
        QTest::newRow("insert3b") << "{append({'foo':123});insert(1,{'foo':456});get(0).foo}" << 123 << "" << dr;
        QTest::newRow("insert3c") << "{append({'foo':123});insert(1,{'foo':456});get(1).foo}" << 456 << "" << dr;
        QTest::newRow("insert3d") << "{append({'foo':123});insert(0,{'foo':456});get(0).foo}" << 456 << "" << dr;
        QTest::newRow("insert3e") << "{append({'foo':123});insert(0,{'foo':456});get(1).foo}" << 123 << "" << dr;
        QTest::newRow("insert4") << "{append({'foo':123});insert(-1,{'foo':456});count}" << 1 << "<Unknown File>: QML ListModel: insert: index -1 out of range" << dr;
        QTest::newRow("insert5a") << "{insert(0,123)}" << 0 << "<Unknown File>: QML ListModel: insert: value is not an object" << dr;
        QTest::newRow("insert5b") << "{insert(0,[{'foo':11},{'foo':22},{'foo':33}]);count}" << 3 << "" << dr;
        QTest::newRow("insert5c") << "{insert(0,[{'foo':11},{'foo':22},{'foo':33}]);get(2).foo}" << 33 << "" << dr;

        QTest::newRow("set1") << "{append({'foo':123});set(0,{'foo':456});count}" << 1 << "" << dr;
        QTest::newRow("set2") << "{append({'foo':123});set(0,{'foo':456});get(0).foo}" << 456 << "" << dr;
        QTest::newRow("set3a") << "{append({'foo':123,'bar':456});set(0,{'foo':999});get(0).foo}" << 999 << "" << dr;
        QTest::newRow("set3b") << "{append({'foo':123,'bar':456});set(0,{'foo':999});get(0).bar}" << 456 << "" << dr;
        QTest::newRow("set4a") << "{set(0,{'foo':456});count}" << 1 << "" << dr;
        QTest::newRow("set4c") << "{set(-1,{'foo':456})}" << 0 << "<Unknown File>: QML ListModel: set: index -1 out of range" << dr;
        QTest::newRow("set5a") << "{append({'foo':123,'bar':456});set(0,123);count}" << 1 << "<Unknown File>: QML ListModel: set: value is not an object" << dr;
        QTest::newRow("set5b") << "{append({'foo':123,'bar':456});set(0,[1,2,3]);count}" << 1 << "" << dr;
        QTest::newRow("set6") << "{append({'foo':123});set(1,{'foo':456});count}" << 2 << "" << dr;

        QTest::newRow("setprop1") << "{append({'foo':123});setProperty(0,'foo',456);count}" << 1 << "" << dr;
        QTest::newRow("setprop2") << "{append({'foo':123});setProperty(0,'foo',456);get(0).foo}" << 456 << "" << dr;
        QTest::newRow("setprop3a") << "{append({'foo':123,'bar':456});setProperty(0,'foo',999);get(0).foo}" << 999 << "" << dr;
        QTest::newRow("setprop3b") << "{append({'foo':123,'bar':456});setProperty(0,'foo',999);get(0).bar}" << 456 << "" << dr;
        QTest::newRow("setprop4a") << "{setProperty(0,'foo',456)}" << 0 << "<Unknown File>: QML ListModel: set: index 0 out of range" << dr;
        QTest::newRow("setprop4b") << "{setProperty(-1,'foo',456)}" << 0 << "<Unknown File>: QML ListModel: set: index -1 out of range" << dr;
        QTest::newRow("setprop4c") << "{append({'foo':123,'bar':456});setProperty(1,'foo',456);count}" << 1 << "<Unknown File>: QML ListModel: set: index 1 out of range" << dr;
        QTest::newRow("setprop5") << "{append({'foo':123,'bar':456});append({'foo':111});setProperty(1,'bar',222);get(1).bar}" << 222 << "" << dr;

        QTest::newRow("move1a") << "{append({'foo':123});append({'foo':456});move(0,1,1);count}" << 2 << "" << dr;
        QTest::newRow("move1b") << "{append({'foo':123});append({'foo':456});move(0,1,1);get(0).foo}" << 456 << "" << dr;
        QTest::newRow("move1c") << "{append({'foo':123});append({'foo':456});move(0,1,1);get(1).foo}" << 123 << "" << dr;
        QTest::newRow("move1d") << "{append({'foo':123});append({'foo':456});move(1,0,1);get(0).foo}" << 456 << "" << dr;
        QTest::newRow("move1e") << "{append({'foo':123});append({'foo':456});move(1,0,1);get(1).foo}" << 123 << "" << dr;
        QTest::newRow("move2a") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(0,1,2);count}" << 3 << "" << dr;
        QTest::newRow("move2b") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(0,1,2);get(0).foo}" << 789 << "" << dr;
        QTest::newRow("move2c") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(0,1,2);get(1).foo}" << 123 << "" << dr;
        QTest::newRow("move2d") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(0,1,2);get(2).foo}" << 456 << "" << dr;
        QTest::newRow("move3a") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(1,0,3);count}" << 3 << "<Unknown File>: QML ListModel: move: out of range" << dr;
        QTest::newRow("move3b") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(1,-1,1);count}" << 3 << "<Unknown File>: QML ListModel: move: out of range" << dr;
        QTest::newRow("move3c") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(1,0,-1);count}" << 3 << "<Unknown File>: QML ListModel: move: out of range" << dr;
        QTest::newRow("move3d") << "{append({'foo':123});append({'foo':456});append({'foo':789});move(0,3,1);count}" << 3 << "<Unknown File>: QML ListModel: move: out of range" << dr;

        QTest::newRow("large1") << "{append({'a':1,'b':2,'c':3,'d':4,'e':5,'f':6,'g':7,'h':8});get(0).h}" << 8 << "" << dr;

        QTest::newRow("datatypes1") << "{append({'a':1});append({'a':'string'});}" << 0 << "<Unknown File>: Can't assign to existing role 'a' of different type [String -> Number]" << dr;

        QTest::newRow("null") << "{append({'a':null});}" << 0 << "" << dr;
        QTest::newRow("setNull") << "{append({'a':1});set(0, {'a':null});}" << 0 << "" << dr;
        QTest::newRow("setString") << "{append({'a':'hello'});set(0, {'a':'world'});get(0).a == 'world'}" << 1 << "" << dr;
        QTest::newRow("setInt") << "{append({'a':5});set(0, {'a':10});get(0).a}" << 10 << "" << dr;
        QTest::newRow("setNumber") << "{append({'a':6});set(0, {'a':5.5});get(0).a < 5.6}" << 1 << "" << dr;
        QTest::newRow("badType0") << "{append({'a':'hello'});set(0, {'a':1});}" << 0 << "<Unknown File>: Can't assign to existing role 'a' of different type [Number -> String]" << dr;
        QTest::newRow("invalidInsert0") << "{insert(0);}" << 0 << "<Unknown File>: QML ListModel: insert: value is not an object" << dr;
        QTest::newRow("invalidAppend0") << "{append();}" << 0 << "<Unknown File>: QML ListModel: append: value is not an object" << dr;
        QTest::newRow("invalidInsert1") << "{insert(0, 34);}" << 0 << "<Unknown File>: QML ListModel: insert: value is not an object" << dr;
        QTest::newRow("invalidAppend1") << "{append(37);}" << 0 << "<Unknown File>: QML ListModel: append: value is not an object" << dr;

        // QObjects
        QTest::newRow("qobject0") << "{append({'a':dummyItem0});}" << 0 << "" << dr;
        QTest::newRow("qobject1") << "{append({'a':dummyItem0});set(0,{'a':dummyItem1});get(0).a == dummyItem1;}" << 1 << "" << dr;
        QTest::newRow("qobject2") << "{append({'a':dummyItem0});get(0).a == dummyItem0;}" << 1 << "" << dr;
        QTest::newRow("qobject3") << "{append({'a':dummyItem0});append({'b':1});}" << 0 << "" << dr;

        // JS objects
        QTest::newRow("js1") << "{append({'foo':{'prop':1}});count}" << 1 << "" << dr;
        QTest::newRow("js2") << "{append({'foo':{'prop':27}});get(0).foo.prop}" << 27 << "" << dr;
        QTest::newRow("js3") << "{append({'foo':{'prop':27}});append({'bar':1});count}" << 2 << "" << dr;
        QTest::newRow("js4") << "{append({'foo':{'prop':27}});append({'bar':1});set(0, {'foo':{'prop':28}});get(0).foo.prop}" << 28 << "" << dr;
        QTest::newRow("js5") << "{append({'foo':{'prop':27}});append({'bar':1});set(1, {'foo':{'prop':33}});get(1).foo.prop}" << 33 << "" << dr;
        QTest::newRow("js6") << "{append({'foo':{'prop':27}});clear();count}" << 0 << "" << dr;
        QTest::newRow("js7") << "{append({'foo':{'prop':27}});set(0, {'foo':null});count}" << 1 << "" << dr;
        QTest::newRow("js8") << "{append({'foo':{'prop':27}});set(0, {'foo':{'prop2':31}});get(0).foo.prop2}" << 31 << "" << dr;

        // Nested models
        QTest::newRow("nested-append1") << "{append({'foo':123,'bars':[{'a':1},{'a':2},{'a':3}]});count}" << 1 << "" << dr;
        QTest::newRow("nested-append2") << "{append({'foo':123,'bars':[{'a':1},{'a':2},{'a':3}]});get(0).bars.get(1).a}" << 2 << "" << dr;
        QTest::newRow("nested-append3") << "{append({'foo':123,'bars':[{'a':1},{'a':2},{'a':3}]});get(0).bars.append({'a':4});get(0).bars.get(3).a}" << 4 << "" << dr;

        QTest::newRow("nested-insert") << "{append({'foo':123});insert(0,{'bars':[{'a':1},{'b':2},{'c':3}]});get(0).bars.get(0).a}" << 1 << "" << dr;
        QTest::newRow("nested-set") << "{append({'foo':[{'x':1}]});set(0,{'foo':[{'x':123}]});get(0).foo.get(0).x}" << 123 << "" << dr;

        QTest::newRow("nested-count") << "{append({'foo':123,'bars':[{'a':1},{'a':2},{'a':3}]}); get(0).bars.count}" << 3 << "" << dr;
        QTest::newRow("nested-clear") << "{append({'foo':123,'bars':[{'a':1},{'a':2},{'a':3}]}); get(0).bars.clear(); get(0).bars.count}" << 0 << "" << dr;
    }
}

void tst_qqmllistmodelworkerscript::dynamic_worker_data()
{
    dynamic_data();
}

void tst_qqmllistmodelworkerscript::dynamic_worker()
{
    QFETCH(QString, script);
    QFETCH(int, result);
    QFETCH(QString, warning);
    QFETCH(bool, dynamicRoles);

    if (QByteArray(QTest::currentDataTag()).startsWith("qobject"))
        return;

    // This is same as dynamic() except it applies the test to a ListModel called
    // from a WorkerScript.

    QQmlListModel model;
    model.setDynamicRoles(dynamicRoles);
    QQmlEngine eng;
    QQmlComponent component(&eng, testFileUrl("model.qml"));
    QQuickItem *item = createWorkerTest(&eng, &component, &model);
    QVERIFY(item != 0);

    QSignalSpy spyCount(&model, SIGNAL(countChanged()));

    if (script[0] == QLatin1Char('{') && script[script.length()-1] == QLatin1Char('}'))
        script = script.mid(1, script.length() - 2);
    QVariantList operations;
    foreach (const QString &s, script.split(';')) {
        if (!s.isEmpty())
            operations << s;
    }

    if (isValidErrorMessage(warning, dynamicRoles))
        QTest::ignoreMessage(QtWarningMsg, warning.toLatin1());

    QVERIFY(QMetaObject::invokeMethod(item, "evalExpressionViaWorker",
            Q_ARG(QVariant, operations)));
    waitForWorker(item);
    QCOMPARE(QQmlProperty(item, "result").read().toInt(), result);

    if (model.count() > 0)
        QVERIFY(spyCount.count() > 0);

    delete item;
    qApp->processEvents();
}

void tst_qqmllistmodelworkerscript::dynamic_worker_sync_data()
{
    dynamic_data();
}

void tst_qqmllistmodelworkerscript::dynamic_worker_sync()
{
    QFETCH(QString, script);
    QFETCH(int, result);
    QFETCH(QString, warning);
    QFETCH(bool, dynamicRoles);

    if (QByteArray(QTest::currentDataTag()).startsWith("qobject"))
        return;

    // This is the same as dynamic_worker() except that it executes a set of list operations
    // from the worker script, calls sync(), and tests the changes are reflected in the
    // list in the main thread

    QQmlListModel model;
    model.setDynamicRoles(dynamicRoles);
    QQmlEngine eng;
    QQmlComponent component(&eng, testFileUrl("model.qml"));
    QQuickItem *item = createWorkerTest(&eng, &component, &model);
    QVERIFY(item != 0);

    if (script[0] == QLatin1Char('{') && script[script.length()-1] == QLatin1Char('}'))
        script = script.mid(1, script.length() - 2);
    QVariantList operations;
    foreach (const QString &s, script.split(';')) {
        if (!s.isEmpty())
            operations << s;
    }

    if (isValidErrorMessage(warning, dynamicRoles))
        QTest::ignoreMessage(QtWarningMsg, warning.toLatin1());

    // execute a set of commands on the worker list model, then check the
    // changes are reflected in the list model in the main thread
    QVERIFY(QMetaObject::invokeMethod(item, "evalExpressionViaWorker",
            Q_ARG(QVariant, operations.mid(0, operations.length()-1))));
    waitForWorker(item);

    QQmlExpression e(eng.rootContext(), &model, operations.last().toString());
    QCOMPARE(e.evaluate().toInt(), result);

    delete item;
    qApp->processEvents();
}

void tst_qqmllistmodelworkerscript::get_data()
{
    QTest::addColumn<QString>("expression");
    QTest::addColumn<int>("index");
    QTest::addColumn<QString>("roleName");
    QTest::addColumn<QVariant>("roleValue");
    QTest::addColumn<bool>("dynamicRoles");

    for (int i=0 ; i < 2 ; ++i) {
        bool dr = (i != 0);

        QTest::newRow("simple value") << "get(0).roleA = 500" << 0 << "roleA" << QVariant(500) << dr;
        QTest::newRow("simple value 2") << "get(1).roleB = 500" << 1 << "roleB" << QVariant(500) << dr;

        QVariantMap map;
        QVariantList list;
        map.clear(); map["a"] = 50; map["b"] = 500;
        list << map;
        map.clear(); map["c"] = 1000;
        list << map;
        QTest::newRow("list of objects") << "get(2).roleD = [{'a': 50, 'b': 500}, {'c': 1000}]" << 2 << "roleD" << QVariant::fromValue(list) << dr;
    }
}

void tst_qqmllistmodelworkerscript::get_worker()
{
    QFETCH(QString, expression);
    QFETCH(int, index);
    QFETCH(QString, roleName);
    QFETCH(QVariant, roleValue);
    QFETCH(bool, dynamicRoles);

    QQmlListModel model;
    model.setDynamicRoles(dynamicRoles);
    QQmlEngine eng;
    QQmlComponent component(&eng, testFileUrl("model.qml"));
    QQuickItem *item = createWorkerTest(&eng, &component, &model);
    QVERIFY(item != 0);

    // Add some values like get() test
    RUNEVAL(item, "model.append({roleA: 100})");
    RUNEVAL(item, "model.append({roleA: 200, roleB: 400})");
    RUNEVAL(item, "model.append({roleA: 200, roleB: 400})");
    RUNEVAL(item, "model.append({roleC: {} })");
    RUNEVAL(item, "model.append({roleD: [ { a:1, b:2 }, { c: 3 } ] })");

    int role = roleFromName(&model, roleName);
    QVERIFY(role >= 0);

    QSignalSpy spy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));

    // in the worker thread, change the model data and call sync()
    QVERIFY(QMetaObject::invokeMethod(item, "evalExpressionViaWorker",
            Q_ARG(QVariant, QStringList(expression))));
    waitForWorker(item);

    // see if we receive the model changes in the main thread's model
    if (roleValue.type() == QVariant::List) {
        const QVariantList &list = roleValue.toList();
        QVERIFY(compareVariantList(list, model.data(index, role)));
    } else {
        QCOMPARE(model.data(index, role), roleValue);
    }

    QCOMPARE(spy.count(), 1);

    QList<QVariant> spyResult = spy.takeFirst();
    QCOMPARE(spyResult.at(0).value<QModelIndex>(), model.index(index, 0, QModelIndex()));
    QCOMPARE(spyResult.at(1).value<QModelIndex>(), model.index(index, 0, QModelIndex()));  // only 1 item is modified at a time
    QVERIFY(spyResult.at(2).value<QVector<int> >().contains(role));

    delete item;
}

void tst_qqmllistmodelworkerscript::get_worker_data()
{
    get_data();
}

void tst_qqmllistmodelworkerscript::property_changes_data()
{
    QTest::addColumn<QString>("script_setup");
    QTest::addColumn<QString>("script_change");
    QTest::addColumn<QString>("roleName");
    QTest::addColumn<int>("listIndex");
    QTest::addColumn<bool>("itemsChanged");
    QTest::addColumn<QString>("testExpression");
    QTest::addColumn<bool>("dynamicRoles");

    for (int i=0 ; i < 2 ; ++i) {
        bool dr = (i != 0);

        QTest::newRow("set: plain") << "append({'a':123, 'b':456, 'c':789});" << "set(0,{'b':123});"
                << "b" << 0 << true << "get(0).b == 123" << dr;
        QTest::newRow("setProperty: plain") << "append({'a':123, 'b':456, 'c':789});" << "setProperty(0, 'b', 123);"
                << "b" << 0 << true << "get(0).b == 123" << dr;

        QTest::newRow("set: plain, no changes") << "append({'a':123, 'b':456, 'c':789});" << "set(0,{'b':456});"
                << "b" << 0 << false << "get(0).b == 456" << dr;
        QTest::newRow("setProperty: plain, no changes") << "append({'a':123, 'b':456, 'c':789});" << "setProperty(0, 'b', 456);"
                << "b" << 0 << false << "get(0).b == 456" << dr;

        QTest::newRow("set: inserted item")
                << "{append({'a':123, 'b':456, 'c':789}); get(0); insert(0, {'a':0, 'b':0, 'c':0});}"
                << "set(1, {'a':456});"
                << "a" << 1 << true << "get(1).a == 456" << dr;
        QTest::newRow("setProperty: inserted item")
                << "{append({'a':123, 'b':456, 'c':789}); get(0); insert(0, {'a':0, 'b':0, 'c':0});}"
                << "setProperty(1, 'a', 456);"
                << "a" << 1 << true << "get(1).a == 456" << dr;
        QTest::newRow("get: inserted item")
                << "{append({'a':123, 'b':456, 'c':789}); get(0); insert(0, {'a':0, 'b':0, 'c':0});}"
                << "get(1).a = 456;"
                << "a" << 1 << true << "get(1).a == 456" << dr;
        QTest::newRow("set: removed item")
                << "{append({'a':0, 'b':0, 'c':0}); append({'a':123, 'b':456, 'c':789}); get(1); remove(0);}"
                << "set(0, {'a':456});"
                << "a" << 0 << true << "get(0).a == 456" << dr;
        QTest::newRow("setProperty: removed item")
                << "{append({'a':0, 'b':0, 'c':0}); append({'a':123, 'b':456, 'c':789}); get(1); remove(0);}"
                << "setProperty(0, 'a', 456);"
                << "a" << 0 << true << "get(0).a == 456" << dr;
        QTest::newRow("get: removed item")
                << "{append({'a':0, 'b':0, 'c':0}); append({'a':123, 'b':456, 'c':789}); get(1); remove(0);}"
                << "get(0).a = 456;"
                << "a" << 0 << true << "get(0).a == 456" << dr;

        // Following tests only call set() since setProperty() only allows plain
        // values, not lists, as the argument.
        // Note that when a list is changed, itemsChanged() is currently always
        // emitted regardless of whether it actually changed or not.

        QTest::newRow("nested-set: list, new size") << "append({'a':123, 'b':[{'a':1},{'a':2},{'a':3}], 'c':789});" << "set(0,{'b':[{'a':1},{'a':2}]});"
                << "b" << 0 << true << "get(0).b.get(0).a == 1 && get(0).b.get(1).a == 2" << dr;

        QTest::newRow("nested-set: list, empty -> non-empty") << "append({'a':123, 'b':[], 'c':789});" << "set(0,{'b':[{'a':1},{'a':2},{'a':3}]});"
                << "b" << 0 << true << "get(0).b.get(0).a == 1 && get(0).b.get(1).a == 2 && get(0).b.get(2).a == 3" << dr;

        QTest::newRow("nested-set: list, non-empty -> empty") << "append({'a':123, 'b':[{'a':1},{'a':2},{'a':3}], 'c':789});" << "set(0,{'b':[]});"
                << "b" << 0 << true << "get(0).b.count == 0" << dr;

        QTest::newRow("nested-set: list, same size, different values") << "append({'a':123, 'b':[{'a':1},{'a':2},{'a':3}], 'c':789});" << "set(0,{'b':[{'a':1},{'a':222},{'a':3}]});"
                << "b" << 0 << true << "get(0).b.get(0).a == 1 && get(0).b.get(1).a == 222 && get(0).b.get(2).a == 3" << dr;

        QTest::newRow("nested-set: list, no changes") << "append({'a':123, 'b':[{'a':1},{'a':2},{'a':3}], 'c':789});" << "set(0,{'b':[{'a':1},{'a':2},{'a':3}]});"
                << "b" << 0 << true << "get(0).b.get(0).a == 1 && get(0).b.get(1).a == 2 && get(0).b.get(2).a == 3" << dr;

        QTest::newRow("nested-set: list, no changes, empty") << "append({'a':123, 'b':[], 'c':789});" << "set(0,{'b':[]});"
                << "b" << 0 << true << "get(0).b.count == 0" << dr;
    }
}

void tst_qqmllistmodelworkerscript::property_changes_worker()
{
    QFETCH(QString, script_setup);
    QFETCH(QString, script_change);
    QFETCH(QString, roleName);
    QFETCH(int, listIndex);
    QFETCH(bool, itemsChanged);
    QFETCH(bool, dynamicRoles);

    QQmlListModel model;
    model.setDynamicRoles(dynamicRoles);
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("model.qml"));
    QVERIFY2(component.errorString().isEmpty(), component.errorString().toUtf8());
    QQuickItem *item = createWorkerTest(&engine, &component, &model);
    QVERIFY(item != 0);

    QQmlExpression expr(engine.rootContext(), &model, script_setup);
    expr.evaluate();
    QVERIFY2(!expr.hasError(), QTest::toString(expr.error().toString()));

    QSignalSpy spyItemsChanged(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));

    QVERIFY(QMetaObject::invokeMethod(item, "evalExpressionViaWorker",
            Q_ARG(QVariant, QStringList(script_change))));
    waitForWorker(item);

    // test itemsChanged() is emitted correctly
    if (itemsChanged) {
        QCOMPARE(spyItemsChanged.count(), 1);
        QCOMPARE(spyItemsChanged.at(0).at(0).value<QModelIndex>(), model.index(listIndex, 0, QModelIndex()));
        QCOMPARE(spyItemsChanged.at(0).at(1).value<QModelIndex>(), model.index(listIndex, 0, QModelIndex()));
    } else {
        QCOMPARE(spyItemsChanged.count(), 0);
    }

    delete item;
    qApp->processEvents();
}

void tst_qqmllistmodelworkerscript::property_changes_worker_data()
{
    property_changes_data();
}

void tst_qqmllistmodelworkerscript::worker_sync_data()
{
    QTest::addColumn<bool>("dynamicRoles");

    QTest::newRow("staticRoles") << false;
    QTest::newRow("dynamicRoles") << true;
}

void tst_qqmllistmodelworkerscript::worker_sync()
{
    QFETCH(bool, dynamicRoles);

    QQmlListModel model;
    model.setDynamicRoles(dynamicRoles);
    QQmlEngine eng;
    QQmlComponent component(&eng, testFileUrl("workersync.qml"));
    QQuickItem *item = createWorkerTest(&eng, &component, &model);
    QVERIFY(item != 0);

    QCOMPARE(model.count(), 0);

    QVERIFY(QMetaObject::invokeMethod(item, "addItem0"));

    QCOMPARE(model.count(), 2);
    QVariant childData = model.data(0, 0);
    QQmlListModel *childModel = qobject_cast<QQmlListModel *>(childData.value<QObject *>());
    QVERIFY(childModel);
    QCOMPARE(childModel->count(), 1);

    QSignalSpy spyModelInserted(&model, SIGNAL(rowsInserted(QModelIndex,int,int)));
    QSignalSpy spyChildInserted(childModel, SIGNAL(rowsInserted(QModelIndex,int,int)));

    QVERIFY(QMetaObject::invokeMethod(item, "addItemViaWorker"));
    waitForWorker(item);

    QCOMPARE(model.count(), 2);
    QCOMPARE(childModel->count(), 1);
    QCOMPARE(spyModelInserted.count(), 0);
    QCOMPARE(spyChildInserted.count(), 0);

    QVERIFY(QMetaObject::invokeMethod(item, "doSync"));
    waitForWorker(item);

    QCOMPARE(model.count(), 2);
    QCOMPARE(childModel->count(), 2);
    QCOMPARE(spyModelInserted.count(), 0);
    QCOMPARE(spyChildInserted.count(), 1);

    QVERIFY(QMetaObject::invokeMethod(item, "addItemViaWorker"));
    waitForWorker(item);

    QCOMPARE(model.count(), 2);
    QCOMPARE(childModel->count(), 2);
    QCOMPARE(spyModelInserted.count(), 0);
    QCOMPARE(spyChildInserted.count(), 1);

    QVERIFY(QMetaObject::invokeMethod(item, "doSync"));
    waitForWorker(item);

    QCOMPARE(model.count(), 2);
    QCOMPARE(childModel->count(), 3);
    QCOMPARE(spyModelInserted.count(), 0);
    QCOMPARE(spyChildInserted.count(), 2);

    delete item;
    qApp->processEvents();
}

void tst_qqmllistmodelworkerscript::worker_remove_element_data()
{
    worker_sync_data();
}

void tst_qqmllistmodelworkerscript::worker_remove_element()
{
    QFETCH(bool, dynamicRoles);

    QQmlListModel model;
    model.setDynamicRoles(dynamicRoles);
    QQmlEngine eng;
    QQmlComponent component(&eng, testFileUrl("workerremoveelement.qml"));
    QQuickItem *item = createWorkerTest(&eng, &component, &model);
    QVERIFY(item != 0);

    QSignalSpy spyModelRemoved(&model, SIGNAL(rowsRemoved(QModelIndex,int,int)));

    QCOMPARE(model.count(), 0);
    QCOMPARE(spyModelRemoved.count(), 0);

    QVERIFY(QMetaObject::invokeMethod(item, "addItem"));

    QCOMPARE(model.count(), 1);

    QVERIFY(QMetaObject::invokeMethod(item, "removeItemViaWorker"));
    waitForWorker(item);

    QCOMPARE(model.count(), 1);
    QCOMPARE(spyModelRemoved.count(), 0);

    QVERIFY(QMetaObject::invokeMethod(item, "doSync"));
    waitForWorker(item);

    QCOMPARE(model.count(), 0);
    QCOMPARE(spyModelRemoved.count(), 1);

    delete item;
    qApp->processEvents();

    {
        //don't crash if model was deleted earlier
        QQmlListModel* model = new QQmlListModel;
        model->setDynamicRoles(dynamicRoles);
        QQmlEngine eng;
        QQmlComponent component(&eng, testFileUrl("workerremoveelement.qml"));
        QQuickItem *item = createWorkerTest(&eng, &component, model);
        QVERIFY(item != 0);

        QVERIFY(QMetaObject::invokeMethod(item, "addItem"));

        QCOMPARE(model->count(), 1);

        QVERIFY(QMetaObject::invokeMethod(item, "removeItemViaWorker"));
        QVERIFY(QMetaObject::invokeMethod(item, "doSync"));
        delete model;
        qApp->processEvents(); //must not crash here
        waitForWorker(item);

        delete item;
    }
}

void tst_qqmllistmodelworkerscript::worker_remove_list_data()
{
    worker_sync_data();
}

void tst_qqmllistmodelworkerscript::worker_remove_list()
{
    QFETCH(bool, dynamicRoles);

    QQmlListModel model;
    model.setDynamicRoles(dynamicRoles);
    QQmlEngine eng;
    QQmlComponent component(&eng, testFileUrl("workerremovelist.qml"));
    QQuickItem *item = createWorkerTest(&eng, &component, &model);
    QVERIFY(item != 0);

    QSignalSpy spyModelRemoved(&model, SIGNAL(rowsRemoved(QModelIndex,int,int)));

    QCOMPARE(model.count(), 0);
    QCOMPARE(spyModelRemoved.count(), 0);

    QVERIFY(QMetaObject::invokeMethod(item, "addList"));

    QCOMPARE(model.count(), 1);

    QVERIFY(QMetaObject::invokeMethod(item, "removeListViaWorker"));
    waitForWorker(item);

    QCOMPARE(model.count(), 1);
    QCOMPARE(spyModelRemoved.count(), 0);

    QVERIFY(QMetaObject::invokeMethod(item, "doSync"));
    waitForWorker(item);

    QCOMPARE(model.count(), 0);
    QCOMPARE(spyModelRemoved.count(), 1);

    delete item;
    qApp->processEvents();
}

void tst_qqmllistmodelworkerscript::dynamic_role_data()
{
    QTest::addColumn<QString>("preamble");
    QTest::addColumn<QString>("script");
    QTest::addColumn<int>("result");

    QTest::newRow("sync1") << "{append({'a':[{'b':1},{'b':2}]})}" << "{get(0).a = 'string';count}" << 1;
}

void tst_qqmllistmodelworkerscript::dynamic_role()
{
    QFETCH(QString, preamble);
    QFETCH(QString, script);
    QFETCH(int, result);

    QQmlListModel model;
    model.setDynamicRoles(true);
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("model.qml"));
    QQuickItem *item = createWorkerTest(&engine, &component, &model);
    QVERIFY(item != 0);

    QQmlExpression preExp(engine.rootContext(), &model, preamble);
    QCOMPARE(preExp.evaluate().toInt(), 0);

    if (script[0] == QLatin1Char('{') && script[script.length()-1] == QLatin1Char('}'))
        script = script.mid(1, script.length() - 2);
    QVariantList operations;
    foreach (const QString &s, script.split(';')) {
        if (!s.isEmpty())
            operations << s;
    }

    // execute a set of commands on the worker list model, then check the
    // changes are reflected in the list model in the main thread
    QVERIFY(QMetaObject::invokeMethod(item, "evalExpressionViaWorker",
            Q_ARG(QVariant, operations.mid(0, operations.length()-1))));
    waitForWorker(item);

    QQmlExpression e(engine.rootContext(), &model, operations.last().toString());
    QCOMPARE(e.evaluate().toInt(), result);

    delete item;
    qApp->processEvents();
}

QTEST_MAIN(tst_qqmllistmodelworkerscript)

#include "tst_qqmllistmodelworkerscript.moc"
