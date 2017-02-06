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

#include <QtCore>
#include <QtTest>

#include <QtNetworkAuth/qoauth1signature.h>

class tst_OAuth1Signature : public QObject
{
    Q_OBJECT

public:
    QOAuth1Signature createTwitterSignature();

private Q_SLOTS:
    void signature();
    void copyAndModify();
};

QOAuth1Signature tst_OAuth1Signature::createTwitterSignature()
{
    // Example from https://dev.twitter.com/oauth/overview/creating-signatures

    const QUrl url("https://api.twitter.com/1/statuses/update.json?include_entities=true");
    QOAuth1Signature signature(url,
                               QStringLiteral("kAcSOqF21Fu85e7zjz7ZN2U4ZRhfV3WpwPAoE3Z7kBw"),
                               QStringLiteral("LswwdoUaIvS8ltyTt5jkRh4J50vUPVVHtR2YPi5kE"),
                               QOAuth1Signature::HttpRequestMethod::Post);
    const QString body = QUrl::fromPercentEncoding("status=Hello%20Ladies%20%2b%20Gentlemen%2c%20a"
                                                   "%20signed%20OAuth%20request%21");

    signature.insert(QStringLiteral("oauth_consumer_key"),
                     QStringLiteral("xvz1evFS4wEEPTGEFPHBog"));
    signature.insert(QStringLiteral("oauth_nonce"),
                     QStringLiteral("kYjzVBB8Y0ZFabxSWbWovY3uYSQ2pTgmZeNu2VS4cg"));
    signature.insert(QStringLiteral("oauth_signature_method"),
                     QStringLiteral("HMAC-SHA1"));
    signature.insert(QStringLiteral("oauth_timestamp"), QStringLiteral("1318622958"));
    signature.insert(QStringLiteral("oauth_token"),
                     QStringLiteral("370773112-GmHxMAgYyLbNEtIKZeRNFsMKPR9EyMZeS9weJAEb"));
    signature.insert(QStringLiteral("oauth_version"), QStringLiteral("1.0"));
    signature.addRequestBody(QUrlQuery(body));
    return signature;
}

void tst_OAuth1Signature::signature()
{
    const QOAuth1Signature signature = createTwitterSignature();
    QByteArray signatureData = signature.hmacSha1();
    QCOMPARE(signatureData.toBase64(), QByteArray("tnnArxj06cWHq44gCs1OSKk/jLY="));
}

void tst_OAuth1Signature::copyAndModify()
{
    const QOAuth1Signature signature = createTwitterSignature();
    QOAuth1Signature copy = signature;
    QCOMPARE(signature.hmacSha1(), copy.hmacSha1());
    copy.insert(QStringLiteral("signature"), QStringLiteral("modified"));
    QVERIFY(signature.hmacSha1() != copy.hmacSha1());
}

QTEST_MAIN(tst_OAuth1Signature)
#include "tst_oauth1signature.moc"
