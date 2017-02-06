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

#include "jsonparserhelpers.h"
#include "../qplacemanagerengine_nokiav2.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QVariantMap>
#include <QtPositioning/QGeoCoordinate>
#include <QtLocation/QPlaceImage>
#include <QtLocation/QPlaceReview>
#include <QtLocation/QPlaceEditorial>
#include <QtLocation/QPlaceUser>
#include <QtLocation/QPlaceContactDetail>
#include <QtLocation/QPlaceCategory>

QT_BEGIN_NAMESPACE

QGeoCoordinate parseCoordinate(const QJsonArray &coordinateArray)
{
    return QGeoCoordinate(coordinateArray.at(0).toDouble(), coordinateArray.at(1).toDouble());
}

QPlaceSupplier parseSupplier(const QJsonObject &supplierObject,
                             const QPlaceManagerEngineNokiaV2 *engine)
{
    Q_ASSERT(engine);

    QPlaceSupplier supplier;
    supplier.setName(supplierObject.value(QStringLiteral("title")).toString());
    supplier.setUrl(supplierObject.value(QStringLiteral("href")).toString());

    supplier.setIcon(engine->icon(supplierObject.value(QStringLiteral("icon")).toString()));

    return supplier;
}

QPlaceCategory parseCategory(const QJsonObject &categoryObject,
                             const QPlaceManagerEngineNokiaV2 *engine)
{
    Q_ASSERT(engine);

    QPlaceCategory category;

    category.setName(categoryObject.value(QStringLiteral("title")).toString());

    const QUrl href(categoryObject.value(QStringLiteral("href")).toString());
    const QString hrefPath(href.path());
    category.setCategoryId(hrefPath.mid(hrefPath.lastIndexOf(QLatin1Char('/')) + 1));


    category.setIcon(engine->icon(categoryObject.value(QStringLiteral("icon")).toString()));
    return category;
}

QList<QPlaceCategory> parseCategories(const QJsonArray &categoryArray,
                                     const QPlaceManagerEngineNokiaV2 *engine)
{
    Q_ASSERT(engine);

    QList<QPlaceCategory> categoryList;
    for (int i = 0; i < categoryArray.count(); ++i)
        categoryList.append(parseCategory(categoryArray.at(i).toObject(),
                                          engine));

    return categoryList;
}

QList<QPlaceContactDetail> parseContactDetails(const QJsonArray &contacts)
{
    QList<QPlaceContactDetail> contactDetails;

    for (int i = 0; i < contacts.count(); ++i) {
        QJsonObject contact = contacts.at(i).toObject();

        QPlaceContactDetail detail;
        detail.setLabel(contact.value(QStringLiteral("label")).toString());
        detail.setValue(contact.value(QStringLiteral("value")).toString());

        contactDetails.append(detail);
    }

    return contactDetails;
}

QPlaceImage parseImage(const QJsonObject &imageObject,
                       const QPlaceManagerEngineNokiaV2 *engine)
{
    Q_ASSERT(engine);

    QPlaceImage image;

    image.setAttribution(imageObject.value(QStringLiteral("attribution")).toString());
    image.setUrl(imageObject.value(QStringLiteral("src")).toString());
    image.setSupplier(parseSupplier(imageObject.value(QStringLiteral("supplier")).toObject(),
                                    engine));

    return image;
}

QPlaceReview parseReview(const QJsonObject &reviewObject,
                         const QPlaceManagerEngineNokiaV2 *engine)
{
    Q_ASSERT(engine);

    QPlaceReview review;

    review.setDateTime(QDateTime::fromString(reviewObject.value(QStringLiteral("date")).toString()));

    if (reviewObject.contains(QStringLiteral("title")))
        review.setTitle(reviewObject.value(QStringLiteral("title")).toString());

    if (reviewObject.contains(QStringLiteral("rating")))
        review.setRating(reviewObject.value(QStringLiteral("rating")).toDouble());

    review.setText(reviewObject.value(QStringLiteral("description")).toString());

    QJsonObject userObject = reviewObject.value(QStringLiteral("user")).toObject();

    QPlaceUser user;
    user.setUserId(userObject.value(QStringLiteral("id")).toString());
    user.setName(userObject.value(QStringLiteral("title")).toString());
    review.setUser(user);

    review.setAttribution(reviewObject.value(QStringLiteral("attribution")).toString());

    review.setLanguage(reviewObject.value(QStringLiteral("language")).toString());

    review.setSupplier(parseSupplier(reviewObject.value(QStringLiteral("supplier")).toObject(),
                                     engine));

    //if (reviewObject.contains(QStringLiteral("via"))) {
    //    QJsonObject viaObject = reviewObject.value(QStringLiteral("via")).toObject();
    //}

    return review;
}

QPlaceEditorial parseEditorial(const QJsonObject &editorialObject,
                               const QPlaceManagerEngineNokiaV2 *engine)
{
    Q_ASSERT(engine);

    QPlaceEditorial editorial;

    editorial.setAttribution(editorialObject.value(QStringLiteral("attribution")).toString());

    //if (editorialObject.contains(QStringLiteral("via"))) {
    //    QJsonObject viaObject = editorialObject.value(QStringLiteral("via")).toObject();
    //}

    editorial.setSupplier(parseSupplier(editorialObject.value(QStringLiteral("supplier")).toObject(),
                                        engine));
    editorial.setLanguage(editorialObject.value(QStringLiteral("language")).toString());
    editorial.setText(editorialObject.value(QStringLiteral("description")).toString());

    return editorial;
}

void parseCollection(QPlaceContent::Type type, const QJsonObject &object,
                     QPlaceContent::Collection *collection, int *totalCount,
                     QPlaceContentRequest *previous, QPlaceContentRequest *next,
                     const QPlaceManagerEngineNokiaV2 *engine)
{
    Q_ASSERT(engine);

    if (totalCount)
        *totalCount = object.value(QStringLiteral("available")).toDouble();

    int offset = 0;
    if (object.contains(QStringLiteral("offset")))
        offset = object.value(QStringLiteral("offset")).toDouble();

    if (previous && object.contains(QStringLiteral("previous"))) {
        previous->setContentType(type);
        previous->setContentContext(QUrl(object.value(QStringLiteral("previous")).toString()));
    }

    if (next && object.contains(QStringLiteral("next"))) {
        next->setContentType(type);
        next->setContentContext(QUrl(object.value(QStringLiteral("next")).toString()));
    }

    if (collection) {
        QJsonArray items = object.value(QStringLiteral("items")).toArray();
        for (int i = 0; i < items.count(); ++i) {
            QJsonObject itemObject = items.at(i).toObject();

            switch (type) {
            case QPlaceContent::ImageType:
                collection->insert(offset + i, parseImage(itemObject, engine));
                break;
            case QPlaceContent::ReviewType:
                collection->insert(offset + i, parseReview(itemObject, engine));
                break;
            case QPlaceContent::EditorialType:
                collection->insert(offset + i, parseEditorial(itemObject, engine));
                break;
            case QPlaceContent::NoType:
                break;
            }
        }
    }
}

QT_END_NAMESPACE
