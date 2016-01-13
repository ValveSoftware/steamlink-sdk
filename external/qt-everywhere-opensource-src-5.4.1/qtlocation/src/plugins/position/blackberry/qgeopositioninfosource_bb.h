/****************************************************************************
**
** Copyright (C) 2012 - 2013 BlackBerry Limited. All rights reserved.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtPositioning module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QGEOPOSITIONINFOSOURCE_BB_H
#define QGEOPOSITIONINFOSOURCE_BB_H

#include <QGeoPositionInfoSource>

#include <bb/location/PositionErrorCode>

#include <QString>
#include <QVariantMap>
#include <QScopedPointer>
#include <QUrl>

class QGeoPositionInfoSourceBbPrivate;
class BB_LOCATION_EXPORT QGeoPositionInfoSourceBb : public QGeoPositionInfoSource
{
    Q_OBJECT

    Q_PROPERTY(double period READ period WRITE setPeriod FINAL)
    Q_PROPERTY(double accuracy READ accuracy WRITE setAccuracy FINAL)
    Q_PROPERTY(double responseTime READ responseTime WRITE setResponseTime FINAL)
    Q_PROPERTY(bool canRunInBackground READ canRunInBackground WRITE setCanRunInBackground FINAL)
    Q_PROPERTY(QString provider READ provider WRITE setProvider FINAL)
    Q_PROPERTY(QString fixType READ fixType WRITE setFixType FINAL)
    Q_PROPERTY(QString appId READ appId WRITE setAppId FINAL)
    Q_PROPERTY(QString appPassword READ appPassword WRITE setAppPassword FINAL)
    Q_PROPERTY(QUrl pdeUrl READ pdeUrl WRITE setPdeUrl FINAL)
    Q_PROPERTY(QUrl slpUrl READ slpUrl WRITE setSlpUrl FINAL)

    Q_PROPERTY(QVariantMap replyDat READ replyDat FINAL)
    Q_PROPERTY(bb::location::PositionErrorCode::Type replyErrorCode READ replyErrorCode FINAL)
    Q_PROPERTY(QString replyErr READ replyErr FINAL)
    Q_PROPERTY(QString replyErrStr READ replyErrStr FINAL)

    Q_PROPERTY(bool locationServicesEnabled READ locationServicesEnabled FINAL)

    Q_PROPERTY(QString reset READ resetType WRITE requestReset FINAL)

public:
    explicit QGeoPositionInfoSourceBb(QObject *parent = 0);
    virtual ~QGeoPositionInfoSourceBb();

    void setUpdateInterval(int msec);
    void setPreferredPositioningMethods(PositioningMethods methods);
    QGeoPositionInfo lastKnownPosition(bool fromSatellitePositioningMethodsOnly = false) const;
    PositioningMethods supportedPositioningMethods() const;
    int minimumUpdateInterval() const;
    Error error() const;

    double period() const;
    void setPeriod(double period);

    double accuracy() const;
    void setAccuracy(double accuracy);

    double responseTime() const;
    void setResponseTime(double responseTime);

    bool canRunInBackground() const;
    void setCanRunInBackground(bool canRunInBackground);

    QString provider() const;
    void setProvider(const QString &provider);

    QString fixType() const;
    void setFixType(const QString &fixType);

    QString appId() const;
    void setAppId(const QString &appId);

    QString appPassword() const;
    void setAppPassword(const QString &appPassword);

    QUrl pdeUrl() const;
    void setPdeUrl(const QUrl &pdeUrl);

    QUrl slpUrl() const;
    void setSlpUrl(const QUrl &slpUrl);

    QVariantMap replyDat() const;

    bb::location::PositionErrorCode::Type replyErrorCode() const;

    QString replyErr() const;

    QString replyErrStr() const;

    bool locationServicesEnabled() const;

    QString resetType() const;
    void requestReset(const QString &resetType);


public Q_SLOTS:
    void startUpdates();
    void stopUpdates();
    void requestUpdate(int timeout = 0);

private:
    Q_DECLARE_PRIVATE(QGeoPositionInfoSourceBb)
    Q_DISABLE_COPY(QGeoPositionInfoSourceBb)
    QScopedPointer<QGeoPositionInfoSourceBbPrivate> d_ptr;
};

#endif
