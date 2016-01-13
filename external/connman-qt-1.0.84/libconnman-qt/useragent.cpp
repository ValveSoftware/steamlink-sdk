/*
 * Copyright © 2012, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include "useragent.h"
#include "networkmanager.h"

static const char AGENT_PATH[] = "/ConnectivityUserAgent";

UserAgent::UserAgent(QObject* parent) :
    QObject(parent),
    m_req_data(NULL),
    m_manager(NetworkManagerFactory::createInstance()),
    requestType(TYPE_DEFAULT),
    agentPath(QString())
{
    QString agentpath = QLatin1String("/ConnectivityUserAgent");
    setAgentPath(agentpath);
    connect(m_manager, SIGNAL(availabilityChanged(bool)),
            this, SLOT(updateMgrAvailability(bool)));
    requestTimer = new QTimer(this);
    requestTimer->setSingleShot(true);
    connect(requestTimer,SIGNAL(timeout()),this,SLOT(requestTimeout()));
}

UserAgent::~UserAgent()
{
    m_manager->unregisterAgent(QString(agentPath));
}

void UserAgent::requestUserInput(ServiceRequestData* data)
{
    m_req_data = data;
    Q_EMIT userInputRequested(data->objectPath, data->fields);
}

void UserAgent::cancelUserInput()
{
    delete m_req_data;
    m_req_data = NULL;
    Q_EMIT userInputCanceled();
}

void UserAgent::reportError(const QString &servicePath, const QString &error)
{
    Q_EMIT errorReported(servicePath, error);
}

void UserAgent::sendUserReply(const QVariantMap &input)
{
    if (m_req_data == NULL) {
        qWarning() << "Got reply for non-existing request";
        return;
    }

    if (!input.isEmpty()) {
        QDBusMessage &reply = m_req_data->reply;
        reply << input;
        QDBusConnection::systemBus().send(reply);
    } else {
        QDBusMessage error = m_req_data->msg.createErrorReply(
            QString("net.connman.Agent.Error.Canceled"),
            QString("canceled by user"));
        QDBusConnection::systemBus().send(error);
    }
    delete m_req_data;
    m_req_data = NULL;
}

void UserAgent::requestTimeout()
{
    qDebug() << Q_FUNC_INFO << requestMessage.arguments();
    setConnectionRequestType("Clear");
}

void UserAgent::sendConnectReply(const QString &replyMessage, int timeout)
{
    setConnectionRequestType(replyMessage);

    if (!requestTimer->isActive())
        requestTimer->start(timeout * 1000);
}

void UserAgent::updateMgrAvailability(bool available)
{
    if (available) {
        m_manager->registerAgent(QString(agentPath));
    } else {
        if (requestTimer->isActive())
            requestTimer->stop();
    }
}

void UserAgent::setConnectionRequestType(const QString &type)
{
    if (type == "Suppress") {
        requestType = TYPE_SUPPRESS;
    } else if (type == "Clear") {
        requestType = TYPE_CLEAR;
    } else {
        requestType = TYPE_DEFAULT;
    }
}

QString UserAgent::connectionRequestType() const
{
    switch (requestType) {
    case TYPE_SUPPRESS:
        return "Suppress";
        break;
    case TYPE_CLEAR:
        return "Clear";
        break;
    default:
        break;
    }
    return QString();
}

void UserAgent::requestConnect(const QDBusMessage &msg)
{
    QList<QVariant> arguments2;
    arguments2 << QVariant("Clear");
    requestMessage = msg.createReply(arguments2);

    QList<QVariant> arguments;
    arguments << QVariant(connectionRequestType());
    QDBusMessage error = msg.createReply(arguments);

    if (!QDBusConnection::systemBus().send(error)) {
        qWarning() << "Could not queue message";
    }

    if (connectionRequestType() == "Suppress") {
        return;
    }

    Q_EMIT connectionRequest();
    Q_EMIT userConnectRequested(msg);
    setConnectionRequestType("Suppress");
}

QString UserAgent::path() const
{
    return agentPath;
}

void UserAgent::setAgentPath(QString &path)
{
    if (path.isEmpty())
        return;

    new AgentAdaptor(this); // this object will be freed when UserAgent is freed
    agentPath = path;
    QDBusConnection::systemBus().registerObject(agentPath, this);

    if (m_manager->isAvailable()) {
        m_manager->registerAgent(QString(agentPath));
    }
}

void UserAgent::requestBrowser(const QString &servicePath, const QString &url,
                               const QDBusMessage &message)
{
    qDebug() << message.arguments();
    Q_EMIT browserRequested(servicePath, url);
}

////////////////////

AgentAdaptor::AgentAdaptor(UserAgent* parent)
  : QDBusAbstractAdaptor(parent),
    m_userAgent(parent)
{
    browserRequestTimer.invalidate();
}

AgentAdaptor::~AgentAdaptor()
{
}

void AgentAdaptor::Release()
{
}

void AgentAdaptor::ReportError(const QDBusObjectPath &service_path, const QString &error)
{
    m_userAgent->reportError(service_path.path(), error);
}

void AgentAdaptor::RequestBrowser(const QDBusObjectPath &service_path, const QString &url,
                                  const QDBusMessage &message)
{
    message.setDelayedReply(true);
    m_userAgent->requestBrowser(service_path.path(), url, message);
}

void AgentAdaptor::RequestInput(const QDBusObjectPath &service_path,
                                       const QVariantMap &fields,
                                       const QDBusMessage &message)
{
    QVariantMap json;
    Q_FOREACH (const QString &key, fields.keys()){
        QVariantMap payload = qdbus_cast<QVariantMap>(fields[key]);
        json.insert(key, payload);
    }

    message.setDelayedReply(true);

    ServiceRequestData *reqdata = new ServiceRequestData;
    reqdata->objectPath = service_path.path();
    reqdata->fields = json;
    reqdata->reply = message.createReply();
    reqdata->msg = message;

    m_userAgent->requestUserInput(reqdata);
}

void AgentAdaptor::Cancel()
{
    m_userAgent->cancelUserInput();
}

void AgentAdaptor::RequestConnect(const QDBusMessage &message)
{
    message.setDelayedReply(true);
    m_userAgent->requestConnect(message);
}


