/****************************************************************************
**
** Copyright (C) 2016 Aaron McCarthy <mccarthy.aaron@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtFoo module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qplacemanagerengineosm.h"
#include "qplacesearchreplyosm.h"
#include "qplacecategoriesreplyosm.h"

#include <QtCore/QUrlQuery>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QRegularExpression>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtPositioning/QGeoCircle>
#include <QtLocation/private/unsupportedreplies_p.h>

#include <QtCore/QElapsedTimer>

namespace
{
QString SpecialPhrasesBaseUrl = QStringLiteral("http://wiki.openstreetmap.org/wiki/Special:Export/Nominatim/Special_Phrases/");

QString nameForTagKey(const QString &tagKey)
{
    if (tagKey == QLatin1String("aeroway"))
        return QPlaceManagerEngineOsm::tr("Aeroway");
    else if (tagKey == QLatin1String("amenity"))
        return QPlaceManagerEngineOsm::tr("Amenity");
    else if (tagKey == QLatin1String("building"))
        return QPlaceManagerEngineOsm::tr("Building");
    else if (tagKey == QLatin1String("highway"))
        return QPlaceManagerEngineOsm::tr("Highway");
    else if (tagKey == QLatin1String("historic"))
        return QPlaceManagerEngineOsm::tr("Historic");
    else if (tagKey == QLatin1String("landuse"))
        return QPlaceManagerEngineOsm::tr("Land use");
    else if (tagKey == QLatin1String("leisure"))
        return QPlaceManagerEngineOsm::tr("Leisure");
    else if (tagKey == QLatin1String("man_made"))
        return QPlaceManagerEngineOsm::tr("Man made");
    else if (tagKey == QLatin1String("natural"))
        return QPlaceManagerEngineOsm::tr("Natural");
    else if (tagKey == QLatin1String("place"))
        return QPlaceManagerEngineOsm::tr("Place");
    else if (tagKey == QLatin1String("railway"))
        return QPlaceManagerEngineOsm::tr("Railway");
    else if (tagKey == QLatin1String("shop"))
        return QPlaceManagerEngineOsm::tr("Shop");
    else if (tagKey == QLatin1String("tourism"))
        return QPlaceManagerEngineOsm::tr("Tourism");
    else if (tagKey == QLatin1String("waterway"))
        return QPlaceManagerEngineOsm::tr("Waterway");
    else
        return tagKey;
}

}

QPlaceManagerEngineOsm::QPlaceManagerEngineOsm(const QVariantMap &parameters,
                                               QGeoServiceProvider::Error *error,
                                               QString *errorString)
:   QPlaceManagerEngine(parameters), m_networkManager(new QNetworkAccessManager(this)),
    m_categoriesReply(0)
{
    if (parameters.contains(QStringLiteral("osm.useragent")))
        m_userAgent = parameters.value(QStringLiteral("osm.useragent")).toString().toLatin1();
    else
        m_userAgent = "Qt Location based application";

    if (parameters.contains(QStringLiteral("osm.places.host")))
        m_urlPrefix = parameters.value(QStringLiteral("osm.places.host")).toString();
    else
        m_urlPrefix = QStringLiteral("http://nominatim.openstreetmap.org/search");

    *error = QGeoServiceProvider::NoError;
    errorString->clear();
}

QPlaceManagerEngineOsm::~QPlaceManagerEngineOsm()
{
}

QPlaceSearchReply *QPlaceManagerEngineOsm::search(const QPlaceSearchRequest &request)
{
    bool unsupported = false;

    // Only public visibility supported
    unsupported |= request.visibilityScope() != QLocation::UnspecifiedVisibility &&
                   request.visibilityScope() != QLocation::PublicVisibility;
    unsupported |= request.searchTerm().isEmpty() && request.categories().isEmpty();

    if (unsupported)
        return QPlaceManagerEngine::search(request);

    QUrlQuery queryItems;

    queryItems.addQueryItem(QStringLiteral("format"), QStringLiteral("jsonv2"));

    //queryItems.addQueryItem(QStringLiteral("accept-language"), QStringLiteral("en"));

    QGeoRectangle boundingBox;
    QGeoShape searchArea = request.searchArea();
    switch (searchArea.type()) {
    case QGeoShape::CircleType: {
        QGeoCircle c(searchArea);
        qreal radius = c.radius();
        if (radius < 0)
            radius = 50000;

        boundingBox = QGeoRectangle(c.center().atDistanceAndAzimuth(radius, -45),
                                    c.center().atDistanceAndAzimuth(radius, 135));
        break;
    }
    case QGeoShape::RectangleType:
        boundingBox = searchArea;
        break;
    default:
        ;
    }

    if (!boundingBox.isEmpty()) {
        queryItems.addQueryItem(QStringLiteral("bounded"), QStringLiteral("1"));
        QString coordinates;
        coordinates = QString::number(boundingBox.topLeft().longitude()) + QLatin1Char(',') +
                      QString::number(boundingBox.topLeft().latitude()) + QLatin1Char(',') +
                      QString::number(boundingBox.bottomRight().longitude()) + QLatin1Char(',') +
                      QString::number(boundingBox.bottomRight().latitude());
        queryItems.addQueryItem(QStringLiteral("viewbox"), coordinates);
    }

    QStringList queryParts;
    if (!request.searchTerm().isEmpty())
        queryParts.append(request.searchTerm());

    foreach (const QPlaceCategory &category, request.categories()) {
        QString id = category.categoryId();
        int index = id.indexOf(QLatin1Char('='));
        if (index != -1)
            id = id.mid(index+1);
        queryParts.append(QLatin1Char('[') + id + QLatin1Char(']'));
    }

    queryItems.addQueryItem(QStringLiteral("q"), queryParts.join(QLatin1Char('+')));

    QVariantMap parameters = request.searchContext().toMap();

    QStringList placeIds = parameters.value(QStringLiteral("ExcludePlaceIds")).toStringList();
    if (!placeIds.isEmpty())
        queryItems.addQueryItem(QStringLiteral("exclude_place_ids"), placeIds.join(QLatin1Char(',')));

    queryItems.addQueryItem(QStringLiteral("addressdetails"), QStringLiteral("1"));

    QUrl requestUrl(m_urlPrefix);
    requestUrl.setQuery(queryItems);

    QNetworkReply *networkReply = m_networkManager->get(QNetworkRequest(requestUrl));

    QPlaceSearchReplyOsm *reply = new QPlaceSearchReplyOsm(request, networkReply, this);
    connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
            this, SLOT(replyError(QPlaceReply::Error,QString)));

    return reply;
}

QPlaceReply *QPlaceManagerEngineOsm::initializeCategories()
{
    // Only fetch categories once
    if (m_categories.isEmpty() && !m_categoriesReply) {
        m_categoryLocales = m_locales;
        m_categoryLocales.append(QLocale(QLocale::English));
        fetchNextCategoryLocale();
    }

    QPlaceCategoriesReplyOsm *reply = new QPlaceCategoriesReplyOsm(this);
    connect(reply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(reply, SIGNAL(error(QPlaceReply::Error,QString)),
            this, SLOT(replyError(QPlaceReply::Error,QString)));

    // TODO delayed finished() emission
    if (!m_categories.isEmpty())
        reply->emitFinished();

    m_pendingCategoriesReply.append(reply);
    return reply;
}

QString QPlaceManagerEngineOsm::parentCategoryId(const QString &categoryId) const
{
    Q_UNUSED(categoryId)

    // Only a two category levels
    return QString();
}

QStringList QPlaceManagerEngineOsm::childCategoryIds(const QString &categoryId) const
{
    return m_subcategories.value(categoryId);
}

QPlaceCategory QPlaceManagerEngineOsm::category(const QString &categoryId) const
{
    return m_categories.value(categoryId);
}

QList<QPlaceCategory> QPlaceManagerEngineOsm::childCategories(const QString &parentId) const
{
    QList<QPlaceCategory> categories;
    foreach (const QString &id, m_subcategories.value(parentId))
        categories.append(m_categories.value(id));
    return categories;
}

QList<QLocale> QPlaceManagerEngineOsm::locales() const
{
    return m_locales;
}

void QPlaceManagerEngineOsm::setLocales(const QList<QLocale> &locales)
{
    m_locales = locales;
}

void QPlaceManagerEngineOsm::categoryReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    reply->deleteLater();

    QXmlStreamReader parser(reply);
    while (!parser.atEnd() && parser.readNextStartElement()) {
        if (parser.name() == QLatin1String("mediawiki"))
            continue;
        if (parser.name() == QLatin1String("page"))
            continue;
        if (parser.name() == QLatin1String("revision"))
            continue;
        if (parser.name() == QLatin1String("text")) {
            // parse
            QString page = parser.readElementText();
            QRegularExpression regex(QStringLiteral("\\| ([^|]+) \\|\\| ([^|]+) \\|\\| ([^|]+) \\|\\| ([^|]+) \\|\\| ([\\-YN])"));
            QRegularExpressionMatchIterator i = regex.globalMatch(page);
            while (i.hasNext()) {
                QRegularExpressionMatch match = i.next();
                QString name = match.capturedRef(1).toString();
                QString tagKey = match.capturedRef(2).toString();
                QString tagValue = match.capturedRef(3).toString();
                QString op = match.capturedRef(4).toString();
                QString plural = match.capturedRef(5).toString();

                // Only interested in any operator plural forms
                if (op != QLatin1String("-") || plural != QLatin1String("Y"))
                    continue;

                if (!m_categories.contains(tagKey)) {
                    QPlaceCategory category;
                    category.setCategoryId(tagKey);
                    category.setName(nameForTagKey(tagKey));
                    m_categories.insert(category.categoryId(), category);
                    m_subcategories[QString()].append(tagKey);
                    emit categoryAdded(category, QString());
                }

                QPlaceCategory category;
                category.setCategoryId(tagKey + QLatin1Char('=') + tagValue);
                category.setName(name);

                if (!m_categories.contains(category.categoryId())) {
                    m_categories.insert(category.categoryId(), category);
                    m_subcategories[tagKey].append(category.categoryId());
                    emit categoryAdded(category, tagKey);
                }
            }
        }

        parser.skipCurrentElement();
    }

    if (m_categories.isEmpty() && !m_categoryLocales.isEmpty()) {
        fetchNextCategoryLocale();
        return;
    } else {
        m_categoryLocales.clear();
    }

    foreach (QPlaceCategoriesReplyOsm *reply, m_pendingCategoriesReply)
        reply->emitFinished();
    m_pendingCategoriesReply.clear();
}

void QPlaceManagerEngineOsm::categoryReplyError()
{
    foreach (QPlaceCategoriesReplyOsm *reply, m_pendingCategoriesReply)
        reply->setError(QPlaceReply::CommunicationError, tr("Network request error"));
}

void QPlaceManagerEngineOsm::replyFinished()
{
    QPlaceReply *reply = qobject_cast<QPlaceReply *>(sender());
    if (reply)
        emit finished(reply);
}

void QPlaceManagerEngineOsm::replyError(QPlaceReply::Error errorCode, const QString &errorString)
{
    QPlaceReply *reply = qobject_cast<QPlaceReply *>(sender());
    if (reply)
        emit error(reply, errorCode, errorString);
}

void QPlaceManagerEngineOsm::fetchNextCategoryLocale()
{
    if (m_categoryLocales.isEmpty()) {
        qWarning("No locales specified to fetch categories for");
        return;
    }

    QLocale locale = m_categoryLocales.takeFirst();

    // FIXME: Categories should be cached.
    QUrl requestUrl = QUrl(SpecialPhrasesBaseUrl + locale.name().left(2).toUpper());

    m_categoriesReply = m_networkManager->get(QNetworkRequest(requestUrl));
    connect(m_categoriesReply, SIGNAL(finished()), this, SLOT(categoryReplyFinished()));
    connect(m_categoriesReply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(categoryReplyError()));
}
