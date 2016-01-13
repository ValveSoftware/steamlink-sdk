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

#ifndef QGEOPOSITIONINFOSOURCEBB_P_H
#define QGEOPOSITIONINFOSOURCEBB_P_H

#include "qgeopositioninfosource_bb.h"
#include "qgeopositioninfo.h"

#include <bb/location/PositionErrorCode>

#include <QObject>
#include <QVariantMap>
#include <QUrl>

namespace bb
{
class PpsObject;
}

class QGeoPositionInfoSourceBbPrivate : public QObject
{
    Q_OBJECT
public:
    ~QGeoPositionInfoSourceBbPrivate();

    void startUpdates();
    void stopUpdates();
    void requestUpdate(int msec);

    bool _startUpdatesInvoked;
    bool _requestUpdateInvoked;

private Q_SLOTS:
    void singleUpdateTimeout();
    void receivePeriodicPositionReply();
    void receiveSinglePositionReply();

    void emitUpdateTimeout();

Q_SIGNALS:
    void queuedUpdateTimeout();

private:
    Q_DECLARE_PUBLIC(QGeoPositionInfoSourceBb)
    explicit QGeoPositionInfoSourceBbPrivate(QGeoPositionInfoSourceBb *parent);

    void periodicUpdatesTimeout();

    void emitPositionUpdated(const QGeoPositionInfo &update);
    bool requestPositionInfo(bool periodic, int singleRequestMsec = 0);
    void cancelPositionInfo(bool periodic);
    void resetLocationProviders();
    QGeoPositionInfo lastKnownPosition(bool fromSatellitePositioningMethodsOnly) const;

    QVariantMap populateLocationRequest(bool periodic, int singleRequestMsec = 0) const;
    QVariantMap populateResetRequest() const;

    bool receivePositionReply(bb::PpsObject &ppsObject);

    bool _canEmitPeriodicUpdatesTimeout;

    QGeoPositionInfoSourceBb *q_ptr;
    bb::PpsObject *_periodicUpdatePpsObject;
    bb::PpsObject *_singleUpdatePpsObject;
    QGeoPositionInfo _currentPosition;
    QGeoPositionInfoSource::Error _sourceError;

    // these are Location Manager parameters that are represented by properties. These parameters
    // represent additional functionality beyond what is provided by the base class
    // QGeoPositionInfoSource
    double _accuracy;
    double _responseTime;
    bool _canRunInBackground;
    QString _fixType;
    QString _appId;
    QString _appPassword;
    QUrl _pdeUrl;
    QUrl _slpUrl;
    QVariantMap _replyDat;
    bb::location::PositionErrorCode::Type _replyErrorCode;
    QString _replyErr;
    QString _replyErrStr;
    QString _resetType;

};

#endif
