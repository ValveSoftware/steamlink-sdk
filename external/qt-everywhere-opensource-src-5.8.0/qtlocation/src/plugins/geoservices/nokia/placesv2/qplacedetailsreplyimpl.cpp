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

#include "qplacedetailsreplyimpl.h"
#include "jsonparserhelpers.h"
#include "../qplacemanagerengine_nokiav2.h"
#include "../qgeoerror_messages.h"

#include <QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtNetwork/QNetworkReply>
#include <QtLocation/QPlaceManager>
#include <QtLocation/QPlaceSupplier>
#include <QtLocation/QPlaceImage>
#include <QtLocation/QPlaceEditorial>
#include <QtLocation/QPlaceReview>
#include <QtLocation/QPlaceUser>

QT_BEGIN_NAMESPACE

// These countries format the street address as: {house number} {street name}
// All other countries format it as: {street name} {house number}
static const char COUNTRY_TABLE_string[] =
    "CAN\0"
    "NZL\0"
    "GBR\0"
    "AUS\0"
    "LKA\0"
    "USA\0"
    "SGP\0"
    "FRA\0"
    "BHS\0"
    "CHN\0"
    "IND\0"
    "IRL\0"
    "ARE\0"
    "\0";

static const int COUNTRY_TABLE_indices[] = {
       0,    4,    8,   12,   16,   20,   24,   28,
      32,   36,   40,   44,   48,   -1
};

static bool countryTableContains(const QString &countryCode)
{
    for (int i = 0; COUNTRY_TABLE_indices[i] != -1; ++i) {
        if (countryCode == QLatin1String(COUNTRY_TABLE_string + COUNTRY_TABLE_indices[i]))
            return true;
    }

    return false;
}

QPlaceDetailsReplyImpl::QPlaceDetailsReplyImpl(QNetworkReply *reply,
                                               QPlaceManagerEngineNokiaV2 *parent)
    :   QPlaceDetailsReply(parent), m_reply(reply), m_engine(parent)
{
    Q_ASSERT(parent);

    if (!m_reply)
        return;

    m_reply->setParent(this);
    connect(m_reply, SIGNAL(finished()), this, SLOT(replyFinished()));
}

QPlaceDetailsReplyImpl::~QPlaceDetailsReplyImpl()
{
}

void QPlaceDetailsReplyImpl::abort()
{
    if (m_reply)
        m_reply->abort();
}

void QPlaceDetailsReplyImpl::setError(QPlaceReply::Error error_, const QString &errorString)
{
    QPlaceReply::setError(error_, errorString);
    emit error(error_, errorString);
    setFinished(true);
    emit finished();
}

void QPlaceDetailsReplyImpl::replyFinished()
{
    if (m_reply->error() != QNetworkReply::NoError) {
        switch (m_reply->error()) {
        case QNetworkReply::OperationCanceledError:
            setError(CancelError, "Request canceled.");
            break;
        case QNetworkReply::ContentNotFoundError:
            setError(PlaceDoesNotExistError,
                     QString::fromLatin1("The id, %1, does not reference an existing place")
                     .arg(m_placeId));
            break;
        default:
            setError(CommunicationError, "Network error.");
        }
        return;
    }

    QJsonDocument document = QJsonDocument::fromJson(m_reply->readAll());
    if (!document.isObject()) {
        setError(ParseError, QCoreApplication::translate(NOKIA_PLUGIN_CONTEXT_NAME, PARSE_ERROR));
        return;
    }

    QJsonObject object = document.object();

    QPlace place;

    place.setPlaceId(object.value(QLatin1String("placeId")).toString());

    //const QUrl view = object.value(QLatin1String("view")).toString();

    place.setName(object.value(QLatin1String("name")).toString());

    //if (object.contains(QLatin1String("distance")))
    //    double distance = object.value(QLatin1String("distance")).toDouble();

    //if (object.contains(QLatin1String("alternativeNames"))) {
    //    QJsonArray alternativeNames = object.value(QLatin1String("alternativeNames")).toArray();
    //}

    QGeoLocation location;

    QJsonObject locationObject = object.value(QLatin1String("location")).toObject();

    //if (locationObject.contains(QLatin1String("locationId")))
    //    const QString locationId = locationObject.value(QLatin1String("locationId")).toString();

    QJsonArray position = locationObject.value(QLatin1String("position")).toArray();
    location.setCoordinate(QGeoCoordinate(position.at(0).toDouble(), position.at(1).toDouble()));

    QGeoAddress address;

    QJsonObject addressObject = locationObject.value(QLatin1String("address")).toObject();

    address.setText(addressObject.value(QLatin1String("text")).toString());

    address.setCountry(addressObject.value(QLatin1String("country")).toString());
    address.setCountryCode(addressObject.value(QLatin1String("countryCode")).toString());

    QString house;
    QString street;

    if (addressObject.contains(QLatin1String("house")))
        house = addressObject.value(QLatin1String("house")).toString();
    if (addressObject.contains(QLatin1String("street")))
        street = addressObject.value(QLatin1String("street")).toString();

    if (countryTableContains(address.countryCode())) {
        if (!house.isEmpty() && !street.startsWith(house))
            street = house + QLatin1Char(' ') + street;
    } else {
        if (!house.isEmpty() && !street.endsWith(house))
            street += QLatin1Char(' ') + house;
    }

    address.setStreet(street);

    if (addressObject.contains(QLatin1String("city")))
        address.setCity(addressObject.value(QLatin1String("city")).toString());
    if (addressObject.contains(QLatin1String("district")))
        address.setDistrict(addressObject.value(QLatin1String("district")).toString());
    if (addressObject.contains(QLatin1String("state")))
        address.setState(addressObject.value(QLatin1String("state")).toString());
    if (addressObject.contains(QLatin1String("county")))
        address.setCounty(addressObject.value(QLatin1String("county")).toString());
    if (addressObject.contains(QLatin1String("postalCode")))
        address.setPostalCode(addressObject.value(QLatin1String("postalCode")).toString());

    location.setAddress(address);

    if (locationObject.contains(QLatin1String("bbox"))) {
        QJsonArray bbox = locationObject.value(QLatin1String("bbox")).toArray();
        QGeoRectangle box(QGeoCoordinate(bbox.at(3).toDouble(), bbox.at(0).toDouble()),
                            QGeoCoordinate(bbox.at(1).toDouble(), bbox.at(2).toDouble()));
        location.setBoundingBox(box);
    }

    place.setLocation(location);

    place.setCategories(parseCategories(object.value(QLatin1String("categories")).toArray(),
                                        m_engine));

    place.setIcon(m_engine->icon(object.value(QLatin1String("icon")).toString(),
                                 place.categories()));

    if (object.contains(QLatin1String("contacts"))) {
        QJsonObject contactsObject = object.value(QLatin1String("contacts")).toObject();

        if (contactsObject.contains(QLatin1String("phone"))) {
            place.setContactDetails(QPlaceContactDetail::Phone,
                                    parseContactDetails(contactsObject.value(QLatin1String("phone")).toArray()));
        }
        if (contactsObject.contains(QLatin1String("fax"))) {
            place.setContactDetails(QPlaceContactDetail::Fax,
                                    parseContactDetails(contactsObject.value(QLatin1String("fax")).toArray()));
        }
        if (contactsObject.contains(QLatin1String("website"))) {
            place.setContactDetails(QPlaceContactDetail::Website,
                                    parseContactDetails(contactsObject.value(QLatin1String("website")).toArray()));
        }
        if (contactsObject.contains(QLatin1String("email"))) {
            place.setContactDetails(QPlaceContactDetail::Email,
                                    parseContactDetails(contactsObject.value(QLatin1String("email")).toArray()));
        }
    }

    //if (object.contains(QLatin1String("verifiedByOwner")))
    //    bool verifiedByOwner = object.value(QLatin1String("verifiedByOwner")).toBool();

    if (object.contains(QLatin1String("attribution")))
        place.setAttribution(object.value(QLatin1String("attribution")).toString());

    if (object.contains(QLatin1String("supplier"))) {
        place.setSupplier(parseSupplier(object.value(QLatin1String("supplier")).toObject(),
                                        m_engine));
    }

    if (object.contains(QLatin1String("ratings"))) {
        QJsonObject ratingsObject = object.value(QLatin1String("ratings")).toObject();

        QPlaceRatings ratings;
        ratings.setAverage(ratingsObject.value(QLatin1String("average")).toDouble());
        ratings.setCount(ratingsObject.value(QLatin1String("count")).toDouble());
        ratings.setMaximum(5.0);

        place.setRatings(ratings);
    }

    if (object.contains(QLatin1String("extended"))) {
        QJsonObject extendedObject = object.value(QLatin1String("extended")).toObject();

        for (auto it = extendedObject.constBegin(), end = extendedObject.constEnd(); it != end; ++it) {
            QJsonObject attributeObject = it.value().toObject();

            QPlaceAttribute attribute;

            attribute.setLabel(attributeObject.value(QLatin1String("label")).toString());
            attribute.setText(attributeObject.value(QLatin1String("text")).toString());

            QString key = it.key();
            if (key == QLatin1String("payment"))
                place.setExtendedAttribute(QPlaceAttribute::Payment, attribute);
            else if (key == QLatin1String("openingHours"))
                place.setExtendedAttribute(QPlaceAttribute::OpeningHours, attribute);
            else
                place.setExtendedAttribute(key, attribute);
        }
    }

    if (object.contains(QLatin1String("media"))) {
        QJsonObject mediaObject = object.value(QLatin1String("media")).toObject();

        if (mediaObject.contains(QLatin1String("images"))) {
            QPlaceContent::Collection collection;
            int totalCount = 0;

            parseCollection(QPlaceContent::ImageType,
                            mediaObject.value(QLatin1String("images")).toObject(),
                            &collection, &totalCount, 0, 0, m_engine);

            place.setTotalContentCount(QPlaceContent::ImageType, totalCount);
            place.setContent(QPlaceContent::ImageType, collection);
        }
        if (mediaObject.contains(QLatin1String("editorials"))) {
            QPlaceContent::Collection collection;
            int totalCount = 0;

            parseCollection(QPlaceContent::EditorialType,
                            mediaObject.value(QLatin1String("editorials")).toObject(),
                            &collection, &totalCount, 0, 0, m_engine);

            place.setTotalContentCount(QPlaceContent::EditorialType, totalCount);
            place.setContent(QPlaceContent::EditorialType, collection);
        }
        if (mediaObject.contains(QLatin1String("reviews"))) {
            QPlaceContent::Collection collection;
            int totalCount = 0;

            parseCollection(QPlaceContent::ReviewType,
                            mediaObject.value(QLatin1String("reviews")).toObject(),
                            &collection, &totalCount, 0, 0, m_engine);

            place.setTotalContentCount(QPlaceContent::ReviewType, totalCount);
            place.setContent(QPlaceContent::ReviewType, collection);
        }
    }

    //if (object.contains(QLatin1String("related"))) {
    //    QJsonObject relatedObject = object.value(QLatin1String("related")).toObject();
    //}

    QPlaceAttribute provider;
    provider.setText(QLatin1String("here"));
    place.setExtendedAttribute(QPlaceAttribute::Provider, provider);

    place.setVisibility(QLocation::PublicVisibility);
    place.setDetailsFetched(true);
    setPlace(place);

    m_reply->deleteLater();
    m_reply = 0;

    setFinished(true);
    emit finished();
}

QT_END_NAMESPACE
