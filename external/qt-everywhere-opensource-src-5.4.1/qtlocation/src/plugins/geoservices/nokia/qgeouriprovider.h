/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
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
** This file is part of the Ovi services plugin for the Maps and
** Navigation API.  The use of these services, whether by use of the
** plugin or by other means, is governed by the terms and conditions
** described by the file OVI_SERVICES_TERMS_AND_CONDITIONS.txt in
** this package, located in the directory containing the Ovi services
** plugin source code.
**
****************************************************************************/
#ifndef QGEO_MOBILE_COUNTRY_TRACKER_H
#define QGEO_MOBILE_COUNTRY_TRACKER_H

#include <QObject>
#include <QPointer>

QT_BEGIN_NAMESPACE

class QNetworkInfo;

class QGeoUriProvider : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(QGeoUriProvider)

public:
    QGeoUriProvider(QObject *parent,
                    const QVariantMap &parameters,
                    const QString &hostParameterName,
                    const QString &internationalHost,
                    const QString &localizedHost = QString());

    QString getCurrentHost() const;

private Q_SLOTS:
    void mobileCountryCodeChanged(int interface, const QString& mcc);

private:
    bool isInternationalNetwork() const;
    void setCurrentHost(const QString &host);

#ifdef USE_CHINA_NETWORK_REGISTRATION
    QNetworkInfo *m_networkInfo;
#endif
    const QString m_internationalHost;
    const QString m_localizedHost;
    QString m_currentHost;
    QChar m_firstSubdomain;
    unsigned char m_maxSubdomains;
};

QT_END_NAMESPACE

#endif // QGEO_MOBILE_COUNTRY_TRACKER_H
