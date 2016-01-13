/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
** This file is part of the Nokia services plugin for the Maps and
** Navigation API.  The use of these services, whether by use of the
** plugin or by other means, is governed by the terms and conditions
** described by the file NOKIA_TERMS_AND_CONDITIONS.txt in
** this package, located in the directory containing the Nokia services
** plugin source code.
**
****************************************************************************/

#include "qgeocodexmlparser.h"

#include <QtCore/QXmlStreamReader>
#include <QtCore/QThreadPool>
#include <QtPositioning/QGeoLocation>
#include <QtPositioning/QGeoAddress>
#include <QtPositioning/QGeoCoordinate>
#include <QtPositioning/QGeoRectangle>

QT_BEGIN_NAMESPACE

QGeoCodeXmlParser::QGeoCodeXmlParser()
{
}

QGeoCodeXmlParser::~QGeoCodeXmlParser()
{
}

void QGeoCodeXmlParser::setBounds(const QGeoShape &bounds)
{
    m_bounds = bounds;
}

void QGeoCodeXmlParser::parse(const QByteArray &data)
{
    m_data = data;
    QThreadPool::globalInstance()->start(this);
}

void QGeoCodeXmlParser::run()
{
    m_reader = new QXmlStreamReader(m_data);

    if (!parseRootElement())
        emit error(m_reader->errorString());
    else
        emit results(m_results);

    delete m_reader;
    m_reader = 0;
}

bool QGeoCodeXmlParser::parseRootElement()
{
    /*
    <xsd:element name="places">
        <xsd:complexType>
            <xsd:sequence>
                <xsd:element minOccurs="0" maxOccurs="unbounded" name="place" type="gc:Place"/>
            </xsd:sequence>
            <xsd:attribute name="resultCode" type="gc:ResultCodes"/>
            <xsd:attribute name="resultDescription" type="xsd:string"/>
            <xsd:attribute name="resultsTotal" type="xsd:nonNegativeInteger"/>
        </xsd:complexType>
    </xsd:element>

    <xsd:simpleType name="ResultCodes">
        <xsd:restriction base="xsd:string">
            <xsd:enumeration value="OK"/>
            <xsd:enumeration value="FAILED"/>
        </xsd:restriction>
    </xsd:simpleType>
    */

    if (m_reader->readNextStartElement()) {
        if (m_reader->name() == "places") {
            if (m_reader->attributes().hasAttribute("resultCode")) {
                QStringRef result = m_reader->attributes().value("resultCode");
                if (result == "FAILED") {
                    QString resultDesc = m_reader->attributes().value("resultDescription").toString();
                    if (resultDesc.isEmpty())
                        resultDesc = "The attribute \"resultCode\" of the element \"places\" indicates that the request failed.";

                    m_reader->raiseError(resultDesc);

                    return false;
                } else if (result != "OK") {
                    m_reader->raiseError(QString("The attribute \"resultCode\" of the element \"places\" has an unknown value (value was %1).").arg(result.toString()));
                    return false;
                }
            }

            while (m_reader->readNextStartElement()) {
                if (m_reader->name() == "place") {
                    QGeoLocation location;

                    if (!parsePlace(&location))
                        return false;

                    if (!m_bounds.isValid() || m_bounds.contains(location.coordinate()))
                        m_results.append(location);
                } else {
                    m_reader->raiseError(QString("The element \"places\" did not expect a child element named \"%1\".").arg(m_reader->name().toString()));
                    return false;
                }
            }
        } else {
            m_reader->raiseError(QString("The root element is expected to have the name \"places\" (root element was named \"%1\").").arg(m_reader->name().toString()));
            return false;
        }
    } else {
        m_reader->raiseError("Expected a root element named \"places\" (no root element found).");
        return false;
    }

    if (m_reader->readNextStartElement()) {
        m_reader->raiseError(QString("A single root element named \"places\" was expected (second root element was named \"%1\")").arg(m_reader->name().toString()));
        return false;
    }

    return true;
}


//Note: the term Place here is semi-confusing since
//      the xml 'place' is actually a location ie coord + address
bool QGeoCodeXmlParser::parsePlace(QGeoLocation *location)
{
    /*
    <xsd:complexType name="Place">
        <xsd:all>
            <xsd:element name="location" type="gc:Location"/>
            <xsd:element minOccurs="0" name="address" type="gc:Address"/>
            <xsd:element minOccurs="0" name="alternatives" type="gc:Alternatives"/>
        </xsd:all>
        <xsd:attribute name="title" type="xsd:string" use="required"/>
        <xsd:attribute name="language" type="gc:LanguageCode" use="required"/>
    </xsd:complexType>

    <xsd:simpleType name="LanguageCode">
        <xsd:restriction base="xsd:string">
            <xsd:length value="3"/>
        </xsd:restriction>
    </xsd:simpleType>
    */

    Q_ASSERT(m_reader->isStartElement() && m_reader->name() == "place");

    if (!m_reader->attributes().hasAttribute("title")) {
        m_reader->raiseError("The element \"place\" did not have the required attribute \"title\".");
        return false;
    }

    if (!m_reader->attributes().hasAttribute("language")) {
        //m_reader->raiseError("The element \"place\" did not have the required attribute \"language\".");
        //return false;
    } else {
        QString lang = m_reader->attributes().value("language").toString();

        if (lang.length() != 3) {
            m_reader->raiseError(QString("The attribute \"language\" of the element \"place\" was not of length 3 (length was %1).").arg(lang.length()));
            return false;
        }
    }

    bool parsedLocation = false;
    bool parsedAddress = false;
    bool parsedAlternatives = false;

    while (m_reader->readNextStartElement()) {
        QString name = m_reader->name().toString();
        if (name == "location") {
            if (parsedLocation) {
                m_reader->raiseError("The element \"place\" has multiple child elements named \"location\" (exactly one expected)");
                return false;
            }

            if (!parseLocation(location))
                return false;

            parsedLocation = true;
        } else if (name == "address") {
            if (parsedAddress) {
                m_reader->raiseError("The element \"place\" has multiple child elements named \"address\" (at most one expected)");
                return false;
            }

            QGeoAddress address;
            if (!parseAddress(&address))
                return false;
            else
                location->setAddress(address);

            location->setAddress(address);

            parsedAddress = true;
        } else if (name == "alternatives") {
            if (parsedAlternatives) {
                m_reader->raiseError("The element \"place\" has multiple child elements named \"alternatives\" (at most one expected)");
                return false;
            }

            // skip alternatives for now
            // need to work out if we have a use for them at all
            // and how to store them if we get them
            m_reader->skipCurrentElement();

            parsedAlternatives = true;
        } else {
            m_reader->raiseError(QString("The element \"place\" did not expect a child element named \"%1\".").arg(m_reader->name().toString()));
            return false;
        }
    }

    if (!parsedLocation) {
        m_reader->raiseError("The element \"place\" has no child elements named \"location\" (exactly one expected)");
        return false;
    }

    return true;
}

//Note: the term Place here is semi-confusing since
//      the xml 'location' is actually a parital location i.e coord
//      as opposed to coord + address
bool QGeoCodeXmlParser::parseLocation(QGeoLocation *location)
{
    /*
    <xsd:complexType name="Location">
        <xsd:all>
            <xsd:element name="position" type="gc:GeoCoord"/>
            <xsd:element minOccurs="0" name="boundingBox" type="gc:GeoBox"/>
        </xsd:all>
    </xsd:complexType>

    <xsd:complexType name="GeoBox">
        <xsd:sequence>
            <xsd:element name="topLeft" type="gc:GeoCoord"/>
            <xsd:element name="bottomRight" type="gc:GeoCoord"/>
        </xsd:sequence>
    </xsd:complexType>
    */

    Q_ASSERT(m_reader->isStartElement() && m_reader->name() == "location");

    bool parsedPosition = false;
    bool parsedBounds = false;

    while (m_reader->readNextStartElement()) {
        QString name = m_reader->name().toString();
        if (name == "position") {
            if (parsedPosition) {
                m_reader->raiseError("The element \"location\" has multiple child elements named \"position\" (exactly one expected)");
                return false;
            }

            QGeoCoordinate coord;
            if (!parseCoordinate(&coord, "position"))
                return false;

            location->setCoordinate(coord);

            parsedPosition = true;
        } else if (name == "boundingBox") {
            if (parsedBounds) {
                m_reader->raiseError("The element \"location\" has multiple child elements named \"boundingBox\" (at most one expected)");
                return false;
            }

            QGeoRectangle bounds;

            if (!parseBoundingBox(&bounds))
                return false;

            location->setBoundingBox(bounds);

            parsedBounds = true;
        } else {
            m_reader->raiseError(QString("The element \"location\" did not expect a child element named \"%1\".").arg(m_reader->name().toString()));
            return false;
        }
    }

    if (!parsedPosition) {
        m_reader->raiseError("The element \"location\" has no child elements named \"position\" (exactly one expected)");
        return false;
    }

    return true;
}

bool QGeoCodeXmlParser::parseAddress(QGeoAddress *address)
{
    /*
    <xsd:complexType name="Address">
        <xsd:sequence>
            <xsd:element minOccurs="0" maxOccurs="1" name="country" type="xsd:string"/>
            <xsd:element minOccurs="0" maxOccurs="1" name="countryCode" type="gc:CountryCode"/>
            <xsd:element minOccurs="0" maxOccurs="1" name="state" type="xsd:string"/>
            <xsd:element minOccurs="0" maxOccurs="1" name="county" type="xsd:string"/>
            <xsd:element minOccurs="0" maxOccurs="1" name="city" type="xsd:string"/>
            <xsd:element minOccurs="0" maxOccurs="1" name="district" type="xsd:string"/>
            <xsd:element minOccurs="0" maxOccurs="1" name="thoroughfare" type="gc:Thoroughfare"/>
            <xsd:element minOccurs="0" maxOccurs="1" name="postCode" type="xsd:string"/>
        </xsd:sequence>
        <xsd:attribute name="type" type="xsd:string"/>
    </xsd:complexType>

    <xsd:simpleType name="CountryCode">
        <xsd:restriction base="xsd:string">
            <xsd:length value="3" fixed="true"/>
        </xsd:restriction>
    </xsd:simpleType>

    <xsd:complexType name="Thoroughfare">
        <xsd:sequence>
            <xsd:element minOccurs="0" name="name" type="xsd:string"/>
            <xsd:element minOccurs="0" name="number" type="xsd:string"/>
        </xsd:sequence>
    </xsd:complexType>
    */

    Q_ASSERT(m_reader->isStartElement() && m_reader->name() == "address");

    // currently ignoring the type of the address

    if (!m_reader->readNextStartElement())
        return true;

    if (m_reader->name() == "country") {
        address->setCountry(m_reader->readElementText());
        if (!m_reader->readNextStartElement())
            return true;
    }

    if (m_reader->name() == "countryCode") {
        address->setCountryCode(m_reader->readElementText());

        if (address->countryCode().length() != 3) {
            m_reader->raiseError(QString("The text of the element \"countryCode\" was not of length 3 (length was %1).").arg(address->countryCode().length()));
            return false;
        }

        if (!m_reader->readNextStartElement())
            return true;
    }

    if (m_reader->name() == "state") {
        address->setState(m_reader->readElementText());
        if (!m_reader->readNextStartElement())
            return true;
    }

    if (m_reader->name() == "county") {
        address->setCounty(m_reader->readElementText());
        if (!m_reader->readNextStartElement())
            return true;
    }

    if (m_reader->name() == "city") {
        address->setCity(m_reader->readElementText());
        if (!m_reader->readNextStartElement())
            return true;
    }

    if (m_reader->name() == "district") {
        address->setDistrict(m_reader->readElementText());
        if (!m_reader->readNextStartElement())
            return true;
    }

    bool inThoroughfare = false;

    if (m_reader->name() == "thoroughfare") {
        inThoroughfare = m_reader->readNextStartElement();

        if (inThoroughfare && (m_reader->name() == "name")) {
            address->setStreet(m_reader->readElementText());
            if (!m_reader->readNextStartElement())
                inThoroughfare = false;
        }

        if (inThoroughfare && (m_reader->name() == "number")) {
            address->setStreet(m_reader->readElementText() + " " + address->street());
            if (!m_reader->readNextStartElement())
                inThoroughfare = false;
        }

        if (inThoroughfare) {
            m_reader->raiseError(QString("The element \"thoroughFare\" did not expect the child element \"%1\" at this point (unknown child element or child element out of order).").arg(m_reader->name().toString()));
            return false;
        }

        if (!m_reader->readNextStartElement())
            return true;
    }

    if (m_reader->name() == "postCode") {
        address->setPostalCode(m_reader->readElementText());
        if (!m_reader->readNextStartElement())
            return true;
    }

    m_reader->raiseError(QString("The element \"address\" did not expect the child element \"%1\" at this point (unknown child element or child element out of order).").arg(m_reader->name().toString()));
    return false;
}

bool QGeoCodeXmlParser::parseBoundingBox(QGeoRectangle *bounds)
{
    /*
    <xsd:complexType name="GeoBox">
        <xsd:sequence>
            <xsd:element name="topLeft" type="gc:GeoCoord"/>
            <xsd:element name="bottomRight" type="gc:GeoCoord"/>
        </xsd:sequence>
    </xsd:complexType>
    */

    Q_ASSERT(m_reader->isStartElement() && m_reader->name() == "boundingBox");

    if (!m_reader->readNextStartElement()) {
        m_reader->raiseError("The element \"boundingBox\" was expected to have 2 child elements (0 found)");
        return false;
    }

    QGeoCoordinate nw;

    if (m_reader->name() == "topLeft") {
        if (!parseCoordinate(&nw, "topLeft"))
            return false;
    } else {
        m_reader->raiseError(QString("The element \"boundingBox\" expected this child element to be named \"topLeft\" (found an element named \"%1\")").arg(m_reader->name().toString()));
        return false;
    }

    if (!m_reader->readNextStartElement()) {
        m_reader->raiseError("The element \"boundingBox\" was expected to have 2 child elements (1 found)");
        return false;
    }

    QGeoCoordinate se;

    if (m_reader->name() == "bottomRight") {
        if (!parseCoordinate(&se, "bottomRight"))
            return false;
    } else {
        m_reader->raiseError(QString("The element \"boundingBox\" expected this child element to be named \"bottomRight\" (found an element named \"%1\")").arg(m_reader->name().toString()));
        return false;
    }

    if (m_reader->readNextStartElement()) {
        m_reader->raiseError("The element \"boundingBox\" was expected to have 2 child elements (more than 2 found)");
        return false;
    }

    *bounds = QGeoRectangle(nw, se);

    return true;
}

bool QGeoCodeXmlParser::parseCoordinate(QGeoCoordinate *coordinate, const QString &elementName)
{
    /*
    <xsd:complexType name="GeoCoord">
        <xsd:sequence>
            <xsd:element name="latitude" type="gc:Latitude"/>
            <xsd:element name="longitude" type="gc:Longitude"/>
        </xsd:sequence>
    </xsd:complexType>

    <xsd:simpleType name="Latitude">
        <xsd:restriction base="xsd:float">
            <xsd:minInclusive value="-90.0"/>
            <xsd:maxInclusive value="90.0"/>
        </xsd:restriction>
    </xsd:simpleType>

    <xsd:simpleType name="Longitude">
        <xsd:restriction base="xsd:float">
            <xsd:minInclusive value="-180.0"/>
            <xsd:maxInclusive value="180.0"/>
        </xsd:restriction>
    </xsd:simpleType>
    */

    Q_ASSERT(m_reader->isStartElement() && m_reader->name() == elementName);

    if (!m_reader->readNextStartElement()) {
        m_reader->raiseError(QString("The element \"%1\" was expected to have 2 child elements (0 found)").arg(elementName));
        return false;
    }

    if (m_reader->name() == "latitude") {
        bool ok = false;
        QString s = m_reader->readElementText();
        double lat = s.toDouble(&ok);

        if (!ok) {
            m_reader->raiseError(QString("The element \"latitude\" expected a value convertable to type float (value was \"%1\")").arg(s));
            return false;
        }

        if (lat < -90.0 || 90.0 < lat) {
            m_reader->raiseError(QString("The element \"latitude\" expected a value between -90.0 and 90.0 inclusive (value was %1)").arg(lat));
            return false;
        }

        coordinate->setLatitude(lat);
    } else {
        m_reader->raiseError(QString("The element \"%1\" expected this child element to be named \"latitude\" (found an element named \"%2\")").arg(elementName).arg(m_reader->name().toString()));
    }

    if (!m_reader->readNextStartElement()) {
        m_reader->raiseError(QString("The element \"%1\" was expected to have 2 child elements (1 found)").arg(elementName));
        return false;
    }

    if (m_reader->name() == "longitude") {
        bool ok = false;
        QString s = m_reader->readElementText();
        double lng = s.toDouble(&ok);

        if (!ok) {
            m_reader->raiseError(QString("The element \"longitude\" expected a value convertable to type float (value was \"%1\")").arg(s));
            return false;
        }

        if (lng < -180.0 || 180.0 < lng) {
            m_reader->raiseError(QString("The element \"longitude\" expected a value between -180.0 and 180.0 inclusive (value was %1)").arg(lng));
            return false;
        }

        coordinate->setLongitude(lng);
    } else {
        m_reader->raiseError(QString("The element \"%1\" expected this child element to be named \"longitude\" (found an element named \"%2\")").arg(elementName).arg(m_reader->name().toString()));
    }

    if (m_reader->readNextStartElement()) {
        m_reader->raiseError(QString("The element \"%1\" was expected to have 2 child elements (more than 2 found)").arg(elementName));
        return false;
    }

    return true;
}

QT_END_NAMESPACE
