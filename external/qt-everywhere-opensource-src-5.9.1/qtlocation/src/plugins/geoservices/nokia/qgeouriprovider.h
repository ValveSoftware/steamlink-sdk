/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
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

    const QString m_internationalHost;
    const QString m_localizedHost;
    QString m_currentHost;
    QChar m_firstSubdomain;
    unsigned char m_maxSubdomains;
};

QT_END_NAMESPACE

#endif // QGEO_MOBILE_COUNTRY_TRACKER_H
