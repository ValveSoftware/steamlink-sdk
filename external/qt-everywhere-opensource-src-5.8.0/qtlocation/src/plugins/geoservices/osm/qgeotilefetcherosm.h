/****************************************************************************
**
** Copyright (C) 2016 Aaron McCarthy <mccarthy.aaron@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#ifndef QGEOTILEFETCHEROSM_H
#define QGEOTILEFETCHEROSM_H

#include "qgeotileproviderosm.h"
#include <QtLocation/private/qgeotilefetcher_p.h>
#include <QVector>

QT_BEGIN_NAMESPACE

class QNetworkAccessManager;
class QGeoTileFetcherOsmPrivate;

class QGeoTileFetcherOsm : public QGeoTileFetcher
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QGeoTileFetcherOsm)

    friend class QGeoMapReplyOsm;
    friend class QGeoTiledMappingManagerEngineOsm;
public:
    QGeoTileFetcherOsm(const QVector<QGeoTileProviderOsm *> &providers,
                       QNetworkAccessManager *nm,
                       QObject *parent = 0);

    void setUserAgent(const QByteArray &userAgent);

Q_SIGNALS:
    void providerDataUpdated(const QGeoTileProviderOsm *provider);

protected:
    bool initialized() const Q_DECL_OVERRIDE;

protected Q_SLOTS:
    void onProviderResolutionFinished(const QGeoTileProviderOsm *provider);
    void onProviderResolutionError(const QGeoTileProviderOsm *provider);
    void restartTimer();

private:
    QGeoTiledMapReply *getTileImage(const QGeoTileSpec &spec) Q_DECL_OVERRIDE;
    void readyUpdated();

    QByteArray m_userAgent;
    QVector<QGeoTileProviderOsm *> m_providers;
    QNetworkAccessManager *m_nm;
    bool m_ready;
};

QT_END_NAMESPACE

#endif // QGEOTILEFETCHEROSM_H

