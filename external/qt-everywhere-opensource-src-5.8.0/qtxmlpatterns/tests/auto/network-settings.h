/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QString>
#ifdef QT_NETWORK_LIB
#include <QtNetwork/QHostInfo>
#endif

class QtNetworkSettings
{
public:

    static QString serverLocalName()
    {
        return QString("qt-test-server");
    }
    static QString serverDomainName()
    {
        return QString("qt-test-net");
    }
    static QString serverName()
    {
        return serverLocalName() + "." + serverDomainName();
    }
    static QString winServerName()
    {
        return serverName();
    }
    static QString wildcardServerName()
    {
        return "qt-test-server.wildcard.dev." + serverDomainName();
    }

#ifdef QT_NETWORK_LIB
    static QHostAddress serverIP()
    {
        return QHostInfo::fromName(serverName()).addresses().first();
    }
#endif

    static bool compareReplyIMAP(QByteArray const& actual)
    {
        const QByteArray expected[] = {

            // Mandriva; old test server
            QByteArray( "* OK [CAPABILITY IMAP4 IMAP4rev1 LITERAL+ ID STARTTLS LOGINDISABLED] " )
            .append(QtNetworkSettings::serverName().toLatin1())
            .append(" Cyrus IMAP4 v2.3.11-Mandriva-RPM-2.3.11-6mdv2008.1 server ready\r\n"),

            // Ubuntu 10.04; new test server
            QByteArray( "* OK " )
            .append(QtNetworkSettings::serverLocalName().toLatin1())
            .append(" Cyrus IMAP4 v2.2.13-Debian-2.2.13-19 server ready\r\n"),

            // Feel free to add more as needed
        };

        for (const QByteArray &ba : expected) {
            if (ba == actual) {
                return true;
            }
        }

        return false;
    }

    static bool compareReplyIMAPSSL(QByteArray const& actual)
    {
        const QByteArray expected[] = {

            // Mandriva; old test server
            QByteArray( "* OK [CAPABILITY IMAP4 IMAP4rev1 LITERAL+ ID AUTH=PLAIN SASL-IR] " )
            .append(QtNetworkSettings::serverName().toLatin1())
            .append(" Cyrus IMAP4 v2.3.11-Mandriva-RPM-2.3.11-6mdv2008.1 server ready\r\n"),

            // Ubuntu 10.04; new test server
            QByteArray( "* OK " )
            .append(QtNetworkSettings::serverLocalName().toLatin1())
            .append(" Cyrus IMAP4 v2.2.13-Debian-2.2.13-19 server ready\r\n"),

            // Feel free to add more as needed
        };

        for (const QByteArray &ba : expected) {
            if (ba == actual) {
                return true;
            }
        }

        return false;
    }

    static bool compareReplyFtp(QByteArray const& actual)
    {
        const QByteArray expected[] = {

            // A few different vsFTPd versions.
            // Feel free to add more as needed
            QByteArray( "220 (vsFTPd 2.0.5)\r\n221 Goodbye.\r\n" ),
            QByteArray( "220 (vsFTPd 2.2.2)\r\n221 Goodbye.\r\n" ),

        };

        for (const QByteArray &ba : expected) {
            if (ba == actual) {
                return true;
            }
        }

        return false;
    }

#ifdef QT_NETWORK_LIB
    static bool verifyTestNetworkSettings()
    {
        QHostInfo testServerResult = QHostInfo::fromName(QtNetworkSettings::serverName());
        if (testServerResult.error() != QHostInfo::NoError) {
            qWarning() << "Could not lookup" << QtNetworkSettings::serverName();
            qWarning() << "Please configure the test environment!";
            qWarning() << "See /etc/hosts or network-settings.h";
            return false;
        }
        return true;
    }
#endif
};
