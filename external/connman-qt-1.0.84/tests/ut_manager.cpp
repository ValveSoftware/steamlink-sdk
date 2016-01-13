#include <QtCore/QPointer>

#include "../libconnman-qt/networkmanager.h"
#include "testbase.h"

namespace Tests {

class UtManager : public TestBase
{
    Q_OBJECT

public:
    class ManagerMock;
    class ServiceMock;
    class TechnologyMock;

public:
    UtManager();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testProperties_data();
    void testProperties();
    void testWriteProperties_data();
    void testWriteProperties();
    void testState();
    void testServiceAdded();
    void testAddedServiceProperties_data();
    void testAddedServiceProperties();
    void testTechnologyAdded();
    void testAddedTechnologyProperties_data();
    void testAddedTechnologyProperties();
    void testAvailabilityChanged();
    void testServiceRemoved();
    void testTechnologyRemoved();
    void testRegisterCounter();

private:
    QPointer<NetworkManager> m_manager;
};

class UtManager::ManagerMock : public MainObjectMock
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.Manager")

public:
    ManagerMock();

public:
    Q_SCRIPTABLE QVariantMap GetProperties() const;
    Q_SCRIPTABLE void SetProperty(const QString &name, const QDBusVariant &value,
        const QDBusMessage &message);
    Q_SCRIPTABLE ConnmanObjectList GetTechnologies() const;
    Q_SCRIPTABLE ConnmanObjectList GetServices() const;
    Q_SCRIPTABLE void RegisterCounter(const QDBusObjectPath &path, quint32 accuracy, quint32 period,
            const QDBusMessage &message);
    Q_SCRIPTABLE void UnregisterCounter(const QDBusObjectPath &path, const QDBusMessage &message);


    // mock API
    Q_SCRIPTABLE void mock_setState(const QString &state);
    Q_SCRIPTABLE void mock_temporarilyUnregister();
    Q_SCRIPTABLE void mock_addService(const QString &path, const QVariantMap &properties,
            const QDBusMessage &message);
    Q_SCRIPTABLE void mock_removeService(const QString &path, const QDBusMessage &message);
    Q_SCRIPTABLE void mock_addTechnology(const QString &path, const QVariantMap &properties,
            const QDBusMessage &message);
    Q_SCRIPTABLE void mock_removeTechnology(const QString &path, const QDBusMessage &message);
    Q_SCRIPTABLE quint32 mock_counterAccuracy(const QString &path, const QDBusMessage &message);
    Q_SCRIPTABLE quint32 mock_counterPeriod(const QString &path, const QDBusMessage &message);
    Q_SCRIPTABLE bool mock_counterRegistered(const QString &path);

signals:
    Q_SCRIPTABLE void PropertyChanged(const QString &name, const QDBusVariant &value);
    Q_SCRIPTABLE void ServicesChanged(ConnmanObjectList changed,
            const QList<QDBusObjectPath> &removed);
    Q_SCRIPTABLE void TechnologyAdded(const QDBusObjectPath &path, const QVariantMap &properties);
    Q_SCRIPTABLE void TechnologyRemoved(const QDBusObjectPath &path);

private:
    QVariantMap m_properties;
    QMap<QString, ServiceMock *> m_services;
    QMap<QString, TechnologyMock *> m_technologies;
    QMap<QString, QPair<quint32, quint32> > m_counters;
};

class UtManager::ServiceMock : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.Service")

public:
    ServiceMock(const QVariantMap &properties, ManagerMock *manager)
        : QObject(manager),
          m_properties(properties)
    {
    }

    QVariantMap properties() const { return m_properties; }

private:
    QVariantMap m_properties;
};

class UtManager::TechnologyMock : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.Technology")

public:
    TechnologyMock(const QVariantMap &properties, ManagerMock *manager)
        : QObject(manager),
          m_properties(properties)
    {
    }

    QVariantMap properties() const { return m_properties; }

private:
    QVariantMap m_properties;
};

} // namespace Tests

using namespace Tests;

/*
 * \class Tests::UtManager
 */

Q_DECLARE_METATYPE(QDBusPendingCallWatcher*) // needed by waitForSignal

UtManager::UtManager()
{
    qRegisterMetaType<QDBusPendingCallWatcher *>(); // needed by waitForSignal
}

void UtManager::initTestCase()
{
    QVERIFY(waitForService("net.connman", "/", "net.connman.Manager"));
    m_manager = new NetworkManager(this);
}

void UtManager::cleanupTestCase()
{
    delete m_manager;
}

void UtManager::testProperties_data()
{
    QTest::addColumn<QVariant>("expected");

    createTestPropertyData(defaultManagerProperties(), managerDBusProperty2QtProperty);
    QTest::newRow("available") << QVariant(true);
    QTest::newRow("defaultRoute") << QVariant::fromValue((NetworkService *)0);
}

void UtManager::testProperties()
{
    QFETCH(QVariant, expected);

    testProperty(*m_manager, QTest::currentDataTag(), expected);
}

void UtManager::testWriteProperties_data()
{
    QTest::addColumn<QVariant>("newValue");

    QTest::newRow("offlineMode") << QVariant(true);
    QTest::newRow("sessionMode") << QVariant(false);
}

void UtManager::testWriteProperties()
{
    QFETCH(QVariant, newValue);

    testWriteProperty(m_manager, QTest::currentDataTag(), newValue);
}

void UtManager::testState()
{
    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus());

    SignalSpy stateChangedSpy(m_manager, SIGNAL(stateChanged(QString)));

    const QString injectedState = "online";

    QDBusPendingReply<> reply = manager.asyncCall("mock_setState", injectedState);

    QVERIFY(waitForSignal(&stateChangedSpy));
    QCOMPARE(stateChangedSpy.count(), 1);
    QCOMPARE(stateChangedSpy.at(0).at(0).toString(), injectedState);

    QCOMPARE(m_manager->state(), injectedState);
}

void UtManager::testServiceAdded()
{
    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus());

    SignalSpy servicesChangedSpy(m_manager, SIGNAL(servicesChanged()));
    SignalSpy serviceAddedSpy(m_manager, SIGNAL(serviceAdded(QString)));

    const QString injectedServicePath = "/service_just_added";
    const QVariantMap injectedServiceProperties = defaultServiceProperties();
    const QString injectedServiceType = injectedServiceProperties["Type"].toString();

    QDBusPendingReply<> reply = manager.asyncCall("mock_addService", injectedServicePath,
            injectedServiceProperties);

    QVERIFY(waitForSignal(&servicesChangedSpy));
    QCOMPARE(servicesChangedSpy.count(), 1);

    QVERIFY(waitForSignal(&serviceAddedSpy));
    QCOMPARE(serviceAddedSpy.count(), 1);
    QCOMPARE(serviceAddedSpy.at(0).at(0).toString(), injectedServicePath);

    const QVector<NetworkService *> services = m_manager->getServices();
    QCOMPARE(services.count(), 1);
    QCOMPARE(services.at(0)->path(), injectedServicePath);

    QCOMPARE(m_manager->servicesList(injectedServiceType), QStringList() << injectedServicePath);
}

void UtManager::testAddedServiceProperties_data()
{
    QTest::addColumn<QVariant>("expected");

    const QVariantMap injectedServiceProperties = defaultServiceProperties();
    createTestPropertyData(injectedServiceProperties, serviceDBusProperty2QtProperty);
}

void UtManager::testAddedServiceProperties()
{
    Q_ASSERT(QString(QTest::currentDataTag()).count('/') <= 1);

    const QVector<NetworkService *> services = m_manager->getServices();
    QCOMPARE(services.count(), 1);

    QFETCH(QVariant, expected);

    testProperty(*services.at(0), QTest::currentDataTag(), expected);
}

void UtManager::testTechnologyAdded()
{
    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus());

    SignalSpy technologiesChangedSpy(m_manager, SIGNAL(technologiesChanged()));

    const QString injectedTechnologyPath = "/technology_just_added";
    const QVariantMap injectedTechnologyProperties = defaultTechnologyProperties();
    const QString injectedTechnologyType = injectedTechnologyProperties["Type"].toString();

    QDBusPendingReply<> reply = manager.asyncCall("mock_addTechnology", injectedTechnologyPath,
            injectedTechnologyProperties);

    QVERIFY(waitForSignal(&technologiesChangedSpy));
    QCOMPARE(technologiesChangedSpy.count(), 1);

    const QVector<NetworkTechnology *> technologies = m_manager->getTechnologies();
    QCOMPARE(technologies.count(), 1);
    QCOMPARE(technologies.at(0)->path(), injectedTechnologyPath);

    NetworkTechnology *const technology = m_manager->getTechnology(injectedTechnologyType);
    QVERIFY(technology != 0);
    QCOMPARE(technology->path(), injectedTechnologyPath);

    QCOMPARE(m_manager->technologyPathForType(injectedTechnologyType), injectedTechnologyPath);

    QCOMPARE(m_manager->technologiesList(), QStringList() << injectedTechnologyType);
}

void UtManager::testAddedTechnologyProperties_data()
{
    QTest::addColumn<QVariant>("expected");

    const QVariantMap injectedTechnologyProperties = defaultTechnologyProperties();
    createTestPropertyData(injectedTechnologyProperties, technologyDBusProperty2QtProperty);
}

void UtManager::testAddedTechnologyProperties()
{
    QCOMPARE(m_manager->getTechnologies().count(), 1);

    QFETCH(QVariant, expected);

    testProperty(*m_manager->getTechnologies().at(0), QTest::currentDataTag(), expected);
}

void UtManager::testAvailabilityChanged()
{
    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus());

    SignalSpy availabilityChangedSpy(m_manager, SIGNAL(availabilityChanged(bool)));

    QDBusPendingReply<> reply = manager.asyncCall("mock_temporarilyUnregister");

    QDBusPendingCallWatcher watcher(reply);
    QVERIFY(waitForSignal(&watcher, SIGNAL(finished(QDBusPendingCallWatcher*))));
    QVERIFY(reply.isValid());

    QCOMPARE(availabilityChangedSpy.count(), 2);
    QCOMPARE(availabilityChangedSpy.at(0).at(0).toBool(), false);
    QCOMPARE(availabilityChangedSpy.at(1).at(0).toBool(), true);
}

void UtManager::testServiceRemoved()
{
    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus());

    SignalSpy servicesChangedSpy(m_manager, SIGNAL(servicesChanged()));
    SignalSpy serviceRemovedSpy(m_manager, SIGNAL(serviceRemoved(QString)));

    const QString injectedServicePath = "/service_just_added";

    QDBusPendingReply<> reply = manager.asyncCall("mock_removeService", injectedServicePath);

    QVERIFY(waitForSignal(&servicesChangedSpy));
    QCOMPARE(servicesChangedSpy.count(), 1);

    QVERIFY(waitForSignal(&serviceRemovedSpy));
    QCOMPARE(serviceRemovedSpy.count(), 1);
    QCOMPARE(serviceRemovedSpy.at(0).at(0).toString(), injectedServicePath);

    const QVector<NetworkService *> services = m_manager->getServices();
    QCOMPARE(services.count(), 0);
}

void UtManager::testTechnologyRemoved()
{
    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus());

    SignalSpy technologiesChangedSpy(m_manager, SIGNAL(technologiesChanged()));

    const QString injectedTechnologyPath = "/technology_just_added";

    QDBusPendingReply<> reply = manager.asyncCall("mock_removeTechnology", injectedTechnologyPath);

    QVERIFY(waitForSignal(&technologiesChangedSpy));
    QCOMPARE(technologiesChangedSpy.count(), 1);

    const QVector<NetworkTechnology *> technologies = m_manager->getTechnologies();
    QCOMPARE(technologies.count(), 0);
}

void UtManager::testRegisterCounter()
{
    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus());

    const QString counterPath = "/counter1";
    const quint32 counterAccuracy = 42;
    const quint32 counterPeriod = 24;

    m_manager->registerCounter(counterPath, counterAccuracy, counterPeriod);

    QDBusReply<quint32> accuracy = manager.call("mock_counterAccuracy", counterPath);
    QVERIFY2(accuracy.isValid(), qPrintable(accuracy.error().message()));
    QCOMPARE(accuracy.value(), counterAccuracy);

    QDBusReply<quint32> period = manager.call("mock_counterPeriod", counterPath);
    QVERIFY2(period.isValid(), qPrintable(period.error().message()));
    QCOMPARE(period.value(), counterPeriod);

    m_manager->unregisterCounter(counterPath);

    QDBusReply<bool> counterRegistered = manager.call("mock_counterRegistered", counterPath);

    QVERIFY(counterRegistered.isValid());
    QCOMPARE(counterRegistered.value(), false);
}

/*
 * \class Tests::UtManager::ManagerMock
 */

UtManager::ManagerMock::ManagerMock()
    : MainObjectMock("net.connman", "/"),
      m_properties(defaultManagerProperties())
{
}

QVariantMap UtManager::ManagerMock::GetProperties() const
{
    return m_properties;
}

void UtManager::ManagerMock::SetProperty(const QString &name, const QDBusVariant &value,
        const QDBusMessage &message)
{
    if (name != "OfflineMode" && name != "SessionMode") {
        const QString err = QString("Property not writable '%1'").arg(name);
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }

    m_properties[name] = value.variant();

    Q_EMIT PropertyChanged(name, value);
}

ConnmanObjectList UtManager::ManagerMock::GetTechnologies() const
{
    ConnmanObjectList technologies;
    QMapIterator<QString, TechnologyMock *> it(m_technologies);
    while (it.hasNext()) {
        it.next();

        ConnmanObject object = {
            QDBusObjectPath(it.key()),
            it.value()->properties(),
        };

        technologies.append(object);
    }

    return technologies;
}

ConnmanObjectList UtManager::ManagerMock::GetServices() const
{
    ConnmanObjectList services;
    QMapIterator<QString, ServiceMock *> it(m_services);
    while (it.hasNext()) {
        it.next();

        ConnmanObject object = {
            QDBusObjectPath(it.key()),
            it.value()->properties(),
        };

        services.append(object);
    }

    return services;
}

void UtManager::ManagerMock::RegisterCounter(const QDBusObjectPath &path, quint32 accuracy,
        quint32 period, const QDBusMessage &message)
{
    if (m_counters.contains(path.path())) {
        const QString err = QString("Counter at path '%1' already exists").arg(path.path());
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }

    m_counters[path.path()] = qMakePair(accuracy, period);
}

void UtManager::ManagerMock::UnregisterCounter(const QDBusObjectPath &path,
        const QDBusMessage &message)
{
    if (!m_counters.contains(path.path())) {
        const QString err = QString("Counter at path '%1' does not exist").arg(path.path());
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }

    m_counters.remove(path.path());
}

void UtManager::ManagerMock::mock_setState(const QString &state)
{
    m_properties["State"] = state;
    Q_EMIT PropertyChanged("State", QDBusVariant(state));
}

void UtManager::ManagerMock::mock_temporarilyUnregister()
{
    bus().unregisterService("net.connman");

    if (!bus().registerService("net.connman")) {
        qWarning("%s: Failed to re-register service", Q_FUNC_INFO);
    }
}

void UtManager::ManagerMock::mock_addService(const QString &path, const QVariantMap &properties,
        const QDBusMessage &message)
{
    if (m_services.contains(path)) {
        const QString err = QString("Service at path '%1' already exists").arg(path);
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }

    ServiceMock *const service = new ServiceMock(properties, this);

    if (!bus().registerObject(path, service, QDBusConnection::ExportScriptableContents)) {
        const QString err = QString("Failed to register service object: %1")
            .arg(bus().lastError().message());
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        delete service;
        return;
    }

    m_services[path] = service;


    ConnmanObject object = {
        QDBusObjectPath(path),
        service->properties(),
    };

    Q_EMIT ServicesChanged(ConnmanObjectList() << object, QList<QDBusObjectPath>());
}

void UtManager::ManagerMock::mock_removeService(const QString &path, const QDBusMessage &message)
{
    if (!m_services.contains(path)) {
        const QString err = QString("Service at path '%1' does not exist").arg(path);
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }

    ServiceMock *const service = m_services.take(path);

    bus().unregisterObject(path);

    delete service;

    Q_EMIT ServicesChanged(ConnmanObjectList(), QList<QDBusObjectPath>() << QDBusObjectPath(path));
}

void UtManager::ManagerMock::mock_addTechnology(const QString &path, const QVariantMap &properties,
        const QDBusMessage &message)
{
    if (m_technologies.contains(path)) {
        const QString err = QString("Technology at path '%1' already exists").arg(path);
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }

    TechnologyMock *const technology = new TechnologyMock(properties, this);

    if (!bus().registerObject(path, technology, QDBusConnection::ExportScriptableContents)) {
        const QString err = QString("Failed to register technology object: %1")
            .arg(bus().lastError().message());
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        delete technology;
        return;
    }

    m_technologies[path] = technology;

    Q_EMIT TechnologyAdded(QDBusObjectPath(path), technology->properties());
}

void UtManager::ManagerMock::mock_removeTechnology(const QString &path, const QDBusMessage &message)
{
    if (!m_technologies.contains(path)) {
        const QString err = QString("Technology at path '%1' does not exist").arg(path);
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }

    TechnologyMock *const technology = m_technologies.take(path);

    bus().unregisterObject(path);

    delete technology;

    Q_EMIT TechnologyRemoved(QDBusObjectPath(path));
}

quint32 UtManager::ManagerMock::mock_counterAccuracy(const QString &path,
        const QDBusMessage &message)
{
    if (!m_counters.contains(path)) {
        const QString err = QString("Counter at path '%1' does not exist").arg(path);
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return 0;
    }

    return m_counters[path].first;
}

quint32 UtManager::ManagerMock::mock_counterPeriod(const QString &path,
        const QDBusMessage &message)
{
    if (!m_counters.contains(path)) {
        const QString err = QString("Counter at path '%1' does not exist").arg(path);
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return 0;
    }

    return m_counters[path].second;
}

bool UtManager::ManagerMock::mock_counterRegistered(const QString &path)
{
    return m_counters.contains(path);
}

TEST_MAIN_WITH_MOCK(UtManager, UtManager::ManagerMock)

#include "ut_manager.moc"
