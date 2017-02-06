/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QGEOAREAMONITORPOLLING_H
#define QGEOAREAMONITORPOLLING_H

#include <QtPositioning/qgeoareamonitorsource.h>
#include <QtPositioning/qgeopositioninfosource.h>


/**
 *  QGeoAreaMonitorPolling
 *
 */

class QGeoAreaMonitorPollingPrivate;
class QGeoAreaMonitorPolling : public QGeoAreaMonitorSource
{
    Q_OBJECT
public :
    explicit QGeoAreaMonitorPolling(QObject *parent = 0);
    ~QGeoAreaMonitorPolling();

    void setPositionInfoSource(QGeoPositionInfoSource *source) Q_DECL_OVERRIDE;
    QGeoPositionInfoSource* positionInfoSource() const Q_DECL_OVERRIDE;

    Error error() const Q_DECL_OVERRIDE;

    bool startMonitoring(const QGeoAreaMonitorInfo &monitor) Q_DECL_OVERRIDE;
    bool requestUpdate(const QGeoAreaMonitorInfo &monitor,
                       const char *signal) Q_DECL_OVERRIDE;
    bool stopMonitoring(const QGeoAreaMonitorInfo &monitor) Q_DECL_OVERRIDE;

    QList<QGeoAreaMonitorInfo> activeMonitors() const Q_DECL_OVERRIDE;
    QList<QGeoAreaMonitorInfo> activeMonitors(const QGeoShape &region) const Q_DECL_OVERRIDE;

    QGeoAreaMonitorSource::AreaMonitorFeatures supportedAreaMonitorFeatures() const Q_DECL_OVERRIDE;

    inline bool isValid() { return positionInfoSource(); }

    bool signalsAreConnected;

private Q_SLOTS:
    void positionError(QGeoPositionInfoSource::Error error);
    void timeout(const QGeoAreaMonitorInfo &monitor);
    void processAreaEvent(const QGeoAreaMonitorInfo &minfo, const QGeoPositionInfo &pinfo, bool isEnteredEvent);

private:
    QGeoAreaMonitorPollingPrivate* d;
    QGeoAreaMonitorSource::Error lastError;

    void connectNotify(const QMetaMethod &signal) Q_DECL_OVERRIDE;
    void disconnectNotify(const QMetaMethod &signal) Q_DECL_OVERRIDE;

    int idForSignal(const char *signal);
};

#endif // QGEOAREAMONITORPOLLING_H
