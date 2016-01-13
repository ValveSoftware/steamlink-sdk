/*
 * Copyright © 2010, Intel Corporation.
 * Copyright © 2012, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include <QDebug>
#include "networkingmodel.h"

static const char AGENT_PATH[] = "/WifiSettings";

#define CONNECT_WIFI_SIGNALS(wifi) \
    connect(wifi, \
        SIGNAL(poweredChanged(bool)), \
        this, \
        SIGNAL(wifiPoweredChanged(bool))); \
    connect(wifi, \
            SIGNAL(scanFinished()), \
            this, \
            SIGNAL(scanRequestFinished()))

NetworkingModel::NetworkingModel(QObject* parent)
  : QObject(parent),
    m_manager(NULL),
    m_wifi(NULL)
{
    m_manager = NetworkManagerFactory::createInstance();

    new UserInputAgent(this); // this object will be freed when NetworkingModel is freed

    m_wifi = m_manager->getTechnology("wifi"); // TODO: use constant literal
    if (m_wifi) {
        CONNECT_WIFI_SIGNALS(m_wifi);
    }

    connect(m_manager, SIGNAL(availabilityChanged(bool)),
            this, SLOT(managerAvailabilityChanged(bool)));

    connect(m_manager,
            SIGNAL(technologiesChanged()),
            this,
            SLOT(updateTechnologies()));

    connect(m_manager,
            SIGNAL(servicesChanged()),
            this,
            SIGNAL(networksChanged()));

    QDBusConnection::systemBus().registerObject(AGENT_PATH, this);
    m_manager->registerAgent(QString(AGENT_PATH));
}

NetworkingModel::~NetworkingModel()
{
    m_manager->unregisterAgent(QString(AGENT_PATH));
}

bool NetworkingModel::isAvailable() const
{
    return m_manager->isAvailable();
}

QList<QObject*> NetworkingModel::networks() const
{
    QList<QObject*> networks;
    // FIXME: how to get rid of this douple looping since we
    // must return a QList<QObject*>?
    Q_FOREACH (NetworkService* network, m_manager->getServices("wifi")) {
      networks.append(network);
    }
    return networks;
}

bool NetworkingModel::isWifiPowered() const
{
    if (m_wifi) {
        return m_wifi->powered();
    } else {
        qWarning() << "Can't get: wifi technology is NULL";
        return false;
    }
}

void NetworkingModel::setWifiPowered(const bool &wifiPowered)
{
    if (m_wifi) {
        m_wifi->setPowered(wifiPowered);
    } else {
        qWarning() << "Can't set: wifi technology is NULL";
    }
}

void NetworkingModel::requestScan() const
{
    qDebug() << "scan requested for wifi";
    if (m_wifi) {
        m_wifi->scan();
    }
}

void NetworkingModel::updateTechnologies()
{
    NetworkTechnology *test = NULL;
    if (m_wifi) {
        if ((test = m_manager->getTechnology("wifi")) == NULL) {
            // if wifi is set and manager doesn't return a wifi, it means
            // that wifi was removed
            m_wifi = NULL;
        }
    } else {
        if ((test = m_manager->getTechnology("wifi")) != NULL) {
            // if wifi is not set and manager returns a wifi, it means
            // that wifi was added
            m_wifi = test;

            CONNECT_WIFI_SIGNALS(m_wifi);
        }
    }

    Q_EMIT technologiesChanged();
}

void NetworkingModel::managerAvailabilityChanged(bool available)
{
    if (available)
        m_manager->registerAgent(QString(AGENT_PATH));

    Q_EMIT availabilityChanged(available);
}

void NetworkingModel::requestUserInput(ServiceReqData* data)
{
    m_req_data = data;
    Q_EMIT userInputRequested(data->fields);
}

void NetworkingModel::reportError(const QString &error) {
    Q_EMIT errorReported(error);
}

void NetworkingModel::sendUserReply(const QVariantMap &input) {
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
}

// DBus-adaptor for NetworkingModel /////////////////////

UserInputAgent::UserInputAgent(NetworkingModel* parent)
  : QDBusAbstractAdaptor(parent),
    m_networkingmodel(parent)
{
    // TODO
}

UserInputAgent::~UserInputAgent() {}

void UserInputAgent::Release()
{
    // here do clean up
}

void UserInputAgent::ReportError(const QDBusObjectPath &service_path, const QString &error)
{
    qDebug() << "From " << service_path.path() << " got this error:\n" << error;
    m_networkingmodel->reportError(error);
}

void UserInputAgent::RequestBrowser(const QDBusObjectPath &service_path, const QString &url)
{
    qDebug() << "Service " << service_path.path() << " wants browser to open hotspot's url " << url;
}

void UserInputAgent::RequestInput(const QDBusObjectPath &service_path,
                                       const QVariantMap &fields,
                                       const QDBusMessage &message)
{
    qDebug() << "Service " << service_path.path() << " wants user input";

    QVariantMap json;
    Q_FOREACH (const QString &key, fields.keys()){
        QVariantMap payload = qdbus_cast<QVariantMap>(fields[key]);
        json.insert(key, payload);
    }

    message.setDelayedReply(true);

    ServiceReqData *reqdata = new ServiceReqData;
    reqdata->fields = json;
    reqdata->reply = message.createReply();
    reqdata->msg = message;

    m_networkingmodel->requestUserInput(reqdata);
}

void UserInputAgent::Cancel()
{
    qDebug() << "WARNING: request to agent got canceled";
}
