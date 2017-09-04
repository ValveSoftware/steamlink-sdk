/****************************************************************************
**
** Copyright (C) 2016 Aaron McCarthy <mccarthy.aaron@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite module of the Qt Toolkit.
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
#include <QtLocation/QPlaceProposedSearchResult>
#include <QtLocation/QPlaceIcon>
#include <QtLocation/QPlaceSearchRequest>
#include <QtTest/QtTest>

#include "../utils/qlocationtestutils_p.h"

QT_USE_NAMESPACE

class tst_QPlaceProposedSearchResult : public QObject
{
    Q_OBJECT

public:
    QPlaceProposedSearchResult initialSubObject();
    bool checkType(const QPlaceSearchResult &result);
    void detach(QPlaceSearchResult *result);
    void setSubClassProperty(QPlaceProposedSearchResult *result);

private Q_SLOTS:
    void constructorTest();
    void title();
    void icon();
    void searchRequest();
    void conversion();
};

QPlaceProposedSearchResult tst_QPlaceProposedSearchResult::initialSubObject()
{
    QPlaceProposedSearchResult proposedSearchResult;
    proposedSearchResult.setTitle(QStringLiteral("title"));

    QPlaceIcon icon;
    QVariantMap parameters;
    parameters.insert(QPlaceIcon::SingleUrl,
                      QUrl(QStringLiteral("file:///opt/icons/icon.png")));
    icon.setParameters(parameters);
    proposedSearchResult.setIcon(icon);

    QPlaceSearchRequest searchRequest;
    searchRequest.setSearchContext(QUrl(QStringLiteral("http://www.example.com/")));
    proposedSearchResult.setSearchRequest(searchRequest);

    return proposedSearchResult;
}

bool tst_QPlaceProposedSearchResult::checkType(const QPlaceSearchResult &result)
{
    return result.type() == QPlaceSearchResult::ProposedSearchResult;
}

void tst_QPlaceProposedSearchResult::detach(QPlaceSearchResult *result)
{
    result->setTitle(QStringLiteral("title"));
}

void tst_QPlaceProposedSearchResult::setSubClassProperty(QPlaceProposedSearchResult *result)
{
    QPlaceSearchRequest request;
    request.setSearchContext(QUrl(QStringLiteral("http://www.example.com/place-search")));
    result->setSearchRequest(request);
}

void tst_QPlaceProposedSearchResult::constructorTest()
{
    QPlaceProposedSearchResult result;
    QCOMPARE(result.type(), QPlaceSearchResult::ProposedSearchResult);

    result.setTitle(QStringLiteral("title"));

    QPlaceIcon icon;
    QVariantMap parameters;
    parameters.insert(QStringLiteral("paramKey"), QStringLiteral("paramValue"));
    icon.setParameters(parameters);
    result.setIcon(icon);

    QPlaceSearchRequest searchRequest;
    searchRequest.setSearchContext(QUrl(QStringLiteral("http://www.example.com/place-search")));
    result.setSearchRequest(searchRequest);

    //check copy constructor
    QPlaceProposedSearchResult result2(result);
    QCOMPARE(result2.title(), QStringLiteral("title"));
    QCOMPARE(result2.icon(), icon);
    QCOMPARE(result2.searchRequest(), searchRequest);

    QVERIFY(QLocationTestUtils::compareEquality(result, result2));

    //check results are the same after detachment of underlying shared data pointer
    result2.setTitle(QStringLiteral("title"));
    QVERIFY(QLocationTestUtils::compareEquality(result, result2));

    //check construction of SearchResult using a ProposedSearchResult
    QPlaceSearchResult searchResult(result);
    QCOMPARE(searchResult.title(), QStringLiteral("title"));
    QCOMPARE(searchResult.icon(), icon);
    QVERIFY(QLocationTestUtils::compareEquality(searchResult, result));
    QVERIFY(searchResult.type() == QPlaceSearchResult::ProposedSearchResult);
    result2 = searchResult;
    QVERIFY(QLocationTestUtils::compareEquality(result, result2));

    //check construction of a SearchResult using a SearchResult
    //that is actually a PlaceResult underneath
    QPlaceSearchResult searchResult2(searchResult);
    QCOMPARE(searchResult2.title(), QStringLiteral("title"));
    QCOMPARE(searchResult2.icon(), icon);
    QVERIFY(QLocationTestUtils::compareEquality(searchResult2, result));
    QVERIFY(QLocationTestUtils::compareEquality(searchResult, searchResult2));
    QVERIFY(searchResult2.type() == QPlaceSearchResult::ProposedSearchResult);
    result2 = searchResult2;
    QVERIFY(QLocationTestUtils::compareEquality(result, result2));
}

void tst_QPlaceProposedSearchResult::title()
{
    QPlaceProposedSearchResult result;
    QVERIFY(result.title().isEmpty());

    result.setTitle(QStringLiteral("title"));
    QCOMPARE(result.title(), QStringLiteral("title"));

    result.setTitle(QString());
    QVERIFY(result.title().isEmpty());

    QPlaceProposedSearchResult result2;
    QVERIFY(QLocationTestUtils::compareEquality(result, result2));

    result2.setTitle("title");
    QVERIFY(QLocationTestUtils::compareInequality(result, result2));

    result.setTitle("title");
    QVERIFY(QLocationTestUtils::compareEquality(result, result2));
}

void tst_QPlaceProposedSearchResult::icon()
{
    QPlaceProposedSearchResult result;
    QVERIFY(result.icon().isEmpty());

    QPlaceIcon icon;
    QVariantMap iconParams;
    iconParams.insert(QStringLiteral("paramKey"), QStringLiteral("paramValue"));
    icon.setParameters(iconParams);
    result.setIcon(icon);
    QCOMPARE(result.icon(), icon);

    result.setIcon(QPlaceIcon());
    QVERIFY(result.icon().isEmpty());

    QPlaceProposedSearchResult result2;
    QVERIFY(QLocationTestUtils::compareEquality(result, result2));

    result2.setIcon(icon);
    QVERIFY(QLocationTestUtils::compareInequality(result, result2));

    result.setIcon(icon);
    QVERIFY(QLocationTestUtils::compareEquality(result, result2));
}

void tst_QPlaceProposedSearchResult::searchRequest()
{
    QPlaceProposedSearchResult result;
    QCOMPARE(result.searchRequest(), QPlaceSearchRequest());

    QPlaceSearchRequest placeSearchRequest;
    placeSearchRequest.setSearchContext(QUrl(QStringLiteral("http://www.example.com/")));
    result.setSearchRequest(placeSearchRequest);
    QCOMPARE(result.searchRequest(), placeSearchRequest);

    result.setSearchRequest(QPlaceSearchRequest());
    QCOMPARE(result.searchRequest(), QPlaceSearchRequest());

    QPlaceProposedSearchResult result2;
    QVERIFY(QLocationTestUtils::compareEquality(result, result2));

    result2.setSearchRequest(placeSearchRequest);
    QVERIFY(QLocationTestUtils::compareInequality(result, result2));

    result.setSearchRequest(placeSearchRequest);
    QVERIFY(QLocationTestUtils::compareEquality(result, result2));
}

void tst_QPlaceProposedSearchResult::conversion()
{
    QLocationTestUtils::testConversion<tst_QPlaceProposedSearchResult,
                                       QPlaceSearchResult,
                                       QPlaceProposedSearchResult>(this);
}

QTEST_APPLESS_MAIN(tst_QPlaceProposedSearchResult)

#include "tst_qproposedsearchresult.moc"
