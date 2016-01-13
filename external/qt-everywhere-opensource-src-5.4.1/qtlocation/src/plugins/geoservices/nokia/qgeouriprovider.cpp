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
#include "qgeouriprovider.h"

#ifdef USE_CHINA_NETWORK_REGISTRATION
#include <QNetworkInfo>
#endif

#include <QMap>
#include <QVariant>
#include <QSet>
#include <QString>

QT_BEGIN_NAMESPACE

namespace
{
    const QString CHINA_MCC = QLatin1String("460");     // China mobile country code
    const QString CHINA2_MCC = QLatin1String("461");    // China mobile country code
    const QString HONG_KONG_MCC = QLatin1String("454"); // Hong Kong mobile country code
    const QString MACAU_MCC = QLatin1String("455");     // Macau mobile country code
}

QGeoUriProvider::QGeoUriProvider(
                QObject *parent,
                const QVariantMap &parameters,
                const QString &hostParameterName,
                const QString &internationalHost,
                const QString &localizedHost)
    : QObject(parent)
#ifdef USE_CHINA_NETWORK_REGISTRATION
    , m_networkInfo(new QNetworkInfo(this))
#endif
    , m_internationalHost(parameters.value(hostParameterName, internationalHost).toString())
    , m_localizedHost(localizedHost)
    , m_firstSubdomain(QChar::Null)
    , m_maxSubdomains(0)
{
#ifdef USE_CHINA_NETWORK_REGISTRATION
    QObject::connect(m_networkInfo, SIGNAL(currentMobileCountryCodeChanged(int,QString)), this, SLOT(mobileCountryCodeChanged(int,QString)));
#endif
    setCurrentHost(isInternationalNetwork() || m_localizedHost.isEmpty() ? m_internationalHost : m_localizedHost);
}

QString QGeoUriProvider::getCurrentHost() const
{
    if (m_maxSubdomains) {
        QString result(m_firstSubdomain.toLatin1() + qrand() % m_maxSubdomains);
        result += "." + m_currentHost;
        return result;
    }
    return m_currentHost;
}

void QGeoUriProvider::setCurrentHost(const QString &host)
{
    if (host.length() > 4 && host.at(1) == QChar('-') && host.at(3) == QChar('.')) {
        QString realHost = host.right(host.length() - 4);
        m_firstSubdomain = host.at(0);
        m_maxSubdomains = host.at(2).toLatin1() - host.at(0).toLatin1() + 1;
        m_currentHost = realHost;
    } else {
        m_currentHost = host;
        m_firstSubdomain = QChar::Null;
        m_maxSubdomains = 0;
    }
}

void QGeoUriProvider::mobileCountryCodeChanged(int interface, const QString& mcc)
{
    Q_UNUSED(interface)
    Q_UNUSED(mcc)

    setCurrentHost(isInternationalNetwork() || m_localizedHost.isEmpty() ? m_internationalHost : m_localizedHost);
}

bool QGeoUriProvider::isInternationalNetwork() const
{
#ifndef USE_CHINA_NETWORK_REGISTRATION
    return true;
#else
    static QSet<QString> codes;
    if (codes.empty()) {
        codes.insert(CHINA_MCC);
        codes.insert(CHINA2_MCC);
    }

    QNetworkInfo::NetworkMode mode = m_networkInfo->currentNetworkMode();

    int interfaces = m_networkInfo->networkInterfaceCount(mode);
    for (int i = 0; i < interfaces; ++i) {
        QString mcc = m_networkInfo->currentMobileCountryCode(interfaces);
        if (codes.contains(mcc))
            return false;
    }

    return true;
#endif // USE_CHINA_NETWORK_REGISTRATION
}

QT_END_NAMESPACE
