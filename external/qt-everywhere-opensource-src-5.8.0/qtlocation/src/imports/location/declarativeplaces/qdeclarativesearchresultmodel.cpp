/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdeclarativesearchresultmodel_p.h"
#include "qdeclarativeplace_p.h"
#include "qdeclarativeplaceicon_p.h"

#include <QtQml/QQmlEngine>
#include <QtQml/QQmlInfo>
#include <QtLocation/QGeoServiceProvider>
#include <QtLocation/QPlaceSearchReply>
#include <QtLocation/QPlaceManager>
#include <QtLocation/QPlaceMatchRequest>
#include <QtLocation/QPlaceMatchReply>
#include <QtLocation/QPlaceResult>
#include <QtLocation/QPlaceProposedSearchResult>

QT_USE_NAMESPACE

/*!
    \qmltype PlaceSearchModel
    \instantiates QDeclarativeSearchResultModel
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-places
    \ingroup qml-QtLocation5-places-models
    \since Qt Location 5.5

    \brief Provides access to place search results.

    PlaceSearchModel provides a model of place search results within the \l searchArea.  The
    \l searchTerm and \l categories properties can be set to restrict the search results to
    places matching those criteria.

    The PlaceSearchModel returns both sponsored and
    \l {http://en.wikipedia.org/wiki/Organic_search}{organic search results}.  Sponsored search
    results will have the \c sponsored role set to true.

    \target PlaceSearchModel Roles
    The model returns data for the following roles:

    \table
        \header
            \li Role
            \li Type
            \li Description
        \row
            \li type
            \li enum
            \li The type of search result.
        \row
            \li title
            \li string
            \li A string describing the search result.
        \row
            \li icon
            \li PlaceIcon
            \li Icon representing the search result.
        \row
            \li distance
            \li real
            \li Valid only when the \c type role is \c PlaceResult, the distance to the place
                from the center of the \l searchArea. If no \l searchArea
                has been specified, the distance is NaN.
        \row
            \li place
            \li \l Place
            \li Valid only when the \c type role is \c PlaceResult, an object representing the
               place.
        \row
            \li sponsored
            \li bool
            \li Valid only when the \c type role is \c PlaceResult, true if the search result is a
               sponsored result.
    \endtable

    \section2 Search Result Types

    The \c type role can take on the following values:

    \table
        \row
            \li PlaceSearchModel.UnknownSearchResult
            \li The contents of the search result are unknown.
        \row
            \li PlaceSearchModel.PlaceResult
            \li The search result contains a place.
        \row
            \li PlaceSearchModel.ProposedSearchResult
            \li The search result contains a proposed search which may be relevant.
    \endtable


    It can often be helpful to use a \l Loader to create a delegate
    that will choose different \l {Component}s based on the search result type.

    \snippet declarative/places_loader.qml Handle Result Types

    \section1 Detection of Updated and Removed Places

    The PlaceSearchModel listens for places that have been updated or removed from its plugin's backend.
    If it detects that a place has been updated and that place is currently present in the model, then
    it will call \l Place::getDetails to refresh the details.  If it detects that a place has been
    removed, then correspondingly the place will be removed from the model if it is currently
    present.

    \section1 Example

    The following example shows how to use the PlaceSearchModel to search for Pizza restaurants in
    close proximity of a given position.  A \l searchTerm and \l searchArea are provided to the model
    and \l update() is used to perform a lookup query.  Note that the model does not incrementally
    fetch search results, but rather performs a single fetch when \l update() is run.  The \l count
    is set to the number of search results returned during the fetch.

    \snippet places_list/places_list.qml Imports
    \codeline
    \snippet places_list/places_list.qml PlaceSearchModel

    \sa CategoryModel, {QPlaceManager}

    \section1 Paging
    The PlaceSearchModel API has some limited support
    for paging. The \l nextPage() and \l previousPage() functions as well as
    the \l limit property can be used to access
    paged search results. When the \l limit property is set
    the search result page contains at most \l limit entries (of type place result).
    For example, if the backend has 5 search results in total
    [a,b,c,d,e], and assuming the first page is shown and limit of 3 has been set
    then a,b,c is returned. The \l nextPage() would return d,e. The
    \l nextPagesAvailable and \l previousPagesAvailable properties
    can be used to check for further pages. At the moment the API does not
    support the means to retrieve the total number of items available from the
    backed. Note that support for \l nextPage(), previousPage() and \l limit can vary
    according to the \l plugin.
*/

/*!
    \qmlproperty Plugin PlaceSearchModel::plugin

    This property holds the \l Plugin which will be used to perform the search.
*/

/*!
    \qmlproperty Plugin PlaceSearchModel::favoritesPlugin

    This property holds the \l Plugin which will be used to search for favorites.
    Any places from the search which can be cross-referenced or matched
    in the favoritesPlugin will have their \l {Place::favorite}{favorite} property
    set to the corresponding \l Place from the favoritesPlugin.

    If the favoritesPlugin is not set, the \l {Place::favorite}{favorite} property
    of the places in the results will always be null.

    \sa Favorites
*/

/*!
    \qmlproperty VariantMap PlaceSearchModel::favoritesMatchParameters

    This property holds a set of parameters used to specify how search result places
    are matched to favorites in the favoritesPlugin.

    By default the parameter map is empty and implies that the favorites plugin
    matches by \l {Alternative Identifier Cross-Referencing}{alternative identifiers}.  Generally,
    an application developer will not need to set this property.

    In cases where the favorites plugin does not support matching by alternative identifiers,
    then the \l {Qt Location#Plugin References and Parameters}{plugin documentation} should
    be consulted to see precisely what key-value parameters to set.
*/

/*!
    \qmlproperty variant PlaceSearchModel::searchArea

    This property holds the search area.  The search result returned by the model will be within
    the search area.

    If this property is set to a \l {geocircle} its
    \l {geocircle}{radius} property may be left unset, in which case the \l Plugin
    will choose an appropriate radius for the search.

    Support for specifying a search area can vary according to the \l plugin backend
    implementation.  For example, some may support a search center only while others may only
    support geo rectangles.
*/

/*!
    \qmlproperty int PlaceSearchModel::limit

    This property holds the limit of the number of items that will be returned.
*/

/*!
    \qmlproperty bool PlaceSearchModel::previousPagesAvailable

    This property holds whether there is one or more previous pages of search results available.

    \sa previousPage()
*/

/*!
    \qmlproperty bool PlaceSearchModel::nextPagesAvailable

    This property holds whether there is one or more additional pages of search results available.

    \sa nextPage()
*/

/*!
    \qmlproperty enum PlaceSearchModel::status

    This property holds the status of the model.  It can be one of:

    \table
        \row
            \li PlaceSearchModel.Null
            \li No search query has been executed.  The model is empty.
        \row
            \li PlaceSearchModel.Ready
            \li The search query has completed, and the results are available.
        \row
            \li PlaceSearchModel.Loading
            \li A search query is currently being executed.
        \row
            \li PlaceSearchModel.Error
            \li An error occurred when executing the previous search query.
    \endtable
*/

/*!
    \qmlmethod void PlaceSearchModel::update()

    Updates the model based on the provided query parameters.  The model will be populated with a
    list of places matching the search parameters specified by the type's properties.  Search
    criteria is specified by setting properties such as the \l searchTerm, \l categories, \l searchArea and \l limit.
    Support for these properties may vary according to \l plugin.  \c update() then
    submits the set of criteria to the \l plugin to process.

    While the model is updating the \l status of the model is set to
    \c PlaceSearchModel.Loading.  If the model is successfully updated the \l status is set to
    \c PlaceSearchModel.Ready, while if it unsuccessfully completes, the \l status is set to
    \c PlaceSearchModel.Error and the model cleared.

    \code
    PlaceSearchModel {
        id: model
        plugin: backendPlugin
        searchArea: QtPositioning.circle(QtPositioning.coordinate(10, 10))
        ...
    }

    MouseArea {
        ...
        onClicked: {
            model.searchTerm = "pizza";
            model.categories = null;  //not searching by any category
            model.searchArea.center.latitude = -27.5;
            model.searchArea.center.longitude = 153;
            model.update();
        }
    }
    \endcode

    \sa cancel(), status
*/

/*!
    \qmlmethod void PlaceSearchModel::cancel()

    Cancels an ongoing search operation immediately and sets the model
    status to PlaceSearchModel.Ready.  The model retains any search
    results it had before the operation was started.

    If an operation is not ongoing, invoking cancel() has no effect.

    \sa update(), status
*/

/*!
    \qmlmethod void PlaceSearchModel::reset()

    Resets the model.  All search results are cleared, any outstanding requests are aborted and
    possible errors are cleared.  Model status will be set to PlaceSearchModel.Null.
*/

/*!
    \qmlmethod string PlaceSearchModel::errorString() const

    This read-only property holds the textual presentation of the latest place search model error.
    If no error has occurred or if the model was cleared, an empty string is returned.

    An empty string may also be returned if an error occurred which has no associated
    textual representation.
*/

/*!
    \qmlmethod void PlaceSearchModel::previousPage()

    Updates the model to display the previous page of search results. If there is no previous page
    then this method does nothing.
*/

/*!
    \qmlmethod void PlaceSearchModel::nextPage()

    Updates the model to display the next page of search results. If there is no next page then
    this method does nothing.
*/

QDeclarativeSearchResultModel::QDeclarativeSearchResultModel(QObject *parent)
    :   QDeclarativeSearchModelBase(parent), m_favoritesPlugin(0)
{
}

QDeclarativeSearchResultModel::~QDeclarativeSearchResultModel()
{
}

/*!
    \qmlproperty string PlaceSearchModel::searchTerm

    This property holds search term used in query.  The search term is a free-form text string.
*/
QString QDeclarativeSearchResultModel::searchTerm() const
{
    return m_request.searchTerm();
}

void QDeclarativeSearchResultModel::setSearchTerm(const QString &searchTerm)
{
    m_request.setSearchContext(QVariant());

    if (m_request.searchTerm() == searchTerm)
        return;

    m_request.setSearchTerm(searchTerm);
    emit searchTermChanged();
}

/*!
    \qmlproperty list<Category> PlaceSearchModel::categories

    This property holds a list of categories to be used when searching.  Returned search results
    will be for places that match at least one of the categories.
*/
QQmlListProperty<QDeclarativeCategory> QDeclarativeSearchResultModel::categories()
{
    return QQmlListProperty<QDeclarativeCategory>(this,
                                                          0, // opaque data parameter
                                                          categories_append,
                                                          categories_count,
                                                          category_at,
                                                          categories_clear);
}

void QDeclarativeSearchResultModel::categories_append(QQmlListProperty<QDeclarativeCategory> *list,
                                                      QDeclarativeCategory *declCategory)
{
    QDeclarativeSearchResultModel *searchModel = qobject_cast<QDeclarativeSearchResultModel *>(list->object);
    if (searchModel && declCategory) {
        searchModel->m_request.setSearchContext(QVariant());
        searchModel->m_categories.append(declCategory);
        QList<QPlaceCategory> categories = searchModel->m_request.categories();
        categories.append(declCategory->category());
        searchModel->m_request.setCategories(categories);
        emit searchModel->categoriesChanged();
    }
}

int QDeclarativeSearchResultModel::categories_count(QQmlListProperty<QDeclarativeCategory> *list)
{
    QDeclarativeSearchResultModel *searchModel = qobject_cast<QDeclarativeSearchResultModel *>(list->object);
    if (searchModel)
        return searchModel->m_categories.count();
    else
        return -1;
}

QDeclarativeCategory *QDeclarativeSearchResultModel::category_at(QQmlListProperty<QDeclarativeCategory> *list,
                                                                          int index)
{
    QDeclarativeSearchResultModel *searchModel = qobject_cast<QDeclarativeSearchResultModel *>(list->object);
    if (searchModel && (searchModel->m_categories.count() > index) && (index > -1))
        return searchModel->m_categories.at(index);
    else
        return 0;
}

void QDeclarativeSearchResultModel::categories_clear(QQmlListProperty<QDeclarativeCategory> *list)
{
    QDeclarativeSearchResultModel *searchModel = qobject_cast<QDeclarativeSearchResultModel *>(list->object);
    if (searchModel) {
        //note: we do not need to delete each of the objects in m_categories since the search model
        //should never be the parent of the categories anyway.
        searchModel->m_request.setSearchContext(QVariant());
        searchModel->m_categories.clear();
        searchModel->m_request.setCategories(QList<QPlaceCategory>());
        emit searchModel->categoriesChanged();
    }
}

/*!
    \qmlproperty string PlaceSearchModel::recommendationId

    This property holds the placeId to be used in order to find recommendations
    for similar places.
*/
QString QDeclarativeSearchResultModel::recommendationId() const
{
    return m_request.recommendationId();
}

void QDeclarativeSearchResultModel::setRecommendationId(const QString &placeId)
{
    if (m_request.recommendationId() == placeId)
        return;

    m_request.setRecommendationId(placeId);
    emit recommendationIdChanged();
}

/*!
    \qmlproperty enumeration PlaceSearchModel::relevanceHint

    This property holds a relevance hint used in the search query.  The hint is given to the
    provider to help but not dictate the ranking of results.  For example, the distance hint may
    give closer places a higher ranking but it does not necessarily mean the results will be
    strictly ordered according to distance. A provider may ignore the hint altogether.

    \table
        \row
            \li SearchResultModel.UnspecifiedHint
            \li No relevance hint is given to the provider.
        \row
            \li SearchResultModel.DistanceHint
            \li The distance of the place from the user's current location is important to the user.
               This hint is only meaningful when a circular search area is used.
        \row
            \li SearchResultModel.LexicalPlaceNameHint
            \li The lexical ordering of place names (in ascending alphabetical order) is relevant to
               the user.  This hint is useful for providers based on a local data store.
    \endtable
*/
QDeclarativeSearchResultModel::RelevanceHint QDeclarativeSearchResultModel::relevanceHint() const
{
    return static_cast<QDeclarativeSearchResultModel::RelevanceHint>(m_request.relevanceHint());
}

void QDeclarativeSearchResultModel::setRelevanceHint(QDeclarativeSearchResultModel::RelevanceHint hint)
{
    if (m_request.relevanceHint() != static_cast<QPlaceSearchRequest::RelevanceHint>(hint)) {
        m_request.setRelevanceHint(static_cast<QPlaceSearchRequest::RelevanceHint>(hint));
        emit relevanceHintChanged();
    }
}

/*!
    \qmlproperty enum PlaceSearchModel::visibilityScope

    This property holds the visibility scope of the places to search.  Only places with the
    specified visibility will be returned in the search results.

    The visibility scope can be one of:

    \table
        \row
            \li Place.UnspecifiedVisibility
            \li No explicit visibility scope specified, places with any visibility may be part of
               search results.
        \row
            \li Place.DeviceVisibility
            \li Only places stored on the local device will be part of the search results.
        \row
            \li Place.PrivateVisibility
            \li Only places that are private to the current user will be part of the search results.
        \row
            \li Place.PublicVisibility
            \li Only places that are public will be part of the search results.
    \endtable
*/
QDeclarativePlace::Visibility QDeclarativeSearchResultModel::visibilityScope() const
{
    return QDeclarativePlace::Visibility(int(m_visibilityScope));
}

void QDeclarativeSearchResultModel::setVisibilityScope(QDeclarativePlace::Visibility visibilityScope)
{
    QLocation::VisibilityScope scope = QLocation::VisibilityScope(visibilityScope);

    if (m_visibilityScope == scope)
        return;

    m_visibilityScope = scope;
    emit visibilityScopeChanged();
}

/*!
    \internal
*/
QDeclarativeGeoServiceProvider *QDeclarativeSearchResultModel::favoritesPlugin() const
{
    return m_favoritesPlugin;
}

/*!
    \internal
*/
void QDeclarativeSearchResultModel::setFavoritesPlugin(QDeclarativeGeoServiceProvider *plugin)
{

    if (m_favoritesPlugin == plugin)
        return;

    m_favoritesPlugin = plugin;

    if (m_favoritesPlugin) {
        QGeoServiceProvider *serviceProvider = m_favoritesPlugin->sharedGeoServiceProvider();
        if (serviceProvider) {
            QPlaceManager *placeManager = serviceProvider->placeManager();
            if (placeManager) {
                if (placeManager->childCategoryIds().isEmpty()) {
                    QPlaceReply *reply = placeManager->initializeCategories();
                    connect(reply, SIGNAL(finished()), reply, SLOT(deleteLater()));
                }
            }
        }
    }

    emit favoritesPluginChanged();
}

/*!
    \internal
*/
QVariantMap QDeclarativeSearchResultModel::favoritesMatchParameters() const
{
    return m_matchParameters;
}

/*!
    \internal
*/
void QDeclarativeSearchResultModel::setFavoritesMatchParameters(const QVariantMap &parameters)
{
    if (m_matchParameters == parameters)
        return;

    m_matchParameters = parameters;
    emit favoritesMatchParametersChanged();
}

/*!
    \internal
*/
int QDeclarativeSearchResultModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return m_results.count();
}

void QDeclarativeSearchResultModel::clearData(bool suppressSignal)
{
    QDeclarativeSearchModelBase::clearData(suppressSignal);

    qDeleteAll(m_places);
    m_places.clear();
    qDeleteAll(m_icons);
    m_icons.clear();
    if (!m_results.isEmpty()) {
        m_results.clear();

        if (!suppressSignal)
            emit rowCountChanged();
    }
}

QVariant QDeclarativeSearchResultModel::data(const QModelIndex &index, int role) const
{
    if (index.row() > m_results.count())
        return QVariant();

    const QPlaceSearchResult &result = m_results.at(index.row());

    switch (role) {
    case SearchResultTypeRole:
        return result.type();
    case Qt::DisplayRole:
    case TitleRole:
        return result.title();
    case IconRole:
        return QVariant::fromValue(static_cast<QObject *>(m_icons.at(index.row())));
    case DistanceRole:
        if (result.type() == QPlaceSearchResult::PlaceResult) {
            QPlaceResult placeResult = result;
            return placeResult.distance();
        }
        break;
    case PlaceRole:
        if (result.type() == QPlaceSearchResult::PlaceResult)
            return QVariant::fromValue(static_cast<QObject *>(m_places.at(index.row())));
    case SponsoredRole:
        if (result.type() == QPlaceSearchResult::PlaceResult) {
            QPlaceResult placeResult = result;
            return placeResult.isSponsored();
        }
        break;
    }
    return QVariant();
}

/*!
    \internal
*/
QVariant QDeclarativeSearchResultModel::data(int index, const QString &role) const
{
    QModelIndex modelIndex = createIndex(index, 0);
    return data(modelIndex, roleNames().key(role.toLatin1()));
}

QHash<int, QByteArray> QDeclarativeSearchResultModel::roleNames() const
{
    QHash<int, QByteArray> roles = QDeclarativeSearchModelBase::roleNames();
    roles.insert(SearchResultTypeRole, "type");
    roles.insert(TitleRole, "title");
    roles.insert(IconRole, "icon");
    roles.insert(DistanceRole, "distance");
    roles.insert(PlaceRole, "place");
    roles.insert(SponsoredRole, "sponsored");

    return roles;
}

/*!
    \qmlmethod void PlaceSearchModel::updateWith(int proposedSearchIndex)

    Updates the model based on the ProposedSearchResult at index \a proposedSearchIndex. The model
    will be populated with a list of places matching the proposed search. Model status will be set
    to PlaceSearchModel.Loading. If the model is updated successfully status will be set to
    PlaceSearchModel.Ready. If an error occurs status will be set to PlaceSearchModel.Error and the
    model cleared.

    If \a proposedSearchIndex does not reference a ProposedSearchResult this method does nothing.
*/
void QDeclarativeSearchResultModel::updateWith(int proposedSearchIndex)
{
    if (m_results.at(proposedSearchIndex).type() != QPlaceSearchResult::ProposedSearchResult)
        return;

    m_request = QPlaceProposedSearchResult(m_results.at(proposedSearchIndex)).searchRequest();
    update();
}

QPlaceReply *QDeclarativeSearchResultModel::sendQuery(QPlaceManager *manager,
                                                      const QPlaceSearchRequest &request)
{
    Q_ASSERT(manager);
    return manager->search(request);
}

/*!
    \internal
*/
void QDeclarativeSearchResultModel::initializePlugin(QDeclarativeGeoServiceProvider *plugin)
{
    //disconnect the manager of the old plugin if we have one
    if (m_plugin) {
        QGeoServiceProvider *serviceProvider = m_plugin->sharedGeoServiceProvider();
        if (serviceProvider) {
            QPlaceManager *placeManager = serviceProvider->placeManager();
            if (placeManager) {
                disconnect(placeManager, SIGNAL(placeUpdated(QString)), this, SLOT(placeUpdated(QString)));
                disconnect(placeManager, SIGNAL(placeRemoved(QString)), this, SLOT(placeRemoved(QString)));
                connect(placeManager, SIGNAL(dataChanged()), this, SIGNAL(dataChanged()));
            }
        }
    }

    //connect to the manager of the new plugin.
    if (plugin) {
        QGeoServiceProvider *serviceProvider = plugin->sharedGeoServiceProvider();
        if (serviceProvider) {
            QPlaceManager *placeManager = serviceProvider->placeManager();
            if (placeManager) {
                connect(placeManager, SIGNAL(placeUpdated(QString)), this, SLOT(placeUpdated(QString)));
                connect(placeManager, SIGNAL(placeRemoved(QString)), this, SLOT(placeRemoved(QString)));
                disconnect(placeManager, SIGNAL(dataChanged()), this, SIGNAL(dataChanged()));
            }
        }
    }
    QDeclarativeSearchModelBase::initializePlugin(plugin);
}

/*!
    \internal
*/
void QDeclarativeSearchResultModel::queryFinished()
{
    if (!m_reply)
        return;
    QPlaceReply *reply = m_reply;
    m_reply = 0;
    if (reply->error() != QPlaceReply::NoError) {
        m_resultsBuffer.clear();
        updateLayout();
        setStatus(Error, reply->errorString());
        reply->deleteLater();
        return;
    }

    if (reply->type() == QPlaceReply::SearchReply) {
        QPlaceSearchReply *searchReply = qobject_cast<QPlaceSearchReply *>(reply);
        Q_ASSERT(searchReply);

        m_resultsBuffer = searchReply->results();
        setPreviousPageRequest(searchReply->previousPageRequest());
        setNextPageRequest(searchReply->nextPageRequest());

        reply->deleteLater();

        if (!m_favoritesPlugin) {
            updateLayout();
            setStatus(Ready);
        } else {
            QGeoServiceProvider *serviceProvider = m_favoritesPlugin->sharedGeoServiceProvider();
            if (!serviceProvider) {
                updateLayout();
                setStatus(Error, QStringLiteral("Favorites plugin returns a null QGeoServiceProvider instance"));
                return;
            }

            QPlaceManager *favoritesManager = serviceProvider->placeManager();
            if (!favoritesManager) {
                updateLayout();
                setStatus(Error, QStringLiteral("Favorites plugin returns a null QPlaceManager"));
                return;
            }

            QPlaceMatchRequest request;
            if (m_matchParameters.isEmpty()) {
                if (!m_plugin) {
                    reply->deleteLater();
                    setStatus(Error, QStringLiteral("Plugin not assigned"));
                    return;
                }

                QVariantMap params;
                params.insert(QPlaceMatchRequest::AlternativeId, QString::fromLatin1("x_id_") + m_plugin->name());
                request.setParameters(params);
            } else {
                request.setParameters(m_matchParameters);
            }

            request.setResults(m_resultsBuffer);
            m_reply = favoritesManager->matchingPlaces(request);
            connect(m_reply, SIGNAL(finished()), this, SLOT(queryFinished()));
        }
    } else if (reply->type() == QPlaceReply::MatchReply) {
        QPlaceMatchReply *matchReply = qobject_cast<QPlaceMatchReply *>(reply);
        Q_ASSERT(matchReply);
        updateLayout(matchReply->places());
        setStatus(Ready);
        reply->deleteLater();
    } else {
        setStatus(Error, QStringLiteral("Unknown reply type"));
        reply->deleteLater();
    }
}

/*!
    \qmlmethod void PlaceSearchModel::data(int index, string role)

    Returns the data for a given \a role at the specified row \a index.
*/

/*!
    \qmlproperty int PlaceSearchModel::count

    This property holds the number of results the model has.

    Note that it does not refer to the total number of search results
    available in the backend.  The total number of search results
    is not currently supported by the API.
*/

/*!
    \internal
    Note: m_results buffer should be correctly populated before
    calling this function
*/
void QDeclarativeSearchResultModel::updateLayout(const QList<QPlace> &favoritePlaces)
{
    int oldRowCount = rowCount();

    beginResetModel();
    clearData(true);
    m_results = m_resultsBuffer;
    m_resultsBuffer.clear();

    for (int i = 0; i < m_results.count(); ++i) {
        const QPlaceSearchResult &result = m_results.at(i);

        if (result.type() == QPlaceSearchResult::PlaceResult) {
            QPlaceResult placeResult = result;
            QDeclarativePlace *place = new QDeclarativePlace(placeResult.place(), plugin(), this);
            m_places.append(place);

            if ((favoritePlaces.count() == m_results.count()) && favoritePlaces.at(i) != QPlace())
                m_places[i]->setFavorite(new QDeclarativePlace(favoritePlaces.at(i),
                                                               m_favoritesPlugin, m_places[i]));
        } else if (result.type() == QPlaceSearchResult::ProposedSearchResult) {
            m_places.append(0);
        }

        QDeclarativePlaceIcon *icon = 0;
        if (!result.icon().isEmpty())
            icon = new QDeclarativePlaceIcon(result.icon(), plugin(), this);
        m_icons.append(icon);
    }

    endResetModel();
    if (m_results.count() != oldRowCount)
        emit rowCountChanged();
}

/*!
    \internal
*/
void QDeclarativeSearchResultModel::placeUpdated(const QString &placeId)
{
    int row = getRow(placeId);
    if (row < 0 || row > m_places.count())
        return;

    if (m_places.at(row))
        m_places.at(row)->getDetails();
}

/*!
    \internal
*/
void QDeclarativeSearchResultModel::placeRemoved(const QString &placeId)
{
    int row = getRow(placeId);
    if (row < 0 || row > m_places.count())
        return;

    beginRemoveRows(QModelIndex(), row, row);
    delete m_places.at(row);
    m_places.removeAt(row);
    m_results.removeAt(row);
    endRemoveRows();

    emit rowCountChanged();
}

/*!
    \internal
*/
int QDeclarativeSearchResultModel::getRow(const QString &placeId) const
{
    for (int i = 0; i < m_places.count(); ++i) {
        if (!m_places.at(i))
            continue;
        else if (m_places.at(i)->placeId() == placeId)
            return i;
    }

    return -1;
}

/*!
    \qmlsignal PlaceSearchResultModel::dataChanged()

   This signal is emitted when significant changes have been made to the underlying datastore.

   Applications should act on this signal at their own discretion.  The data
   provided by the model could be out of date and so the model should be reupdated
   sometime, however an immediate reupdate may be disconcerting to users if the results
   change without any action on their part.

   The corresponding handler is \c onDataChanged.
*/

