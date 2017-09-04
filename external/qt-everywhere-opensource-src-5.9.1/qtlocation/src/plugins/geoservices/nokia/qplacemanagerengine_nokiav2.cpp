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

#include "qplacemanagerengine_nokiav2.h"

#include "placesv2/qplacecategoriesreplyhere.h"
#include "placesv2/qplacecontentreplyimpl.h"
#include "placesv2/qplacesearchsuggestionreplyimpl.h"
#include "placesv2/qplacesearchreplyhere.h"
#include "placesv2/qplacedetailsreplyimpl.h"
#include "placesv2/qplaceidreplyimpl.h"
#include "qgeonetworkaccessmanager.h"
#include "qgeouriprovider.h"
#include "uri_constants.h"
#include "qgeoerror_messages.h"

#include <QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QNetworkProxyFactory>

#include <QtLocation/QPlaceContentRequest>
#include <QtPositioning/QGeoCircle>

QT_BEGIN_NAMESPACE

static const char FIXED_CATEGORIES_string[] =
    "eat-drink\0"
    "going-out\0"
    "sights-museums\0"
    "transport\0"
    "accommodation\0"
    "shopping\0"
    "leisure-outdoor\0"
    "administrative-areas-buildings\0"
    "natural-geographical\0"
    "petrol-station\0"
    "atm-bank-exchange\0"
    "toilet-rest-area\0"
    "hospital-health-care-facility\0"
    "eat-drink|restaurant\0" // subcategories always start after relative parent category
    "eat-drink|coffee-tea\0"
    "eat-drink|snacks-fast-food\0"
    "transport|airport"
    "\0";

static const int FIXED_CATEGORIES_indices[] = {
       0,   10,   20,   35,   45,   59,   68,   84,
     115,  136,  151,  169,  186,  216,  237,  258,
     285,   -1
};

static const char * const NokiaIcon = "nokiaIcon";
static const char * const IconPrefix = "iconPrefix";
static const char * const NokiaIconGenerated = "nokiaIconGenerated";

static const char * const IconThemeKey = "places.icons.theme";
static const char * const LocalDataPathKey = "places.local_data_path";

class CategoryParser
{
public:
    CategoryParser();
    bool parse(const QString &fileName);

    QPlaceCategoryTree tree() const { return m_tree; }
    QHash<QString, QUrl> restIdToIconHash() const { return m_restIdToIconHash; }

    QString errorString() const;

private:
    void processCategory(int level, const QString &id,
                         const QString &parentId = QString());

    QJsonObject m_exploreObject;
    QPlaceCategoryTree m_tree;
    QString m_errorString;

    QHash<QString, QUrl> m_restIdToIconHash;
};

CategoryParser::CategoryParser()
{
}

bool CategoryParser::parse(const QString &fileName)
{
    m_exploreObject = QJsonObject();
    m_tree.clear();
    m_errorString.clear();

    QFile mappingFile(fileName);

    if (mappingFile.open(QIODevice::ReadOnly)) {
        QJsonDocument document = QJsonDocument::fromJson(mappingFile.readAll());
        if (document.isObject()) {
            QJsonObject docObject = document.object();
            if (docObject.contains(QStringLiteral("offline_explore"))) {
                m_exploreObject = docObject.value(QStringLiteral("offline_explore"))
                                                .toObject();
                if (m_exploreObject.contains(QStringLiteral("ROOT"))) {
                    processCategory(0, QString());
                    return true;
                }
            } else {
                m_errorString = fileName
                    + QStringLiteral("does not contain the offline_explore property");
                return false;
            }
        } else {
            m_errorString = fileName + QStringLiteral("is not an json object");
            return false;
        }
    }
    m_errorString = QString::fromLatin1("Unable to open ") + fileName;
    return false;
}

void CategoryParser::processCategory(int level, const QString &id, const QString &parentId)
{
    //We are basing the tree on a DAG  from the input file, however we are simplyfing
    //this into a 2 level tree, and a given category only has one parent
    //
    // A->B->Z
    // A->C->Z
    // Z in this case is not in the tree because it is 3 levels deep.
    //
    // X->Z
    // Y->Z
    // Only one of these is shown in the tree since Z can only have one parent
    // the choice made between X and Y is arbitrary.
    const int maxLevel = 2;
    PlaceCategoryNode node;
    node.category.setCategoryId(id);
    node.parentId = parentId;

    m_tree.insert(node.category.categoryId(), node);
    //this is simply to mark the node as being visited.
    //a proper assignment to the tree happens at the end of function

    QJsonObject categoryJson = m_exploreObject.value(id.isEmpty()
                                                     ? QStringLiteral("ROOT") : id).toObject();
    QJsonArray children = categoryJson.value(QStringLiteral("children")).toArray();

    if (level + 1 <= maxLevel && !categoryJson.contains(QStringLiteral("final"))) {
        for (int i = 0; i < children.count(); ++i)  {
            QString childId = children.at(i).toString();
            if (!m_tree.contains(childId)) {
                node.childIds.append(childId);
                processCategory(level + 1, childId, id);
            }
        }
    }

    m_tree.insert(node.category.categoryId(), node);
}

QPlaceManagerEngineNokiaV2::QPlaceManagerEngineNokiaV2(
    QGeoNetworkAccessManager *networkManager,
    const QVariantMap &parameters,
    QGeoServiceProvider::Error *error,
    QString *errorString)
    : QPlaceManagerEngine(parameters)
    , m_manager(networkManager)
    , m_uriProvider(new QGeoUriProvider(this, parameters, QStringLiteral("here.places.host"), PLACES_HOST))
{
    Q_ASSERT(networkManager);
    m_manager->setParent(this);

    m_locales.append(QLocale());

    m_appId = parameters.value(QStringLiteral("here.app_id")).toString();
    m_appCode = parameters.value(QStringLiteral("here.token")).toString();

    m_theme = parameters.value(IconThemeKey, QString()).toString();

    if (m_theme == QStringLiteral("default"))
        m_theme.clear();

    m_localDataPath = parameters.value(LocalDataPathKey, QString()).toString();
    if (m_localDataPath.isEmpty()) {
        QStringList dataLocations = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);

        if (!dataLocations.isEmpty() && !dataLocations.first().isEmpty()) {
            m_localDataPath = dataLocations.first()
                                + QStringLiteral("/here/qtlocation/data");
        }
    }

    if (error)
        *error = QGeoServiceProvider::NoError;

    if (errorString)
        errorString->clear();
}

QPlaceManagerEngineNokiaV2::~QPlaceManagerEngineNokiaV2() {}

QPlaceDetailsReply *QPlaceManagerEngineNokiaV2::getPlaceDetails(const QString &placeId)
{
    QUrl requestUrl(QString::fromLatin1("http://") + m_uriProvider->getCurrentHost() +
                    QStringLiteral("/places/v1/places/") + placeId);

    QUrlQuery queryItems;

    queryItems.addQueryItem(QStringLiteral("tf"), QStringLiteral("html"));
    //queryItems.append(qMakePair<QString, QString>(QStringLiteral("size"), QString::number(5)));
    //queryItems.append(qMakePair<QString, QString>(QStringLiteral("image_dimensions"), QStringLiteral("w64-h64,w100")));

    requestUrl.setQuery(queryItems);

    QNetworkReply *networkReply = sendRequest(requestUrl);

    QPlaceDetailsReplyImpl *reply = new QPlaceDetailsReplyImpl(networkReply, this);
    reply->setPlaceId(placeId);
    connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
            this, SLOT(replyError(QPlaceReply::Error,QString)));

    return reply;
}

QPlaceContentReply *QPlaceManagerEngineNokiaV2::getPlaceContent(const QPlaceContentRequest &request)
{
    QNetworkReply *networkReply = 0;

    if (request.contentContext().userType() == qMetaTypeId<QUrl>()) {
        QUrl u = request.contentContext().value<QUrl>();

       networkReply = sendRequest(u);
    } else {
        QUrl requestUrl(QString::fromLatin1("http://") + m_uriProvider->getCurrentHost() +
                        QStringLiteral("/places/v1/places/") + request.placeId() +
                        QStringLiteral("/media/"));

        QUrlQuery queryItems;

        switch (request.contentType()) {
        case QPlaceContent::ImageType:
            requestUrl.setPath(requestUrl.path() + QStringLiteral("images"));

            queryItems.addQueryItem(QStringLiteral("tf"), QStringLiteral("html"));

            if (request.limit() > 0)
                queryItems.addQueryItem(QStringLiteral("size"), QString::number(request.limit()));

            //queryItems.append(qMakePair<QString, QString>(QStringLiteral("image_dimensions"), QStringLiteral("w64-h64,w100")));

            requestUrl.setQuery(queryItems);

            networkReply = sendRequest(requestUrl);
            break;
        case QPlaceContent::ReviewType:
            requestUrl.setPath(requestUrl.path() + QStringLiteral("reviews"));

            queryItems.addQueryItem(QStringLiteral("tf"), QStringLiteral("html"));

            if (request.limit() > 0)
                queryItems.addQueryItem(QStringLiteral("size"), QString::number(request.limit()));

            requestUrl.setQuery(queryItems);

            networkReply = sendRequest(requestUrl);
            break;
        case QPlaceContent::EditorialType:
            requestUrl.setPath(requestUrl.path() + QStringLiteral("editorials"));

            queryItems.addQueryItem(QStringLiteral("tf"), QStringLiteral("html"));

            if (request.limit() > 0)
                queryItems.addQueryItem(QStringLiteral("size"), QString::number(request.limit()));

            requestUrl.setQuery(queryItems);

            networkReply = sendRequest(requestUrl);
            break;
        case QPlaceContent::NoType:
            ;
        }
    }

    QPlaceContentReply *reply = new QPlaceContentReplyImpl(request, networkReply, this);
    connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
            this, SLOT(replyError(QPlaceReply::Error,QString)));

    if (!networkReply) {
        QMetaObject::invokeMethod(reply, "setError", Qt::QueuedConnection,
                                  Q_ARG(QPlaceReply::Error, QPlaceReply::UnsupportedError),
                                  Q_ARG(QString, QString("Retrieval of given content type not supported.")));
    }

    return reply;
}

static bool addAtForBoundingArea(const QGeoShape &area,
                                 QUrlQuery *queryItems)
{
    QGeoCoordinate center = area.center();
    if (!center.isValid())
        return false;

    queryItems->addQueryItem(QStringLiteral("at"),
                             QString::number(center.latitude()) +
                             QLatin1Char(',') +
                             QString::number(center.longitude()));
    return true;
}

QPlaceSearchReply *QPlaceManagerEngineNokiaV2::search(const QPlaceSearchRequest &query)
{
    bool unsupported = false;

    unsupported |= query.visibilityScope() != QLocation::UnspecifiedVisibility &&
                   query.visibilityScope() != QLocation::PublicVisibility;

    // Both a search term and search categories are not supported.
    unsupported |= !query.searchTerm().isEmpty() && !query.categories().isEmpty();

    //only a recommendation id by itself is supported.
    unsupported |= !query.recommendationId().isEmpty()
                   && (!query.searchTerm().isEmpty() || !query.categories().isEmpty()
                       || query.searchArea().type() != QGeoShape::UnknownType);

    if (unsupported) {
        QPlaceSearchReplyHere *reply = new QPlaceSearchReplyHere(query, 0, this);
        connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
        connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
                this, SLOT(replyError(QPlaceReply::Error,QString)));
        QMetaObject::invokeMethod(reply, "setError", Qt::QueuedConnection,
                                  Q_ARG(QPlaceReply::Error, QPlaceReply::BadArgumentError),
                                  Q_ARG(QString, "Unsupported search request options specified."));
        return reply;
    }

    QUrlQuery queryItems;

    // Check that the search area is valid for all searches except recommendation and proposed
    // searches, which do not need search centers.
    if (query.recommendationId().isEmpty() && !query.searchContext().isValid()) {
        if (!addAtForBoundingArea(query.searchArea(), &queryItems)) {
            QPlaceSearchReplyHere *reply = new QPlaceSearchReplyHere(query, 0, this);
            connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
            connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
                    this, SLOT(replyError(QPlaceReply::Error,QString)));
            QMetaObject::invokeMethod(reply, "setError", Qt::QueuedConnection,
                                      Q_ARG(QPlaceReply::Error, QPlaceReply::BadArgumentError),
                                      Q_ARG(QString, "Invalid search area provided"));
            return reply;
        }
    }

    QNetworkReply *networkReply = 0;

    if (query.searchContext().userType() == qMetaTypeId<QUrl>()) {
        // provided search context
        QUrl u = query.searchContext().value<QUrl>();

        typedef QPair<QString, QString> QueryItem;
        QList<QueryItem> queryItemList = queryItems.queryItems(QUrl::FullyEncoded);
        queryItems = QUrlQuery(u);
        foreach (const QueryItem &item, queryItemList)
            queryItems.addQueryItem(item.first, item.second);

        if (query.limit() > 0)
            queryItems.addQueryItem(QStringLiteral("size"), QString::number(query.limit()));

        u.setQuery(queryItems);

        networkReply = sendRequest(u);
    } else if (!query.searchTerm().isEmpty()) {
        // search term query
        QUrl requestUrl(QString::fromLatin1("http://") + m_uriProvider->getCurrentHost() +
                        QStringLiteral("/places/v1/discover/search"));

        queryItems.addQueryItem(QStringLiteral("q"), query.searchTerm());
        queryItems.addQueryItem(QStringLiteral("tf"), QStringLiteral("html"));

        if (query.limit() > 0) {
            queryItems.addQueryItem(QStringLiteral("size"),
                                    QString::number(query.limit()));
        }

        requestUrl.setQuery(queryItems);

        QNetworkReply *networkReply = sendRequest(requestUrl);

        QPlaceSearchReplyHere *reply = new QPlaceSearchReplyHere(query, networkReply, this);
        connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
        connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
                this, SLOT(replyError(QPlaceReply::Error,QString)));

        return reply;
    } else if (!query.recommendationId().isEmpty()) {
        QUrl requestUrl(QString::fromLatin1("http://") + m_uriProvider->getCurrentHost() +
                        QStringLiteral("/places/v1/places/") + query.recommendationId() +
                        QStringLiteral("/related/recommended"));

        queryItems.addQueryItem(QStringLiteral("tf"), QStringLiteral("html"));

        requestUrl.setQuery(queryItems);

        networkReply = sendRequest(requestUrl);
    } else {
        // category search
        QUrl requestUrl(QStringLiteral("http://") + m_uriProvider->getCurrentHost() +
             QStringLiteral("/places/v1/discover/explore"));

        QStringList ids;
        foreach (const QPlaceCategory &category, query.categories())
            ids.append(category.categoryId());

        QUrlQuery queryItems;

        if (!ids.isEmpty())
            queryItems.addQueryItem(QStringLiteral("cat"), ids.join(QStringLiteral(",")));

        addAtForBoundingArea(query.searchArea(), &queryItems);

        queryItems.addQueryItem(QStringLiteral("tf"), QStringLiteral("html"));

        if (query.limit() > 0) {
            queryItems.addQueryItem(QStringLiteral("size"),
                                    QString::number(query.limit()));
        }

        requestUrl.setQuery(queryItems);

        networkReply = sendRequest(requestUrl);
    }

    QPlaceSearchReplyHere *reply = new QPlaceSearchReplyHere(query, networkReply, this);
    connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
            this, SLOT(replyError(QPlaceReply::Error,QString)));

    return reply;
}

QPlaceSearchSuggestionReply *QPlaceManagerEngineNokiaV2::searchSuggestions(const QPlaceSearchRequest &query)
{
    bool unsupported = false;

    unsupported |= query.visibilityScope() != QLocation::UnspecifiedVisibility &&
                   query.visibilityScope() != QLocation::PublicVisibility;

    unsupported |= !query.categories().isEmpty();
    unsupported |= !query.recommendationId().isEmpty();

    if (unsupported) {
        QPlaceSearchSuggestionReplyImpl *reply = new QPlaceSearchSuggestionReplyImpl(0, this);
        connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
        connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
                this, SLOT(replyError(QPlaceReply::Error,QString)));
        QMetaObject::invokeMethod(reply, "setError", Qt::QueuedConnection,
                                  Q_ARG(QPlaceReply::Error, QPlaceReply::BadArgumentError),
                                  Q_ARG(QString, "Unsupported search request options specified."));
        return reply;
    }

    QUrl requestUrl(QString::fromLatin1("http://") + m_uriProvider->getCurrentHost() +
                    QStringLiteral("/places/v1/suggest"));

    QUrlQuery queryItems;

    queryItems.addQueryItem(QStringLiteral("q"), query.searchTerm());

    if (!addAtForBoundingArea(query.searchArea(), &queryItems)) {
        QPlaceSearchSuggestionReplyImpl *reply = new QPlaceSearchSuggestionReplyImpl(0, this);
        connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
        connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
                this, SLOT(replyError(QPlaceReply::Error,QString)));
        QMetaObject::invokeMethod(reply, "setError", Qt::QueuedConnection,
                                  Q_ARG(QPlaceReply::Error, QPlaceReply::BadArgumentError),
                                  Q_ARG(QString, "Invalid search area provided"));
        return reply;
    }

    requestUrl.setQuery(queryItems);

    QNetworkReply *networkReply = sendRequest(requestUrl);

    QPlaceSearchSuggestionReplyImpl *reply = new QPlaceSearchSuggestionReplyImpl(networkReply, this);
    connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
            this, SLOT(replyError(QPlaceReply::Error,QString)));

    return reply;
}

QPlaceIdReply *QPlaceManagerEngineNokiaV2::savePlace(const QPlace &place)
{
    QPlaceIdReplyImpl *reply = new QPlaceIdReplyImpl(QPlaceIdReply::SavePlace, this);
    reply->setId(place.placeId());
    QMetaObject::invokeMethod(reply, "setError", Qt::QueuedConnection,
                              Q_ARG(QPlaceReply::Error, QPlaceReply::UnsupportedError),
                              Q_ARG(QString, QCoreApplication::translate(NOKIA_PLUGIN_CONTEXT_NAME, SAVING_PLACE_NOT_SUPPORTED)));
    connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
            this, SLOT(replyError(QPlaceReply::Error,QString)));
    return reply;
}

QPlaceIdReply *QPlaceManagerEngineNokiaV2::removePlace(const QString &placeId)
{
    QPlaceIdReplyImpl *reply = new QPlaceIdReplyImpl(QPlaceIdReply::RemovePlace, this);
    reply->setId(placeId);
    QMetaObject::invokeMethod(reply, "setError", Qt::QueuedConnection,
                              Q_ARG(QPlaceReply::Error, QPlaceReply::UnsupportedError),
                              Q_ARG(QString, QCoreApplication::translate(NOKIA_PLUGIN_CONTEXT_NAME, REMOVING_PLACE_NOT_SUPPORTED)));
    connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
            this, SLOT(replyError(QPlaceReply::Error,QString)));
    return reply;
}

QPlaceIdReply *QPlaceManagerEngineNokiaV2::saveCategory(const QPlaceCategory &category, const QString &parentId)
{
    Q_UNUSED(parentId)

    QPlaceIdReplyImpl *reply = new QPlaceIdReplyImpl(QPlaceIdReply::SaveCategory, this);
    reply->setId(category.categoryId());
    QMetaObject::invokeMethod(reply, "setError", Qt::QueuedConnection,
                              Q_ARG(QPlaceReply::Error, QPlaceReply::UnsupportedError),
                              Q_ARG(QString, QCoreApplication::translate(NOKIA_PLUGIN_CONTEXT_NAME, SAVING_CATEGORY_NOT_SUPPORTED)));
    connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
            this, SLOT(replyError(QPlaceReply::Error,QString)));
    return reply;
}

QPlaceIdReply *QPlaceManagerEngineNokiaV2::removeCategory(const QString &categoryId)
{
    QPlaceIdReplyImpl *reply = new QPlaceIdReplyImpl(QPlaceIdReply::RemoveCategory, this);
    reply->setId(categoryId);
    QMetaObject::invokeMethod(reply, "setError", Qt::QueuedConnection,
                              Q_ARG(QPlaceReply::Error, QPlaceReply::UnsupportedError),
                              Q_ARG(QString, QCoreApplication::translate(NOKIA_PLUGIN_CONTEXT_NAME, REMOVING_CATEGORY_NOT_SUPPORTED)));
    connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
            this, SLOT(replyError(QPlaceReply::Error,QString)));
    return reply;
}

QPlaceReply *QPlaceManagerEngineNokiaV2::initializeCategories()
{
    if (m_categoryReply)
        return m_categoryReply.data();

    m_tempTree.clear();
    CategoryParser parser;

    if (parser.parse(m_localDataPath + QStringLiteral("/offline/offline-mapping.json"))) {
        m_tempTree = parser.tree();
    } else {
        PlaceCategoryNode rootNode;

        for (int i = 0; FIXED_CATEGORIES_indices[i] != -1; ++i) {
            const QString id = QString::fromLatin1(FIXED_CATEGORIES_string +
                                                   FIXED_CATEGORIES_indices[i]);

            int subCatDivider = id.indexOf(QChar('|'));
            if (subCatDivider >= 0) {
                // found a sub category
                const QString subCategoryId = id.mid(subCatDivider+1);
                const QString parentCategoryId = id.left(subCatDivider);

                if (m_tempTree.contains(parentCategoryId)) {
                    PlaceCategoryNode node;
                    node.category.setCategoryId(subCategoryId);
                    node.parentId = parentCategoryId;

                    // find parent
                    PlaceCategoryNode &parent = m_tempTree[parentCategoryId];
                    parent.childIds.append(subCategoryId);
                    m_tempTree.insert(subCategoryId, node);
                }

            } else {
                PlaceCategoryNode node;
                node.category.setCategoryId(id);

                m_tempTree.insert(id, node);
                rootNode.childIds.append(id);
            }
        }

        m_tempTree.insert(QString(), rootNode);
    }

    //request all categories in the tree from the server
    //because we don't want the root node, we skip it
    for (auto it = m_tempTree.keyBegin(), end = m_tempTree.keyEnd(); it != end; ++it) {
        if (*it == QString())
            continue;
        QUrl requestUrl(QString::fromLatin1("http://") + m_uriProvider->getCurrentHost() +
                        QStringLiteral("/places/v1/categories/places/") + *it);
        QNetworkReply *networkReply = sendRequest(requestUrl);
        connect(networkReply, SIGNAL(finished()), this, SLOT(categoryReplyFinished()));
        connect(networkReply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(categoryReplyError()));

        m_categoryRequests.insert(*it, networkReply);
    }

    QPlaceCategoriesReplyHere *reply = new QPlaceCategoriesReplyHere(this);
    connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
            this, SLOT(replyError(QPlaceReply::Error,QString)));

    m_categoryReply = reply;
    return reply;
}

QString QPlaceManagerEngineNokiaV2::parentCategoryId(const QString &categoryId) const
{
    return m_categoryTree.value(categoryId).parentId;
}

QStringList QPlaceManagerEngineNokiaV2::childCategoryIds(const QString &categoryId) const
{
    return m_categoryTree.value(categoryId).childIds;
}

QPlaceCategory QPlaceManagerEngineNokiaV2::category(const QString &categoryId) const
{
    return m_categoryTree.value(categoryId).category;
}

QList<QPlaceCategory> QPlaceManagerEngineNokiaV2::childCategories(const QString &parentId) const
{
    QList<QPlaceCategory> results;
    foreach (const QString &childId, m_categoryTree.value(parentId).childIds)
        results.append(m_categoryTree.value(childId).category);
    return results;
}

QList<QLocale> QPlaceManagerEngineNokiaV2::locales() const
{
    return m_locales;
}

void QPlaceManagerEngineNokiaV2::setLocales(const QList<QLocale> &locales)
{
    m_locales = locales;
}

QPlaceIcon QPlaceManagerEngineNokiaV2::icon(const QString &remotePath,
                                            const QList<QPlaceCategory> &categories) const
{
    QPlaceIcon icon;
    QVariantMap params;

    QRegExp rx("(.*)(/icons/categories/.*)");

    QString iconPrefix;
    QString nokiaIcon;
    if (rx.indexIn(remotePath) != -1 && !rx.cap(1).isEmpty() && !rx.cap(2).isEmpty()) {
            iconPrefix = rx.cap(1);
            nokiaIcon = rx.cap(2);

        if (QFile::exists(m_localDataPath + nokiaIcon))
            iconPrefix = QString::fromLatin1("file://") + m_localDataPath;

        params.insert(NokiaIcon, nokiaIcon);
        params.insert(IconPrefix, iconPrefix);

        foreach (const QPlaceCategory &category, categories) {
            if (category.icon().parameters().value(NokiaIcon) == nokiaIcon) {
                params.insert(NokiaIconGenerated, true);
                break;
            }
        }
    } else {
        QString path = remotePath + (!m_theme.isEmpty()
                                     ? QLatin1Char('.') + m_theme : QString());
        params.insert(QPlaceIcon::SingleUrl, QUrl(path));

        if (!nokiaIcon.isEmpty()) {
            params.insert(NokiaIcon, nokiaIcon);
            params.insert(IconPrefix, iconPrefix);
            params.insert(NokiaIconGenerated, true);
        }
    }

    icon.setParameters(params);

    if (!icon.isEmpty())
        icon.setManager(manager());

    return icon;
}

QUrl QPlaceManagerEngineNokiaV2::constructIconUrl(const QPlaceIcon &icon,
                                                        const QSize &size) const
{
    Q_UNUSED(size)
    QVariantMap params = icon.parameters();
    QString nokiaIcon = params.value(NokiaIcon).toString();

    if (!nokiaIcon.isEmpty()) {
        nokiaIcon.append(!m_theme.isEmpty() ?
                         QLatin1Char('.') + m_theme : QString());

        if (params.contains(IconPrefix)) {
            return QUrl(params.value(IconPrefix).toString() +
                        nokiaIcon);
        } else {
            return QUrl(QString::fromLatin1("file://") + m_localDataPath
                                     + nokiaIcon);
        }
    }

    return QUrl();
}

void QPlaceManagerEngineNokiaV2::replyFinished()
{
    QPlaceReply *reply = qobject_cast<QPlaceReply *>(sender());
    if (reply)
        emit finished(reply);
}

void QPlaceManagerEngineNokiaV2::replyError(QPlaceReply::Error error_, const QString &errorString)
{
    QPlaceReply *reply = qobject_cast<QPlaceReply *>(sender());
    if (reply)
        emit error(reply, error_, errorString);
}

void QPlaceManagerEngineNokiaV2::categoryReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    QString categoryId;

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
        if (!document.isObject()) {
            if (m_categoryReply) {
                QMetaObject::invokeMethod(m_categoryReply.data(), "setError", Qt::QueuedConnection,
                                          Q_ARG(QPlaceReply::Error, QPlaceReply::ParseError),
                                          Q_ARG(QString, QCoreApplication::translate(NOKIA_PLUGIN_CONTEXT_NAME, PARSE_ERROR)));
            }
            return;
        }

        QJsonObject category = document.object();

        categoryId = category.value(QStringLiteral("categoryId")).toString();
        if (m_tempTree.contains(categoryId)) {
            PlaceCategoryNode node = m_tempTree.value(categoryId);
            node.category.setName(category.value(QStringLiteral("name")).toString());
            node.category.setCategoryId(categoryId);
            node.category.setIcon(icon(category.value(QStringLiteral("icon")).toString()));

            m_tempTree.insert(categoryId, node);
        }
    } else {
        categoryId = m_categoryRequests.key(reply);
        PlaceCategoryNode rootNode = m_tempTree.value(QString());
        rootNode.childIds.removeAll(categoryId);
        m_tempTree.insert(QString(), rootNode);
        m_tempTree.remove(categoryId);
    }

    m_categoryRequests.remove(categoryId);
    reply->deleteLater();

    if (m_categoryRequests.isEmpty()) {
        m_categoryTree = m_tempTree;
        m_tempTree.clear();

        if (m_categoryReply)
            m_categoryReply.data()->emitFinished();
    }
}

void QPlaceManagerEngineNokiaV2::categoryReplyError()
{
    if (m_categoryReply) {
        QMetaObject::invokeMethod(m_categoryReply.data(), "setError", Qt::QueuedConnection,
                                  Q_ARG(QPlaceReply::Error, QPlaceReply::CommunicationError),
                                  Q_ARG(QString, QCoreApplication::translate(NOKIA_PLUGIN_CONTEXT_NAME, NETWORK_ERROR)));
    }
}

QNetworkReply *QPlaceManagerEngineNokiaV2::sendRequest(const QUrl &url)
{
    QUrlQuery queryItems(url);
    queryItems.addQueryItem(QStringLiteral("app_id"), m_appId);
    queryItems.addQueryItem(QStringLiteral("app_code"), m_appCode);

    QUrl requestUrl = url;
    requestUrl.setQuery(queryItems);

    QNetworkRequest request;
    request.setUrl(requestUrl);

    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Accept-Language", createLanguageString());

    return m_manager->get(request);
}

QByteArray QPlaceManagerEngineNokiaV2::createLanguageString() const
{
    QByteArray language;

    QList<QLocale> locales = m_locales;
    if (locales.isEmpty())
        locales << QLocale();

    foreach (const QLocale &loc, locales) {
        language.append(loc.name().replace(2, 1, QLatin1Char('-')).toLatin1());
        language.append(", ");
    }
    language.chop(2);

    return language;
}

QT_END_NAMESPACE
