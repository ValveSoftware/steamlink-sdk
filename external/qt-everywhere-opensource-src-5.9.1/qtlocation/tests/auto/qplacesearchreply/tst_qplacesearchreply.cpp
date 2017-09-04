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

#include <QtPositioning/QGeoCircle>
#include <QtLocation/QPlaceSearchRequest>
#include <QtLocation/QPlaceSearchReply>
#include <QtLocation/QPlaceResult>


QT_USE_NAMESPACE

class TestSearchReply : public QPlaceSearchReply
{
    Q_OBJECT
public:
    TestSearchReply(QObject *parent) : QPlaceSearchReply(parent) {}
    TestSearchReply() {}

    void setResults(const QList<QPlaceSearchResult> &results) {
        QPlaceSearchReply::setResults(results);
    }

    void setRequest(const QPlaceSearchRequest &request) {
        QPlaceSearchReply::setRequest(request);
    }
};

class tst_QPlaceSearchReply : public QObject
{
    Q_OBJECT

public:
    tst_QPlaceSearchReply();

private Q_SLOTS:
    void constructorTest();
    void typeTest();
    void requestTest();
    void resultsTest();
};

tst_QPlaceSearchReply::tst_QPlaceSearchReply()
{
}

void tst_QPlaceSearchReply::constructorTest()
{
    TestSearchReply *reply = new TestSearchReply(this);
    QVERIFY(reply->parent() == this);
    delete reply;
}

void tst_QPlaceSearchReply::typeTest()
{
    TestSearchReply *reply = new TestSearchReply(this);
    QVERIFY(reply->type() == QPlaceReply::SearchReply);
    delete reply;
}

void tst_QPlaceSearchReply::requestTest()
{
    TestSearchReply *reply = new TestSearchReply(this);
    QPlaceSearchRequest request;
    request.setLimit(10);

    QGeoCircle circle;
    circle.setCenter(QGeoCoordinate(10,20));
    request.setSearchArea(circle);

    request.setSearchTerm("pizza");

    reply->setRequest(request);
    QCOMPARE(reply->request(), request);
    reply->setRequest(QPlaceSearchRequest());
    QCOMPARE(reply->request(), QPlaceSearchRequest());
    delete reply;
}

void tst_QPlaceSearchReply::resultsTest()
{
    TestSearchReply *reply = new TestSearchReply(this);
    QList<QPlaceSearchResult> results;
    QPlace winterfell;
    winterfell.setName("Winterfell");
    QPlace casterlyRock;
    casterlyRock.setName("Casterly Rock");
    QPlace stormsEnd;
    stormsEnd.setName("Storm's end");

    QPlaceResult result1;
    result1.setPlace(winterfell);
    QPlaceResult result2;
    result2.setPlace(casterlyRock);
    QPlaceResult result3;
    result3.setPlace(stormsEnd);
    results << result1 << result2 << result3;

    reply->setResults(results);
    QCOMPARE(reply->results(), results);
    reply->setResults(QList<QPlaceSearchResult>());
    QCOMPARE(reply->results(), QList<QPlaceSearchResult>());
    delete reply;
}

QTEST_APPLESS_MAIN(tst_QPlaceSearchReply)

#include "tst_qplacesearchreply.moc"
