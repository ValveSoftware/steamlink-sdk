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

#include <QtCore/QString>
#include <QtTest/QtTest>

#include <QtLocation/QPlaceMatchRequest>
#include <QtLocation/QPlaceResult>

QT_USE_NAMESPACE

class tst_QPlaceMatchRequest : public QObject
{
    Q_OBJECT

public:
    tst_QPlaceMatchRequest();

private Q_SLOTS:
    void constructorTest();
    void placesTest();
    void resultsTest();
    void parametersTest();
    void clearTest();
};

tst_QPlaceMatchRequest::tst_QPlaceMatchRequest()
{
}

void tst_QPlaceMatchRequest::constructorTest()
{
    QPlaceMatchRequest request;
    QVariantMap params;
    params.insert(QStringLiteral("key"), QStringLiteral("val"));

    QPlace place1;
    place1.setName(QStringLiteral("place1"));

    QPlace place2;
    place2.setName(QStringLiteral("place2"));

    QList<QPlace> places;
    places << place1 << place2;

    request.setPlaces(places);
    request.setParameters(params);

    QPlaceMatchRequest copy(request);
    QCOMPARE(copy, request);
    QCOMPARE(copy.places(), places);
    QCOMPARE(copy.parameters(), params);
}

void tst_QPlaceMatchRequest::placesTest()
{
    QPlaceMatchRequest request;
    QCOMPARE(request.places().count(), 0);

    QPlace place1;
    place1.setName(QStringLiteral("place1"));

    QPlace place2;
    place2.setName(QStringLiteral("place2"));

    QList<QPlace> places;
    places << place1 << place2;

    request.setPlaces(places);
    QCOMPARE(request.places(), places);

    request.setPlaces(QList<QPlace>());
    QCOMPARE(request.places().count(), 0);
}

void tst_QPlaceMatchRequest::resultsTest()
{
    QPlaceMatchRequest request;
    QCOMPARE(request.places().count(), 0);

    QPlace place1;
    place1.setName(QStringLiteral("place1"));
    QPlaceResult result1;
    result1.setPlace(place1);

    QPlace place2;
    place2.setName(QStringLiteral("place2"));
    QPlaceResult result2;
    result2.setPlace(place2);

    QList<QPlaceSearchResult> results;
    results << result1 << result2;

    request.setResults(results);

    QCOMPARE(request.places().count(), 2);
    QCOMPARE(request.places().at(0), place1);
    QCOMPARE(request.places().at(1), place2);

    request.setResults(QList<QPlaceSearchResult>());
    QCOMPARE(request.places().count(), 0);
}

void tst_QPlaceMatchRequest::parametersTest()
{
    QPlaceMatchRequest request;
    QVERIFY(request.parameters().isEmpty());

    QVariantMap params;
    params.insert(QStringLiteral("key"), QStringLiteral("value"));

    request.setParameters(params);
    QCOMPARE(request.parameters(), params);
}

void tst_QPlaceMatchRequest::clearTest()
{
    QPlaceMatchRequest request;
    QVariantMap params;
    params.insert(QStringLiteral("key"), QStringLiteral("value"));

    QPlace place1;
    place1.setName(QStringLiteral("place1"));

    QPlace place2;
    place2.setName(QStringLiteral("place2"));

    QList<QPlace> places;
    places << place1 << place2;

    request.setPlaces(places);
    request.setParameters(params);

    request.clear();
    QVERIFY(request.places().isEmpty());
    QVERIFY(request.parameters().isEmpty());
}

QTEST_APPLESS_MAIN(tst_QPlaceMatchRequest)

#include "tst_qplacematchrequest.moc"
