/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd, author: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QGEOSATELLITEINFOSOURCE_GEOCLUEMASTER_H
#define QGEOSATELLITEINFOSOURCE_GEOCLUEMASTER_H

#include "qgeocluemaster.h"

#include <QtCore/QTimer>
#include <QtPositioning/QGeoSatelliteInfoSource>

class OrgFreedesktopGeoclueInterface;
class OrgFreedesktopGeoclueSatelliteInterface;

QT_BEGIN_NAMESPACE

class QDBusMessage;
class QDBusPendingCallWatcher;

class QGeoSatelliteInfoSourceGeoclueMaster : public QGeoSatelliteInfoSource
{
    Q_OBJECT

public:
    explicit QGeoSatelliteInfoSourceGeoclueMaster(QObject *parent = 0);
    ~QGeoSatelliteInfoSourceGeoclueMaster();

    int minimumUpdateInterval() const Q_DECL_OVERRIDE;
    void setUpdateInterval(int msec) Q_DECL_OVERRIDE;

    Error error() const Q_DECL_OVERRIDE;

    void startUpdates() Q_DECL_OVERRIDE;
    void stopUpdates() Q_DECL_OVERRIDE;
    void requestUpdate(int timeout = 0) Q_DECL_OVERRIDE;

private slots:
    void positionProviderChanged(const QString &name, const QString &description,
                                 const QString &service, const QString &path);
    void requestUpdateTimeout();

    void getSatelliteFinished(QDBusPendingCallWatcher *watcher);
    void satelliteChanged(int timestamp, int satellitesUsed, int satellitesVisible,
                          const QList<int> &usedPrn, const QList<QGeoSatelliteInfo> &satInfos);
    void satelliteChanged(const QDBusMessage &message);

private:
    void configureSatelliteSource();
    void cleanupSatelliteSource();

    void updateSatelliteInfo(int timestamp, int satellitesUsed, int satellitesVisible,
                              const QList<int> &usedPrn, const QList<QGeoSatelliteInfo> &satInfos);

    QGeoclueMaster *m_master;

    OrgFreedesktopGeoclueInterface *m_provider;
    OrgFreedesktopGeoclueSatelliteInterface *m_sat;

    QTimer m_requestTimer;
    QList<QGeoSatelliteInfo> m_inView;
    QList<QGeoSatelliteInfo> m_inUse;
    Error m_error;
    bool m_satellitesChangedConnected;
    bool m_running;
};

QT_END_NAMESPACE

#endif // QGEOSATELLITEINFOSOURCE_GEOCLUEMASTER_H
