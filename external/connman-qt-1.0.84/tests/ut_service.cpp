#include <QtCore/QPointer>

#include "../libconnman-qt/networkservice.h"
#include "testbase.h"

namespace Tests {

class UtService : public TestBase
{
    Q_OBJECT

public:
    class ManagerMock;
    class ServiceMock;

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testProperties_data();
    void testProperties();
    void testWriteProperties_data();
    void testWriteProperties();
    void testSetPath();
    void testPropertiesAfterSetPath_data();
    void testPropertiesAfterSetPath();
    void testPropertySpontaneousChange_data();
    void testPropertySpontaneousChange();
    void testConnect();
    void testDisconnect();
    void testConnectFailure();
    void testRemove();

private:
    QPointer<NetworkService> m_service;
    // Second instance used to verify change is really propagated through connman
    QPointer<NetworkService> m_otherService;
};

class UtService::ManagerMock : public MainObjectMock
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.Manager")

public:
    ManagerMock();
};

class UtService::ServiceMock : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.Service")

public:
    ServiceMock(const QVariantMap &properties, ManagerMock *manager);

public:
    Q_SCRIPTABLE QVariantMap GetProperties() const;
    Q_SCRIPTABLE void SetProperty(const QString &name, const QDBusVariant &value,
        const QDBusMessage &message);
    Q_SCRIPTABLE void Connect(const QDBusMessage &message);
    Q_SCRIPTABLE void Disconnect();
    Q_SCRIPTABLE void Remove();

    // mock API
    Q_SCRIPTABLE void mock_setConnectShouldFail(bool connectShouldFail);
    Q_SCRIPTABLE bool mock_removeCalled() const;
    Q_SCRIPTABLE void mock_setProperty(const QString &name, const QDBusVariant &value,
        const QDBusMessage &message);

signals:
    Q_SCRIPTABLE void PropertyChanged(const QString &name, const QDBusVariant &value);

private:
    QVariantMap m_properties;
    bool m_connectShouldFail;
    bool m_removeCalled;
};

} // namespace Tests

using namespace Tests;

/*
 * \class Tests::UtService
 */

void UtService::initTestCase()
{
    QVERIFY(waitForService("net.connman", "/", "net.connman.Manager"));
    m_service = new NetworkService("/service0", QVariantMap(), this);
    m_otherService = new NetworkService("/service0", QVariantMap(), this);
}

void UtService::cleanupTestCase()
{
    delete m_service;
    delete m_otherService;
}

void UtService::testProperties_data()
{
    QTest::addColumn<QVariant>("expected");

    createTestPropertyData(defaultServiceProperties(), serviceDBusProperty2QtProperty);
    QTest::newRow("path") << QVariant("/service0");
}

void UtService::testProperties()
{
    QFETCH(QVariant, expected);

    testProperty(*m_service, QTest::currentDataTag(), expected);
}

void UtService::testWriteProperties_data()
{
    QTest::addColumn<QVariant>("newValue");

    QTest::newRow("autoConnect") << QVariant(true);
    QVariantMap ipv4Config;
    ipv4Config["Method"] = "dhcp";
    ipv4Config["Address"] = "10.0.24.24";
    ipv4Config["Netmask"] = "255.255.224.0";
    ipv4Config["Gateway"] = "10.0.24.1";
    QTest::newRow("ipv4Config") << QVariant(ipv4Config);
    QVariantMap ipv6Config;
    ipv6Config["Method"] = "dhcp";
    ipv6Config["Address"] = "fd30:84f5:4397:1676:d:e:a:d";
    ipv6Config["PrefixLength"] = "65";
    ipv6Config["Gateway"] = "fd30:84f5:4397:1676:d:e:a:1";
    ipv6Config["Privacy"] = "disabled";
    QTest::newRow("ipv6Config") << QVariant(ipv6Config);
    QTest::newRow("nameserversConfig") << QVariant(QStringList() << "127.0.1.1" << "127.0.1.2");
    QTest::newRow("domainsConfig") << QVariant(QStringList() << "foo.org" << "foo.com");
    QVariantMap proxyConfig;
    proxyConfig["Method"] = "auto";
    proxyConfig["URL"] = "http://proxy.foo.org";
    proxyConfig["Sercers"] = QStringList() << "proxy24.foo.org";
    proxyConfig["Excludes"] = QStringList() << "drct1.com" << "drct2.com";
    QTest::newRow("proxyConfig") << QVariant(proxyConfig);
}

void UtService::testWriteProperties()
{
    QFETCH(QVariant, newValue);

    testWriteProperty(m_service, m_otherService, QTest::currentDataTag(), newValue);
}

void UtService::testSetPath()
{
    // notice that m_otherService service is switched path here. This is for
    // testPropertySpontaneousChange() to no collide with this test.
    m_otherService->setPath("/service1");
    QCOMPARE(m_otherService->path(), QString("/service1"));
}

void UtService::testPropertiesAfterSetPath_data()
{
    QTest::addColumn<QVariant>("expected");

    createTestPropertyData(alternateDefaultServiceProperties(),
            serviceDBusProperty2QtProperty);
    QTest::newRow("path") << QVariant("/service1");
}

void UtService::testPropertiesAfterSetPath()
{
    QFETCH(QVariant, expected);

    testProperty(*m_otherService, QTest::currentDataTag(), expected);
}

void UtService::testPropertySpontaneousChange_data()
{
    QTest::addColumn<QVariant>("newValue");

    const QVariantMap properties(alternateDefaultServiceProperties());
    QMapIterator<QString, QVariant> it(properties);
    while (it.hasNext()) {
        it.next();

        if (serviceDBusProperty2QtProperty(it.key()).isEmpty()) {
            continue;
        }

        QTest::newRow(qPrintable(it.key())) << it.value();
    }
}

void UtService::testPropertySpontaneousChange()
{
    QDBusInterface service("net.connman", "/service0", "net.connman.Service", bus());

    const char *const property = QTest::currentDataTag();
    const QByteArray qtProperty = serviceDBusProperty2QtProperty(property).toLocal8Bit();
    Q_ASSERT(!qtProperty.isEmpty());

    if (property == QLatin1String("Type")) {
        QSKIP("For some reason the property is matched with first letter small case in "
                "NetworkService::updateProperty() -> change would not be detected", SkipSingle);
    }

    QFETCH(QVariant, newValue);

    const QByteArray notifySignal = TestBase::notifySignal(*m_service, qtProperty);

    SignalSpy spy(m_service, notifySignal);

    // set it
    QDBusReply<void> reply = service.call("mock_setProperty", property,
            QVariant::fromValue(QDBusVariant(newValue)));
    QVERIFY2(reply.isValid(), qPrintable(reply.error().message()));

    QVERIFY(waitForSignals(SignalSpyList() << &spy));

    // TODO: test signal argument if available
    QVERIFY(m_service->property(qtProperty).isValid());
    QCOMPARE(m_service->property(qtProperty), newValue);
}

void UtService::testConnect()
{
    SignalSpy stateChangedSpy(m_service, SIGNAL(stateChanged(QString)));

    m_service->requestConnect();

    QVERIFY(waitForSignal(&stateChangedSpy));

    QCOMPARE(stateChangedSpy.count(), 1);
    QCOMPARE(stateChangedSpy.at(0).at(0).toString(), QString("online"));
    QCOMPARE(m_service->state(), QString("online"));
}

void UtService::testDisconnect()
{
    SignalSpy stateChangedSpy(m_service, SIGNAL(stateChanged(QString)));

    m_service->requestDisconnect();

    QVERIFY(waitForSignal(&stateChangedSpy));

    QCOMPARE(stateChangedSpy.count(), 1);
    QCOMPARE(stateChangedSpy.at(0).at(0).toString(), QString("disconnect"));
    QCOMPARE(m_service->state(), QString("disconnect"));
}

void UtService::testConnectFailure()
{
    QDBusInterface service("net.connman", "/service0", "net.connman.Service", bus());

    SignalSpy connectRequestFailedSpy(m_service, SIGNAL(connectRequestFailed(QString)));
    SignalSpy stateChangedSpy(m_service, SIGNAL(stateChanged(QString)));

    QDBusReply<void> reply = service.call("mock_setConnectShouldFail", true);
    QVERIFY(reply.isValid());

    m_service->requestConnect();

    QVERIFY(waitForSignals(SignalSpyList() << &connectRequestFailedSpy << &stateChangedSpy));

    QCOMPARE(connectRequestFailedSpy.count(), 1);
    QCOMPARE(m_service->state(), QString("failure"));

    QCOMPARE(stateChangedSpy.count(), 1);
    QCOMPARE(stateChangedSpy.at(0).at(0).toString(), QString("failure"));
    QCOMPARE(m_service->state(), QString("failure"));
}

void UtService::testRemove()
{
    QDBusInterface service("net.connman", "/service0", "net.connman.Service", bus());

    m_service->remove();

    QDBusReply<bool> removed = service.call("mock_removeCalled");
    QVERIFY(removed.isValid());
    QCOMPARE(removed.value(), true);
}

/*
 * \class Tests::UtService::ManagerMock
 */

UtService::ManagerMock::ManagerMock()
    : MainObjectMock("net.connman", "/")
{
    ServiceMock *const service0 = new ServiceMock(defaultServiceProperties(), this);
    if (!bus().registerObject("/service0", service0, QDBusConnection::ExportScriptableContents)) {
        qFatal("Failed to register /service0: %s", qPrintable(bus().lastError().message()));
    }

    ServiceMock *const service1 = new ServiceMock(alternateDefaultServiceProperties(), this);
    if (!bus().registerObject("/service1", service1, QDBusConnection::ExportScriptableContents)) {
        qFatal("Failed to register /service1: %s", qPrintable(bus().lastError().message()));
    }
}

/*
 * \class Tests::UtService::ServiceMock
 */

UtService::ServiceMock::ServiceMock(const QVariantMap &properties, ManagerMock *manager)
    : QObject(manager),
      m_properties(properties),
      m_connectShouldFail(false),
      m_removeCalled(false)
{
}

QVariantMap UtService::ServiceMock::GetProperties() const
{
    return m_properties;
}

void UtService::ServiceMock::SetProperty(const QString &name, const QDBusVariant &value,
        const QDBusMessage &message)
{
    static const QSet<QString> writableProperties = QSet<QString>()
        << "AutoConnect"
        << "IPv4.Configuration"
        << "IPv6.Configuration"
        << "Nameservers.Configuration"
        << "Domains.Configuration"
        << "Proxy.Configuration";

    if (!writableProperties.contains(name)) {
        const QString err = QString("Property not writable '%1'").arg(name);
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }

    m_properties[name] = value.variant();

    Q_EMIT PropertyChanged(name, value);
}

void UtService::ServiceMock::Connect(const QDBusMessage &message)
{
    if (m_connectShouldFail) {
        bus().send(message.createErrorReply(QDBusError::Failed, "Failed to connect"));
        m_properties["State"] = "failure";
        Q_EMIT PropertyChanged("State", QDBusVariant("failure"));
        return;
    }

    m_properties["State"] = "online";
    Q_EMIT PropertyChanged("State", QDBusVariant("online"));
}

void UtService::ServiceMock::Disconnect()
{
    m_properties["State"] = "disconnect";
    Q_EMIT PropertyChanged("State", QDBusVariant("disconnect"));
}

void UtService::ServiceMock::Remove()
{
    m_removeCalled = true;
}

void UtService::ServiceMock::mock_setConnectShouldFail(bool connectShouldFail)
{
    m_connectShouldFail = connectShouldFail;
}

bool UtService::ServiceMock::mock_removeCalled() const
{
    return m_removeCalled;
}

void UtService::ServiceMock::mock_setProperty(const QString &name, const QDBusVariant &value,
        const QDBusMessage &message)
{
    if (!m_properties.contains(name)) {
        const QString err = QString("Invalid property name '%1'").arg(name);
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }

    m_properties[name] = value.variant();
    Q_EMIT PropertyChanged(name, value);
}

TEST_MAIN_WITH_MOCK(UtService, UtService::ManagerMock)

#include "ut_service.moc"
