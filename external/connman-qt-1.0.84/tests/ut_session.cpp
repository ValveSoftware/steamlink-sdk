#include <QtCore/QPointer>

#include "../libconnman-qt/connman_session_interface.h"
#include "../libconnman-qt/networksession.h"
#include "../libconnman-qt/sessionagent.h"
#include "testbase.h"

namespace Tests {

class UtSession : public TestBase
{
    Q_OBJECT

public:
    class ManagerMock;
    class SessionMock;

public:
    UtSession();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testProperties_data();
    void testProperties();
    void testWriteProperties_data();
    void testWriteProperties();
    void testRequestConnect();
    void testRequestDisconnect();
    void testRequestDestroy();
    void testSetPath();
    void testPropertiesAfterSetPath_data();
    void testPropertiesAfterSetPath();

private:
    QObject *findSessionNotificationAdaptor() const;

private:
    QPointer<NetworkSession> m_session;
    QMap<QString, QString> m_dbus2QtPropertyMapping;
};

class UtSession::ManagerMock : public MainObjectMock
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.Manager")

public:
    ManagerMock();

public:
    Q_SCRIPTABLE QVariantMap GetProperties() const;
    Q_SCRIPTABLE void SetProperty(const QString &name, const QVariant &value,
            const QDBusMessage &message);
    Q_SCRIPTABLE ConnmanObjectList GetTechnologies() const;
    Q_SCRIPTABLE ConnmanObjectList GetServices() const;
    Q_SCRIPTABLE QDBusObjectPath CreateSession(const QVariantMap &settings,
            const QDBusObjectPath &notifier, const QDBusMessage &message);
    Q_SCRIPTABLE void DestroySession(const QDBusObjectPath &path, const QDBusMessage &message);

signals:
    Q_SCRIPTABLE void PropertyChanged(const QString &name, const QVariant &value);

private:
    bool m_sessionMode;
    int m_sessionNextIndex;
    QMap<QString, SessionMock *> m_sessions;
};

class UtSession::SessionMock : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.Session")

public:
    SessionMock(const QString &path, const QString &notifierService,
        const QString &notifierPath, ManagerMock *manager);

public:
    Q_SCRIPTABLE void Connect(const QDBusMessage &message);
    Q_SCRIPTABLE void Disconnect(const QDBusMessage &message);
    Q_SCRIPTABLE void Destroy(const QDBusMessage &message);
    Q_SCRIPTABLE void Change(const QString &name, const QDBusVariant &value,
        const QDBusMessage &message);

    // mock API
    QString notifierService() const { return m_notifierService; }
    QString notifierPath() const { return m_notifierPath; }

    static QVariantMap defaultSettings();

private:
    ManagerMock *manager() const { return qobject_cast<ManagerMock *>(parent()); }

private slots:
    void notifyInitialUpdate();

private:
    const QString m_path;
    const QString m_notifierService;
    const QString m_notifierPath;
    QVariantMap m_settings;
};

} // namespace Tests

using namespace Tests;

/*
 * \class Tests::UtSession
 */

UtSession::UtSession()
{
    m_dbus2QtPropertyMapping["Interface"] = "sessionInterface";
    m_dbus2QtPropertyMapping["IPv4"] = "ipv4";
    m_dbus2QtPropertyMapping["IPv6"] = "ipv6";
}

void UtSession::initTestCase()
{
    QVERIFY(waitForService("net.connman", "/", "net.connman.Manager"));
    m_session = new NetworkSession(this);
    QVERIFY(waitForSignal(m_session, SIGNAL(settingsChanged(QVariantMap))));
}

void UtSession::cleanupTestCase()
{
    delete m_session;
}

void UtSession::testProperties_data()
{
    QTest::addColumn<QVariant>("expected");

    QTest::newRow("path") << QVariant("/ConnmanQmlSessionAgent");

    QMapIterator<QString, QVariant> it(SessionMock::defaultSettings());
    while (it.hasNext()) {
        it.next();
        const QString keyFirstLower = QString(it.key()).replace(0, 1, it.key().at(0).toLower());
        const QString propertyName = m_dbus2QtPropertyMapping.value(it.key(), keyFirstLower);

        QTest::newRow(qPrintable(propertyName)) << it.value();
    }
}

void UtSession::testProperties()
{
    QFETCH(QVariant, expected);

    QVERIFY(m_session->property(QTest::currentDataTag()).isValid());
    QCOMPARE(m_session->property(QTest::currentDataTag()), expected);
}

void UtSession::testWriteProperties_data()
{
    QTest::addColumn<QVariant>("newValue");

    QTest::newRow("allowedBearers") << QVariant(QStringList() << "ethernet" << "wifi");
    QTest::newRow("connectionType") << QVariant("local");
}

void UtSession::testWriteProperties()
{
    QFETCH(QVariant, newValue);

    const QByteArray notifySignal = this->notifySignal(*m_session, QTest::currentDataTag());

    SignalSpy spy(m_session, notifySignal);

    QVERIFY(m_session->setProperty(QTest::currentDataTag(), newValue));

    QVERIFY(waitForSignals(SignalSpyList() << &spy));

    QVERIFY(m_session->property(QTest::currentDataTag()).isValid());
    QCOMPARE(m_session->property(QTest::currentDataTag()), newValue);
}

void UtSession::testRequestConnect()
{
    SignalSpy stateSpy(m_session, SIGNAL(stateChanged(QString)));

    m_session->requestConnect();

    QVERIFY(waitForSignals(SignalSpyList() << &stateSpy));

    QVERIFY(m_session->property("state").isValid());
    QCOMPARE(m_session->property("state"), QVariant("connected"));
}

void UtSession::testRequestDisconnect()
{
    SignalSpy stateSpy(m_session, SIGNAL(stateChanged(QString)));

    m_session->requestDisconnect();

    QVERIFY(waitForSignals(SignalSpyList() << &stateSpy));

    QVERIFY(m_session->property("state").isValid());
    QCOMPARE(m_session->property("state"), QVariant("disconnect"));
}

void UtSession::testRequestDestroy()
{
    SignalSpy releasedSpy(m_session->m_sessionAgent, SIGNAL(released()));

    m_session->requestDestroy();

    QVERIFY(waitForSignals(SignalSpyList() << &releasedSpy));
}

void UtSession::testSetPath()
{
    SignalSpy stateSpy(m_session, SIGNAL(stateChanged(QString)));
    SignalSpy nameSpy(m_session, SIGNAL(nameChanged(QString)));
    SignalSpy bearerSpy(m_session, SIGNAL(bearerChanged(QString)));
    SignalSpy sessionInterfaceSpy(m_session, SIGNAL(sessionInterfaceChanged(QString)));
    SignalSpy ipv4Spy(m_session, SIGNAL(ipv4Changed(QVariantMap)));
    SignalSpy ipv6Spy(m_session, SIGNAL(ipv6Changed(QVariantMap)));
    SignalSpy allowedBearersSpy(m_session, SIGNAL(allowedBearersChanged(QStringList)));
    SignalSpy connectionTypeSpy(m_session, SIGNAL(connectionTypeChanged(QString)));

    QVERIFY(m_session->setProperty("path", "/ConnmanQmlSessionAgentNewPath"));

    QVERIFY(waitForSignals(SignalSpyList() << &stateSpy << &nameSpy << &bearerSpy
                << &sessionInterfaceSpy << &ipv4Spy << &ipv6Spy << &allowedBearersSpy
                << &connectionTypeSpy));
}

void UtSession::testPropertiesAfterSetPath_data()
{
    QTest::addColumn<QVariant>("expected");

    QTest::newRow("path") << QVariant("/ConnmanQmlSessionAgentNewPath");

    QMapIterator<QString, QVariant> it(SessionMock::defaultSettings());
    while (it.hasNext()) {
        it.next();
        const QString keyFirstLower = QString(it.key()).replace(0, 1, it.key().at(0).toLower());
        const QString propertyName = m_dbus2QtPropertyMapping.value(it.key(), keyFirstLower);

        QTest::newRow(qPrintable(propertyName)) << it.value();
    }
}

void UtSession::testPropertiesAfterSetPath()
{
    QFETCH(QVariant, expected);

    QVERIFY(m_session->property(QTest::currentDataTag()).isValid());
    QCOMPARE(m_session->property(QTest::currentDataTag()), expected);
}

QObject *UtSession::findSessionNotificationAdaptor() const
{
    QObject *sessionNotificationAdaptor = 0;
    Q_FOREACH (QObject *const object, m_session->m_sessionAgent->findChildren<QObject *>()) {
        if (object->inherits("SessionNotificationAdaptor")) {
            sessionNotificationAdaptor = object;
            break;
        }
    }
    Q_ASSERT(sessionNotificationAdaptor != 0);
    return sessionNotificationAdaptor;
}

/*
 * \class Tests::UtSession::ManagerMock
 */

UtSession::ManagerMock::ManagerMock()
    : MainObjectMock("net.connman", "/"),
      m_sessionMode(false),
      m_sessionNextIndex(0)
{
}

QVariantMap UtSession::ManagerMock::GetProperties() const
{
    QVariantMap properties;
    properties["State"] = "offline";
    properties["OfflineMode"] = false;
    properties["SessionMode"] = m_sessionMode;
    return properties;
}

void UtSession::ManagerMock::SetProperty(const QString &name, const QVariant &value,
        const QDBusMessage &message)
{
    if (name == "SessionMode") {
        m_sessionMode = value.toBool();
        Q_EMIT PropertyChanged(name, value);
    } else {
        const QString err = QString("Property '%1' not handled").arg(name);
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }
}

ConnmanObjectList UtSession::ManagerMock::GetTechnologies() const
{
    return ConnmanObjectList();
}

ConnmanObjectList UtSession::ManagerMock::GetServices() const
{
    return ConnmanObjectList();
}

QDBusObjectPath UtSession::ManagerMock::CreateSession(const QVariantMap &settings,
        const QDBusObjectPath &notifier, const QDBusMessage &message)
{
    if (!settings.isEmpty()) {
        const QString err = QString("Unimplemented case: settings is not empty");
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return QDBusObjectPath();
    }

    const int index = m_sessionNextIndex++;
    const QString path = QString("/session%1").arg(index);

    SessionMock *const session = new SessionMock(path, message.service(), notifier.path(), this);

    if (!bus().registerObject(path, session, QDBusConnection::ExportScriptableContents)) {
        const QString err = QString("Failed to register session object: %1")
            .arg(bus().lastError().message());
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        delete session;
        return QDBusObjectPath();
    }

    m_sessions[path] = session;
    return QDBusObjectPath(path);
}

void UtSession::ManagerMock::DestroySession(const QDBusObjectPath &path, const QDBusMessage &message)
{
    if (!m_sessions.contains(path.path())) {
        const QString err = QString("Session does not exists '%1'").arg(path.path());
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }

    SessionMock *const session = m_sessions.take(path.path());

    bus().send(message.createReply());

    QDBusInterface notifier(session->notifierService(), session->notifierPath(),
            "net.connman.Notification", bus());
    QDBusPendingReply<> releaseReply = notifier.asyncCall("Release");
    releaseReply.waitForFinished();
    if (releaseReply.isError()) {
        qWarning("%s: Failed to call Release() on notifier: %s", Q_FUNC_INFO,
                qPrintable(releaseReply.error().message()));
    }

    session->deleteLater();
}

/*
 * \class Tests::UtSession::SessionMock
 */

UtSession::SessionMock::SessionMock(const QString &path, const QString &notifierService,
        const QString &notifierPath, ManagerMock *manager)
    : QObject(manager),
      m_path(path),
      m_notifierService(notifierService),
      m_notifierPath(notifierPath),
      m_settings(defaultSettings())
{
    QTimer::singleShot(0, this, SLOT(notifyInitialUpdate()));
}

void UtSession::SessionMock::Connect(const QDBusMessage &message)
{
    m_settings["State"] = "connected";

    bus().send(message.createReply());

    QVariantMap changed;
    changed["State"] = "connected";

    QDBusInterface notifier(m_notifierService, m_notifierPath, "net.connman.Notification", bus());
    QDBusPendingReply<> updateReply = notifier.asyncCall("Update", changed);
    updateReply.waitForFinished();
    if (updateReply.isError()) {
        qWarning("%s: Failed to call 'Update' on notifier '%s': %s", Q_FUNC_INFO,
                qPrintable(m_notifierPath), qPrintable(updateReply.error().message()));
    }
}

void UtSession::SessionMock::Disconnect(const QDBusMessage &message)
{
    m_settings["State"] = "disconnect";

    bus().send(message.createReply());

    QVariantMap changed;
    changed["State"] = "disconnect";

    QDBusInterface notifier(m_notifierService, m_notifierPath, "net.connman.Notification", bus());
    QDBusPendingReply<> updateReply = notifier.asyncCall("Update", changed);
    updateReply.waitForFinished();
    if (updateReply.isError()) {
        qWarning("%s: Failed to call 'Update' on notifier '%s': %s", Q_FUNC_INFO,
                qPrintable(m_notifierPath), qPrintable(updateReply.error().message()));
    }
}

void UtSession::SessionMock::Destroy(const QDBusMessage &message)
{
    manager()->DestroySession(QDBusObjectPath(m_path), message);
}

void UtSession::SessionMock::Change(const QString &name, const QDBusVariant &value,
        const QDBusMessage &message)
{
    m_settings[name] = value.variant();

    bus().send(message.createReply());

    QVariantMap changed;
    changed[name] = value.variant();

    QDBusInterface notifier(m_notifierService, m_notifierPath, "net.connman.Notification", bus());
    QDBusPendingReply<> updateReply = notifier.asyncCall("Update", changed);
    updateReply.waitForFinished();
    if (updateReply.isError()) {
        qWarning("%s: Failed to call 'Update' on notifier '%s': %s", Q_FUNC_INFO,
                qPrintable(m_notifierPath), qPrintable(updateReply.error().message()));
    }
}

void UtSession::SessionMock::notifyInitialUpdate()
{
    QDBusInterface notifier(m_notifierService, m_notifierPath, "net.connman.Notification", bus());
    QDBusPendingReply<> updateReply = notifier.asyncCall("Update", m_settings);
    updateReply.waitForFinished();
    if (updateReply.isError()) {
        qWarning("%s: Failed to call 'Update' on '%s': %s", Q_FUNC_INFO, qPrintable(m_notifierPath),
                qPrintable(notifier.lastError().message()));
    }
}

QVariantMap UtSession::SessionMock::defaultSettings()
{
    static QVariantMap defaultSettings;
    static bool initialized = false;

    if (initialized) {
        return defaultSettings;
    }

    defaultSettings["State"] = "disconnect";
    defaultSettings["Name"] = "Wi-Fi FOO";
    defaultSettings["Bearer"] = "wifi";
    defaultSettings["Interface"] = "wlan0";
    QVariantMap ipv4;
    ipv4["Method"] = "dhcp";
    ipv4["Address"] = "10.0.0.42";
    ipv4["Netmask"] = "255.255.255.0";
    ipv4["Gateway"] = "10.0.0.1";
    defaultSettings["IPv4"] = ipv4;
    QVariantMap ipv6;
    ipv6["Method"] = "dhcp";
    ipv6["Address"] = "fd30:84f5:4397:1676:d:e:a:d";
    ipv6["PrefixLength"] = 64;
    ipv6["Gateway"] = "fd30:84f5:4397:1676:d:e:a:1";
    ipv6["Privacy"] = "prefered";
    defaultSettings["IPv6"] = ipv6;
    defaultSettings["AllowedBearers"] = "wifi";
    defaultSettings["ConnectionType"] = "internet";

    initialized = true;

    return defaultSettings;
}

TEST_MAIN_WITH_MOCK(UtSession, UtSession::ManagerMock)

#include "ut_session.moc"
