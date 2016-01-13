/*
 * Copyright © 2010, Intel Corporation.
 * Copyright © 2012, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef NETWORKINGMODEL_H
#define NETWORKINGMODEL_H

#include <QDBusAbstractAdaptor>
#include <networkmanager.h>
#include <networktechnology.h>
#include <networkservice.h>

struct ServiceReqData
{
    QVariantMap fields;
    QDBusMessage reply;
    QDBusMessage msg;
};

/*
 * WARNING: this class is going to be deprecated. Use TechnologyModel and
 *          UserAgent classes instead.
 */
class NetworkingModel : public QObject
{
    Q_OBJECT;

    Q_PROPERTY(bool available READ isAvailable NOTIFY availabilityChanged);
    Q_PROPERTY(bool wifiPowered READ isWifiPowered WRITE setWifiPowered NOTIFY wifiPoweredChanged);
    Q_PROPERTY(QList<QObject*> networks READ networks NOTIFY networksChanged);

public:
    NetworkingModel(QObject* parent=0);
    virtual ~NetworkingModel();

    bool isAvailable() const;

    QList<QObject*> networks() const;
    bool isWifiPowered() const;
    void requestUserInput(ServiceReqData* data);
    void reportError(const QString &error);

public Q_SLOTS:
    void setWifiPowered(const bool &wifiPowered);
    void requestScan() const;
    void sendUserReply(const QVariantMap &input);

Q_SIGNALS:
    void availabilityChanged(bool available);
    void wifiPoweredChanged(const bool &wifiPowered);
    void networksChanged();
    void technologiesChanged();
    void userInputRequested(QVariantMap fields);
    void errorReported(const QString &error);
    void scanRequestFinished();

private:
    NetworkManager* m_manager;
    NetworkTechnology* m_wifi;
    ServiceReqData* m_req_data;

private Q_SLOTS:
    void updateTechnologies();
    void managerAvailabilityChanged(bool available);

private:
    Q_DISABLE_COPY(NetworkingModel);
};

class UserInputAgent : public QDBusAbstractAdaptor
{
    Q_OBJECT;
    Q_CLASSINFO("D-Bus Interface", "net.connman.Agent");

public:
    UserInputAgent(NetworkingModel* parent);
    virtual ~UserInputAgent();

public Q_SLOTS:
    void Release();
    void ReportError(const QDBusObjectPath &service_path, const QString &error);
    void RequestBrowser(const QDBusObjectPath &service_path, const QString &url);
    Q_NOREPLY void RequestInput(const QDBusObjectPath &service_path,
                                const QVariantMap &fields,
                                const QDBusMessage &message);
    void Cancel();

private:
    NetworkingModel* m_networkingmodel;
};

#endif //NETWORKINGMODEL_H
