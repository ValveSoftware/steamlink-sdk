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

#ifndef QPLACEMANAGERENGINE_TEST_H
#define QPLACEMANAGERENGINE_TEST_H

#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QUuid>
#include <QtPositioning/QGeoCoordinate>
#include <QtPositioning/QGeoLocation>
#include <QtLocation/QPlaceContentReply>
#include <QtLocation/QPlaceManager>
#include <QtLocation/QPlaceManagerEngine>
#include <QtLocation/QPlaceReply>
#include <QtLocation/QPlaceDetailsReply>
#include <QtLocation/QPlaceEditorial>
#include <QtLocation/QPlaceIdReply>
#include <QtLocation/QPlaceImage>
#include <QtLocation/QPlaceSearchSuggestionReply>
#include <QtLocation/QPlaceSearchReply>
#include <QtLocation/QPlaceResult>
#include <QtLocation/QPlaceCategory>
#include <QtLocation/QPlace>
#include <QtLocation/QPlaceReview>
#include <QtTest/QTest>

QT_BEGIN_NAMESPACE

inline uint qHash(const QPlaceCategory &category)
{
    return qHash(QUuid(category.categoryId().toLatin1()));
}

QT_END_NAMESPACE

QT_USE_NAMESPACE

class PlaceReply : public QPlaceReply
{
    Q_OBJECT

    friend class QPlaceManagerEngineTest;

public:
    PlaceReply(QObject *parent = 0)
    :   QPlaceReply(parent)
    { }

    Q_INVOKABLE void emitFinished()
    {
        emit finished();
    }
};

class ContentReply : public QPlaceContentReply
{
    Q_OBJECT

    friend class QPlaceManagerEngineTest;

public:
    ContentReply(QObject *parent = 0)
    : QPlaceContentReply(parent)
    {}

    Q_INVOKABLE void emitError()
    {
        emit error(error(), errorString());
    }

    Q_INVOKABLE void emitFinished()
    {
        emit finished();
    }
};

class DetailsReply : public QPlaceDetailsReply
{
    Q_OBJECT

    friend class QPlaceManagerEngineTest;

public:
    DetailsReply(QObject *parent = 0)
    :   QPlaceDetailsReply(parent)
    { }

    Q_INVOKABLE void emitError()
    {
        emit error(error(), errorString());
    }

    Q_INVOKABLE void emitFinished()
    {
        emit finished();
    }
};

class IdReply : public QPlaceIdReply
{
    Q_OBJECT

    friend class QPlaceManagerEngineTest;

public:
    IdReply(QPlaceIdReply::OperationType type, QObject *parent = 0)
    :   QPlaceIdReply(type, parent)
    { }

    Q_INVOKABLE void emitError()
    {
        emit error(error(), errorString());
    }

    Q_INVOKABLE void emitFinished()
    {
        emit finished();
    }
};

class PlaceSearchReply : public QPlaceSearchReply
{
    Q_OBJECT

public:
    PlaceSearchReply(const QList<QPlaceSearchResult> &results, QObject *parent = 0)
    :   QPlaceSearchReply(parent)
    {
        setResults(results);
    }

    Q_INVOKABLE void emitError()
    {
        emit error(error(), errorString());
    }

    Q_INVOKABLE void emitFinished()
    {
        emit finished();
    }
};

class SuggestionReply : public QPlaceSearchSuggestionReply
{
    Q_OBJECT

public:
    SuggestionReply(const QStringList &suggestions, QObject *parent = 0)
    :   QPlaceSearchSuggestionReply(parent)
    {
        setSuggestions(suggestions);
    }

    Q_INVOKABLE void emitError()
    {
        emit error(error(), errorString());
    }

    Q_INVOKABLE void emitFinished()
    {
        emit finished();
    }
};

class QPlaceManagerEngineTest : public QPlaceManagerEngine
{
    Q_OBJECT
public:
    QPlaceManagerEngineTest(const QVariantMap &parameters)
        : QPlaceManagerEngine(parameters)
    {
        m_locales << QLocale();
        if (parameters.value(QStringLiteral("initializePlaceData"), false).toBool()) {
            QFile placeData(QFINDTESTDATA("place_data.json"));
            QVERIFY(placeData.exists());
            if (placeData.open(QIODevice::ReadOnly)) {
                QJsonDocument document = QJsonDocument::fromJson(placeData.readAll());

                if (document.isObject()) {
                    QJsonObject o = document.object();

                    if (o.contains(QStringLiteral("categories"))) {
                        QJsonArray categories = o.value(QStringLiteral("categories")).toArray();

                        for (int i = 0; i < categories.count(); ++i) {
                            QJsonObject c = categories.at(i).toObject();

                            QPlaceCategory category;

                            category.setName(c.value(QStringLiteral("name")).toString());
                            category.setCategoryId(c.value(QStringLiteral("id")).toString());

                            m_categories.insert(category.categoryId(), category);

                            const QString parentId = c.value(QStringLiteral("parentId")).toString();
                            m_childCategories[parentId].append(category.categoryId());
                        }
                    }

                    if (o.contains(QStringLiteral("places"))) {
                        QJsonArray places = o.value(QStringLiteral("places")).toArray();

                        for (int i = 0; i < places.count(); ++i) {
                            QJsonObject p = places.at(i).toObject();

                            QPlace place;

                            place.setName(p.value(QStringLiteral("name")).toString());
                            place.setPlaceId(p.value(QStringLiteral("id")).toString());

                            QList<QPlaceCategory> categories;
                            QJsonArray ca = p.value(QStringLiteral("categories")).toArray();
                            for (int j = 0; j < ca.count(); ++j) {
                                QPlaceCategory c = m_categories.value(ca.at(j).toString());
                                if (!c.isEmpty())
                                    categories.append(c);
                            }
                            place.setCategories(categories);

                            QGeoCoordinate coordinate;
                            QJsonObject lo = p.value(QStringLiteral("location")).toObject();
                            coordinate.setLatitude(lo.value(QStringLiteral("latitude")).toDouble());
                            coordinate.setLongitude(lo.value(QStringLiteral("longitude")).toDouble());

                            QGeoLocation location;
                            location.setCoordinate(coordinate);

                            place.setLocation(location);

                            m_places.insert(place.placeId(), place);

                            QStringList recommendations;
                            QJsonArray ra = p.value(QStringLiteral("recommendations")).toArray();
                            for (int j = 0; j < ra.count(); ++j)
                                recommendations.append(ra.at(j).toString());
                            m_placeRecommendations.insert(place.placeId(), recommendations);

                            QJsonArray revArray = p.value(QStringLiteral("reviews")).toArray();
                            QList<QPlaceReview> reviews;
                            for (int j = 0; j < revArray.count(); ++j) {
                                QJsonObject ro = revArray.at(j).toObject();
                                QPlaceReview review;
                                if (ro.contains(QStringLiteral("title")))
                                    review.setTitle(ro.value(QStringLiteral("title")).toString());
                                if (ro.contains(QStringLiteral("text")))
                                    review.setText(ro.value(QStringLiteral("text")).toString());

                                if (ro.contains(QStringLiteral("language")))
                                    review.setLanguage(ro.value("language").toString());

                                if (ro.contains(QStringLiteral("rating")))
                                    review.setRating(ro.value("rating").toDouble());

                                if (ro.contains(QStringLiteral("dateTime")))
                                    review.setDateTime(QDateTime::fromString(
                                                           ro.value(QStringLiteral("dateTime")).toString(),
                                                           QStringLiteral("hh:mm dd-MM-yyyy")));
                                if (ro.contains(QStringLiteral("reviewId")))
                                    review.setReviewId(ro.value("reviewId").toString());

                                reviews << review;
                            }
                            m_placeReviews.insert(place.placeId(), reviews);

                            QJsonArray imgArray = p.value(QStringLiteral("images")).toArray();
                            QList<QPlaceImage> images;
                            for (int j = 0; j < imgArray.count(); ++j) {
                                QJsonObject imgo = imgArray.at(j).toObject();
                                QPlaceImage image;
                                if (imgo.contains(QStringLiteral("url")))
                                    image.setUrl(imgo.value(QStringLiteral("url")).toString());

                                if (imgo.contains("imageId"))
                                    image.setImageId(imgo.value(QStringLiteral("imageId")).toString());

                                if (imgo.contains("mimeType"))
                                    image.setMimeType(imgo.value(QStringLiteral("mimeType")).toString());

                                images << image;
                            }

                            m_placeImages.insert(place.placeId(), images);

                            QJsonArray edArray = p.value(QStringLiteral("editorials")).toArray();
                            QList<QPlaceEditorial> editorials;
                            for (int j = 0; j < edArray.count(); ++j) {
                                QJsonObject edo = edArray.at(j).toObject();
                                QPlaceEditorial editorial;
                                if (edo.contains(QStringLiteral("title")))
                                    editorial.setTitle(edo.value(QStringLiteral("title")).toString());

                                if (edo.contains(QStringLiteral("text")))
                                    editorial.setText(edo.value(QStringLiteral("text")).toString());

                                if (edo.contains(QStringLiteral("language")))
                                    editorial.setLanguage(edo.value(QStringLiteral("language")).toString());

                                editorials << editorial;
                            }

                            m_placeEditorials.insert(place.placeId(), editorials);
                        }
                    }
                }
            }
        }
    }

    QPlaceDetailsReply *getPlaceDetails(const QString &placeId) Q_DECL_OVERRIDE
    {
        DetailsReply *reply = new DetailsReply(this);

        if (placeId.isEmpty() || !m_places.contains(placeId)) {
            reply->setError(QPlaceReply::PlaceDoesNotExistError, tr("Place does not exist"));
            QMetaObject::invokeMethod(reply, "emitError", Qt::QueuedConnection);
        } else {
            reply->setPlace(m_places.value(placeId));
        }

        QMetaObject::invokeMethod(reply, "emitFinished", Qt::QueuedConnection);

        return reply;
    }

    QPlaceContentReply *getPlaceContent(const QPlaceContentRequest &query) Q_DECL_OVERRIDE
    {
        ContentReply *reply = new ContentReply(this);
        if (query.placeId().isEmpty() || !m_places.contains(query.placeId())) {
            reply->setError(QPlaceReply::PlaceDoesNotExistError, tr("Place does not exist"));
            QMetaObject::invokeMethod(reply, "emitError", Qt::QueuedConnection);

        } else {
                QPlaceContent::Collection collection;
                int totalCount = 0;
                switch (query.contentType()) {
                case QPlaceContent::ReviewType:
                    totalCount = m_placeReviews.value(query.placeId()).count();
                    break;
                case QPlaceContent::ImageType:
                    totalCount = m_placeImages.value(query.placeId()).count();
                    break;
                case QPlaceContent::EditorialType:
                    totalCount = m_placeEditorials.value(query.placeId()).count();
                default:
                    //do nothing
                    break;
                }

                QVariantMap context = query.contentContext().toMap();

                int offset = context.value(QStringLiteral("offset"), 0).toInt();
                int max = (query.limit() == -1) ? totalCount
                                                : qMin(offset + query.limit(), totalCount);
                for (int i = offset; i < max; ++i) {
                    switch (query.contentType()) {
                    case QPlaceContent::ReviewType:
                        collection.insert(i, m_placeReviews.value(query.placeId()).at(i));
                        break;
                    case QPlaceContent::ImageType:
                        collection.insert(i, m_placeImages.value(query.placeId()).at(i));
                        break;
                    case QPlaceContent::EditorialType:
                        collection.insert(i, m_placeEditorials.value(query.placeId()).at(i));
                    default:
                        //do nothing
                        break;
                    }
                }

                reply->setContent(collection);
                reply->setTotalCount(totalCount);

                if (max != totalCount) {
                    context.clear();
                    context.insert(QStringLiteral("offset"), offset + query.limit());
                    QPlaceContentRequest request = query;
                    request.setContentContext(context);
                    reply->setNextPageRequest(request);
                }
                if (offset > 0) {
                    context.clear();
                    context.insert(QStringLiteral("offset"), qMin(0, offset - query.limit()));
                    QPlaceContentRequest request = query;
                    request.setContentContext(context);
                    reply->setPreviousPageRequest(request);
                }
        }

        QMetaObject::invokeMethod(reply, "emitFinished", Qt::QueuedConnection);
        return reply;
    }

    QPlaceSearchReply *search(const QPlaceSearchRequest &query) Q_DECL_OVERRIDE
    {
        QList<QPlaceSearchResult> results;

        if (!query.searchTerm().isEmpty()) {
            foreach (const QPlace &place, m_places) {
                if (!place.name().contains(query.searchTerm(), Qt::CaseInsensitive))
                    continue;

                QPlaceResult r;
                r.setPlace(place);
                r.setTitle(place.name());

                results.append(r);
            }
        } else if (!query.categories().isEmpty()) {
            QSet<QPlaceCategory> categories = query.categories().toSet();
            foreach (const QPlace &place, m_places) {
                if (place.categories().toSet().intersect(categories).isEmpty())
                    continue;

                QPlaceResult r;
                r.setPlace(place);
                r.setTitle(place.name());

                results.append(r);
            }
        } else if (!query.recommendationId().isEmpty()) {
            QStringList recommendations = m_placeRecommendations.value(query.recommendationId());
            foreach (const QString &id, recommendations) {
                QPlaceResult r;
                r.setPlace(m_places.value(id));
                r.setTitle(r.place().name());

                results.append(r);
            }
        }

        PlaceSearchReply *reply = new PlaceSearchReply(results, this);

        QMetaObject::invokeMethod(reply, "emitFinished", Qt::QueuedConnection);

        return reply;
    }

    QPlaceSearchSuggestionReply *searchSuggestions(const QPlaceSearchRequest &query) Q_DECL_OVERRIDE
    {
        QStringList suggestions;
        if (query.searchTerm() == QLatin1String("test")) {
            suggestions << QStringLiteral("test1");
            suggestions << QStringLiteral("test2");
            suggestions << QStringLiteral("test3");
        }

        SuggestionReply *reply = new SuggestionReply(suggestions, this);

        QMetaObject::invokeMethod(reply, "emitFinished", Qt::QueuedConnection);

        return reply;
    }

    QPlaceIdReply *savePlace(const QPlace &place) Q_DECL_OVERRIDE
    {
        IdReply *reply = new IdReply(QPlaceIdReply::SavePlace, this);

        if (!place.placeId().isEmpty() && !m_places.contains(place.placeId())) {
            reply->setError(QPlaceReply::PlaceDoesNotExistError, tr("Place does not exist"));
            QMetaObject::invokeMethod(reply, "emitError", Qt::QueuedConnection);
        } else if (!place.placeId().isEmpty()) {
            m_places.insert(place.placeId(), place);
            reply->setId(place.placeId());
        } else {
            QPlace p = place;
            p.setPlaceId(QUuid::createUuid().toString());
            m_places.insert(p.placeId(), p);

            reply->setId(p.placeId());
        }

        QMetaObject::invokeMethod(reply, "emitFinished", Qt::QueuedConnection);

        return reply;
    }

    QPlaceIdReply *removePlace(const QString &placeId) Q_DECL_OVERRIDE
    {
        IdReply *reply = new IdReply(QPlaceIdReply::RemovePlace, this);
        reply->setId(placeId);

        if (!m_places.contains(placeId)) {
            reply->setError(QPlaceReply::PlaceDoesNotExistError, tr("Place does not exist"));
            QMetaObject::invokeMethod(reply, "emitError", Qt::QueuedConnection);
        } else {
            m_places.remove(placeId);
        }

        QMetaObject::invokeMethod(reply, "emitFinished", Qt::QueuedConnection);

        return reply;
    }

    QPlaceIdReply *saveCategory(const QPlaceCategory &category, const QString &parentId) Q_DECL_OVERRIDE
    {
        IdReply *reply = new IdReply(QPlaceIdReply::SaveCategory, this);

        if ((!category.categoryId().isEmpty() && !m_categories.contains(category.categoryId())) ||
            (!parentId.isEmpty() && !m_categories.contains(parentId))) {
            reply->setError(QPlaceReply::CategoryDoesNotExistError, tr("Category does not exist"));
            QMetaObject::invokeMethod(reply, "emitError", Qt::QueuedConnection);
        } else if (!category.categoryId().isEmpty()) {
            m_categories.insert(category.categoryId(), category);
            QStringList children = m_childCategories.value(parentId);

            QMutableHashIterator<QString, QStringList> i(m_childCategories);
            while (i.hasNext()) {
                i.next();
                i.value().removeAll(category.categoryId());
            }

            if (!children.contains(category.categoryId())) {
                children.append(category.categoryId());
                m_childCategories.insert(parentId, children);
            }
            reply->setId(category.categoryId());
        } else {
            QPlaceCategory c = category;
            c.setCategoryId(QUuid::createUuid().toString());
            m_categories.insert(c.categoryId(), c);
            QStringList children = m_childCategories.value(parentId);
            if (!children.contains(c.categoryId())) {
                children.append(c.categoryId());
                m_childCategories.insert(parentId, children);
            }

            reply->setId(c.categoryId());
        }

        QMetaObject::invokeMethod(reply, "emitFinished", Qt::QueuedConnection);

        return reply;
    }

    QPlaceIdReply *removeCategory(const QString &categoryId) Q_DECL_OVERRIDE
    {
        IdReply *reply = new IdReply(QPlaceIdReply::RemoveCategory, this);
        reply->setId(categoryId);

        if (!m_categories.contains(categoryId)) {
            reply->setError(QPlaceReply::CategoryDoesNotExistError, tr("Category does not exist"));
            QMetaObject::invokeMethod(reply, "emitError", Qt::QueuedConnection);
        } else {
            m_categories.remove(categoryId);

            QMutableHashIterator<QString, QStringList> i(m_childCategories);
            while (i.hasNext()) {
                i.next();
                i.value().removeAll(categoryId);
            }
        }

        QMetaObject::invokeMethod(reply, "emitFinished", Qt::QueuedConnection);

        return reply;
    }

    QPlaceReply *initializeCategories() Q_DECL_OVERRIDE
    {
        QPlaceReply *reply = new PlaceReply(this);

        QMetaObject::invokeMethod(reply, "emitFinished", Qt::QueuedConnection);

        return reply;
    }

    QString parentCategoryId(const QString &categoryId) const Q_DECL_OVERRIDE
    {
        QHashIterator<QString, QStringList> i(m_childCategories);
        while (i.hasNext()) {
            i.next();
            if (i.value().contains(categoryId))
                return i.key();
        }

        return QString();
    }

    virtual QStringList childCategoryIds(const QString &categoryId) const Q_DECL_OVERRIDE
    {
        return m_childCategories.value(categoryId);
    }

    virtual QPlaceCategory category(const QString &categoryId) const Q_DECL_OVERRIDE
    {
        return m_categories.value(categoryId);
    }

    QList<QPlaceCategory> childCategories(const QString &parentId) const Q_DECL_OVERRIDE
    {
        QList<QPlaceCategory> categories;

        foreach (const QString &id, m_childCategories.value(parentId))
            categories.append(m_categories.value(id));

        return categories;
    }

    QList<QLocale> locales() const Q_DECL_OVERRIDE
    {
        return m_locales;
    }

    void setLocales(const QList<QLocale> &locales) Q_DECL_OVERRIDE
    {
        m_locales = locales;
    }

    QUrl constructIconUrl(const QPlaceIcon &icon, const QSize &size) const Q_DECL_OVERRIDE
    {
        QList<QPair<int, QUrl> > candidates;

        QMap<QString, int> sizeDictionary;
        sizeDictionary.insert(QStringLiteral("s"), 20);
        sizeDictionary.insert(QStringLiteral("m"), 30);
        sizeDictionary.insert(QStringLiteral("l"), 50);

        QStringList sizeKeys;
        sizeKeys << QStringLiteral("s") << QStringLiteral("m") << QStringLiteral("l");

        foreach (const QString &sizeKey, sizeKeys)
        {
            if (icon.parameters().contains(sizeKey))
                candidates.append(QPair<int, QUrl>(sizeDictionary.value(sizeKey),
                                  icon.parameters().value(sizeKey).toUrl()));
        }

        if (candidates.isEmpty())
            return QUrl();
        else if (candidates.count() == 1) {
            return candidates.first().second;
        } else {
            //we assume icons are squarish so we can use height to
            //determine which particular icon to return
            int requestedHeight = size.height();

            for (int i = 0; i < candidates.count() - 1; ++i) {
                int thresholdHeight = (candidates.at(i).first + candidates.at(i+1).first) / 2;
                if (requestedHeight < thresholdHeight)
                    return candidates.at(i).second;
            }
            return candidates.last().second;
        }
    }

    QPlace compatiblePlace(const QPlace &original) const Q_DECL_OVERRIDE
    {
        QPlace place;
        place.setName(original.name());
        return place;
    }

private:
    QList<QLocale> m_locales;
    QHash<QString, QPlace> m_places;
    QHash<QString, QPlaceCategory> m_categories;
    QHash<QString, QStringList> m_childCategories;
    QHash<QString, QStringList> m_placeRecommendations;
    QHash<QString, QList<QPlaceReview> > m_placeReviews;
    QHash<QString, QList<QPlaceImage> > m_placeImages;
    QHash<QString, QList<QPlaceEditorial> > m_placeEditorials;
};

#endif
