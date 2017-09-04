/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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
#include <QMetaType>
#include <QProcess>
#include <QFileInfo>
#include <qremoteobjectreplica.h>
#include <QRemoteObjectNode>
#include "engine.h"
#include "speedometer.h"
#include "rep_engine_replica.h"
#include "rep_speedometer_merged.h"
#include "rep_enum_merged.h"
#include "rep_pod_merged.h"
#include "rep_localdatacenter_source.h"
#include "rep_tcpdatacenter_source.h"
#include "rep_localdatacenter_replica.h"
#include "rep_tcpdatacenter_replica.h"

#define SET_NODE_NAME(obj) (obj).setName(QLatin1String(#obj))

#if !defined(BACKEND) || !defined(HOST_URL) || !defined(REGISTRY_URL)
  #error "Integration test needs BACKEND, HOST_URL and REGISTRY_URL defined."
#else
  const QUrl hostUrl = QUrl(QLatin1String(HOST_URL));
  const QUrl registryUrl = QUrl(QLatin1String(REGISTRY_URL));
#endif

//DUMMY impl for variant comparison
bool operator<(const QVector<int> &lhs, const QVector<int> &rhs)
{
    return lhs.size() < rhs.size();
}

class TestLargeData: public QObject
{
    Q_OBJECT

Q_SIGNALS:
    void send(const QByteArray &data);
};

class TestDynamic : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
public:
    TestDynamic(QObject *parent=nullptr) :
        QObject(parent),
        m_value(0) {}

    int value() const { return m_value; }
    void setValue(int value)
    {
        if (m_value == value)
            return;

        m_value = value;
        emit valueChanged();
    }

signals:
    void valueChanged();

private:
    int m_value;
};

class Persist : public QRemoteObjectPersistedStore
{
public:
    Persist() : type(EngineReplica::HYBRID) {}
    void saveProperties(const QString &, const QByteArray &, const QVariantList &values) override
    {
        type = values.at(0).value<EngineReplica::EngineType>();
    }
    QVariantList restoreProperties(const QString &, const QByteArray &) override
    {
        return QVariantList() << QVariant::fromValue(type);
    }
    EngineReplica::EngineType type;
};

class tst_Integration: public QObject
{
    Q_OBJECT

signals:
    void forwardResult(int);

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules("qt.remoteobjects.warning=false");

        qDebug() << "Running tst_Integration for backend:" << BACKEND;
    }

    void cleanup()
    {
        // wait for delivery of RemoveObject events to the source
        QTest::qWait(200);
    }

    void basicTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        e.setRpm(1234);
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        engine_r->waitForSource();
        QCOMPARE(engine_r->rpm(), e.rpm());
    }

    void persistRestoreTest()
    {
        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);
        client.setPersistedStore(new Persist, QRemoteObjectNode::PassOwnershipToNode);

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        QCOMPARE(engine_r->engineType(), EngineReplica::HYBRID);
    }

    void persistTest()
    {
        Persist persist;

        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        e.setEngineType(EngineSimpleSource::ELECTRIC);
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);
        client.setPersistedStore(&persist);

        QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        engine_r->waitForSource();
        QCOMPARE(engine_r->engineType(), EngineReplica::ELECTRIC);

        // Delete to persist
        engine_r.reset();
        host.disableRemoting(&e);

        engine_r.reset(client.acquire<EngineReplica>());
        QCOMPARE(engine_r->waitForSource(1000), false);
        QCOMPARE(engine_r->engineType(), EngineReplica::ELECTRIC);
    }

    void enumTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);

        QRemoteObjectNode client;
        Q_SET_OBJECT_NAME(client);
        client.connectToNode(hostUrl);

        TestClassSimpleSource tc;
        tc.setTestEnum(TestEnum::FALSE);
        tc.setClassEnum(TestClassSimpleSource::One);
        tc.setClassEnumRW(TestClassSimpleSource::One);
        host.enableRemoting(&tc);
        const QScopedPointer<TestClassReplica> tc_rep(client.acquire<TestClassReplica>());
        tc_rep->waitForSource();
        QCOMPARE(tc.testEnum(), tc_rep->testEnum());
        QCOMPARE((qint32)tc.classEnum(), (qint32)TestClassSimpleSource::One);

        // set property on the replica (test property change packet)
        {
            QSignalSpy spy(tc_rep.data(), SIGNAL(classEnumChanged(TestClassReplica::ClassEnum)));
            tc_rep->pushClassEnum(TestClassReplica::Two);
            QVERIFY(spy.wait());

            QCOMPARE((qint32)tc.classEnum(), (qint32)tc_rep->classEnum());
        }

        // set property on the source (test property change packet)
        {
            QSignalSpy spy(tc_rep.data(), SIGNAL(classEnumChanged(TestClassReplica::ClassEnum)));
            tc.setClassEnum(TestClassSimpleSource::One);
            QVERIFY(spy.wait());

            QCOMPARE((qint32)tc.classEnum(), (qint32)tc_rep->classEnum());
        }

        QScopedPointer<QRemoteObjectDynamicReplica> tc_repDynamic(client.acquireDynamic(QStringLiteral("TestClass")));

        tc_repDynamic->waitForSource(1000);
        QVERIFY(tc_repDynamic->isInitialized());

        const QMetaObject *metaObject = tc_repDynamic->metaObject();

        int propertyIndex = metaObject->indexOfProperty("classEnumRW");
        QVERIFY(propertyIndex >= 0);

        QMetaProperty property = metaObject->property(propertyIndex);
        QVERIFY(property.isValid());
        QCOMPARE(property.typeName(), "ClassEnum");

        // read enum on the dynamic replica
        {
            QCOMPARE(property.read(tc_repDynamic.data()).value<TestClassReplica::ClassEnum>(), TestClassReplica::One);
        }

        // write enum on the dynamic replica
        {
            QSignalSpy spy(tc_rep.data(), SIGNAL(classEnumRWChanged(TestClassReplica::ClassEnum)));
            property.write(tc_repDynamic.data(), TestClassReplica::Two);
            QVERIFY(spy.wait());

            QCOMPARE(tc_rep->classEnumRW(), TestClassReplica::Two);
        }

        propertyIndex = metaObject->indexOfProperty("classEnum");
        QVERIFY(propertyIndex >= 0);

        property = metaObject->property(propertyIndex);
        QVERIFY(property.isValid());
        QCOMPARE(property.typeName(), "ClassEnum");

        // read enum on the dynamic replica
        {
            QCOMPARE(property.read(tc_repDynamic.data()).value<TestClassReplica::ClassEnum>(), TestClassReplica::One);
        }

        // ensure write enum fails on ReadPush
        {
            QSignalSpy spy(tc_rep.data(), SIGNAL(classEnumChanged(TestClassReplica::ClassEnum)));
            bool res = property.write(tc_repDynamic.data(), TestClassReplica::Two);
            QVERIFY(!res);
            int methodIndex = metaObject->indexOfMethod("pushClassEnum(ClassEnum)");
            QVERIFY(methodIndex >= 0);
            QMetaMethod method = metaObject->method(methodIndex);
            QVERIFY(method.isValid());

            QVERIFY(method.invoke(tc_repDynamic.data(), Q_ARG(TestClassReplica::ClassEnum, TestClassReplica::Two)));
            QVERIFY(spy.wait());

            QCOMPARE(tc_rep->classEnum(), TestClassReplica::Two);
        }
    }

    void namedObjectTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        Q_SET_OBJECT_NAME(client);
        client.connectToNode(hostUrl);

        e.setRpm(3333);
        Engine *e2 = new Engine();
        QScopedPointer<Engine> engineSave;
        engineSave.reset(e2);
        e2->setRpm(4444);
        host.enableRemoting(&e);
        host.enableRemoting(e2, QStringLiteral("MyTestEngine"));

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        const QScopedPointer<EngineReplica> namedEngine_r(client.acquire<EngineReplica>(QStringLiteral("MyTestEngine")));
        engine_r->waitForSource();
        QCOMPARE(engine_r->cylinders(), e.cylinders());
        QCOMPARE(engine_r->rpm(), 3333);
        namedEngine_r->waitForSource();
        QCOMPARE(namedEngine_r->cylinders(), e2->cylinders());
        QCOMPARE(namedEngine_r->rpm(), 4444);

        engineSave.reset();
        //Deleting the object before disable remoting will cause disable remoting to
        //return false;
        QVERIFY(!host.disableRemoting(e2));
    }

    void multipleInstancesTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        Q_SET_OBJECT_NAME(client);
        client.connectToNode(hostUrl);

        auto instances = client.instances<EngineReplica>();
        QCOMPARE(instances, QStringList());

        Engine e2;
        host.enableRemoting(&e2, QStringLiteral("Engine2"));

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        const QScopedPointer<EngineReplica> engine2_r(client.acquire<EngineReplica>(QStringLiteral("Engine2")));
        const QScopedPointer<EngineReplica> engine3_r(client.acquire<EngineReplica>(QStringLiteral("Engine_doesnotexist")));
        QVERIFY(engine_r->waitForSource());
        QVERIFY(engine2_r->waitForSource());
        QVERIFY(!engine3_r->waitForSource(500));

        instances = client.instances<EngineReplica>();
        QCOMPARE(instances, QStringList({"Engine", "Engine2"}));

        QSignalSpy spy(engine_r.data(), SIGNAL(stateChanged(State,State)));
        host.disableRemoting(&e);
        spy.wait();
        QCOMPARE(spy.count(), 1);

        instances = client.instances<EngineReplica>();
        QCOMPARE(instances, QStringList({"Engine2"}));
    }

    void registryAddedTest()
    {
        QRemoteObjectRegistryHost registry(registryUrl);
        SET_NODE_NAME(registry);

        QRemoteObjectHost host(hostUrl, registryUrl);
        SET_NODE_NAME(host);

        QRemoteObjectNode client(registryUrl);
        Q_SET_OBJECT_NAME(client);

        QScopedPointer<EngineReplica> regBase, regNamed;
        QScopedPointer<QRemoteObjectDynamicReplica> regDynamic, regDynamicNamed;

        int regAdded = 0;
        connect(client.registry(), &QRemoteObjectRegistry::remoteObjectAdded, [&](QRemoteObjectSourceLocation entry)
            {
                if (entry.first == QLatin1String("Engine")) {
                    ++regAdded;
                    //Add regular replica first, then dynamic one
                    regBase.reset(client.acquire<EngineReplica>());
                    regDynamic.reset(client.acquireDynamic(QStringLiteral("Engine")));
                }
                if (entry.first == QLatin1String("MyTestEngine")) {
                    regAdded += 2;
                    //Now add dynamic replica first, then regular one
                    regDynamicNamed.reset(client.acquireDynamic(QStringLiteral("MyTestEngine")));
                    regNamed.reset(client.acquire<EngineReplica>(QStringLiteral("MyTestEngine")));
                }
            });

        QSignalSpy addedSpy(client.registry(), SIGNAL(remoteObjectAdded(QRemoteObjectSourceLocation)));

        Engine e;
        e.setRpm(1111);
        host.enableRemoting(&e);
        Engine e2;
        e2.setRpm(2222);
        host.enableRemoting(&e2, QStringLiteral("MyTestEngine"));
        while (regAdded < 3) {
            addedSpy.wait(100);
        }
        regBase->waitForSource(100);
        regNamed->waitForSource(100);
        regDynamic->waitForSource(100);
        regDynamicNamed->waitForSource(100);
        QVERIFY(regBase->isInitialized());
        QCOMPARE(regBase->rpm(),e.rpm());
        QVERIFY(regNamed->isInitialized());
        QCOMPARE(regNamed->rpm(),e2.rpm());

        QVERIFY(regDynamic->isInitialized());
        const QMetaObject *metaObject = regDynamic->metaObject();

        const int propertyIndex = metaObject->indexOfProperty("rpm");
        QVERIFY(propertyIndex >= 0);
        const QMetaProperty property = metaObject->property(propertyIndex);
        QVERIFY(property.isValid());

        QCOMPARE(property.read(regDynamic.data()).toInt(),e.rpm());

        QVERIFY(regDynamicNamed->isInitialized());
        QCOMPARE(property.read(regDynamicNamed.data()).toInt(),e2.rpm());

        QVERIFY(host.disableRemoting(&e));
        QVERIFY(host.disableRemoting(&e2));
    }

    void registryTest()
    {
        QRemoteObjectRegistryHost registry(registryUrl);
        SET_NODE_NAME(registry);
        TcpDataCenterSimpleSource source1;
        source1.setData1(5);
        source1.setData2(5.0);
        source1.setData3(QStringLiteral("tcp"));
        source1.setData4(QVector<int>() << 1 << 2 << 3 << 4 << 5);
        registry.enableRemoting(&source1);

        QRemoteObjectHost host(hostUrl, registryUrl);
        SET_NODE_NAME(host);
        LocalDataCenterSimpleSource source2;
        source2.setData1(5);
        source2.setData2(5.0);
        source2.setData3(QStringLiteral("local"));
        source2.setData4(QVector<int>() << 1 << 2 << 3 << 4 << 5);
        host.enableRemoting(&source2);
        QVERIFY(host.waitForRegistry(1000));

        QRemoteObjectNode client(registryUrl);
        Q_SET_OBJECT_NAME(client);

        const QScopedPointer<TcpDataCenterReplica> tcpCentre(client.acquire<TcpDataCenterReplica>());
        const QScopedPointer<LocalDataCenterReplica> localCentre(client.acquire<LocalDataCenterReplica>());
        QTRY_VERIFY(localCentre->waitForSource(100));
        QTRY_VERIFY(tcpCentre->waitForSource(100));

        QCOMPARE(client.registry()->sourceLocations(), host.registry()->sourceLocations());
        QCOMPARE(client.registry()->sourceLocations(), registry.registry()->sourceLocations());
        QTRY_VERIFY(localCentre->isInitialized());
        QTRY_VERIFY(tcpCentre->isInitialized());

        QCOMPARE(tcpCentre->data1(), 5 );
        QCOMPARE(tcpCentre->data2(), 5.0);
        QCOMPARE(tcpCentre->data3(), QStringLiteral("tcp"));
        QCOMPARE(tcpCentre->data4(), QVector<int>() << 1 << 2 << 3 << 4 << 5);

        QCOMPARE(localCentre->data1(), 5);
        QCOMPARE(localCentre->data2(), 5.0);
        QCOMPARE(localCentre->data3(), QStringLiteral("local"));
        QCOMPARE(localCentre->data4(), QVector<int>() << 1 << 2 << 3 << 4 << 5);
    }

    void invalidUrlsTest()
    {
        const QUrl invalidUrl;
        {
            QRemoteObjectHost host(invalidUrl, registryUrl);
            SET_NODE_NAME(host);
            const bool res = host.waitForRegistry(3000);
            QVERIFY(!res);
        }

        {
            QRemoteObjectHost host(hostUrl, invalidUrl);
            SET_NODE_NAME(host);
            const bool res = host.waitForRegistry(3000);
            QVERIFY(!res);
        }

        {
            QRemoteObjectHost host(invalidUrl, invalidUrl);
            SET_NODE_NAME(host);
            const bool res = host.waitForRegistry(3000);
            QVERIFY(!res);
        }
    }

    void noRegistryTest()
    {
        QRemoteObjectHost host(hostUrl, registryUrl);
        SET_NODE_NAME(host);
        const bool res = host.waitForRegistry(3000);
        QVERIFY(!res);
        QCOMPARE(host.registry()->isInitialized(), false);
        const QScopedPointer<Engine> localEngine(new Engine);
        host.enableRemoting(localEngine.data());
        QCOMPARE(host.registry()->sourceLocations().keys().isEmpty(), true);
    }

    void delayedRegistryTest()
    {
        QRemoteObjectNode client(registryUrl);
        Q_SET_OBJECT_NAME(client);

        // create a replica before the registry host started
        // to check whether it gets valid later on
        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        Q_SET_OBJECT_NAME(engine_r.data());
        QTRY_VERIFY(!engine_r->waitForSource(100));

        QRemoteObjectHost host(hostUrl, registryUrl);
        SET_NODE_NAME(host);
        const bool res = host.waitForRegistry(3000);
        QVERIFY(!res);
        QCOMPARE(host.registry()->isInitialized(), false);

        const QScopedPointer<Engine> localEngine(new Engine);
        host.enableRemoting(localEngine.data());
        QCOMPARE(host.registry()->sourceLocations().keys().isEmpty(), true);

        QSignalSpy spy(host.registry(), SIGNAL(initialized()));
        QSignalSpy addedSpy(host.registry(), SIGNAL(remoteObjectAdded(QRemoteObjectSourceLocation)));
        QRemoteObjectRegistryHost registry(registryUrl);
        SET_NODE_NAME(registry);
        bool added = addedSpy.wait();
        QVERIFY(spy.count() > 0);
        QCOMPARE(added, true);
        QCOMPARE(host.registry()->sourceLocations().keys().isEmpty(), false);
        QCOMPARE(host.registry()->sourceLocations().keys().at(0), QStringLiteral("Engine"));
        QCOMPARE(host.registry()->sourceLocations().value(QStringLiteral("Engine")).hostUrl, hostUrl);

        // the replicate should be valid now
        QTRY_VERIFY(engine_r->isInitialized());
        QTRY_VERIFY(engine_r->isReplicaValid());

        //This should produce a warning...
        registry.enableRemoting(localEngine.data());
        QVERIFY(host.registry()->sourceLocations().value(QStringLiteral("Engine")).hostUrl != registryUrl);
    }

    void defaultValueTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        engine_r->waitForSource();
        QCOMPARE(engine_r->cylinders(), 4);
    }

    void notifyTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        QSignalSpy spy(engine_r.data(), SIGNAL(rpmChanged(int)));
        e.setRpm(2345);

        spy.wait();
        QCOMPARE(spy.count(), 1);
        const QList<QVariant> &arguments = spy.first();
        bool ok;
        int res = arguments.at(0).toInt(&ok);
        QVERIFY(ok);
        QCOMPARE(res, e.rpm());
        QCOMPARE(engine_r->rpm(), e.rpm());
    }

    void dynamicNotifyTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        QSignalSpy spy(this, SIGNAL(forwardResult(int)));
        QScopedPointer<QRemoteObjectDynamicReplica> engine_dr(client.acquireDynamic(QStringLiteral("Engine")));
        connect(engine_dr.data(), &QRemoteObjectDynamicReplica::initialized, [&]()
            {
                const QMetaObject *metaObject = engine_dr->metaObject();
                const int propIndex = metaObject->indexOfProperty("rpm");
                QVERIFY(propIndex >= 0);
                const QMetaProperty mp =  metaObject->property(propIndex);
                QVERIFY(connect(engine_dr.data(), QByteArray(QByteArrayLiteral("2")+mp.notifySignal().methodSignature().constData()), this, SIGNAL(forwardResult(int))));
             });
        e.setRpm(3456);
        spy.wait();
        QCOMPARE(spy.count(), 1);
        const QList<QVariant> &arguments = spy.first();
        bool ok;
        int res = arguments.at(0).toInt(&ok);
        QVERIFY(ok);
        QCOMPARE(res, e.rpm());
    }

    void slotTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);
        e.setStarted(false);

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        QEventLoop loop;
        QTimer::singleShot(100, &loop, &QEventLoop::quit);
        connect(engine_r.data(), &EngineReplica::initialized, &loop, &QEventLoop::quit);
        if (!engine_r->isInitialized())
            loop.exec();
        QCOMPARE(engine_r->started(), false);

        QRemoteObjectPendingReply<bool> reply = engine_r->start();
        QCOMPARE(reply.error(), QRemoteObjectPendingCall::InvalidMessage);
        QVERIFY(reply.waitForFinished());
        QVERIFY(reply.isFinished());
        QCOMPARE(reply.returnValue(), true);
        QCOMPARE(reply.error(), QRemoteObjectPendingCall::NoError);

        QCOMPARE(engine_r->started(), true);
    }

    void slotTestWithWatcher()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);
        e.setStarted(false);

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        QEventLoop loop;
        QTimer::singleShot(100, &loop, &QEventLoop::quit);
        connect(engine_r.data(), &EngineReplica::initialized, &loop, &QEventLoop::quit);
        if (!engine_r->isInitialized())
            loop.exec();
        QCOMPARE(engine_r->started(), false);

        QRemoteObjectPendingReply<bool> reply = engine_r->start();
        QCOMPARE(reply.error(), QRemoteObjectPendingCall::InvalidMessage);

        QRemoteObjectPendingCallWatcher watcher(reply);
        QSignalSpy spy(&watcher, SIGNAL(finished(QRemoteObjectPendingCallWatcher *)));
        spy.wait();
        QCOMPARE(spy.count(), 1);

        QVERIFY(reply.isFinished());
        QCOMPARE(reply.returnValue(), true);
        QCOMPARE(engine_r->started(), true);
    }

    void slotTestDynamicReplica()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);
        e.setStarted(false);

        const QScopedPointer<QRemoteObjectDynamicReplica> engine_r(client.acquireDynamic(QStringLiteral("Engine")));
        Q_ASSERT(engine_r);
        QEventLoop loop;
        QTimer::singleShot(100, &loop, &QEventLoop::quit);
        connect(engine_r.data(), &EngineReplica::initialized, &loop, &QEventLoop::quit);
        if (!engine_r->isInitialized())
            loop.exec();

        const QMetaObject *metaObject = engine_r->metaObject();
        const int propIndex = metaObject->indexOfProperty("started");
        QVERIFY(propIndex >= 0);
        const QMetaProperty property = metaObject->property(propIndex);
        bool started = property.read(engine_r.data()).value<bool>();
        QCOMPARE(started, false);

        const int methodIndex = metaObject->indexOfMethod("start()");
        QVERIFY(methodIndex >= 0);
        const QMetaMethod method = metaObject->method(methodIndex);
        QRemoteObjectPendingCall call;
        QVERIFY(method.invoke(engine_r.data(), Q_RETURN_ARG(QRemoteObjectPendingCall, call)));
        QCOMPARE(call.error(), QRemoteObjectPendingCall::InvalidMessage);
        QVERIFY(call.waitForFinished());
        QVERIFY(call.isFinished());
        QCOMPARE(call.returnValue().type(), QVariant::Bool);
        QCOMPARE(call.returnValue().toBool(), true);
        started = property.read(engine_r.data()).value<bool>();
        QCOMPARE(started, true);
    }

    void slotTestDynamicReplicaWithArguments()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        const QScopedPointer<QRemoteObjectDynamicReplica> engine_r(client.acquireDynamic(QStringLiteral("Engine")));
        Q_ASSERT(engine_r);
        bool ok = engine_r->waitForSource();
        QVERIFY(ok);
        const QMetaObject *metaObject = engine_r->metaObject();

        int methodIndex = metaObject->indexOfMethod("setMyTestString(QString)");
        QVERIFY(methodIndex >= 0);
        QMetaMethod method = metaObject->method(methodIndex);
        QVERIFY(method.isValid());

        // The slot has no return-value, calling it with a Q_RETURN_ARG should fail.
        QRemoteObjectPendingCall setCall;
        QString s = QLatin1String("Hello World 1");
        QVERIFY(!method.invoke(engine_r.data(), Q_RETURN_ARG(QRemoteObjectPendingCall, setCall), Q_ARG(QString, s)));
        QVERIFY(!setCall.waitForFinished());
        QVERIFY(!setCall.isFinished());
        QCOMPARE(setCall.error(), QRemoteObjectPendingCall::InvalidMessage);

        // Now call the method without return-value, that should succeed.
        s = QLatin1String("Hello World 2");
        QVERIFY(method.invoke(engine_r.data(), Q_ARG(QString, s)));

        // Verify that the passed argument was proper set.
        methodIndex = metaObject->indexOfMethod("myTestString()");
        QVERIFY(methodIndex >= 0);
        method = metaObject->method(methodIndex);
        QRemoteObjectPendingCall getCall;
        QVERIFY(method.invoke(engine_r.data(), Q_RETURN_ARG(QRemoteObjectPendingCall, getCall)));
        QVERIFY(getCall.waitForFinished());
        QVERIFY(getCall.isFinished());
        QCOMPARE(getCall.error(), QRemoteObjectPendingCall::NoError);
        QCOMPARE(getCall.returnValue().type(), QVariant::String);
        QCOMPARE(getCall.returnValue().toString(), s);
    }

    void expapiTestDynamicReplica()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        const QScopedPointer<QRemoteObjectDynamicReplica> engine_r(client.acquireDynamic(QStringLiteral("Engine")));
        const QMetaObject *metaObject = engine_r->metaObject();
        const int propIndex = metaObject->indexOfProperty("purchasedPart");
        QVERIFY(propIndex < 0);
        const int methodIndex = metaObject->indexOfMethod("setpurchasedPart(bool)");
        QVERIFY(methodIndex < 0);
    }

    void slotTestInProcess()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);
        e.setStarted(false);

        const QScopedPointer<EngineReplica> engine_r(host.acquire<EngineReplica>());
        engine_r->waitForSource();
        QCOMPARE(engine_r->started(), false);

        QRemoteObjectPendingReply<bool> reply = engine_r->start();
        QVERIFY(reply.waitForFinished());
        QVERIFY(reply.isFinished());
        QCOMPARE(reply.returnValue(), true);
        QCOMPARE(reply.error(), QRemoteObjectPendingCall::NoError);

        QCOMPARE(engine_r->started(), true);
    }

    void slotTestWithUnnormalizedSignature()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        engine_r->waitForSource();

        engine_r->unnormalizedSignature(0, 0);
    }

    void setterTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        engine_r->waitForSource();
        QSignalSpy spy(engine_r.data(), SIGNAL(rpmChanged(int)));
        engine_r->setRpm(42);
        spy.wait();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(engine_r->rpm(), 42);
    }

    void pushTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        engine_r->waitForSource();
        QCOMPARE(engine_r->started(), false);
        QSignalSpy spy(engine_r.data(), SIGNAL(startedChanged(bool)));
        engine_r->pushStarted(true);
        spy.wait();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(engine_r->started(), true);
    }

    void dynamicSetterTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        const QScopedPointer<QRemoteObjectDynamicReplica> engine_dr(client.acquireDynamic(QStringLiteral("Engine")));
        engine_dr->waitForSource();
        const QMetaObject *metaObject = engine_dr->metaObject();
        const int propIndex = metaObject->indexOfProperty("rpm");
        const QMetaProperty mp =  metaObject->property(propIndex);
        QSignalSpy spy(engine_dr.data(), QByteArray(QByteArrayLiteral("2")+mp.notifySignal().methodSignature().constData()));
        mp.write(engine_dr.data(), 44);
        spy.wait();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(mp.read(engine_dr.data()).toInt(), 44);
    }

    void slotWithParameterTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);
        e.setRpm(0);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        engine_r->waitForSource();
        QCOMPARE(engine_r->rpm(), 0);

        QSignalSpy spy(engine_r.data(), SIGNAL(rpmChanged(int)));
        engine_r->increaseRpm(1000);
        spy.wait();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(engine_r->rpm(), 1000);
    }

    void slotWithUserReturnTypeTest() {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        e.setTemperature(Temperature(400, QStringLiteral("Kelvin")));

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        engine_r->waitForSource();
        QRemoteObjectPendingReply<Temperature> pendingReply = engine_r->temperature();
        pendingReply.waitForFinished();
        Temperature temperature = pendingReply.returnValue();
        QCOMPARE(temperature, Temperature(400, QStringLiteral("Kelvin")));
    }

    void sequentialReplicaTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        e.setRpm(3456);

        QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        engine_r->waitForSource();
        QCOMPARE(engine_r->rpm(), e.rpm());

        engine_r.reset(client.acquire<EngineReplica>());
        engine_r->waitForSource();
        QCOMPARE(engine_r->rpm(), e.rpm());
    }

    void doubleReplicaTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting(&e);
        e.setRpm(3412);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        const QScopedPointer<EngineReplica> engine_r1(client.acquire< EngineReplica >());
        const QScopedPointer<EngineReplica> engine_r2(client.acquire< EngineReplica >());

        engine_r1->waitForSource();
        engine_r2->waitForSource();

        QCOMPARE(engine_r1->rpm(), e.rpm());
        QCOMPARE(engine_r2->rpm(), e.rpm());
    }

    void twoReplicaTest() {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        Speedometer s;
        host.enableRemoting(&e);
        host.enableRemoting(&s);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        e.setRpm(1234);
        s.setMph(70);

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        engine_r->waitForSource();
        const QScopedPointer<SpeedometerReplica> speedometer_r(client.acquire<SpeedometerReplica>());
        speedometer_r->waitForSource();

        QCOMPARE(engine_r->rpm(), e.rpm());
        QCOMPARE(speedometer_r->mph(), s.mph());
    }

    void rawDynamicReplicaTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        TestDynamic source;
        host.enableRemoting(&source, "TestDynamic");

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        const QScopedPointer<QRemoteObjectDynamicReplica> replica(client.acquireDynamic(QStringLiteral("TestDynamic")));
        replica->waitForSource();
        QVERIFY(replica->isInitialized());

        QSignalSpy spy(replica.data(), SIGNAL(valueChanged()));

        const QMetaObject *metaObject = replica->metaObject();
        const int propIndex = metaObject->indexOfProperty("value");
        QVERIFY(propIndex != -1);
        const int signalIndex = metaObject->indexOfSignal("valueChanged()");
        QVERIFY(signalIndex != -1);

        // replica gets source change
        source.setValue(1);
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(replica->property("value"), QVariant(1));

        // source gets replica change
        replica->setProperty("value", 2);
        QTRY_COMPARE(replica->property("value"), QVariant(2));
        QCOMPARE(source.value(), 2);
    }

    void dynamicReplicaTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        TcpDataCenterSimpleSource t;
        LocalDataCenterSimpleSource l;
        host.enableRemoting(&t);
        host.enableRemoting(&l);

        QRemoteObjectNode client;
        client.connectToNode(hostUrl);
        Q_SET_OBJECT_NAME(client);

        const QScopedPointer<QRemoteObjectDynamicReplica> rep1(client.acquireDynamic(QStringLiteral("TcpDataCenter")));
        const QScopedPointer<QRemoteObjectDynamicReplica> rep2(client.acquireDynamic(QStringLiteral("TcpDataCenter")));
        const QScopedPointer<QRemoteObjectDynamicReplica> rep3(client.acquireDynamic(QStringLiteral("LocalDataCenter")));
        rep1->waitForSource();
        rep2->waitForSource();
        rep3->waitForSource();
        const QMetaObject *metaTcpRep1 = rep1->metaObject();
        const QMetaObject *metaLocalRep1 = rep3->metaObject();
        const QMetaObject *metaTcpSource = t.metaObject();
        const QMetaObject *metaLocalSource = l.metaObject();
        QVERIFY(rep1->isInitialized());
        QVERIFY(rep2->isInitialized());
        QVERIFY(rep3->isInitialized());

        for (int i = 0; i < metaTcpRep1->propertyCount(); ++i)
        {
            const QMetaProperty propLhs =  metaTcpRep1->property(i);
            if (qstrcmp(propLhs.name(), "isReplicaValid") == 0 || qstrcmp(propLhs.name(), "state") == 0 || qstrcmp(propLhs.name(), "node") == 0) //Ignore properties only on the Replica side
                continue;
            const QMetaProperty propRhs =  metaTcpSource->property(metaTcpSource->indexOfProperty(propLhs.name()));
            if (propLhs.notifySignalIndex() == -1)
                QCOMPARE(propRhs.hasNotifySignal(), false);
            else {
                QCOMPARE(propRhs.notifySignalIndex() != -1, true);
                QCOMPARE(metaTcpRep1->method(propLhs.notifySignalIndex()).name(), metaTcpSource->method(propRhs.notifySignalIndex()).name());
            }
            QCOMPARE(propLhs.read(rep1.data()),  propRhs.read(&t));
        }
        for (int i = 0; i < metaLocalRep1->propertyCount(); ++i )
        {
            const QMetaProperty propLhs =  metaLocalRep1->property(i);
            if (qstrcmp(propLhs.name(), "isReplicaValid") == 0 || qstrcmp(propLhs.name(), "state") == 0 || qstrcmp(propLhs.name(), "node") == 0) //Ignore properties only on the Replica side
                continue;
            const QMetaProperty propRhs =  metaLocalSource->property(metaTcpSource->indexOfProperty(propLhs.name()));
            if (propLhs.notifySignalIndex() == -1)
                QCOMPARE(propRhs.hasNotifySignal(), false);
            else {
                QCOMPARE(propRhs.notifySignalIndex() != -1, true);
                QCOMPARE(metaTcpRep1->method(propLhs.notifySignalIndex()).name(), metaTcpSource->method(propRhs.notifySignalIndex()).name());
            }
            QCOMPARE(propLhs.read(rep3.data()),  propRhs.read(&l));
        }

    }

    void apiTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting<EngineSourceAPI>(&e);
        e.setRpm(1234);

        QRemoteObjectNode client;
        Q_SET_OBJECT_NAME(client);
        client.connectToNode(hostUrl);

        const QScopedPointer<EngineReplica> engine_r(client.acquire<EngineReplica>());
        engine_r->waitForSource();

        QCOMPARE(engine_r->rpm(), e.rpm());
    }

    void apiInProcTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting<EngineSourceAPI>(&e);
        e.setRpm(1234);

        const QScopedPointer<EngineReplica> engine_r_inProc(host.acquire<EngineReplica>());
        engine_r_inProc->waitForSource();

        QCOMPARE(engine_r_inProc->rpm(), e.rpm());
    }

    void errorSignalTest()
    {
        QRemoteObjectNode client;
        Q_SET_OBJECT_NAME(client);
        QSignalSpy errorSpy(&client, SIGNAL(error(QRemoteObjectNode::ErrorCode)));
        QVERIFY(!client.connectToNode(QUrl(QLatin1String("invalid:invalid"))));
        QCOMPARE(errorSpy.count(), 1);
        auto emittedErrorCode = errorSpy.first().at(0).value<QRemoteObjectNode::ErrorCode>();
        QCOMPARE(emittedErrorCode, QRemoteObjectNode::RegistryNotAcquired);
        QCOMPARE(client.lastError(), QRemoteObjectNode::RegistryNotAcquired);
    }

    void clientBeforeServerTest() {
        QRemoteObjectNode client;
        Q_SET_OBJECT_NAME(client);
        client.connectToNode(hostUrl);
        const QScopedPointer<EngineReplica> engine_d(client.acquire<EngineReplica>());

        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        Engine e;
        host.enableRemoting<EngineSourceAPI>(&e);
        QSignalSpy spy(engine_d.data(), SIGNAL(rpmChanged(int)));
        e.setRpm(50);

        spy.wait();
        QCOMPARE(spy.count(), 1);

        QCOMPARE(engine_d->rpm(), e.rpm());
    }

    void largeDataTest()
    {
        TestLargeData t;
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);
        host.enableRemoting(&t, QStringLiteral("large"));

        QRemoteObjectNode client;
        Q_SET_OBJECT_NAME(client);
        client.connectToNode(hostUrl);
        const QScopedPointer<QRemoteObjectDynamicReplica> rep(client.acquireDynamic(QStringLiteral("large")));
        rep->waitForSource();
        QVERIFY(rep->isInitialized());
        const QMetaObject *metaObject = rep->metaObject();
        const int sigIndex = metaObject->indexOfSignal("send(QByteArray)");
        QVERIFY(sigIndex != -1);
        const QMetaMethod mm =  metaObject->method(sigIndex);
        QSignalSpy spy(rep.data(), QByteArray(QByteArrayLiteral("2")+mm.methodSignature().constData()));
        const QByteArray data(16384,'y');
        emit t.send(data);
        spy.wait();
        QCOMPARE(spy.count(), 1);
        const QList<QVariant> &arguments = spy.first();
        QVERIFY(arguments.at(0).toByteArray() == data);
        QVERIFY(host.disableRemoting(&t));
    }

    void PODTest()
    {
        QRemoteObjectHost host(hostUrl);
        SET_NODE_NAME(host);

        QRemoteObjectNode client;
        Q_SET_OBJECT_NAME(client);
        client.connectToNode(hostUrl);

        MyPOD shouldPass(1, 2.0, QStringLiteral("pass"));
        MyPOD shouldFail(1, 2.0, QStringLiteral("fail"));
        MyClassSimpleSource m;
        m.setMyPOD(shouldPass);
        host.enableRemoting(&m);
        const QScopedPointer<MyClassReplica> myclass_r(client.acquire<MyClassReplica>());
        myclass_r->waitForSource();

        QVERIFY(myclass_r->myPOD() == m.myPOD());
        QVERIFY(myclass_r->myPOD() != shouldFail);
    }

    void SchemeTest()
    {
        QRemoteObjectHost valid(hostUrl);
        QVERIFY(valid.lastError() == QRemoteObjectNode::NoError);
        QRemoteObjectHost invalid(QUrl(QLatin1String("invalid:invalid")));
        QVERIFY(invalid.lastError() == QRemoteObjectNode::HostUrlInvalid);
        QRemoteObjectNode invalidRegistry(QUrl(QLatin1String("invalid:invalid")));
        QVERIFY(invalidRegistry.lastError() == QRemoteObjectNode::RegistryNotAcquired);
    }

//TODO check Mac support
#ifdef Q_OS_LINUX
    void localServerConnectionTest()
    {
        QProcess testServer;
        const QString progName = QStringLiteral("../../localsockettestserver/localsockettestserver");
        //create a fake socket as killing doesn't produce a necessarily unusable socket
        QFile fake(QDir::temp().absoluteFilePath(QStringLiteral("crashMe")));
        fake.remove();
        QVERIFY(fake.open(QFile::Truncate | QFile::WriteOnly));
        QFileInfo info(QDir::temp().absoluteFilePath(QStringLiteral("crashMe")));
        QVERIFY(info.exists());

        QRemoteObjectNode localSocketTestClient;
        const QUrl connection = QUrl(QStringLiteral("local:crashMe"));
        const QString objectname = QStringLiteral("connectme");
        localSocketTestClient.connectToNode(connection);
        QVERIFY(localSocketTestClient.lastError() == QRemoteObjectNode::NoError);
        QScopedPointer<QRemoteObjectDynamicReplica> replica;
        replica.reset(localSocketTestClient.acquireDynamic(objectname));

        testServer.start(progName);
        QVERIFY(testServer.waitForStarted());
        QVERIFY(localSocketTestClient.lastError() == QRemoteObjectNode::NoError);
        replica->waitForSource(1000);
        QVERIFY(replica->isInitialized());
        testServer.terminate();
        QVERIFY(testServer.waitForFinished());
    }
    // Tests to take over an existing socket if its still valid
    void localServerConnectionTest2()
    {
        QProcess testServer;
        const QString progName = QStringLiteral("../../localsockettestserver/localsockettestserver");

        testServer.start(progName);
        QVERIFY(testServer.waitForStarted());
        QFileInfo info(QDir::temp().absoluteFilePath(QStringLiteral("crashMe")));
        QVERIFY(info.exists());
        testServer.kill();
        testServer.waitForFinished();
        QVERIFY(info.exists());

        QRemoteObjectNode localSocketTestClient;
        const QUrl connection = QUrl(QStringLiteral("local:crashMe"));
        const QString objectname = QStringLiteral("connectme");
        localSocketTestClient.connectToNode(connection);
        QVERIFY(localSocketTestClient.lastError() == QRemoteObjectNode::NoError);
        QScopedPointer<QRemoteObjectDynamicReplica> replica;
        replica.reset(localSocketTestClient.acquireDynamic(objectname));

        testServer.start(progName);
        QVERIFY(testServer.waitForStarted());
        QVERIFY(localSocketTestClient.lastError() == QRemoteObjectNode::NoError);
        replica->waitForSource(1000);
        QVERIFY(replica->isInitialized());
        testServer.terminate();
        QVERIFY(testServer.waitForFinished());
    }
#endif
};

QTEST_MAIN(tst_Integration)

#include "tst_integration.moc"
