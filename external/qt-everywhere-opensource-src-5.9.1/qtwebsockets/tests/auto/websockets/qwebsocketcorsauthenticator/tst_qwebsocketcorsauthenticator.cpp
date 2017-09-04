/****************************************************************************
**
** Copyright (C) 2016 Kurt Pattyn <pattyn.kurt@gmail.com>.
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
#include <QtTest/QtTest>
#include <QtTest/qtestcase.h>
#include <QtCore/QDebug>
#include <QtCore/QByteArray>
#include <QtCore/QtEndian>

#include "QtWebSockets/qwebsocketcorsauthenticator.h"

QT_USE_NAMESPACE

class tst_QWebSocketCorsAuthenticator : public QObject
{
    Q_OBJECT

public:
    tst_QWebSocketCorsAuthenticator();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void tst_initialization();
};

tst_QWebSocketCorsAuthenticator::tst_QWebSocketCorsAuthenticator()
{}

void tst_QWebSocketCorsAuthenticator::initTestCase()
{
}

void tst_QWebSocketCorsAuthenticator::cleanupTestCase()
{}

void tst_QWebSocketCorsAuthenticator::init()
{
}

void tst_QWebSocketCorsAuthenticator::cleanup()
{
}

void tst_QWebSocketCorsAuthenticator::tst_initialization()
{
    {
        QWebSocketCorsAuthenticator authenticator((QString()));

        QCOMPARE(authenticator.allowed(), true);
        QCOMPARE(authenticator.origin(), QString());
    }
    {
        QWebSocketCorsAuthenticator authenticator(QStringLiteral("com.somesite"));

        QCOMPARE(authenticator.allowed(), true);
        QCOMPARE(authenticator.origin(), QStringLiteral("com.somesite"));

        QWebSocketCorsAuthenticator other(authenticator);
        QCOMPARE(other.origin(), authenticator.origin());
        QCOMPARE(other.allowed(), authenticator.allowed());

        authenticator.setAllowed(false);
        QVERIFY(!authenticator.allowed());
        QCOMPARE(other.allowed(), true);   //make sure other is a real copy

        authenticator.setAllowed(true);
        QVERIFY(authenticator.allowed());

        authenticator.setAllowed(false);
        other = authenticator;
        QCOMPARE(other.origin(), authenticator.origin());
        QCOMPARE(other.allowed(), authenticator.allowed());
    }
}

QTEST_MAIN(tst_QWebSocketCorsAuthenticator)

#include "tst_qwebsocketcorsauthenticator.moc"

