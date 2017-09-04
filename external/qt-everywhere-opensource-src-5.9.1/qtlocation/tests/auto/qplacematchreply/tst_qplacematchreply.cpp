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

#include <QtLocation/QPlaceMatchReply>

QT_USE_NAMESPACE

class TestMatchReply : public QPlaceMatchReply
{
    Q_OBJECT
public:
    TestMatchReply(QObject *parent) : QPlaceMatchReply(parent) {}
    TestMatchReply() {}

    void setPlaces(const QList<QPlace> &places) {
        QPlaceMatchReply::setPlaces(places);
    }

    void setRequest(const QPlaceMatchRequest &request) {
        QPlaceMatchReply::setRequest(request);
    }
};

class tst_QPlaceMatchReply : public QObject
{
    Q_OBJECT

public:
    tst_QPlaceMatchReply();

private Q_SLOTS:
    void constructorTest();
    void typeTest();
    void requestTest();
//    void resultsTest();
};

tst_QPlaceMatchReply::tst_QPlaceMatchReply()
{
}

void tst_QPlaceMatchReply::constructorTest()
{
    QPlaceMatchReply *reply = new TestMatchReply(this);
    QVERIFY(reply->parent() == this);
    delete reply;
}

void tst_QPlaceMatchReply::typeTest()
{
    TestMatchReply *reply = new TestMatchReply(this);
    QVERIFY(reply->type() == QPlaceReply::MatchReply);
    delete reply;
}

void tst_QPlaceMatchReply::requestTest()
{
    TestMatchReply *reply = new TestMatchReply(this);
    QPlaceMatchRequest request;

    QPlace place1;
    place1.setName(QStringLiteral("place1"));

    QPlace place2;
    place2.setName(QStringLiteral("place2"));

    QList<QPlace> places;
    places << place1 << place2;

    request.setPlaces(places);

    reply->setRequest(request);
    QCOMPARE(reply->request(), request);

    reply->setRequest(QPlaceMatchRequest());
    QCOMPARE(reply->request(), QPlaceMatchRequest());
    delete reply;
}


QTEST_APPLESS_MAIN(tst_QPlaceMatchReply)

#include "tst_qplacematchreply.moc"
