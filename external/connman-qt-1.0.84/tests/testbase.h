#ifndef TESTBASE_H
#define TESTBASE_H

#include <QtCore/QMetaProperty>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtTest/QSignalSpy>
#include <QTest>

#include "../libconnman-qt/commondbustypes.h"

namespace Tests {

class TestBase : public QObject
{
public:
    class MainObjectMock;

protected:
    class SignalSpy;
    typedef QList<SignalSpy *> SignalSpyList;

    enum {
        SIGNAL_WAIT_TIMEOUT = 5000, // [ms]
    };

protected:
    TestBase();

protected:
    static QDBusConnection bus() { return QDBusConnection::systemBus(); }
    static QByteArray notifySignal(const QObject &object, const char *property);
    static bool waitForSignal(QObject *object, const char *signal);
    static bool waitForSignal(SignalSpy *signalSpy);
    static bool waitForSignals(const SignalSpyList &signalSpies);
    static bool waitForService(const QString &serviceName, const QString &path,
            const QString &interface);
    static void createTestPropertyData(const QVariantMap &expected,
            QString (*dbusProperty2QtProperty)(const QString &));
    static void testProperty(const QObject &object, const QString &property,
        const QVariant &expected);
    static void testWriteProperty(QObject *object, const char *property,
        const QVariant &newValue);
    static void testWriteProperty(QObject *object, QObject *otherObject, const char *property,
        const QVariant &newValue);
    static QString managerDBusProperty2QtProperty(const QString &property);
    static QString serviceDBusProperty2QtProperty(const QString &property);
    static QString technologyDBusProperty2QtProperty(const QString &property);
    static QString clockDBusProperty2QtProperty(const QString &property);
    static QVariantMap defaultManagerProperties();
    static QVariantMap defaultServiceProperties();
    static QVariantMap alternateDefaultServiceProperties();
    static QVariantMap defaultTechnologyProperties();
    static QVariantMap alternateDefaultTechnologyProperties();
    static QVariantMap defaultClockProperties();
};

class TestBase::MainObjectMock : public QObject
{
public:
    MainObjectMock(const QString &serviceName, const QString &path);

public:
    static void msgHandler(QtMsgType type, const char *message);
};

class TestBase::SignalSpy : public QSignalSpy
{
public:
    SignalSpy(QObject *object, const char *signal)
        : QSignalSpy(object, signal),
          m_object(object)
    {
    }

    QObject *object() const { return m_object; }

private:
    QPointer<QObject> m_object;
};

inline TestBase::TestBase()
{
    qputenv("DBUS_SYSTEM_BUS_ADDRESS", qgetenv("DBUS_SESSION_BUS_ADDRESS"));
}

inline QByteArray TestBase::notifySignal(const QObject &object, const char *property)
{
    Q_ASSERT(object.metaObject()->indexOfProperty(property) != -1);
    Q_ASSERT(object.metaObject()->property(object.metaObject()->indexOfProperty(property))
            .hasNotifySignal());

    return QByteArray(QTOSTRING(QSIGNAL_CODE)).append(object.metaObject()->property(
                object.metaObject()->indexOfProperty(property)
                ).notifySignal().signature());
}

inline bool TestBase::waitForSignal(QObject *object, const char *signal)
{
    SignalSpy spy(object, signal);
    return waitForSignals(SignalSpyList() << &spy);
}

inline bool TestBase::waitForSignal(SignalSpy *signalSpy)
{
    return waitForSignals(SignalSpyList() << signalSpy);
}

inline bool TestBase::waitForSignals(const SignalSpyList &signalSpies)
{
    struct H
    {
        static bool allReceived(const SignalSpyList &signalSpies)
        {
            Q_FOREACH (SignalSpy *signalSpy, signalSpies) {
                if (signalSpy->count() == 0) {
                    return false;
                }
            }
            return true;
        }
    };

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    connect(&timeoutTimer, SIGNAL(timeout()), &loop, SLOT(quit()));
    Q_FOREACH (SignalSpy *signalSpy, signalSpies) {
        connect(signalSpy->object(), signalSpy->signal().prepend(QSIGNAL_CODE),
                &loop, SLOT(quit()));
    }

    timeoutTimer.start(SIGNAL_WAIT_TIMEOUT);

    while (!H::allReceived(signalSpies)) {
        loop.exec();
        if (!timeoutTimer.isActive()) {
            return false;
        }
    }

    return true;
}

inline bool TestBase::waitForService(const QString &serviceName, const QString &path,
        const QString &interface)
{
    if (QDBusInterface(serviceName, path, interface, bus()).isValid()) {
        return true;
    }

    QDBusServiceWatcher watcher(serviceName, bus(), QDBusServiceWatcher::WatchForRegistration);
    return waitForSignal(&watcher, SIGNAL(serviceRegistered(QString)));
}

inline void TestBase::createTestPropertyData(const QVariantMap &expected,
        QString (*dbusProperty2QtProperty)(const QString &))
{
    QMapIterator<QString, QVariant> it(expected);

    while (it.hasNext()) {
        it.next();

        const QString qtProperty = (dbusProperty2QtProperty)(it.key());
        if (qtProperty.isEmpty()) {
            continue;
        }

        if (it.value().canConvert(QVariant::Map)) {
            const QVariantMap mapTypeProperty = it.value().toMap();
            QMapIterator<QString, QVariant> it2(mapTypeProperty);
            while (it2.hasNext()) {
                it2.next();

                QTest::newRow(qPrintable(QString("%1/%2").arg(qtProperty).arg(it2.key())))
                    << it2.value();
            }
        } else {
            QTest::newRow(qPrintable(qtProperty)) << it.value();
        }
    }
}

inline void TestBase::testProperty(const QObject &object, const QString &property,
        const QVariant &expected)
{
    Q_ASSERT(property.count('/') <= 1);

    const QString propertyName = QString(property).section('/', 0, 0);
    const QString variantMapKey = QString(property).section('/', 1, 1);

    QVERIFY(object.property(qPrintable(propertyName)).isValid());

    if (variantMapKey.isEmpty()) {
        QVERIFY(object.property(qPrintable(propertyName)).type() != QVariant::Map);
        QCOMPARE(object.property(qPrintable(propertyName)), expected);
    } else {
        QCOMPARE(object.property(qPrintable(propertyName)).type(), QVariant::Map);
        QVERIFY(object.property(qPrintable(propertyName)).toMap().contains(variantMapKey));
        QCOMPARE(object.property(qPrintable(propertyName)).toMap()[variantMapKey], expected);
    }
}

inline void TestBase::testWriteProperty(QObject *object, const char *property,
        const QVariant &newValue)
{
    Q_ASSERT(!QString(property).contains('/'));

    const QByteArray notifySignal = TestBase::notifySignal(*object, property);

    SignalSpy spy(object, notifySignal);

    QVERIFY(object->setProperty(property, newValue));

    QVERIFY(waitForSignals(SignalSpyList() << &spy));

    // TODO: test signal argument if available
    QVERIFY(object->property(property).isValid());
    QCOMPARE(object->property(property), newValue);
}

inline void TestBase::testWriteProperty(QObject *object, QObject *otherObject, const char *property,
        const QVariant &newValue)
{
    Q_ASSERT(!QString(property).contains('/'));

    const QByteArray notifySignal = TestBase::notifySignal(*object, property);

    SignalSpy spy(object, notifySignal);
    SignalSpy otherSpy(otherObject, notifySignal);

    QVERIFY(object->setProperty(property, newValue));

    QVERIFY(waitForSignals(SignalSpyList() << &spy << &otherSpy));

    // TODO: test signal argument if available
    QVERIFY(object->property(property).isValid());
    QCOMPARE(object->property(property), newValue);
    QVERIFY(otherObject->property(property).isValid());
    QCOMPARE(otherObject->property(property), newValue);
}

inline QString TestBase::managerDBusProperty2QtProperty(const QString &property)
{
    const QString firstLower = QString(property).replace(0, 1, property.at(0).toLower());
    return firstLower;
}

inline QString TestBase::serviceDBusProperty2QtProperty(const QString &property)
{
    static const QSet<QString> notInQtApi = QSet<QString>()
        << "Immutable"
        << "Provider"
        << "Timeservers"
        << "Timeservers.Configuration";

    static QMap<QString, QString> extraMapping;
    if (extraMapping.isEmpty()) { // init once
        extraMapping["IPv4"] = "ipv4";
        extraMapping["IPv4.Configuration"] = "ipv4Config";
        extraMapping["IPv6"] = "ipv6";
        extraMapping["IPv6.Configuration"] = "ipv6Config";
        extraMapping["Nameservers.Configuration"] = "nameserversConfig";
        extraMapping["Domains.Configuration"] = "domainsConfig";
        extraMapping["Proxy.Configuration"] = "proxyConfig";
    }

    if (notInQtApi.contains(property)) {
        return QString();
    }

    if (extraMapping.contains(property)) {
        return extraMapping.value(property);
    }

    const QString firstLower = QString(property).replace(0, 1, property.at(0).toLower());
    return firstLower;
}

inline QString TestBase::technologyDBusProperty2QtProperty(const QString &property)
{
    static QMap<QString, QString> extraMapping;
    if (extraMapping.isEmpty()) { // init once
        extraMapping["TetheringIdentifier"] = "tetheringId";
    }

    if (extraMapping.contains(property)) {
        return extraMapping.value(property);
    }

    const QString firstLower = QString(property).replace(0, 1, property.at(0).toLower());
    return firstLower;
}

inline QString TestBase::clockDBusProperty2QtProperty(const QString &property)
{
    static const QSet<QString> notInQtApi = QSet<QString>()
        << "Time";

    if (notInQtApi.contains(property)) {
        return QString();
    }

    const QString firstLower = QString(property).replace(0, 1, property.at(0).toLower());
    return firstLower;
}

inline QVariantMap TestBase::defaultManagerProperties()
{
    static QVariantMap properties;
    static bool initialized = false;

    if (initialized) {
        return properties;
    }

    properties["State"] = "offline";
    properties["OfflineMode"] = false;
    properties["SessionMode"] = true;

    initialized = true;

    return properties;
}

inline QVariantMap TestBase::defaultServiceProperties()
{
    static QVariantMap properties;
    static bool initialized = false;

    if (initialized) {
        return properties;
    }

    properties["State"] = "failure";
    properties["Error"] = "dhcp-error";
    properties["Name"] = "Wireless BAR";
    properties["Type"] = "wifi";
    properties["Security"] = QStringList() << "none" << "wep";
    properties["Strength"] = 42;
    properties["Favorite"] = false;
    properties["Immutable"] = true;
    properties["AutoConnect"] = true;
    properties["Roaming"] = true;
    properties["Nameservers"] = QStringList() << "10.0.0.1" << "10.0.0.2";
    properties["Nameservers.Configuration"] = QStringList() << "127.0.0.1" << "127.0.0.2";
    properties["Timeservers"] = QStringList() << "time1.foo.org" << "time2.foo.org";
    properties["Timeservers.Configuration"] = QStringList() << "time1.foo.com" << "time2.foo.com";
    properties["Domains"] = QStringList() << "foo.org" << "foo.com";
    properties["Domains.Configuration"] = QStringList() << "bar.org" << "bar.com";

    QVariantMap ipv4;
    ipv4["Method"] = "dhcp";
    ipv4["Address"] = "10.0.0.42";
    ipv4["Netmask"] = "255.255.255.0";
    ipv4["Gateway"] = "10.0.0.1";
    properties["IPv4"] = ipv4;

    QVariantMap ipv4Config;
    ipv4Config["Method"] = "manual";
    ipv4Config["Address"] = "10.0.42.42";
    ipv4Config["Netmask"] = "255.255.242.0";
    ipv4Config["Gateway"] = "10.0.42.1";
    properties["IPv4.Configuration"] = ipv4Config;

    QVariantMap ipv6;
    ipv6["Method"] = "dhcp";
    ipv6["Address"] = "fd30:84f5:4397:1676:d:e:a:d";
    ipv6["PrefixLength"] = 64;
    ipv6["Gateway"] = "fd30:84f5:4397:1676:d:e:a:1";
    ipv6["Privacy"] = "prefered";
    properties["IPv6"] = ipv6;

    QVariantMap ipv6Config;
    ipv6Config["Method"] = "manual";
    ipv6Config["Address"] = "fd30:84f5:4397:1676:b:e:e:f";
    ipv6Config["PrefixLength"] = 64;
    ipv6Config["Gateway"] = "fd30:84f5:4397:1676:b:e:e:1";
    ipv6Config["Privacy"] = "enabled";
    properties["IPv6.Configuration"] = ipv6Config;

    QVariantMap proxy;
    proxy["Method"] = "auto";
    proxy["URL"] = "proxy.foo.org";
    proxy["Servers"] = QStringList();
    proxy["Excludes"] = QStringList();
    properties["Proxy"] = proxy;

    QVariantMap proxyConfig;
    proxyConfig["Method"] = "manual";
    proxyConfig["URL"] = "";
    proxyConfig["Servers"] = QStringList() << "proxy42.foo.org";
    proxyConfig["Excludes"] = QStringList() << "drct1.org" << "drct2.org";
    properties["Proxy.Configuration"] = proxyConfig;

    QVariantMap provider;
    provider["Host"] = "42.42.42.42";
    provider["Domain"] = "42.org";
    provider["Name"] = "Fortytwo";
    provider["Type"] = "openvpn";
    properties["Provider"] = provider;

    QVariantMap ethernet;
    ethernet["Method"] = "auto";
    ethernet["Interface"] = "eth123";
    ethernet["Address"] = "de:ad:be:ef:de:ad";
    ethernet["MTU"] = 1542;
    properties["Ethernet"] = ethernet;

    initialized = true;

    return properties;
}

inline QVariantMap TestBase::alternateDefaultServiceProperties()
{
    static QVariantMap properties;
    static bool initialized = false;

    if (initialized) {
        return properties;
    }

    properties["State"] = "disconnect";
    properties["Error"] = "unknown-error";
    properties["Name"] = "Ethernet Foo";
    properties["Type"] = "ethernet";
    properties["Security"] = QStringList() << "none";
    properties["Strength"] = 24;
    properties["Favorite"] = true;
    properties["Immutable"] = false;
    properties["AutoConnect"] = false;
    properties["Roaming"] = false;
    properties["Nameservers"] = QStringList() << "10.1.0.1" << "10.1.0.2";
    properties["Nameservers.Configuration"] = QStringList() << "127.1.0.1" << "127.1.0.2";
    properties["Timeservers"] = QStringList() << "time1.bar.org" << "time2.bar.org";
    properties["Timeservers.Configuration"] = QStringList() << "time1.bar.com" << "time2.bar.com";
    properties["Domains"] = QStringList() << "bar.org" << "bar.com";
    properties["Domains.Configuration"] = QStringList() << "foo.org" << "foo.com";

    QVariantMap ipv4;
    ipv4["Method"] = "manual";
    ipv4["Address"] = "10.0.42.42";
    ipv4["Netmask"] = "255.255.242.0";
    ipv4["Gateway"] = "10.0.42.1";
    properties["IPv4"] = ipv4;

    QVariantMap ipv4Config;
    ipv4Config["Method"] = "dhcp";
    ipv4Config["Address"] = "10.0.0.42";
    ipv4Config["Netmask"] = "255.255.255.0";
    ipv4Config["Gateway"] = "10.0.0.1";
    properties["IPv4.Configuration"] = ipv4Config;

    QVariantMap ipv6;
    ipv6["Method"] = "manual";
    ipv6["Address"] = "fd30:84f5:4397:1676:b:e:e:f";
    ipv6["PrefixLength"] = 64;
    ipv6["Gateway"] = "fd30:84f5:4397:1676:b:e:e:1";
    ipv6["Privacy"] = "enabled";
    properties["IPv6"] = ipv6;

    QVariantMap ipv6Config;
    ipv6Config["Method"] = "dhcp";
    ipv6Config["Address"] = "fd30:84f5:4397:1676:d:e:a:d";
    ipv6Config["PrefixLength"] = 64;
    ipv6Config["Gateway"] = "fd30:84f5:4397:1676:d:e:a:1";
    ipv6Config["Privacy"] = "prefered";
    properties["IPv6.Configuration"] = ipv6Config;

    QVariantMap proxy;
    proxy["Method"] = "manual";
    proxy["URL"] = "";
    proxy["Servers"] = QStringList() << "proxy42.foo.org";
    proxy["Excludes"] = QStringList() << "drct1.org" << "drct2.org";
    properties["Proxy"] = proxy;

    QVariantMap proxyConfig;
    proxyConfig["Method"] = "auto";
    proxyConfig["URL"] = "proxy.foo.org";
    proxyConfig["Servers"] = QStringList();
    proxyConfig["Excludes"] = QStringList();
    properties["Proxy.Configuration"] = proxyConfig;

    QVariantMap provider;
    provider["Host"] = "24.24.24.24";
    provider["Domain"] = "24.org";
    provider["Name"] = "Twentyfour";
    provider["Type"] = "ciscovpn";
    properties["Provider"] = provider;

    QVariantMap ethernet;
    ethernet["Method"] = "manual";
    ethernet["Interface"] = "eth321";
    ethernet["Address"] = "de:ad:be:ef:be:ef";
    ethernet["MTU"] = 1524;
    properties["Ethernet"] = ethernet;

    initialized = true;

    return properties;
}

inline QVariantMap TestBase::defaultTechnologyProperties()
{
    static QVariantMap properties;
    static bool initialized = false;

    if (initialized) {
        return properties;
    }

    properties["Powered"] = true;
    properties["Connected"] = false;
    properties["Name"] = "Wi-Fi interface FOO";
    properties["Type"] = "wifi";
    properties["Tethering"] = false;
    properties["TetheringIdentifier"] = "foo-tether-identifier";
    properties["TetheringPassphrase"] = "foo-tether-passwd";
    properties["IdleTimeout"] = 42;

    initialized = true;

    return properties;
}

inline QVariantMap TestBase::alternateDefaultTechnologyProperties()
{
    static QVariantMap properties;
    static bool initialized = false;

    if (initialized) {
        return properties;
    }

    properties["Powered"] = false;
    properties["Connected"] = true;
    properties["Name"] = "Ethernet interface BAR";
    properties["Type"] = "ethernet";
    properties["Tethering"] = true;
    properties["TetheringIdentifier"] = "bar-tether-identifier";
    properties["TetheringPassphrase"] = "bar-tether-passwd";
    properties["IdleTimeout"] = 24;

    initialized = true;

    return properties;
}

inline QVariantMap TestBase::defaultClockProperties()
{
    static QVariantMap properties;
    static bool initialized = false;

    if (initialized) {
        return properties;
    }

    properties["Time"] = 0;
    properties["Timezone"] = "Europe/Prague";
    properties["TimezoneUpdates"] = "auto";
    properties["TimeUpdates"] = "auto";
    properties["Timeservers"] = QStringList() << "time1.foo.org" << "time2.foo.org";

    initialized = true;

    return properties;
}

/*
 * \class Tests::TestBase::MainObjectMock
 */

inline TestBase::MainObjectMock::MainObjectMock(const QString &serviceName, const QString &path)
{
    registerCommonDataTypes();

    if (!bus().registerObject(path, this, QDBusConnection::ExportScriptableContents)) {
        qFatal("Failed to register mock D-Bus object at path '%s': '%s'",
                qPrintable(path), qPrintable(bus().lastError().message()));
    }

    if (!bus().registerService(serviceName)) {
        qFatal("Failed to register mock D-Bus service '%s': '%s'",
                qPrintable(serviceName), qPrintable(bus().lastError().message()));
    }
}

inline void TestBase::MainObjectMock::msgHandler(QtMsgType type, const char *message)
{
    qInstallMsgHandler(0);
    qt_message_output(type, QByteArray("MOCK   : ").append(message));
    qInstallMsgHandler(msgHandler);
}

#define TEST_MAIN_WITH_MOCK(TestClass, MockClass)                            \
    int main(int argc, char *argv[])                                         \
    {                                                                        \
        if (argc == 2 && argv[1] == QLatin1String("--mock")) {               \
            qInstallMsgHandler(Tests::TestBase::MainObjectMock::msgHandler); \
            QApplication app(argc, argv);                                    \
                                                                             \
            qDebug("%s: starting...", Q_FUNC_INFO);                          \
                                                                             \
            MockClass mock;                                                  \
                                                                             \
            return app.exec();                                               \
        } else {                                                             \
            QApplication app(argc, argv);                                    \
                                                                             \
            TestClass test;                                                  \
                                                                             \
            QProcess mock;                                                   \
            mock.setProcessChannelMode(QProcess::ForwardedChannels);         \
            mock.start(app.applicationFilePath(), QStringList("--mock"));    \
            if (mock.state() == QProcess::NotRunning) {                      \
                qFatal("Failed to start mock");                              \
            }                                                                \
                                                                             \
            const int retv = QTest::qExec(&test, argc, argv);                \
                                                                             \
            mock.terminate();                                                \
            mock.waitForFinished();                                          \
                                                                             \
            return retv;                                                     \
        }                                                                    \
    }                                                                        \


} // namespace Tests

#endif // TESTBASE_H
