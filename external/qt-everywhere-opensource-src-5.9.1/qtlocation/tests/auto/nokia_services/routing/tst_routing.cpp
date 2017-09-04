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

#include <qgeonetworkaccessmanager.h>
#include <qgeoroutereply_nokia.h>

#include <QtTest/QtTest>
#include <QDebug>
#include <QNetworkReply>
#include <QtLocation/QGeoRouteReply>
#include <QtLocation/QGeoServiceProvider>
#include <QtLocation/QGeoRoutingManager>
#include <QtPositioning/QGeoRectangle>
#include <QtLocation/QGeoManeuver>
#include <QtLocation/QGeoRouteSegment>

QT_USE_NAMESPACE

#define CHECK_CLOSE_E(expected, actual, e) QVERIFY((qAbs(actual - expected) <= e))
#define CHECK_CLOSE(expected, actual) CHECK_CLOSE_E(expected, actual, qreal(1e-6))

class MockGeoNetworkReply : public QNetworkReply
{
public:
    MockGeoNetworkReply( QObject* parent = 0);
    virtual void abort();

    void setFile(QFile* file);
    void complete();
    using QNetworkReply::setRequest;
    using QNetworkReply::setOperation;
    using QNetworkReply::setError;

protected:
    virtual qint64 readData(char *data, qint64 maxlen);
    virtual qint64 writeData(const char *data, qint64 len);

private:
    QFile* m_file;
};

MockGeoNetworkReply::MockGeoNetworkReply(QObject* parent)
: QNetworkReply(parent)
, m_file(0)
{
    setOpenMode(QIODevice::ReadOnly);
}

void MockGeoNetworkReply::abort()
{}

qint64 MockGeoNetworkReply::readData(char *data, qint64 maxlen)
{
    if (m_file) {
        const qint64 read = m_file->read(data, maxlen);
        if (read <= 0)
            return -1;
        return read;
    }
    return -1;
}

qint64 MockGeoNetworkReply::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);
    return -1;
}

void MockGeoNetworkReply::setFile(QFile* file)
{
    delete m_file;
    m_file = file;
    if (m_file)
        m_file->setParent(this);
}

void MockGeoNetworkReply::complete()
{
    if (error() != QNetworkReply::NoError)
        emit error(error());
    setFinished(true);
    emit finished();
}

class MockGeoNetworkAccessManager : public QGeoNetworkAccessManager
{
public:
    MockGeoNetworkAccessManager(QObject* parent = 0);
    QNetworkReply* get(const QNetworkRequest& request);
    QNetworkReply *post(const QNetworkRequest &request, const QByteArray &data);

    void setReply(MockGeoNetworkReply* reply);

private:
    MockGeoNetworkReply* m_reply;
};

MockGeoNetworkAccessManager::MockGeoNetworkAccessManager(QObject* parent)
: QGeoNetworkAccessManager(parent)
, m_reply(0)
{}

QNetworkReply* MockGeoNetworkAccessManager::get(const QNetworkRequest& request)
{
    MockGeoNetworkReply* r = m_reply;
    m_reply = 0;
    if (r) {
        r->setRequest(request);
        r->setOperation(QNetworkAccessManager::GetOperation);
        r->setParent(0);
    }

    return r;
}

QNetworkReply* MockGeoNetworkAccessManager::post(const QNetworkRequest &request, const QByteArray &data)
{
    Q_UNUSED(request);
    Q_UNUSED(data);
    QTest::qFail("Not implemented", __FILE__, __LINE__);
    return new MockGeoNetworkReply();
}

void MockGeoNetworkAccessManager::setReply(MockGeoNetworkReply* reply)
{
    delete m_reply;
    m_reply = reply;
    if (m_reply)
        m_reply->setParent(this);
}

class tst_nokia_routing : public QObject
{
    Q_OBJECT

public:
    tst_nokia_routing();

private:
    void calculateRoute();
    void loadReply(const QString& filename);
    void onReply(QGeoRouteReply* reply);
    void verifySaneRoute(const QGeoRoute& route);

    // Infrastructure slots
private Q_SLOTS:
    void routingFinished(QGeoRouteReply* reply);
    void routingError(QGeoRouteReply* reply, QGeoRouteReply::Error error, QString errorString);

    // Test slots
private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    void can_compute_route_for_all_supported_travel_modes();
    void can_compute_route_for_all_supported_travel_modes_data();
    void can_compute_route_for_all_supported_optimizations();
    void can_compute_route_for_all_supported_optimizations_data();
    void can_handle_multiple_routes_in_response();
    void can_handle_no_route_exists_case();
    void can_handle_invalid_server_responses();
    void can_handle_invalid_server_responses_data();
    void can_handle_additions_to_routing_xml();
    void foobar();
    void foobar_data();

private:
    QGeoServiceProvider* m_geoServiceProvider;
    MockGeoNetworkAccessManager* m_networkManager;
    QGeoRoutingManager* m_routingManager;
    QGeoRouteReply* m_reply;
    MockGeoNetworkReply* m_replyUnowned;
    QGeoRouteRequest m_dummyRequest;
    bool m_calculationDone;
    bool m_expectError;
};

tst_nokia_routing::tst_nokia_routing()
: m_geoServiceProvider(0)
, m_networkManager(0)
, m_routingManager(0)
, m_reply(0)
, m_replyUnowned()
, m_calculationDone(true)
, m_expectError(false)
{
}

void tst_nokia_routing::loadReply(const QString& filename)
{
    QFile* file = new QFile(QFINDTESTDATA(filename));
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        file = 0;
        qDebug() << filename;
        QTest::qFail("Failed to open file", __FILE__, __LINE__);
    }

    m_replyUnowned = new MockGeoNetworkReply();
    m_replyUnowned->setFile(file);
    m_networkManager->setReply(m_replyUnowned);
}

void tst_nokia_routing::calculateRoute()
{
    QVERIFY2(m_replyUnowned, "No reply set");
    m_calculationDone = false;
    m_routingManager->calculateRoute(m_dummyRequest);
    m_replyUnowned->complete();
    m_replyUnowned = 0;
    QTRY_VERIFY_WITH_TIMEOUT(m_calculationDone, 100);
}

void tst_nokia_routing::onReply(QGeoRouteReply* reply)
{
    QVERIFY(reply);
    //QVERIFY(0 == m_reply);
    m_reply = reply;
    if (m_reply)
        m_reply->setParent(0);
    m_calculationDone = true;
}

void tst_nokia_routing::verifySaneRoute(const QGeoRoute& route)
{
    QVERIFY(route.distance() > 0);
    QVERIFY(route.travelTime() > 0);
    QVERIFY(route.travelMode() != 0);

    const QGeoRectangle bounds = route.bounds();
    QVERIFY(bounds.width() > 0);
    QVERIFY(bounds.height() > 0);

    const QList<QGeoCoordinate> path = route.path();
    QVERIFY(path.size() >= 2);

    foreach (const QGeoCoordinate& coord, path) {
        QVERIFY(coord.isValid());
        QVERIFY(bounds.contains(coord));
    }

    QGeoRouteSegment segment = route.firstRouteSegment();
    bool first = true, last = false;

    do {
        const QGeoRouteSegment next = segment.nextRouteSegment();
        last = next.isValid();

        QVERIFY(segment.isValid());
        QVERIFY(segment.distance() >= 0);
        QVERIFY(segment.travelTime() >= 0); // times are rounded and thus may end up being zero

        const QList<QGeoCoordinate> path = segment.path();
        foreach (const QGeoCoordinate& coord, path) {
            QVERIFY(coord.isValid());
            if (!first && !last) {
                QVERIFY(bounds.contains(coord)); // on pt and pedestrian
            }
        }

        const QGeoManeuver maneuver = segment.maneuver();

        if (maneuver.isValid()) {
            QVERIFY(!maneuver.instructionText().isEmpty());
            QVERIFY(maneuver.position().isValid());
            if (!first && !last) {
                QVERIFY(bounds.contains(maneuver.position())); // on pt and pedestrian
            }
        }

        segment = next;
        first = false;
    } while (!last);
}

void tst_nokia_routing::routingFinished(QGeoRouteReply* reply)
{
    onReply(reply);
}

void tst_nokia_routing::routingError(QGeoRouteReply* reply, QGeoRouteReply::Error error, QString errorString)
{
    Q_UNUSED(error);

    if (!m_expectError) {
        QFAIL(qPrintable(errorString));
    } else {
        onReply(reply);
    }
}

void tst_nokia_routing::initTestCase()
{
    QStringList providers = QGeoServiceProvider::availableServiceProviders();
    QVERIFY(providers.contains(QStringLiteral("here")));

    m_networkManager = new MockGeoNetworkAccessManager();

    QVariantMap parameters;
    parameters.insert(QStringLiteral("nam"), QVariant::fromValue<void*>(m_networkManager));
    parameters.insert(QStringLiteral("here.app_id"), "stub");
    parameters.insert(QStringLiteral("here.token"), "stub");

    m_geoServiceProvider = new QGeoServiceProvider(QStringLiteral("here"), parameters);
    QVERIFY(m_geoServiceProvider);

    m_routingManager = m_geoServiceProvider->routingManager();
    QVERIFY(m_routingManager);

    connect(m_routingManager, SIGNAL(finished(QGeoRouteReply*)),
            this, SLOT(routingFinished(QGeoRouteReply*)));
    connect(m_routingManager, SIGNAL(error(QGeoRouteReply*,QGeoRouteReply::Error,QString)),
            this, SLOT(routingError(QGeoRouteReply*,QGeoRouteReply::Error,QString)));

    QList<QGeoCoordinate> waypoints;
    waypoints.push_back(QGeoCoordinate(1, 1));
    waypoints.push_back(QGeoCoordinate(2, 2));
    m_dummyRequest.setWaypoints(waypoints);
}

void tst_nokia_routing::cleanupTestCase()
{
    delete m_geoServiceProvider;

    // network access manager will be deleted by plugin

    m_geoServiceProvider = 0;
    m_networkManager = 0;
    m_routingManager = 0;
}

void tst_nokia_routing::cleanup()
{
    delete m_reply;
    m_reply = 0;
    m_replyUnowned = 0;
    m_expectError = false;
}

void tst_nokia_routing::can_compute_route_for_all_supported_travel_modes()
{
    QFETCH(int, travelMode);
    QFETCH(QString, file);
    QFETCH(qreal, distance);
    QFETCH(int, duration);

    loadReply(file);
    calculateRoute();

    QList<QGeoRoute> routes = m_reply->routes();
    QCOMPARE(1, routes.size());
    QGeoRoute& route = routes[0];
    QCOMPARE(travelMode, (int)route.travelMode());
    CHECK_CLOSE(distance, route.distance());
    QCOMPARE(duration, route.travelTime());
    verifySaneRoute(route);
}

void tst_nokia_routing::can_compute_route_for_all_supported_travel_modes_data()
{
    QTest::addColumn<int>("travelMode");
    QTest::addColumn<QString>("file");
    QTest::addColumn<qreal>("distance");
    QTest::addColumn<int>("duration");

    QTest::newRow("Car") << (int)QGeoRouteRequest::CarTravel << QString("travelmode-car.xml") << (qreal)1271.0 << 243;
    QTest::newRow("Pedestrian") << (int)QGeoRouteRequest::PedestrianTravel << QString("travelmode-pedestrian.xml") << (qreal)1107.0 << 798;
    QTest::newRow("Public Transport") << (int)QGeoRouteRequest::PublicTransitTravel << QString("travelmode-public-transport.xml") << (qreal)1388.0 << 641;
}

void tst_nokia_routing::can_compute_route_for_all_supported_optimizations()
{
    QFETCH(int, optimization);
    QFETCH(QString, file);
    QFETCH(qreal, distance);
    QFETCH(int, duration);
    m_dummyRequest.setRouteOptimization((QGeoRouteRequest::RouteOptimization)optimization);
    loadReply(file);
    calculateRoute();
    QList<QGeoRoute> routes = m_reply->routes();
    QCOMPARE(1, routes.size());
    QGeoRoute& route = routes[0];
    CHECK_CLOSE(distance, route.distance());
    QCOMPARE(duration, route.travelTime());
    verifySaneRoute(route);
}

void tst_nokia_routing::can_compute_route_for_all_supported_optimizations_data()
{
    QTest::addColumn<int>("optimization");
    QTest::addColumn<QString>("file");
    QTest::addColumn<qreal>("distance");
    QTest::addColumn<int>("duration");

    QTest::newRow("Shortest") << (int)QGeoRouteRequest::ShortestRoute << QString("optim-shortest.xml") << qreal(1177.0) << 309;
    QTest::newRow("Fastest") << (int)QGeoRouteRequest::FastestRoute << QString("optim-fastest.xml") << qreal(1271.0) << 243;
}

void tst_nokia_routing::can_handle_multiple_routes_in_response()
{
    loadReply(QStringLiteral("multiple-routes-in-response.xml"));
    calculateRoute();
    QList<QGeoRoute> routes = m_reply->routes();
    QCOMPARE(2, routes.size());

    verifySaneRoute(routes[0]);
    verifySaneRoute(routes[1]);
}

void tst_nokia_routing::can_handle_no_route_exists_case()
{
    loadReply(QStringLiteral("error-no-route.xml"));
    calculateRoute();
    QCOMPARE(QGeoRouteReply::NoError, m_reply->error());
    QList<QGeoRoute> routes = m_reply->routes();
    QCOMPARE(0, routes.size());
}

void tst_nokia_routing::can_handle_additions_to_routing_xml()
{
    loadReply(QStringLiteral("littered-with-new-tags.xml"));
    calculateRoute();
    QCOMPARE(QGeoRouteReply::NoError, m_reply->error());
    QList<QGeoRoute> routes = m_reply->routes();
    QVERIFY(routes.size() > 0);
}

void tst_nokia_routing::can_handle_invalid_server_responses()
{
    QFETCH(QString, file);

    m_expectError = true;

    loadReply(file);
    calculateRoute();
    QCOMPARE(QGeoRouteReply::ParseError, m_reply->error());
}

void tst_nokia_routing::can_handle_invalid_server_responses_data()
{
    QTest::addColumn<QString>("file");

    QTest::newRow("Trash") << QString("invalid-response-trash.xml");
    QTest::newRow("Half way through") << QString("invalid-response-half-way-through.xml");
    QTest::newRow("No <CalculateRoute> tag") << QString("invalid-response-no-calculateroute-tag.xml");
}

void tst_nokia_routing::foobar()
{
    QFETCH(int, code);

    m_expectError = true;
    m_replyUnowned = new MockGeoNetworkReply();
    m_replyUnowned->setError(static_cast<QNetworkReply::NetworkError>(code), QStringLiteral("Test error"));
    m_networkManager->setReply(m_replyUnowned);
    calculateRoute();
    QCOMPARE(QGeoRouteReply::CommunicationError, m_reply->error());
}

void tst_nokia_routing::foobar_data()
{
    QTest::addColumn<int>("code");

    QTest::newRow("QNetworkReply::ConnectionRefusedError") << int(QNetworkReply::ConnectionRefusedError);
    QTest::newRow("QNetworkReply::RemoteHostClosedError") << int(QNetworkReply::RemoteHostClosedError);
    QTest::newRow("QNetworkReply::HostNotFoundError") << int(QNetworkReply::HostNotFoundError);
    QTest::newRow("QNetworkReply::TimeoutError") << int(QNetworkReply::TimeoutError);
    QTest::newRow("QNetworkReply::OperationCanceledError") << int(QNetworkReply::OperationCanceledError);
    QTest::newRow("QNetworkReply::SslHandshakeFailedError") << int(QNetworkReply::SslHandshakeFailedError);
    QTest::newRow("QNetworkReply::TemporaryNetworkFailureError") << int(QNetworkReply::TemporaryNetworkFailureError);
    QTest::newRow("QNetworkReply::ProxyConnectionRefusedError") << int(QNetworkReply::ProxyConnectionRefusedError);
    QTest::newRow("QNetworkReply::ProxyConnectionClosedError") << int(QNetworkReply::ProxyConnectionClosedError);
    QTest::newRow("QNetworkReply::ProxyNotFoundError") << int(QNetworkReply::ProxyNotFoundError);
    QTest::newRow("QNetworkReply::ProxyTimeoutError") << int(QNetworkReply::ProxyTimeoutError);
    QTest::newRow("QNetworkReply::ProxyAuthenticationRequiredError") << int(QNetworkReply::ProxyAuthenticationRequiredError);
    QTest::newRow("QNetworkReply::ContentAccessDenied") << int(QNetworkReply::ContentAccessDenied);
    QTest::newRow("QNetworkReply::ContentOperationNotPermittedError") << int(QNetworkReply::ContentOperationNotPermittedError);
    QTest::newRow("QNetworkReply::ContentNotFoundError") << int(QNetworkReply::ContentNotFoundError);
    QTest::newRow("QNetworkReply::ContentReSendError") << int(QNetworkReply::ContentReSendError);
    QTest::newRow("QNetworkReply::ProtocolUnknownError") << int(QNetworkReply::ProtocolUnknownError);
    QTest::newRow("QNetworkReply::ProtocolInvalidOperationError") << int(QNetworkReply::ProtocolInvalidOperationError);
    QTest::newRow("QNetworkReply::UnknownNetworkError") << int(QNetworkReply::UnknownNetworkError);
    QTest::newRow("QNetworkReply::UnknownProxyError") << int(QNetworkReply::UnknownProxyError);
    QTest::newRow("QNetworkReply::ProxyAuthenticationRequiredError") << int(QNetworkReply::ProxyAuthenticationRequiredError);
    QTest::newRow("QNetworkReply::ProtocolFailure") << int(QNetworkReply::ProtocolFailure);
}


QTEST_MAIN(tst_nokia_routing)

#include "tst_routing.moc"
