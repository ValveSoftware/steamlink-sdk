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

#include "placemanager_utils.h"

#include <QtCore/QDebug>
#include <QtLocation/QPlace>
#include <QtLocation/QPlaceManager>
#include <QtLocation/QPlaceSearchReply>
#include <QtLocation/QPlaceResult>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

//constant for timeout to verify signals
const int PlaceManagerUtils::Timeout(10000);

PlaceManagerUtils::PlaceManagerUtils(QObject *parent)
    : QObject(parent), placeManager(0)
{
}

bool PlaceManagerUtils::doSavePlace(QPlaceManager *manager,
                                    const QPlace &place,
                                    QPlaceReply::Error expectedError,
                                    QString *placeId)
{
    Q_ASSERT(manager);
    QPlaceIdReply *saveReply = manager->savePlace(place);
    bool isSuccessful = checkSignals(saveReply, expectedError, manager);
    if (placeId != 0) {
        *placeId = saveReply->id();
    }

    if (saveReply->id().isEmpty() && expectedError == QPlaceReply::NoError) {
        qWarning("ID is empty in reply for save operation");
        qWarning() << "Error string = " << saveReply->errorString();
        isSuccessful = false;
    }

    if (!isSuccessful)
        qWarning() << "Error string = " << saveReply->errorString();

    return isSuccessful;
}

void PlaceManagerUtils::doSavePlaces(QPlaceManager *manager, QList<QPlace> &places)
{
    QPlaceIdReply *saveReply;

    foreach (QPlace place, places) {
        saveReply = manager->savePlace(place);
        QSignalSpy saveSpy(saveReply, SIGNAL(finished()));
        QTRY_VERIFY_WITH_TIMEOUT(saveSpy.count() == 1, Timeout);
        QCOMPARE(saveReply->error(), QPlaceReply::NoError);
        saveSpy.clear();
    }
}

void PlaceManagerUtils::doSavePlaces(QPlaceManager *manager, const QList<QPlace *> &places)
{
    QPlaceIdReply *saveReply;

    static int count= 0;
    foreach (QPlace *place, places) {
        count++;
        saveReply = manager->savePlace(*place);
        QSignalSpy saveSpy(saveReply, SIGNAL(finished()));
        QTRY_VERIFY_WITH_TIMEOUT(saveSpy.count() == 1, Timeout);
        QCOMPARE(saveReply->error(), QPlaceReply::NoError);
        place->setPlaceId(saveReply->id());
        saveSpy.clear();
    }
}

bool PlaceManagerUtils::doSearch(QPlaceManager *manager,
                                const QPlaceSearchRequest &request,
                                 QList<QPlaceSearchResult> *results,
                                 QPlaceReply::Error expectedError)
{
    QPlaceSearchReply *searchReply= manager->search(request);
    bool success = checkSignals(searchReply, expectedError, manager);
    *results = searchReply->results();
    return success;
}

bool PlaceManagerUtils::doSearch(QPlaceManager *manager,
                                       const QPlaceSearchRequest &request,
                                       QList<QPlace> *results, QPlaceReply::Error expectedError)
{
    bool success = false;
    results->clear();
    QList<QPlaceSearchResult> searchResults;
    success = doSearch(manager, request, &searchResults, expectedError);
    foreach (const QPlaceSearchResult &searchResult, searchResults) {
        if (searchResult.type() == QPlaceSearchResult::PlaceResult) {
            QPlaceResult placeResult = searchResult;
            results->append(placeResult.place());
        }
    }
    return success;
}

bool PlaceManagerUtils::doSearchSuggestions(QPlaceManager *manager,
                                            const QPlaceSearchRequest &request,
                                            QStringList *results,
                                            QPlaceReply::Error expectedError)
{
    QPlaceSearchSuggestionReply *reply = manager->searchSuggestions(request);
    bool success = checkSignals(reply, expectedError, manager);
    *results = reply->suggestions();

    if (!success)
        qDebug() << "Error string = " << reply->errorString();

    return success;
}

bool PlaceManagerUtils::doRemovePlace(QPlaceManager *manager,
                                      const QPlace &place,
                                      QPlaceReply::Error expectedError)
{
    QPlaceIdReply *removeReply = manager->removePlace(place.placeId());
    bool isSuccessful = false;
    isSuccessful = checkSignals(removeReply, expectedError, manager)
                    && (removeReply->id() == place.placeId());

    if (!isSuccessful)
        qWarning() << "Place removal unsuccessful errorString = " << removeReply->errorString();

    return isSuccessful;
}

bool PlaceManagerUtils::doFetchDetails(QPlaceManager *manager,
                                             QString placeId, QPlace *place,
                                             QPlaceReply::Error expectedError)
{
    QPlaceDetailsReply *detailsReply = manager->getPlaceDetails(placeId);
    bool success = checkSignals(detailsReply, expectedError, manager);
    *place = detailsReply->place();

    if (!success)
        qDebug() << "Error string = " << detailsReply->errorString();

    return success;
}

bool PlaceManagerUtils::doInitializeCategories(QPlaceManager *manager,
                                               QPlaceReply::Error expectedError)
{
    QPlaceReply *reply = manager->initializeCategories();
    bool success = checkSignals(reply, expectedError, manager);

    if (!success)
        qDebug() << "Error string = " << reply->errorString();

    delete reply;
    return success;
}

bool PlaceManagerUtils::doSaveCategory(QPlaceManager *manager,
                                       const QPlaceCategory &category,
                                       const QString &parentId,
                                       QPlaceReply::Error expectedError,
                                       QString *categoryId)
{
    QPlaceIdReply *idReply = manager->saveCategory(category, parentId);
    bool isSuccessful = checkSignals(idReply, expectedError, manager)
                        && (idReply->error() == expectedError);

    if (categoryId != 0)
        *categoryId = idReply->id();

    if (!isSuccessful)
        qDebug() << "Error string =" << idReply->errorString();
    return isSuccessful;
}

bool PlaceManagerUtils::doRemoveCategory(QPlaceManager *manager,
                                         const QPlaceCategory &category,
                                         QPlaceReply::Error expectedError)
{
    QPlaceIdReply *idReply = manager->removeCategory(category.categoryId());

    bool isSuccessful = checkSignals(idReply, expectedError, manager) &&
                        (idReply->error() == expectedError);
    return isSuccessful;
}

bool PlaceManagerUtils::doFetchCategory(QPlaceManager *manager,
                                        const QString &categoryId,
                                        QPlaceCategory *category,
                                        QPlaceReply::Error expectedError)
{
    Q_ASSERT(category);
    QPlaceReply * catInitReply = manager->initializeCategories();
    bool isSuccessful = checkSignals(catInitReply, expectedError, manager);
    *category = manager->category(categoryId);

    if (!isSuccessful)
        qDebug() << "Error initializing categories, error string = "
                 << catInitReply->errorString();

    if (category->categoryId() != categoryId)
        isSuccessful = false;
    return isSuccessful;
}

bool PlaceManagerUtils::doFetchContent(QPlaceManager *manager,
                                       const QPlaceContentRequest &request,
                                       QPlaceContent::Collection *results,
                                       QPlaceReply::Error expectedError)
{
    Q_ASSERT(results);
    QPlaceContentReply *reply = manager->getPlaceContent(request);
    bool isSuccessful = checkSignals(reply, expectedError, manager);
    *results = reply->content();

    if (!isSuccessful)
        qDebug() << "Error during content fetch, error string = "
                 << reply->errorString();

    return isSuccessful;
}

bool PlaceManagerUtils::doMatch(QPlaceManager *manager,
                                const QPlaceMatchRequest &request,
                                QList<QPlace> *places,
                                QPlaceReply::Error expectedError)
{
    QPlaceMatchReply *reply = manager->matchingPlaces(request);
    bool isSuccessful = checkSignals(reply, expectedError, manager) &&
            (reply->error() == expectedError);
    *places = reply->places();
    if (!isSuccessful)
        qDebug() << "Error for matching operation, error string = "
                 << reply->errorString();
    return isSuccessful;
}

bool PlaceManagerUtils::checkSignals(QPlaceReply *reply, QPlaceReply::Error expectedError,
                                           QPlaceManager *manager)
{
    Q_ASSERT(reply);
    QSignalSpy finishedSpy(reply, SIGNAL(finished()));
    QSignalSpy errorSpy(reply, SIGNAL(error(QPlaceReply::Error,QString)));
    QSignalSpy managerFinishedSpy(manager, SIGNAL(finished(QPlaceReply*)));
    QSignalSpy managerErrorSpy(manager,SIGNAL(error(QPlaceReply*,QPlaceReply::Error,QString)));

    if (expectedError != QPlaceReply::NoError) {
        //check that we get an error signal from the reply
        WAIT_UNTIL(errorSpy.count() == 1);
        if (errorSpy.count() != 1) {
            qWarning() << "Error signal for search operation not received";
            return false;
        }

        //check that we get the correct error from the reply's signal
        QPlaceReply::Error actualError = qvariant_cast<QPlaceReply::Error>(errorSpy.at(0).at(0));
        if (actualError != expectedError) {
            qWarning() << "Actual error code in reply signal does not match expected error code";
            qWarning() << "Actual error code = " << actualError;
            qWarning() << "Expected error coe =" << expectedError;
            return false;
        }

        //check that we get an error  signal from the manager
        WAIT_UNTIL(managerErrorSpy.count() == 1);
        if (managerErrorSpy.count() !=1) {
           qWarning() << "Error signal from manager for search operation not received";
           return false;
        }

        //check that we get the correct reply instance in the error signal from the manager
        if (qvariant_cast<QPlaceReply*>(managerErrorSpy.at(0).at(0)) != reply)  {
            qWarning() << "Reply instance in error signal from manager is incorrect";
            return false;
        }

        //check that we get the correct error from the signal of the manager
        actualError = qvariant_cast<QPlaceReply::Error>(managerErrorSpy.at(0).at(1));
        if (actualError != expectedError) {
            qWarning() << "Actual error code from manager signal does not match expected error code";
            qWarning() << "Actual error code =" << actualError;
            qWarning() << "Expected error code = " << expectedError;
            return false;
        }
    }

    //check that we get a finished signal
    WAIT_UNTIL(finishedSpy.count() == 1);
    if (finishedSpy.count() !=1) {
        qWarning() << "Finished signal from reply not received";
        return false;
    }

    if (reply->error() != expectedError) {
        qWarning() << "Actual error code does not match expected error code";
        qWarning() << "Actual error code: " << reply->error();
        qWarning() << "Expected error code" << expectedError;
        return false;
    }

    if (expectedError == QPlaceReply::NoError && !reply->errorString().isEmpty()) {
        qWarning() << "Expected error was no error but error string was not empty";
        qWarning() << "Error string=" << reply->errorString();
        return false;
    }

    //check that we get the finished signal from the manager
    WAIT_UNTIL(managerFinishedSpy.count() == 1);
    if (managerFinishedSpy.count() != 1) {
        qWarning() << "Finished signal from manager not received";
        return false;
    }

    //check that the reply instance in the finished signal from the manager is correct
    if (qvariant_cast<QPlaceReply *>(managerFinishedSpy.at(0).at(0)) != reply) {
        qWarning() << "Reply instance in finished signal from manager is incorrect";
        return false;
    }

    return true;
}

bool PlaceManagerUtils::compare(const QList<QPlace> &actualResults,
                                      const QList<QPlace> &expectedResults)
{
    QSet<QString> actualIds;
    foreach (const QPlace &place, actualResults)
        actualIds.insert(place.placeId());

    QSet<QString> expectedIds;
    foreach (const QPlace &place, expectedResults)
        expectedIds.insert(place.placeId());

    bool isMatch = (actualIds == expectedIds);
    if (actualResults.count() != expectedResults.count() || !isMatch) {
        qWarning() << "comparison of results by name does not match";
        qWarning() << "actual result ids: " << actualIds;
        qWarning() << "expected result ids : " << expectedIds;
        return false;
    }

    return isMatch;
}

void PlaceManagerUtils::setVisibility(QList<QPlace *> places, QLocation::Visibility visibility)
{
    foreach (QPlace *place, places)
        place->setVisibility(visibility);
}
