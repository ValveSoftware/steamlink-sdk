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

#include <QCoreApplication>
#include <QString>
#include <QtTest/QtTest>

#include <QtCore/QProcessEnvironment>
#include <QtPositioning/QGeoCircle>
#include <QtLocation/QGeoServiceProvider>
#include <QtLocation/QPlaceEditorial>
#include <QtLocation/QPlaceImage>
#include <QtLocation/QPlaceManager>
#include <QtLocation/QPlaceResult>
#include <QtLocation/QPlaceReview>
#include <QtLocation/QPlaceSearchReply>
#include "../../placemanager_utils/placemanager_utils.h"

QT_USE_NAMESPACE

class tst_QPlaceManagerNokia : public PlaceManagerUtils
{
    Q_OBJECT
public:
    enum ExpectedResults {
        AnyResults, //zero or more results expected
        SomeResults, //at least one result expected
        NoResults // zero results expected
    };

    tst_QPlaceManagerNokia();

private Q_SLOTS:
    void initTestCase();
    void search();
    void search_data();
    void searchResultFields();
    void recommendations();
    void recommendations_data();
    void details();
    void categories();
    void suggestions();
    void suggestions_data();
    void suggestionsMisc();
    void locale();
    void content();
    void content_data();
    void unsupportedFunctions();

private:
    void commonAreas(QList<QByteArray> *dataTags, QList<QGeoShape> *areas,
                      QList<QPlaceReply::Error> *errors,
                      QList<ExpectedResults> *results);

    static const QLatin1String ValidKnownPlaceId;
    static const QLatin1String ProxyEnv;
    static const QLatin1String AppIdEnv;
    static const QLatin1String TokenEnv;

    QGeoServiceProvider *provider;
};

Q_DECLARE_METATYPE(tst_QPlaceManagerNokia::ExpectedResults)

// ValidKnownPlaceId is the id of a place with a full complement of place content. Editorials,
// reviews, images, recommendations. If it disappears these tests will fail.
// Currently it is set to an Eiffel Tower tourist office.
const QLatin1String tst_QPlaceManagerNokia::ValidKnownPlaceId("250u09tu-4561b8da952f4fd79c4e1998c3fcf032");

const QLatin1String tst_QPlaceManagerNokia::ProxyEnv("NOKIA_PLUGIN_PROXY");
const QLatin1String tst_QPlaceManagerNokia::AppIdEnv("NOKIA_APPID");
const QLatin1String tst_QPlaceManagerNokia::TokenEnv("NOKIA_TOKEN");

tst_QPlaceManagerNokia::tst_QPlaceManagerNokia()
{
}

void tst_QPlaceManagerNokia::initTestCase()
{
    QVariantMap params;
    QStringList providers = QGeoServiceProvider::availableServiceProviders();
    QVERIFY(providers.contains("here"));
#ifndef QT_NO_PROCESS
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    if (!(env.contains(AppIdEnv) && env.contains(TokenEnv)))
        QSKIP("NOKIA_APPID and NOKIA_TOKEN environment variables not set");\

    params.insert(QStringLiteral("here.app_id"), env.value(AppIdEnv));
    params.insert(QStringLiteral("here.token"), env.value(TokenEnv));

    if (env.contains(ProxyEnv))
        params.insert(QStringLiteral("here.proxy"), env.value(ProxyEnv));
#else
    QSKIP("Cannot parse process environment, NOKIA_APPID and NOKIA_TOKEN not set");
#endif
    provider = new QGeoServiceProvider("here", params);
    placeManager = provider->placeManager();
    QVERIFY(placeManager);
}

void tst_QPlaceManagerNokia::search()
{
    QFETCH(QGeoShape, area);
    QFETCH(QString, searchTerm);
    QFETCH(QList<QPlaceCategory>, categories);
    QFETCH(QPlaceReply::Error, error);
    QFETCH(ExpectedResults, expectedResults);

    QPlaceSearchRequest searchRequest;
    searchRequest.setSearchArea(area);
    searchRequest.setSearchTerm(searchTerm);
    if (categories.count() == 1)
        searchRequest.setCategory(categories.first());
    else
        searchRequest.setCategories(categories);

    QList<QPlace> results;
    QVERIFY(doSearch(searchRequest, &results, error));

    if (expectedResults == NoResults)
        QVERIFY(results.count() == 0);
    else if (expectedResults == SomeResults)
        QVERIFY(results.count() > 0);
}

void tst_QPlaceManagerNokia::search_data()
{
    QTest::addColumn<QGeoShape>("area");
    QTest::addColumn<QString>("searchTerm");
    QTest::addColumn<QList<QPlaceCategory> >("categories");
    QTest::addColumn<QPlaceSearchRequest::RelevanceHint>("hint");
    QTest::addColumn<QPlaceReply::Error> ("error");
    QTest::addColumn<ExpectedResults>("expectedResults");

    for (int i = 0; i < 3; i++) {
        QByteArray suffix;
        QGeoShape area;
        if (i==0) {
            area = QGeoCircle(QGeoCoordinate(-27.5, 153));
            suffix = " (Circle - center only)";
        } else if (i==1) {
            area = QGeoCircle(QGeoCoordinate(-27.5, 153), 5000);
            suffix = " (Circle - with radius specified)";
        } else {
            area = QGeoRectangle(QGeoCoordinate(-26.5, 152), QGeoCoordinate(-28.5, 154));
            suffix = " (Rectangle)";
        }

        QByteArray dataTag = QByteArray("coordinate only") + suffix;
        QTest::newRow(dataTag) << area
                               << QString()
                               << QList<QPlaceCategory>()
                               << QPlaceSearchRequest::UnspecifiedHint
                               << QPlaceReply::NoError
                               << SomeResults;

        dataTag = QByteArray("seach term") + suffix;
        QTest::newRow(dataTag) << area
                               << "sushi"
                               << QList<QPlaceCategory>()
                               << QPlaceSearchRequest::UnspecifiedHint
                               << QPlaceReply::NoError
                               << SomeResults;

        QPlaceCategory eatDrinkCat;
        eatDrinkCat.setCategoryId(QStringLiteral("eat-drink"));
        dataTag = QByteArray("single valid category") + suffix;
        QTest::newRow(dataTag) << area
                               << QString()
                               << (QList<QPlaceCategory>()
                                   << eatDrinkCat)
                               << QPlaceSearchRequest::UnspecifiedHint
                               << QPlaceReply::NoError
                               << SomeResults;

        QPlaceCategory accommodationCat;
        accommodationCat.setCategoryId(QStringLiteral("accommodation"));
        dataTag = QByteArray("multiple valid categories") + suffix;
        QTest::newRow(dataTag) << area
                               << QString()
                               << (QList<QPlaceCategory>()
                                   << eatDrinkCat
                                   << accommodationCat)
                               << QPlaceSearchRequest::UnspecifiedHint
                               << QPlaceReply::NoError
                               << SomeResults;

        QPlaceCategory nonExistentCat;
        nonExistentCat.setCategoryId(QStringLiteral("klingon cuisine"));
        dataTag  = QByteArray("non-existent category") + suffix;
        QTest::newRow(dataTag) << area
                               << QString()
                               << (QList<QPlaceCategory>()
                                   << nonExistentCat)
                               << QPlaceSearchRequest::UnspecifiedHint
                               << QPlaceReply::NoError
                               << NoResults;

        dataTag = QByteArray("search term and category") + suffix;
        QTest::newRow(dataTag) << area
                               << "sushi"
                               << (QList<QPlaceCategory>()
                                   << eatDrinkCat)
                               << QPlaceSearchRequest::UnspecifiedHint
                               << QPlaceReply::BadArgumentError
                               << NoResults;
    }


    //invalid areas and boundary testing
    QList<QByteArray> dataTags;
    QList<QGeoShape> areas;
    QList<QPlaceReply::Error> errors;
    QList<ExpectedResults> results;
    commonAreas(&dataTags, &areas, &errors, &results);

    for (int i = 0; i < dataTags.count(); ++i) {
        QTest::newRow(dataTags.at(i)) << areas.at(i)
                                      << "sushi"
                                      << QList<QPlaceCategory>()
                                      << QPlaceSearchRequest::UnspecifiedHint
                                      << errors.at(i)
                                      << results.at(i);
    }

    //relevancy hints will be ignored by the backend, but should give a valid result
    QTest::newRow("check using a distance relevancy hint")
            << static_cast<QGeoShape>(QGeoCircle(QGeoCoordinate(-27.5, 153)))
            << QStringLiteral("sushi")
            << QList<QPlaceCategory>()
            << QPlaceSearchRequest::DistanceHint
            << QPlaceReply::NoError
            << AnyResults;

    QTest::newRow("check using lexical place name hint")
            << static_cast<QGeoShape>(QGeoCircle(QGeoCoordinate(-27.5, 153)))
            << QStringLiteral("sushi")
            << QList<QPlaceCategory>()
            << QPlaceSearchRequest::LexicalPlaceNameHint
            << QPlaceReply::NoError
            << AnyResults;
}

void tst_QPlaceManagerNokia::searchResultFields()
{
    //check that using a distance relevancy hint will give a valid result
    //even though it will be ignored by the backend.
    QPlaceSearchRequest searchRequest;
    searchRequest.setSearchArea(QGeoCircle(QGeoCoordinate(-27.5, 153)));
    searchRequest.setSearchTerm(QStringLiteral("sushi"));

    QPlaceSearchReply *reply = placeManager->search(searchRequest);
    QSignalSpy spy(reply, SIGNAL(finished()));
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() == 1, Timeout);
    QVERIFY(reply->results().count() > 0);

    //check that search results have basic data filled in
    QPlaceResult result= reply->results().at(0);
    QVERIFY(!result.title().isEmpty());
    QVERIFY(!result.icon().url().isEmpty());
    QVERIFY(!qIsNaN(result.distance()));
    QVERIFY(!result.place().name().isEmpty());
    QVERIFY(result.place().location().coordinate().isValid());
    QVERIFY(!result.place().location().address().text().isEmpty());
    QVERIFY(!result.place().location().address().isTextGenerated());
    QVERIFY(result.place().categories().count() == 1);//only primary category retrieved on
                                                      //search

    //sponsored and ratings fields are optional and thus have not been explicitly tested.
}

void tst_QPlaceManagerNokia::recommendations()
{
    QFETCH(QString, recommendationId);
    QFETCH(QString, searchTerm);
    QFETCH(QGeoShape, searchArea);
    QFETCH(QList<QPlaceCategory>, categories);
    QFETCH(QPlaceReply::Error, error);

    QPlaceSearchRequest searchRequest;
    searchRequest.setRecommendationId(recommendationId);
    searchRequest.setSearchTerm(searchTerm);
    searchRequest.setSearchArea(searchArea);
    searchRequest.setCategories(categories);

    QList<QPlace> results;
    QVERIFY(doSearch(searchRequest, &results, error));

    if (error == QPlaceReply::NoError)
        QVERIFY(results.count() > 0);
}

void tst_QPlaceManagerNokia::recommendations_data()
{
    QTest::addColumn<QString>("recommendationId");
    QTest::addColumn<QString>("searchTerm");
    QTest::addColumn<QGeoShape>("searchArea");
    QTest::addColumn<QList<QPlaceCategory> >("categories");
    QTest::addColumn<QPlaceReply::Error>("error");

    QPlaceCategory eatDrinkCat;
    eatDrinkCat.setCategoryId(QStringLiteral("eat-drink"));

    QTest::newRow("search recommendations with valid id")
            << QString(ValidKnownPlaceId)
            << QString()
            << QGeoShape()
            << QList<QPlaceCategory>()
            << QPlaceReply::NoError;

    QTest::newRow("search for recommendations with invalid id")
            << QStringLiteral("does_not_exist_id")
            << QString()
            << QGeoShape()
            << QList<QPlaceCategory>()
            << QPlaceReply::PlaceDoesNotExistError;

    QTest::newRow("search for recommendations with id and search term")
            << QString(ValidKnownPlaceId)
            << QStringLiteral("sushi")
            << QGeoShape()
            << QList<QPlaceCategory>()
            << QPlaceReply::BadArgumentError;

    QTest::newRow("search for recommendations with an id and category")
            << QString(ValidKnownPlaceId)
            << QString()
            << QGeoShape()
            << (QList<QPlaceCategory>() << eatDrinkCat)
            << QPlaceReply::BadArgumentError;

    QTest::newRow("search for recommendations with id, search term and category")
            << QString(ValidKnownPlaceId)
            << QStringLiteral("sushi")
            << QGeoShape()
            << (QList<QPlaceCategory>() << eatDrinkCat)
            << QPlaceReply::BadArgumentError;

    QTest::newRow("search for recommendations with an id and search area")
            << QString(ValidKnownPlaceId)
            << QString()
            << static_cast<QGeoShape>(QGeoCircle(QGeoCoordinate(-27.5, 153)))
            << QList<QPlaceCategory>()
            << QPlaceReply::BadArgumentError;
}

void tst_QPlaceManagerNokia::details()
{
    QSKIP("Fetching details from HERE place server always fails - QTBUG-44837");
    //fetch the details of a valid place
    QPlace place;
    QVERIFY(doFetchDetails(ValidKnownPlaceId, &place));
    QVERIFY(!place.name().isEmpty());
    QVERIFY(!place.icon().url().isEmpty());
    QStringList contactTypes = place.contactTypes();
    QVERIFY(!contactTypes.isEmpty());
    foreach (const QString &contactType, contactTypes) {
        QList<QPlaceContactDetail> details = place.contactDetails(contactType);
        QVERIFY(details.count() > 0);
        foreach (const QPlaceContactDetail &detail, details) {
            QVERIFY(!detail.label().isEmpty());
            QVERIFY(!detail.value().isEmpty());
        }
    }

    QVERIFY(place.location().coordinate().isValid());
    QVERIFY(!place.location().address().isEmpty());
    QVERIFY(!place.location().address().text().isEmpty());
    QVERIFY(!place.location().address().isTextGenerated());

    QVERIFY(place.ratings().average() >= 1 && place.ratings().average() <= 5);
    QVERIFY(place.ratings().maximum() == 5);
    QVERIFY(place.ratings().count() >  0);

    QVERIFY(place.categories().count() > 0);
    foreach (const QPlaceCategory &category, place.categories()) {
        QVERIFY(!category.name().isEmpty());
        QVERIFY(!category.categoryId().isEmpty());
        QVERIFY(!category.icon().url().isEmpty());
    }

    QVERIFY(!place.extendedAttributeTypes().isEmpty());
    QVERIFY(place.visibility() == QLocation::PublicVisibility);
    QVERIFY(place.detailsFetched());

    //attributions are optional and thus have not been explicitly tested.

    //fetch the details of a non-existent place
    QVERIFY(doFetchDetails(QStringLiteral("does_not_exist"), &place,
                           QPlaceReply::PlaceDoesNotExistError));
}

void tst_QPlaceManagerNokia::categories()
{
    QVERIFY(doInitializeCategories());

    QList<QPlaceCategory> categories = placeManager->childCategories();
    QVERIFY(categories.count() > 0);
    foreach (const QPlaceCategory &category, categories) {
        //check that we have valid fields
        QVERIFY(!category.categoryId().isEmpty());
        QVERIFY(!category.name().isEmpty());
        QVERIFY(!category.icon().url().isEmpty());

        //check we can retrieve the very same category by id
        QCOMPARE(placeManager->category(category.categoryId()), category);

        //the here plugin supports a two-level level category tree
        QVERIFY(placeManager->parentCategoryId(category.categoryId()).isEmpty());
        const QList<QPlaceCategory> childCats =
                placeManager->childCategories(category.categoryId());
        if (!childCats.isEmpty()) {
            foreach (const QPlaceCategory &child, childCats) {
                // only two levels of categories hence 2.nd level has no further children
                QVERIFY(placeManager->childCategories(child.categoryId()).isEmpty());
                QVERIFY(placeManager->parentCategoryId(child.categoryId()) == category.categoryId());
            }
        }
    }
}

void tst_QPlaceManagerNokia::suggestions()
{
    QFETCH(QGeoShape, area);
    QFETCH(QString, searchTerm);
    QFETCH(QPlaceReply::Error, error);
    QFETCH(ExpectedResults, expectedResults);

    QPlaceSearchRequest searchRequest;
    searchRequest.setSearchArea(area);
    searchRequest.setSearchTerm(searchTerm);

    QStringList results;
    QVERIFY(doSearchSuggestions(searchRequest, &results, error));

    if (expectedResults == NoResults)
        QVERIFY(results.count() == 0);
    else if (expectedResults == SomeResults)
        QVERIFY(results.count() > 0);
}


void tst_QPlaceManagerNokia::suggestions_data()
{
    QTest::addColumn<QGeoShape>("area");
    QTest::addColumn<QString>("searchTerm");
    QTest::addColumn<QPlaceReply::Error> ("error");
    QTest::addColumn<ExpectedResults>("expectedResults");

    for (int i = 0; i < 3; i++) {
        QByteArray suffix;
        QGeoShape area;
        if (i == 0) {
            area = QGeoCircle(QGeoCoordinate(-27.5, 153));
            suffix = " (Circle - center only)";
        } else if (i == 1) {
            area = QGeoCircle(QGeoCoordinate(-27.5, 153), 5000);
            suffix = " (Circle - with radius specified)";
        } else {
            area = QGeoRectangle(QGeoCoordinate(-26.5, 152), QGeoCoordinate(-28.5, 154));
            suffix = " (Rectangle)";
        }

        QByteArray dataTag = QByteArray("valid usage") + suffix;
        QTest::newRow(dataTag) << area
                               << "sus"
                               << QPlaceReply::NoError
                               << SomeResults;
    }

    //invalid areas and boundary testing
    QList<QByteArray> dataTags;
    QList<QGeoShape> areas;
    QList<QPlaceReply::Error> errors;
    QList<ExpectedResults> results;
    commonAreas(&dataTags, &areas, &errors, &results);

    for (int i = 0; i < dataTags.count(); ++i) {
        QTest::newRow(dataTags.at(i)) << areas.at(i)
                                      << "sus"
                                      << errors.at(i)
                                      << results.at(i);
    }

    QTest::newRow("no text") << static_cast<QGeoShape>(QGeoCircle(QGeoCoordinate(-27.5, 153)))
                             << QString()
                             << QPlaceReply::NoError
                             << NoResults;
}

void tst_QPlaceManagerNokia::suggestionsMisc()
{
    //check providing a distance relevancy hint (should be ignored)
    QPlaceSearchRequest searchRequest;
    QStringList results;
    searchRequest.setSearchArea(QGeoCircle(QGeoCoordinate(-27.5, 153)));
    searchRequest.setSearchTerm(QStringLiteral("sus"));
    searchRequest.setRelevanceHint(QPlaceSearchRequest::DistanceHint);
    QVERIFY(doSearchSuggestions(searchRequest, &results, QPlaceReply::NoError));
    QVERIFY(results.count() > 0);
    searchRequest.clear();

    //check porviding a lexical place name relevancy hint (should be ignored)
    searchRequest.setSearchArea(QGeoCircle(QGeoCoordinate(-27.5, 153)));
    searchRequest.setSearchTerm(QStringLiteral("sus"));
    searchRequest.setRelevanceHint(QPlaceSearchRequest::LexicalPlaceNameHint);
    QVERIFY(doSearchSuggestions(searchRequest, &results, QPlaceReply::NoError));
    QVERIFY(results.count() > 0);
    searchRequest.clear();

    //check providing a category
    QPlaceCategory eatDrinkCat;
    eatDrinkCat.setCategoryId(QStringLiteral("eat-drink"));
    searchRequest.setSearchArea(QGeoCircle(QGeoCoordinate(-27.5, 153)));
    searchRequest.setSearchTerm(QStringLiteral("sus"));
    searchRequest.setCategory(eatDrinkCat);
    QVERIFY(doSearchSuggestions(searchRequest, &results, QPlaceReply::BadArgumentError));
    QCOMPARE(results.count(), 0);
    searchRequest.clear();

    //check providing a recommendation id
    searchRequest.setSearchArea(QGeoCircle(QGeoCoordinate(-27.5, 153)));
    searchRequest.setSearchTerm(QStringLiteral("sus"));
    searchRequest.setRecommendationId("id");
    QVERIFY(doSearchSuggestions(searchRequest, &results, QPlaceReply::BadArgumentError));
}

void tst_QPlaceManagerNokia::locale()
{
    //check that the defualt locale is set
    QCOMPARE(placeManager->locales().count(), 1);
    QCOMPARE(placeManager->locales().at(0), QLocale());

    //check that we can set different locales for the categories
    placeManager->setLocale(QLocale("en"));
    QVERIFY(doInitializeCategories());
    QList<QPlaceCategory> enCategories = placeManager->childCategories();
    QVERIFY(enCategories.count() > 0);

    placeManager->setLocale(QLocale("fi"));
    QVERIFY(doInitializeCategories());
    QList<QPlaceCategory> fiCategories = placeManager->childCategories();

    foreach (const QPlaceCategory enCat, enCategories) {
        foreach (const QPlaceCategory fiCat, fiCategories) {
            if (enCat.categoryId() == fiCat.categoryId()) {
                QVERIFY(fiCat.name() != enCat.name());
                QVERIFY(fiCat == placeManager->category(fiCat.categoryId()));
            }
        }
    }

    // we are skipping the check below because we are requesting
    // details for a place without a search before. This implies
    // URL templating must be possible which the HERE place
    // server refuses.

    QSKIP("remainder of test skipped due to QTBUG-44837");

    //check that setting a locale will affect place detail fetches.
    QPlace place;
    placeManager->setLocale(QLocale("en"));
    QVERIFY(doFetchDetails(ValidKnownPlaceId,
                           &place));
    QString englishName = place.name();
    placeManager->setLocale(QLocale("fr"));
    QVERIFY(doFetchDetails(ValidKnownPlaceId,
                           &place));
    QVERIFY(englishName != place.name());
}

void tst_QPlaceManagerNokia::content()
{
    QFETCH(QPlaceContent::Type, type);

    //check fetching of content
    QPlaceContentRequest request;
    request.setContentType(type);
    request.setPlaceId(ValidKnownPlaceId);
    QPlaceContent::Collection results;
    QVERIFY(doFetchContent(request, &results));

    QVERIFY(results.count() > 0);

    QMapIterator<int, QPlaceContent> iter(results);
    while (iter.hasNext()) {
        iter.next();
        switch (type) {
        case (QPlaceContent::ImageType): {
            QPlaceImage image = iter.value();
            QVERIFY(!image.url().isEmpty());
            break;
        } case (QPlaceContent::ReviewType) : {
            QPlaceReview review = iter.value();
            QVERIFY(!review.dateTime().isValid());
            QVERIFY(!review.text().isEmpty());
            QVERIFY(review.rating() >= 1 && review.rating() <= 5);

            //title and language fields are optional and thus have not been
            //explicitly tested
            break;
        } case (QPlaceContent::EditorialType): {
            QPlaceEditorial editorial = iter.value();
            QVERIFY(!editorial.text().isEmpty());

            //The language field is optional and thus has not been
            //explicitly tested.
            break;
        } default:
            QFAIL("Unknown content type");
        }
    }

    //check total count
    QPlaceContentReply *contentReply = placeManager->getPlaceContent(request);
    QSignalSpy contentSpy(contentReply, SIGNAL(finished()));
    QTRY_VERIFY_WITH_TIMEOUT(contentSpy.count() ==1, Timeout);
    QVERIFY(contentReply->totalCount() > 0);

    if (contentReply->totalCount() >= 2) {
        //try testing with a limit
        request.setLimit(1);
        QPlaceContent::Collection newResults;
        QVERIFY(doFetchContent(request, &newResults));
        QCOMPARE(newResults.count(), 1);
        QCOMPARE(newResults.values().first(), results.value(0));
    }
}

void tst_QPlaceManagerNokia::content_data()
{
    QTest::addColumn<QPlaceContent::Type>("type");

    QTest::newRow("images") << QPlaceContent::ImageType;
    QTest::newRow("reviews") << QPlaceContent::ReviewType;
    QTest::newRow("editorials") << QPlaceContent::EditorialType;
}

void tst_QPlaceManagerNokia::unsupportedFunctions()
{
    QPlace place;
    place.setName(QStringLiteral("Brisbane"));

    QVERIFY(doSavePlace(place, QPlaceReply::UnsupportedError));
    QVERIFY(doRemovePlace(place, QPlaceReply::UnsupportedError));

    QPlaceCategory category;
    category.setName(QStringLiteral("Accommodation"));
    QVERIFY(doSaveCategory(category, QPlaceReply::UnsupportedError));
    QVERIFY(doRemoveCategory(category, QPlaceReply::UnsupportedError));
}

void tst_QPlaceManagerNokia::commonAreas(QList<QByteArray> *dataTags,
                                          QList<QGeoShape> *areas,
                                          QList<QPlaceReply::Error> *errors,
                                          QList<ExpectedResults> *results)
{
    Q_ASSERT(dataTags);
    Q_ASSERT(areas);
    dataTags->append("Unknown shape for search area");
    areas->append(QGeoShape());
    errors->append(QPlaceReply::BadArgumentError);
    results->append(NoResults);

    dataTags->append("NaN coordinate");
    areas->append(QGeoCircle(QGeoCoordinate()));
    errors->append(QPlaceReply::BadArgumentError);
    results->append(NoResults);

    dataTags->append("Valid latitude (upper boundary)");
    areas->append(QGeoCircle(QGeoCoordinate(90.0, 45)));
    errors->append(QPlaceReply::NoError);
    results->append(AnyResults);

    dataTags->append("Invalid latitude (upper boundary)");
    areas->append(QGeoCircle(QGeoCoordinate(90.1, 45)));
    errors->append(QPlaceReply::BadArgumentError);
    results->append(NoResults);

    dataTags->append("Valid latitude (lower boundary)");
    areas->append(QGeoCircle(QGeoCoordinate(-90.0, 45)));
    errors->append(QPlaceReply::NoError);
    results->append(AnyResults);

    dataTags->append("Invalid latitude (lower boundary)");
    areas->append(QGeoCircle(QGeoCoordinate(-90.1, 45)));
    errors->append(QPlaceReply::BadArgumentError);
    results->append(NoResults);

    dataTags->append("Valid longitude (lower boundary)");
    areas->append(QGeoCircle(QGeoCoordinate(-45, -180.0)));
    errors->append(QPlaceReply::NoError);
    results->append(AnyResults);

    dataTags->append("Invalid longitude (lower boundary)");
    areas->append(QGeoCircle(QGeoCoordinate(-45, -180.1)));
    errors->append(QPlaceReply::BadArgumentError);
    results->append(NoResults);

    dataTags->append("Valid longitude (upper boundary)");
    areas->append(QGeoCircle(QGeoCoordinate(-45, 180.0)));
    errors->append(QPlaceReply::NoError);
    results->append(AnyResults);

    dataTags->append("Invalid longitude (upper boundary)");
    areas->append(QGeoCircle(QGeoCoordinate(-45, 180.1)));
    errors->append(QPlaceReply::BadArgumentError);
    results->append(NoResults);

    dataTags->append("Invalid rectangular area");
    areas->append(QGeoRectangle(QGeoCoordinate(20,20),
                  QGeoCoordinate(30,10)));
    errors->append(QPlaceReply::BadArgumentError);
    results->append(NoResults);
}

QTEST_GUILESS_MAIN(tst_QPlaceManagerNokia)

#include "tst_places.moc"
