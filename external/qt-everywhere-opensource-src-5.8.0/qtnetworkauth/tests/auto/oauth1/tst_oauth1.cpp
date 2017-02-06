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
#include <QtNetwork>

#include <QtNetworkAuth/qoauth1.h>

#include <private/qoauth1_p.h>

Q_DECLARE_METATYPE(QNetworkAccessManager::Operation)
Q_DECLARE_METATYPE(QAbstractOAuth::Error)

// TODO: Test PUT and DELETE operations.
// TODO: Write tests to test errors.
// TODO: Remove common event loop

typedef QSharedPointer<QNetworkReply> QNetworkReplyPtr;

class tst_OAuth1 : public QObject
{
    Q_OBJECT

    using StringPair = QPair<QString, QString>;

    QEventLoop *loop = nullptr;
    enum RunSimpleRequestReturn { Timeout = 0, Success, Failure };
    int returnCode;

    using QObject::connect;
    static bool connect(const QNetworkReplyPtr &ptr,
                        const char *signal,
                        const QObject *receiver,
                        const char *slot,
                        Qt::ConnectionType ct = Qt::AutoConnection)
    {
        return connect(ptr.data(), signal, receiver, slot, ct);
    }
    bool connect(const QNetworkReplyPtr &ptr,
                 const char *signal,
                 const char *slot,
                 Qt::ConnectionType ct = Qt::AutoConnection)
    {
        return connect(ptr.data(), signal, slot, ct);
    }

public:
    int waitForFinish(QNetworkReplyPtr &reply);
    void fillParameters(QVariantMap *parameters, const QUrlQuery &query);

    template<class Type>
    struct PropertyTester
    {
        typedef Type InnerType;
        typedef void(QOAuth1::*ConstSignalType)(const Type &);
        typedef void(QOAuth1::*SignalType)(Type);
        typedef QVector<std::function<void(Type *, QOAuth1 *object)>> SetterFunctions;

    private:
        // Each entry in setters should set its first parameter to an expected value
        // and act on its second, a QOAuth1 object, to trigger signal; this
        // function shall check that signal is passed the value the setter previously
        // told us to expect.
        template<class SignalType>
        static void runImpl(SignalType signal, const SetterFunctions &setters)
        {
            QOAuth1 obj;
            Type expectedValue;
            QSignalSpy spy(&obj, signal);
            connect(&obj, signal, [&](const Type &value) {
                QCOMPARE(expectedValue, value);
            });
            for (const auto &setter : setters) {
                const auto previous = expectedValue;
                setter(&expectedValue, &obj);
                QVERIFY(previous != expectedValue); // To check if the value was modified
            }
            QCOMPARE(spy.count(), setters.size()); // The signal should be emitted
        }

    public:

        static void run(ConstSignalType signal, const SetterFunctions &setters)
        {
            runImpl(signal, setters);
        }

        static void run(SignalType signal, const SetterFunctions &setters)
        {
            runImpl(signal, setters);
        }
    };

public Q_SLOTS:
    void finished();
    void gotError();

private Q_SLOTS:
    void clientIdentifierSignal();
    void clientSharedSecretSignal();
    void tokenSecretSignal();
    void temporaryCredentialsUrlSignal();
    void temporaryTokenCredentialsUrlSignal();
    void tokenCredentialsUrlSignal();
    void signatureMethodSignal();

    void getToken_data();
    void getToken();

    void grant_data();
    void grant();

    void authenticatedCalls_data();
    void authenticatedCalls();
};

int tst_OAuth1::waitForFinish(QNetworkReplyPtr &reply)
{
    int count = 0;

    connect(reply, SIGNAL(finished()), SLOT(finished()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(gotError()));
    returnCode = Success;
    loop = new QEventLoop;
    QSignalSpy spy(reply.data(), SIGNAL(downloadProgress(qint64,qint64)));
    while (!reply->isFinished()) {
        QTimer::singleShot(5000, loop, SLOT(quit()));
        if (loop->exec() == Timeout && count == spy.count() && !reply->isFinished()) {
            returnCode = Timeout;
            break;
        }
        count = spy.count();
    }
    delete loop;
    loop = nullptr;

    return returnCode;
}

void tst_OAuth1::fillParameters(QVariantMap *parameters, const QUrlQuery &query)
{
    const auto list = query.queryItems();
    for (auto it = list.begin(), end = list.end(); it != end; ++it)
        parameters->insert(it->first, it->second);
}

void tst_OAuth1::finished()
{
    if (loop)
        loop->exit(returnCode = Success);
}

void tst_OAuth1::gotError()
{
    if (loop)
        loop->exit(returnCode = Failure);
    disconnect(QObject::sender(), SIGNAL(finished()), this, 0);
}

void tst_OAuth1::clientIdentifierSignal()
{
    using PropertyTester = PropertyTester<QString>;
    PropertyTester::SetterFunctions setters {
        [](QString *expectedValue, QOAuth1 *object) {
            *expectedValue = "setClientIdentifier";
            object->setClientIdentifier(*expectedValue);
        },
        [](QString *expectedValue, QOAuth1 *object) {
            *expectedValue = "setClientCredentials";
            object->setClientCredentials(qMakePair(*expectedValue, QString()));
        }
    };
    PropertyTester::run(&QOAuth1::clientIdentifierChanged, setters);
}

void tst_OAuth1::clientSharedSecretSignal()
{
    using PropertyTester = PropertyTester<QString>;
    PropertyTester::SetterFunctions setters {
        [](QString *expectedValue, QOAuth1 *object) {
            *expectedValue = "setClientSharedSecret";
            object->setClientSharedSecret(*expectedValue);
        },
        [](QString *expectedValue, QOAuth1 *object) {
            *expectedValue = "setClientCredentials";
            object->setClientCredentials(qMakePair(QString(), *expectedValue));
        }
    };
    PropertyTester::run(&QOAuth1::clientSharedSecretChanged, setters);
}

void tst_OAuth1::tokenSecretSignal()
{
    using PropertyTester = PropertyTester<QString>;
    PropertyTester::SetterFunctions setters {
        [](QString *expectedValue, QOAuth1 *object) {
            *expectedValue = "setToken";
            object->setToken(*expectedValue);
        },
        [](QString *expectedValue, QOAuth1 *object) {
            *expectedValue = "setTokenCredentials";
            object->setTokenCredentials(qMakePair(*expectedValue, QString()));
        }
    };
    PropertyTester::run(&QOAuth1::tokenChanged, setters);
}

void tst_OAuth1::temporaryCredentialsUrlSignal()
{
    using PropertyTester = PropertyTester<QUrl>;
    PropertyTester::SetterFunctions setters {
        [](QUrl *expectedValue, QOAuth1 *object) {
            *expectedValue = QUrl("http://example.net/");
            object->setTemporaryCredentialsUrl(*expectedValue);
        }
    };
    PropertyTester::run(&QOAuth1::temporaryCredentialsUrlChanged, setters);
}

void tst_OAuth1::temporaryTokenCredentialsUrlSignal()
{
    using PropertyTester = PropertyTester<QUrl>;
    PropertyTester::SetterFunctions setters {
        [](QUrl *expectedValue, QOAuth1 *object) {
            *expectedValue = QUrl("http://example.net/");
            object->setTemporaryCredentialsUrl(*expectedValue);
        }
    };
    PropertyTester::run(&QOAuth1::temporaryCredentialsUrlChanged, setters);
}

void tst_OAuth1::tokenCredentialsUrlSignal()
{
    using PropertyTester = PropertyTester<QUrl>;
    PropertyTester::SetterFunctions setters {
        [](QUrl *expectedValue, QOAuth1 *object) {
            *expectedValue = QUrl("http://example.net/");
            object->setTokenCredentialsUrl(*expectedValue);
        }
    };
    PropertyTester::run(&QOAuth1::tokenCredentialsUrlChanged, setters);
}

void tst_OAuth1::signatureMethodSignal()
{
    using PropertyTester = PropertyTester<QOAuth1::SignatureMethod>;
    PropertyTester::SetterFunctions setters {
        [](PropertyTester::InnerType *expectedValue, QOAuth1 *object) {
            QVERIFY(object->signatureMethod() != QOAuth1::SignatureMethod::PlainText);
            *expectedValue = QOAuth1::SignatureMethod::PlainText;
            object->setSignatureMethod(*expectedValue);
        }
    };
    PropertyTester::run(&QOAuth1::signatureMethodChanged, setters);
}

void tst_OAuth1::getToken_data()
{
    QTest::addColumn<StringPair>("clientCredentials");
    QTest::addColumn<StringPair>("token");
    QTest::addColumn<StringPair>("expectedToken");
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QNetworkAccessManager::Operation>("requestType");

    // term.ie

    const StringPair emptyCredentials;

    QTest::newRow("term.ie_request_get") << qMakePair(QStringLiteral("key"),
                                                      QStringLiteral("secret"))
                                         << emptyCredentials
                                         << qMakePair(QStringLiteral("requestkey"),
                                                      QStringLiteral("requestsecret"))
                                         << QUrl("http://term.ie/oauth/example/request_token.php")
                                         << QNetworkAccessManager::GetOperation;

    QTest::newRow("term.ie_request_post") << qMakePair(QStringLiteral("key"),
                                                       QStringLiteral("secret"))
                                          << emptyCredentials
                                          << qMakePair(QStringLiteral("requestkey"),
                                                       QStringLiteral("requestsecret"))
                                          << QUrl("http://term.ie/oauth/example/request_token.php")
                                          << QNetworkAccessManager::PostOperation;

    QTest::newRow("term.ie_access_get") << qMakePair(QStringLiteral("key"),
                                                     QStringLiteral("secret"))
                                        << qMakePair(QStringLiteral("requestkey"),
                                                     QStringLiteral("requestsecret"))
                                        << qMakePair(QStringLiteral("accesskey"),
                                                     QStringLiteral("accesssecret"))
                                        << QUrl("http://term.ie/oauth/example/access_token.php")
                                        << QNetworkAccessManager::GetOperation;

    QTest::newRow("term.ie_access_post") << qMakePair(QStringLiteral("key"),
                                                      QStringLiteral("secret"))
                                         << qMakePair(QStringLiteral("requestkey"),
                                                      QStringLiteral("requestsecret"))
                                         << qMakePair(QStringLiteral("accesskey"),
                                                      QStringLiteral("accesssecret"))
                                         << QUrl("http://term.ie/oauth/example/access_token.php")
                                         << QNetworkAccessManager::PostOperation;

    // oauthbin.com

    QTest::newRow("oauthbin.com_request_get") << qMakePair(QStringLiteral("key"),
                                                           QStringLiteral("secret"))
                                              << emptyCredentials
                                              << qMakePair(QStringLiteral("requestkey"),
                                                           QStringLiteral("requestsecret"))
                                              << QUrl("http://oauthbin.com/v1/request-token")
                                              << QNetworkAccessManager::GetOperation;

    QTest::newRow("oauthbin.com_request_post") << qMakePair(QStringLiteral("key"),
                                                            QStringLiteral("secret"))
                                               << emptyCredentials
                                               << qMakePair(QStringLiteral("requestkey"),
                                                            QStringLiteral("requestsecret"))
                                               << QUrl("http://oauthbin.com/v1/request-token")
                                               << QNetworkAccessManager::PostOperation;

    QTest::newRow("oauthbin.com_access_get") << qMakePair(QStringLiteral("key"),
                                                          QStringLiteral("secret"))
                                             << qMakePair(QStringLiteral("requestkey"),
                                                          QStringLiteral("requestsecret"))
                                             << qMakePair(QStringLiteral("accesskey"),
                                                          QStringLiteral("accesssecret"))
                                             << QUrl("http://oauthbin.com/v1/access-token")
                                             << QNetworkAccessManager::GetOperation;

    QTest::newRow("oauthbin.com_access_post") << qMakePair(QStringLiteral("key"),
                                                           QStringLiteral("secret"))
                                              << qMakePair(QStringLiteral("requestkey"),
                                                           QStringLiteral("requestsecret"))
                                              << qMakePair(QStringLiteral("accesskey"),
                                                           QStringLiteral("accesssecret"))
                                              << QUrl("http://oauthbin.com/v1/access-token")
                                              << QNetworkAccessManager::PostOperation;
}

void tst_OAuth1::getToken()
{
    QFETCH(StringPair, clientCredentials);
    QFETCH(StringPair, token);
    QFETCH(StringPair, expectedToken);
    QFETCH(QUrl, url);
    QFETCH(QNetworkAccessManager::Operation, requestType);

    StringPair tokenReceived;
    QNetworkAccessManager networkAccessManager;
    QNetworkReplyPtr reply;

    struct OAuth1 : QOAuth1
    {
        OAuth1(QNetworkAccessManager *manager) : QOAuth1(manager) {}
        using QOAuth1::requestTokenCredentials;
    } o1(&networkAccessManager);

    o1.setClientCredentials(clientCredentials.first, clientCredentials.second);
    o1.setTokenCredentials(token);
    o1.setTemporaryCredentialsUrl(url);
    QVariantMap parameters {{ "c2&a3", "c2=a3" }};
    reply.reset(o1.requestTokenCredentials(requestType, url, token, parameters));
    QVERIFY(!reply.isNull());
    connect(&o1, &QOAuth1::tokenChanged, [&tokenReceived](const QString &token){
        tokenReceived.first = token;
    });
    connect(&o1, &QOAuth1::tokenSecretChanged, [&tokenReceived](const QString &tokenSecret) {
        tokenReceived.second = tokenSecret;
    });
    QVERIFY(waitForFinish(reply) == Success);
    QVERIFY(!tokenReceived.first.isEmpty() && !tokenReceived.second.isEmpty());
}

void tst_OAuth1::grant_data()
{
    QTest::addColumn<QString>("consumerKey");
    QTest::addColumn<QString>("consumerSecret");
    QTest::addColumn<QString>("requestToken");
    QTest::addColumn<QString>("requestTokenSecret");
    QTest::addColumn<QString>("accessToken");
    QTest::addColumn<QString>("accessTokenSecret");
    QTest::addColumn<QUrl>("requestTokenUrl");
    QTest::addColumn<QUrl>("accessTokenUrl");
    QTest::addColumn<QUrl>("authenticatedCallUrl");
    QTest::addColumn<QNetworkAccessManager::Operation>("requestType");

    QTest::newRow("term.ie_get") << "key"
                                 << "secret"
                                 << "requestkey"
                                 << "requestsecret"
                                 << "accesskey"
                                 << "accesssecret"
                                 << QUrl("http://term.ie/oauth/example/request_token.php")
                                 << QUrl("http://term.ie/oauth/example/access_token.php")
                                 << QUrl("http://term.ie/oauth/example/echo_api.php")
                                 << QNetworkAccessManager::GetOperation;
    QTest::newRow("term.ie_post") << "key"
                                  << "secret"
                                  << "requestkey"
                                  << "requestsecret"
                                  << "accesskey"
                                  << "accesssecret"
                                  << QUrl("http://term.ie/oauth/example/request_token.php")
                                  << QUrl("http://term.ie/oauth/example/access_token.php")
                                  << QUrl("http://term.ie/oauth/example/echo_api.php")
                                  << QNetworkAccessManager::PostOperation;
    QTest::newRow("oauthbin.com_get") << "key"
                                      << "secret"
                                      << "requestkey"
                                      << "requestsecret"
                                      << "accesskey"
                                      << "accesssecret"
                                      << QUrl("http://oauthbin.com/v1/request-token")
                                      << QUrl("http://oauthbin.com/v1/access-token")
                                      << QUrl("http://oauthbin.com/v1/echo")
                                      << QNetworkAccessManager::GetOperation;
    QTest::newRow("oauthbin.com_post") << "key"
                                       << "secret"
                                       << "requestkey"
                                       << "requestsecret"
                                       << "accesskey"
                                       << "accesssecret"
                                       << QUrl("http://oauthbin.com/v1/request-token")
                                       << QUrl("http://oauthbin.com/v1/access-token")
                                       << QUrl("http://oauthbin.com/v1/echo")
                                       << QNetworkAccessManager::PostOperation;
}

void tst_OAuth1::grant()
{
    QFETCH(QString, consumerKey);
    QFETCH(QString, consumerSecret);
    QFETCH(QString, requestToken);
    QFETCH(QString, requestTokenSecret);
    QFETCH(QString, accessToken);
    QFETCH(QString, accessTokenSecret);
    QFETCH(QUrl, requestTokenUrl);
    QFETCH(QUrl, accessTokenUrl);

    bool tokenReceived = false;
    QNetworkAccessManager networkAccessManager;

    QOAuth1 o1(&networkAccessManager);

    {
        QSignalSpy clientIdentifierSpy(&o1, &QOAuth1::clientIdentifierChanged);
        QSignalSpy clientSharedSecretSpy(&o1, &QOAuth1::clientSharedSecretChanged);
        o1.setClientCredentials(consumerKey, consumerSecret);
        QCOMPARE(clientIdentifierSpy.count(), 1);
        QCOMPARE(clientSharedSecretSpy.count(), 1);
    }
    {
        QSignalSpy spy(&o1, &QOAuth1::temporaryCredentialsUrlChanged);
        o1.setTemporaryCredentialsUrl(requestTokenUrl);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(&o1, &QOAuth1::tokenCredentialsUrlChanged);
        o1.setTokenCredentialsUrl(accessTokenUrl);
        QCOMPARE(spy.count(), 1);
    }
    connect(&o1, &QAbstractOAuth::statusChanged, [&](QAbstractOAuth::Status status) {
        if (status == QAbstractOAuth::Status::TemporaryCredentialsReceived) {
            if (!requestToken.isEmpty())
                QCOMPARE(requestToken, o1.tokenCredentials().first);
            if (!requestTokenSecret.isEmpty())
                QCOMPARE(requestTokenSecret, o1.tokenCredentials().second);
            tokenReceived = true;
        } else if (status == QAbstractOAuth::Status::Granted) {
            if (!accessToken.isEmpty())
                QCOMPARE(accessToken, o1.tokenCredentials().first);
            if (!accessTokenSecret.isEmpty())
                QCOMPARE(accessTokenSecret, o1.tokenCredentials().second);
            tokenReceived = true;
        }
    });

    QEventLoop eventLoop;

    QTimer::singleShot(10000, &eventLoop, &QEventLoop::quit);
    connect(&o1, &QOAuth1::granted, &eventLoop, &QEventLoop::quit);
    o1.grant();
    eventLoop.exec();
    QVERIFY(tokenReceived);
    QCOMPARE(o1.status(), QAbstractOAuth::Status::Granted);
}

void tst_OAuth1::authenticatedCalls_data()
{
    QTest::addColumn<QString>("consumerKey");
    QTest::addColumn<QString>("consumerSecret");
    QTest::addColumn<QString>("accessKey");
    QTest::addColumn<QString>("accessKeySecret");
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QVariantMap>("parameters");
    QTest::addColumn<QNetworkAccessManager::Operation>("operation");

    const QVariantMap parameters { { QStringLiteral("first"), QStringLiteral("first") },
                                   { QStringLiteral("second"), QStringLiteral("second") },
                                   { QStringLiteral("third"), QStringLiteral("third") },
                                   { QStringLiteral("c2&a3"), QStringLiteral("2=%$&@q") }
                                 };

    QTest::newRow("term.ie_get") << "key"
                                 << "secret"
                                 << "accesskey"
                                 << "accesssecret"
                                 << QUrl("http://term.ie/oauth/example/echo_api.php")
                                 << parameters
                                 << QNetworkAccessManager::GetOperation;
    QTest::newRow("term.ie_post") << "key"
                                  << "secret"
                                  << "accesskey"
                                  << "accesssecret"
                                  << QUrl("http://term.ie/oauth/example/echo_api.php")
                                  << parameters
                                  << QNetworkAccessManager::PostOperation;
    QTest::newRow("oauthbin.com_get") << "key"
                                      << "secret"
                                      << "accesskey"
                                      << "accesssecret"
                                      << QUrl("http://oauthbin.com/v1/echo")
                                      << parameters
                                      << QNetworkAccessManager::GetOperation;
    QTest::newRow("oauthbin.com_post") << "key"
                                       << "secret"
                                       << "accesskey"
                                       << "accesssecret"
                                       << QUrl("http://oauthbin.com/v1/echo")
                                       << parameters
                                       << QNetworkAccessManager::PostOperation;
}

void tst_OAuth1::authenticatedCalls()
{
    QFETCH(QString, consumerKey);
    QFETCH(QString, consumerSecret);
    QFETCH(QString, accessKey);
    QFETCH(QString, accessKeySecret);
    QFETCH(QUrl, url);
    QFETCH(QVariantMap, parameters);
    QFETCH(QNetworkAccessManager::Operation, operation);

    QNetworkAccessManager networkAccessManager;
    QNetworkReplyPtr reply;
    QString receivedData;
    QString parametersString;
    {
        bool first = true;
        for (auto it = parameters.begin(), end = parameters.end(); it != end; ++it) {
            if (first)
                first = false;
            else
                parametersString += QLatin1Char('&');
            parametersString += it.key() + QLatin1Char('=') + it.value().toString();
        }
    }

    QOAuth1 o1(&networkAccessManager);
    o1.setClientCredentials(consumerKey, consumerSecret);
    o1.setTokenCredentials(accessKey, accessKeySecret);
    if (operation == QNetworkAccessManager::GetOperation)
        reply.reset(o1.get(url, parameters));
    else if (operation == QNetworkAccessManager::PostOperation)
        reply.reset(o1.post(url, parameters));
    QVERIFY(!reply.isNull());
    QVERIFY(!reply->isFinished());

    connect(&networkAccessManager, &QNetworkAccessManager::finished,
            [&](QNetworkReply *reply) {
        QByteArray data = reply->readAll();
        QUrlQuery query(QString::fromUtf8(data));
        receivedData = query.toString(QUrl::FullyDecoded);
    });
    QVERIFY(waitForFinish(reply) == Success);
    QCOMPARE(receivedData, parametersString);
    reply.clear();
}

QTEST_MAIN(tst_OAuth1)
#include "tst_oauth1.moc"
