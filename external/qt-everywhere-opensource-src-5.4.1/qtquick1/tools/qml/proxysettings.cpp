/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the tools applications of the Qt Toolkit.
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


#include <QIntValidator>
#include <QSettings>

#include "proxysettings.h"

QT_BEGIN_NAMESPACE

ProxySettings::ProxySettings (QWidget * parent)
    : QDialog (parent), Ui::ProxySettings()
{
    setupUi (this);

    QIntValidator *validator = new QIntValidator (0, 9999, this);
    proxyPortEdit->setValidator(validator);

    QSettings settings;
    proxyCheckBox->setChecked(settings.value(QLatin1String("http_proxy/use"), 0).toBool());
    proxyServerEdit->insert(settings.value(QLatin1String("http_proxy/hostname")).toString());
    proxyPortEdit->insert(settings.value(QLatin1String("http_proxy/port"), QLatin1String("80")).toString ());
    usernameEdit->insert(settings.value(QLatin1String("http_proxy/username")).toString ());
    passwordEdit->insert(settings.value(QLatin1String("http_proxy/password")).toString ());
}

ProxySettings::~ProxySettings()
{
}

void ProxySettings::accept ()
{
    QSettings settings;

    settings.setValue(QLatin1String("http_proxy/use"), proxyCheckBox->isChecked());
    settings.setValue(QLatin1String("http_proxy/hostname"), proxyServerEdit->text());
    settings.setValue(QLatin1String("http_proxy/port"), proxyPortEdit->text());
    settings.setValue(QLatin1String("http_proxy/username"), usernameEdit->text());
    settings.setValue(QLatin1String("http_proxy/password"), passwordEdit->text());

    QDialog::accept ();
}

QNetworkProxy ProxySettings::httpProxy ()
{
    QSettings settings;
    QNetworkProxy proxy;

    bool proxyInUse = settings.value(QLatin1String("http_proxy/use"), 0).toBool();
    if (proxyInUse) {
        proxy.setType (QNetworkProxy::HttpProxy);
        proxy.setHostName (settings.value(QLatin1String("http_proxy/hostname")).toString());// "192.168.220.5"
        proxy.setPort (settings.value(QLatin1String("http_proxy/port"), 80).toInt());  // 8080
        proxy.setUser (settings.value(QLatin1String("http_proxy/username")).toString());
        proxy.setPassword (settings.value(QLatin1String("http_proxy/password")).toString());
        //QNetworkProxy::setApplicationProxy (proxy);
    }
    else {
        proxy.setType (QNetworkProxy::NoProxy);
    }
    return proxy;
}

bool ProxySettings::httpProxyInUse()
{
    QSettings settings;
    return settings.value(QLatin1String("http_proxy/use"), 0).toBool();
}

QT_END_NAMESPACE
