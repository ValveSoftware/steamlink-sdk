#include <QtCore/QPointer>

#include "../libconnman-qt/networkmanager.h"
#include "../libconnman-qt/useragent.h"
#include "testbase.h"

namespace Tests {

class UtAgent : public TestBase
{
    Q_OBJECT

public:
    class ManagerMock;

public:
    UtAgent();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testAgentRegistered();
    void testProperties_data();
    void testProperties();
    void testRequestInput();
    void testRequestInputCanceledByUser();
    void testCancel();
    void testReportError();
    void testConnectionRequestType();
    void testConnectionRequest();
    void testConnectionRequestSuppress();
    void testRegisterAgainOnManagerAvailability();

private:
    QPointer<UserAgent> m_userAgent;
};

class UtAgent::ManagerMock : public MainObjectMock
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.Manager")

public:
    ManagerMock();

public:
    Q_SCRIPTABLE QVariantMap GetProperties() const;
    Q_SCRIPTABLE ConnmanObjectList GetTechnologies() const;
    Q_SCRIPTABLE ConnmanObjectList GetServices() const;
    Q_SCRIPTABLE void RegisterAgent(const QDBusObjectPath &path);
    Q_SCRIPTABLE void UnregisterAgent(const QDBusObjectPath &path);

    // mock API
    Q_SCRIPTABLE int mock_numberRegistered(const QDBusObjectPath &path) const;
    Q_SCRIPTABLE QVariantMap mock_requestInput(const QDBusObjectPath &agentPath,
            const QDBusObjectPath &service, const QVariantMap &fields, const QDBusMessage &message);
    Q_SCRIPTABLE QVariantMap mock_requestInputExpectCancel(const QDBusObjectPath &agentPath,
            const QDBusObjectPath &service, const QVariantMap &fields, const QDBusMessage &message);
    Q_SCRIPTABLE void mock_cancel(const QDBusObjectPath &agentPath,
            const QDBusObjectPath &service, const QVariantMap &fields, const QDBusMessage &message);
    Q_SCRIPTABLE void mock_reportError(const QDBusObjectPath &agentPath,
            const QDBusObjectPath &service, const QString &error, const QDBusMessage &message);
    Q_SCRIPTABLE QString mock_requestConnect(const QDBusObjectPath &agentPath,
            const QDBusMessage &message);
    Q_SCRIPTABLE void mock_temporarilyUnregister(const QDBusMessage &message);

private:
    QMap<QDBusObjectPath, int> m_registeredAgents;
};

} // namespace Tests

using namespace Tests;

/*
 * \class Tests::UtAgent
 */

Q_DECLARE_METATYPE(QDBusPendingCallWatcher*) // needed by waitForSignal

UtAgent::UtAgent()
{
    qRegisterMetaType<QDBusPendingCallWatcher *>(); // needed by waitForSignal
}

void UtAgent::initTestCase()
{
    QVERIFY(waitForService("net.connman", "/", "net.connman.Manager"));
    m_userAgent = new UserAgent(this);
}

void UtAgent::cleanupTestCase()
{
    delete m_userAgent;
}

void UtAgent::testAgentRegistered()
{
    QDBusInterface manager("net.connman", "/", "net.connman.Manager");
    QDBusReply<int> numberRegistered = manager.call("mock_numberRegistered",
            QVariant::fromValue(QDBusObjectPath(m_userAgent->path())));
    QVERIFY(numberRegistered.isValid());
    QCOMPARE(numberRegistered.value(), 1);
}

void UtAgent::testProperties_data()
{
    QTest::addColumn<QVariant>("expected");

    QTest::newRow("connectionRequestType") << QVariant(QString()); // empty == default
    QTest::newRow("path") << QVariant(m_userAgent->path());
}

void UtAgent::testProperties()
{
    QFETCH(QVariant, expected);

    QVERIFY(m_userAgent->property(QTest::currentDataTag()).isValid());
    QCOMPARE(m_userAgent->property(QTest::currentDataTag()), expected);
}

void UtAgent::testRequestInput()
{
    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus());

    SignalSpy userInputRequestedSpy(m_userAgent, SIGNAL(userInputRequested(QString,QVariantMap)));

    const QDBusObjectPath injectedService = QDBusObjectPath("/foo");
    QVariantMap injectedFields;
    injectedFields["name"] = QVariantMap();
    injectedFields["password"] = QVariantMap();

    QDBusPendingReply<QVariantMap> reply = manager.asyncCall("mock_requestInput",
            QVariant::fromValue(QDBusObjectPath(m_userAgent->path())),
            QVariant::fromValue(injectedService), injectedFields);

    QVERIFY(waitForSignal(&userInputRequestedSpy));

    QCOMPARE(userInputRequestedSpy.count(), 1);
    QCOMPARE(userInputRequestedSpy.at(0).at(0).toString(), QString("/foo"));
    QCOMPARE(userInputRequestedSpy.at(0).at(1).toMap(), injectedFields);

    QVariantMap sentFields;
    sentFields["name"] = "myName";
    sentFields["password"] = "myPassword";

    m_userAgent->sendUserReply(sentFields);

    reply.waitForFinished();
    QVERIFY(reply.isValid());
    QCOMPARE(reply.value(), sentFields);
}

void UtAgent::testRequestInputCanceledByUser()
{
    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus());

    SignalSpy userInputRequestedSpy(m_userAgent, SIGNAL(userInputRequested(QString,QVariantMap)));

    const QDBusObjectPath injectedService = QDBusObjectPath("/foo");
    QVariantMap injectedFields;
    injectedFields["name"] = QVariantMap();
    injectedFields["password"] = QVariantMap();

    QDBusPendingReply<QVariantMap> reply = manager.asyncCall("mock_requestInputExpectCancel",
            QVariant::fromValue(QDBusObjectPath(m_userAgent->path())),
            QVariant::fromValue(injectedService), injectedFields);

    QVERIFY(waitForSignal(&userInputRequestedSpy));

    QCOMPARE(userInputRequestedSpy.count(), 1);
    QCOMPARE(userInputRequestedSpy.at(0).at(0).toString(), QString("/foo"));
    QCOMPARE(userInputRequestedSpy.at(0).at(1).toMap(), injectedFields);

    QVariantMap empty;
    m_userAgent->sendUserReply(empty);

    reply.waitForFinished();
    QVERIFY(!reply.isValid());
    QCOMPARE(reply.error().name(), QString("net.connman.Agent.Error.Canceled"));
}

void UtAgent::testCancel()
{
    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus());

    SignalSpy userInputRequestedSpy(m_userAgent, SIGNAL(userInputRequested(QString,QVariantMap)));
    SignalSpy userInputCanceledSpy(m_userAgent, SIGNAL(userInputCanceled()));

    const QDBusObjectPath injectedService = QDBusObjectPath("/foo");
    QVariantMap injectedFields;
    injectedFields["name"] = QVariantMap();
    injectedFields["password"] = QVariantMap();

    QDBusPendingReply<> reply = manager.asyncCall("mock_cancel",
            QVariant::fromValue(QDBusObjectPath(m_userAgent->path())),
            QVariant::fromValue(injectedService), injectedFields);

    QVERIFY(waitForSignal(&userInputRequestedSpy));

    QCOMPARE(userInputRequestedSpy.count(), 1);
    QCOMPARE(userInputRequestedSpy.at(0).at(0).toString(), QString("/foo"));
    QCOMPARE(userInputRequestedSpy.at(0).at(1).toMap(), injectedFields);

    QVERIFY(waitForSignal(&userInputCanceledSpy));

    QCOMPARE(userInputCanceledSpy.count(), 1);

    reply.waitForFinished();
    QVERIFY(reply.isValid());
}

void UtAgent::testReportError()
{
    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus());

    SignalSpy errorReportedSpy(m_userAgent, SIGNAL(errorReported(QString, QString)));

    const QString injectedServicePath = "/foo";
    const QDBusObjectPath injectedService = QDBusObjectPath(injectedServicePath);
    const QString injectedError = "foo-error-foo";

    QDBusPendingReply<> reply = manager.asyncCall("mock_reportError",
            QVariant::fromValue(QDBusObjectPath(m_userAgent->path())),
            QVariant::fromValue(injectedService), injectedError);

    QVERIFY(waitForSignal(&errorReportedSpy));

    QCOMPARE(errorReportedSpy.count(), 1);
    QCOMPARE(errorReportedSpy.at(0).at(0).toString(), injectedServicePath);
    QCOMPARE(errorReportedSpy.at(0).at(1).toString(), injectedError);

    reply.waitForFinished();
    QVERIFY(reply.isValid());
}

void UtAgent::testConnectionRequestType()
{
    m_userAgent->setProperty("connectionRequestType", "Suppress");
    QCOMPARE(m_userAgent->property("connectionRequestType").toString(), QString("Suppress"));
    m_userAgent->setProperty("connectionRequestType", "Clear");
    QCOMPARE(m_userAgent->property("connectionRequestType").toString(), QString("Clear"));
    m_userAgent->setProperty("connectionRequestType", QString());
    QCOMPARE(m_userAgent->property("connectionRequestType").toString(), QString());
}

void UtAgent::testConnectionRequest()
{
    // reset to initial state
    m_userAgent->setProperty("connectionRequestType", QString());
    QCOMPARE(m_userAgent->property("connectionRequestType").toString(), QString());

    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus());

    SignalSpy connectionRequestSpy(m_userAgent, SIGNAL(connectionRequest()));

    QDBusPendingReply<QString> reply = manager.asyncCall("mock_requestConnect",
            QVariant::fromValue(QDBusObjectPath(m_userAgent->path())));

    QVERIFY(waitForSignal(&connectionRequestSpy));

    QCOMPARE(connectionRequestSpy.count(), 1);

    reply.waitForFinished();
    QVERIFY(reply.isValid());
    QCOMPARE(reply.value(), QString());
}

void UtAgent::testConnectionRequestSuppress()
{
    // reset to initial state
    m_userAgent->setProperty("connectionRequestType", QString());
    QCOMPARE(m_userAgent->property("connectionRequestType").toString(), QString());

    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus());

    SignalSpy connectionRequestSpy(m_userAgent, SIGNAL(connectionRequest()));

    QDBusPendingReply<QString> reply1 = manager.asyncCall("mock_requestConnect",
            QVariant::fromValue(QDBusObjectPath(m_userAgent->path())));
    QDBusPendingReply<QString> reply2 = manager.asyncCall("mock_requestConnect",
            QVariant::fromValue(QDBusObjectPath(m_userAgent->path())));

    QVERIFY(waitForSignal(&connectionRequestSpy));

    QCOMPARE(connectionRequestSpy.count(), 1);

    reply1.waitForFinished();
    QVERIFY(reply1.isValid());
    QCOMPARE(reply1.value(), QString());

    QDBusPendingCallWatcher reply2Watcher(reply2);
    QVERIFY(waitForSignal(&reply2Watcher, SIGNAL(finished(QDBusPendingCallWatcher*))));
    QVERIFY(reply2.isValid());
    QCOMPARE(reply2.value(), QString("Suppress"));

    connectionRequestSpy.clear();

    m_userAgent->sendConnectReply(QString(), 0);
    qApp->processEvents();

    QDBusPendingReply<QString> reply3 = manager.asyncCall("mock_requestConnect",
            QVariant::fromValue(QDBusObjectPath(m_userAgent->path())));

    QVERIFY(waitForSignal(&connectionRequestSpy));

    QCOMPARE(connectionRequestSpy.count(), 1);

    reply3.waitForFinished();
    QVERIFY(reply3.isValid());
    QCOMPARE(reply3.value(), QString("Clear"));
}

void UtAgent::testRegisterAgainOnManagerAvailability()
{
    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus());

    QDBusPendingReply<> reply = manager.asyncCall("mock_temporarilyUnregister");

    QDBusPendingCallWatcher watcher(reply);
    QVERIFY(waitForSignal(&watcher, SIGNAL(finished(QDBusPendingCallWatcher*))));
    QVERIFY(reply.isValid());

    QDBusReply<int> numberRegisteredReply = manager.asyncCall("mock_numberRegistered",
            QVariant::fromValue(QDBusObjectPath(m_userAgent->path())));
    QVERIFY(numberRegisteredReply.isValid());
    QCOMPARE(numberRegisteredReply.value(), 1);
}

/*
 * \class Tests::UtAgent::ManagerMock
 */

UtAgent::ManagerMock::ManagerMock()
    : MainObjectMock("net.connman", "/")
{
}

QVariantMap UtAgent::ManagerMock::GetProperties() const
{
    QVariantMap properties;
    properties["State"] = "offline";
    properties["OfflineMode"] = false;
    properties["SessionMode"] = true;
    return properties;
}

ConnmanObjectList UtAgent::ManagerMock::GetTechnologies() const
{
    return ConnmanObjectList();
}

ConnmanObjectList UtAgent::ManagerMock::GetServices() const
{
    return ConnmanObjectList();
}

void UtAgent::ManagerMock::RegisterAgent(const QDBusObjectPath &path)
{
    ++m_registeredAgents[path];
}

void UtAgent::ManagerMock::UnregisterAgent(const QDBusObjectPath &path)
{
    --m_registeredAgents[path];
}

int UtAgent::ManagerMock::mock_numberRegistered(const QDBusObjectPath &path) const
{
    return m_registeredAgents.value(path);
}

QVariantMap UtAgent::ManagerMock::mock_requestInput(const QDBusObjectPath &agentPath,
        const QDBusObjectPath &service, const QVariantMap &fields, const QDBusMessage &message)
{
    QDBusInterface agent(message.service(), agentPath.path(), "net.connman.Agent", bus());
    QDBusPendingReply<QVariantMap> reply = agent.asyncCall("RequestInput",
            QVariant::fromValue(service), fields);
    reply.waitForFinished();
    if (!reply.isValid()) {
        const QString err = QString("Error calling RequestInput() on agent: %1")
            .arg(reply.error().message());
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return QVariantMap();
    }

    return reply.value();
}

QVariantMap UtAgent::ManagerMock::mock_requestInputExpectCancel(const QDBusObjectPath &agentPath,
        const QDBusObjectPath &service, const QVariantMap &fields, const QDBusMessage &message)
{
    QDBusInterface agent(message.service(), agentPath.path(), "net.connman.Agent", bus());
    QDBusPendingReply<QVariantMap> reply = agent.asyncCall("RequestInput",
            QVariant::fromValue(service), fields);
    reply.waitForFinished();
    if (!reply.isValid()) {
        bus().send(message.createErrorReply(reply.error()));
        return QVariantMap();
    }

    qWarning("%s: Succeeded calling RequestInput() while it was expected to fail", Q_FUNC_INFO);

    return reply.value();
}

void UtAgent::ManagerMock::mock_cancel(const QDBusObjectPath &agentPath,
        const QDBusObjectPath &service, const QVariantMap &fields, const QDBusMessage &message)
{
    QDBusInterface agent(message.service(), agentPath.path(), "net.connman.Agent", bus());
    QDBusPendingReply<QVariantMap> reply = agent.asyncCall("RequestInput",
            QVariant::fromValue(service), fields);

    QDBusPendingReply<> cancelReply = agent.asyncCall("Cancel");
    cancelReply.waitForFinished();
    if (!cancelReply.isValid()) {
        const QString err = QString("Error calling Cancel() on agent: %1")
            .arg(cancelReply.error().message());
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }
}

void UtAgent::ManagerMock::mock_reportError(const QDBusObjectPath &agentPath,
        const QDBusObjectPath &service, const QString &error, const QDBusMessage &message)
{
    QDBusInterface agent(message.service(), agentPath.path(), "net.connman.Agent", bus());
    QDBusPendingReply<> reply = agent.asyncCall("ReportError", QVariant::fromValue(service), error);
    reply.waitForFinished();
    if (!reply.isValid()) {
        const QString err = QString("Error calling ReportError() on agent: %1")
            .arg(reply.error().message());
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }
}

QString UtAgent::ManagerMock::mock_requestConnect(const QDBusObjectPath &agentPath,
        const QDBusMessage &message)
{
    QDBusInterface agent(message.service(), agentPath.path(), "net.connman.Agent", bus());
    QDBusPendingReply<QString> reply = agent.asyncCall("RequestConnect");
    reply.waitForFinished();
    if (!reply.isValid()) {
        const QString err = QString("Error calling RequestConnect on agent: %1")
            .arg(reply.error().message());
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return QString();
    }

    return reply.value();
}

void UtAgent::ManagerMock::mock_temporarilyUnregister(const QDBusMessage &message)
{
    bus().unregisterService("net.connman");

    m_registeredAgents.clear();

    if (!bus().registerService("net.connman")) {
        const QString err = QString("Failed to re-register service: %1")
            .arg(bus().lastError().message());
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
    }
}

TEST_MAIN_WITH_MOCK(UtAgent, UtAgent::ManagerMock)

#include "ut_agent.moc"
