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

#ifndef QGEOSATELLITEINFOSOURCE_BB_P_H
#define QGEOSATELLITEINFOSOURCE_BB_P_H

#include "qgeosatelliteinfosource_bb.h"
#include "qgeosatelliteinfo.h"

#include <QObject>
#include <QtCore/QVariantMap>

namespace bb
{
class PpsObject;
}

class QGeoSatelliteInfoSourceBbPrivate : public QObject
{
    Q_OBJECT
public:
    ~QGeoSatelliteInfoSourceBbPrivate();

    void startUpdates();
    void stopUpdates();
    void requestUpdate(int msec);

    bool _startUpdatesInvoked;
    bool _requestUpdateInvoked;

private Q_SLOTS:
    void singleUpdateTimeout();
    void receivePeriodicSatelliteReply();
    void receiveSingleSatelliteReply();

    void emitRequestTimeout();

Q_SIGNALS:
    void queuedRequestTimeout();

private:
    Q_DECLARE_PUBLIC(QGeoSatelliteInfoSourceBb)
    explicit QGeoSatelliteInfoSourceBbPrivate(QGeoSatelliteInfoSourceBb *parent);

    void emitSatelliteUpdated(const QGeoSatelliteInfo &update);
    bool requestSatelliteInfo(bool periodic, int singleRequestMsec = 0);
    void cancelSatelliteInfo(bool periodic);

    QVariantMap populateLocationRequest(bool periodic, int singleRequestMsec = 0);
    void populateSatelliteLists(const QVariantMap &reply);

    bool receiveSatelliteReply(bool periodic);

    QGeoSatelliteInfoSourceBb *q_ptr;
    bb::PpsObject *_periodicUpdatePpsObject;
    bb::PpsObject *_singleUpdatePpsObject;
    QList<QGeoSatelliteInfo> _satellitesInUse;
    QList<QGeoSatelliteInfo> _satellitesInView;
    QGeoSatelliteInfoSource::Error _sourceError;

    // properties (extension of QGeoSatelliteInfoSource for additional Location Manager features)
    bool _backgroundMode;
    int _responseTime;

};

#endif
