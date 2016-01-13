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

#ifndef QGEOSATELLITEINFOSOURCE_BB_H
#define QGEOSATELLITEINFOSOURCE_BB_H

#include <QGeoSatelliteInfoSource>

#include <QString>
#include <QScopedPointer>

class QGeoSatelliteInfoSourceBbPrivate;
class QGeoSatelliteInfoSourceBb : public QGeoSatelliteInfoSource
{
    Q_OBJECT

    Q_PROPERTY(double period READ period WRITE setPeriod FINAL)
    Q_PROPERTY(bool backgroundMode READ backgroundMode WRITE setBackgroundMode FINAL)
    Q_PROPERTY(int responseTime READ responseTime WRITE setResponseTime FINAL)

public:
    explicit QGeoSatelliteInfoSourceBb(QObject *parent = 0);
    virtual ~QGeoSatelliteInfoSourceBb();

    void setUpdateInterval(int msec);
    int minimumUpdateInterval() const;
    Error error() const;

    double period() const;
    void setPeriod(double period);

    bool backgroundMode() const;
    void setBackgroundMode(bool mode);

    int responseTime() const;
    void setResponseTime(int responseTime);

public Q_SLOTS:
    void startUpdates();
    void stopUpdates();
    void requestUpdate(int timeout = 0);

private:
    Q_DECLARE_PRIVATE(QGeoSatelliteInfoSourceBb)
    Q_DISABLE_COPY(QGeoSatelliteInfoSourceBb)
    QScopedPointer<QGeoSatelliteInfoSourceBbPrivate> d_ptr;
};

#endif
