/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include "qgeoaddress.h"
#include "qgeoaddress_p.h"

#include <QtCore/QStringList>

#ifdef QGEOADDRESS_DEBUG
#include <QDebug>
#endif

QT_BEGIN_NAMESPACE

/*
    Combines a  list of address parts into  a single line.

    The parts parameter contains both address elements such as city, state and so on
    as well as separators such as spaces and commas.

    It is expected that an element is always followed by a separator and the last
    sepator is usually a new line delimeter.

    For example: Springfield, 8900
    would have four parts
    ["Springfield", ", ", "8900", "<br>"]

    The addressLine takes care of putting in separators appropriately or leaving
    them out depending on whether the adjacent elements are present or not.
    For example if city were empty in the above scenario the returned string is "8900<br>"
    If the postal code was empty, returned string is "Springfield<br>"
    If both city and postal code were empty, the returned string is "".
*/
static QString addressLine(const QStringList &parts)
{
    QString line;
    Q_ASSERT(parts.count() % 2 == 0);

    //iterate until just before the last pair
    QString penultimateSeparator;
    for (int i = 0; i < parts.count() - 2; i += 2) {
        if (!parts.at(i).isEmpty()) {
            line.append(parts.at(i) + parts.at(i + 1));
            penultimateSeparator = parts.at(i + 1);
        }
    }

    if (parts.at(parts.count() - 2).isEmpty()) {
        line.chop(penultimateSeparator.length());

        if (!line.isEmpty())
            line.append(parts.at(parts.count() - 1));
    } else {
        line.append(parts.at(parts.count() - 2));
        line.append(parts.at(parts.count() - 1));
    }

    return line;
}

/*
    Returns a single formatted string representing the \a address. Lines of the address
    are delimited by \a newLine. By default lines are delimited by <br/>. The \l
    {QGeoAddress::countryCode} {countryCode} of the \a address determines the format of
    the resultant string.
*/
static QString formattedAddress(const QGeoAddress &address,
                                const QString &newLine = QLatin1String("<br/>"))
{
    const QString Comma(QStringLiteral(", "));
    const QString Dash(QStringLiteral("-"));
    const QString Space(QStringLiteral(" "));

    QString text;

    if (address.countryCode() == QLatin1String("ALB")
        || address.countryCode() == QLatin1String("MTQ")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.postalCode() << Comma
                                          << address.city() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("AND")
               || address.countryCode() == QLatin1String("AUT")
               || address.countryCode() == QLatin1String("FRA")
               || address.countryCode() == QLatin1String("GLP")
               || address.countryCode() == QLatin1String("GUF")
               || address.countryCode() == QLatin1String("ITA")
               || address.countryCode() == QLatin1String("LUX")
               || address.countryCode() == QLatin1String("MCO")
               || address.countryCode() == QLatin1String("REU")
               || address.countryCode() == QLatin1String("RUS")
               || address.countryCode() == QLatin1String("SMR")
               || address.countryCode() == QLatin1String("VAT")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.postalCode() << Space
                                          << address.city() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("ARE")
               || address.countryCode() == QLatin1String("BHS")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.district() << Space
                                          << address.city() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("AUS")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << (address.district().isEmpty() ? address.city() : address.district())
                                << Space << address.state() << Space << address.postalCode() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("BHR")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.district() << Comma
                                << address.city() << Comma << address.state() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("BRA")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.district() << Space
                    << address.city() << Dash << address.state() << Space << address.postalCode() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("BRN")
               || address.countryCode() == QLatin1String("JOR")
               || address.countryCode() == QLatin1String("LBN")
               || address.countryCode() == QLatin1String("NZL")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.district() << Space
                                << address.city() << Space << address.postalCode() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("CAN")
               || address.countryCode() == QLatin1String("USA")
               || address.countryCode() == QLatin1String("VIR")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.city() << Comma << address.state() << Space
                                              << address.postalCode() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("CHN")) {
        text += addressLine(QStringList() << address.street() << Comma << address.city() << newLine);
        text += addressLine(QStringList() << address.postalCode() << Space << address.state() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("CHL")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.postalCode() << Space
                                << address.district() << Comma << address.city() << Comma
                                << address.state() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("CYM")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.state() << Space
                                << address.postalCode() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("GBR")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.district() << Comma
                                << address.city() << Comma << address.postalCode() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("GIB")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.city() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("HKG")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.district() << newLine);
        text += addressLine(QStringList() << address.city() << newLine);
    } else if (address.countryCode() == QLatin1String("IND")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.city() << Space << address.postalCode() << Space
                                              << address.state() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("IDN")
               || address.countryCode() == QLatin1String("JEY")
               || address.countryCode() == QLatin1String("LVA")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.city() << Comma << address.postalCode() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("IRL")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.district() << Comma << address.state() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("KWT")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.postalCode() << Comma
                            << address.district() << Comma << address.city() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("MLT")
               || address.countryCode() == QLatin1String("SGP")
               || address.countryCode() == QLatin1String("UKR")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.city() << Space << address.postalCode() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("MEX")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.district() << newLine);
        text += addressLine(QStringList() << address.postalCode() << Space << address.city() << Comma
                                              << address.state() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("MYS")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.postalCode() << Space << address.city() << newLine);
        text += addressLine(QStringList() << address.state() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("OMN")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.district() << Comma
                                << address.postalCode() << Comma
                                << address.city() << Comma
                                << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("PRI")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.district() << Comma << address.city() << Comma
                                                 << address.state() << Comma << address.postalCode() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("QAT")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.district() << Space << address.city() << Comma
                                              << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("SAU")) {
        text += addressLine(QStringList() << address.street() << Space << address.district() << newLine);
        text += addressLine(QStringList() << address.city() << Space << address.postalCode() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("TWN")) {
        text += addressLine(QStringList() << address.street() << Comma
                                << address.district() << Comma << address.city() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("THA")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.district() << Comma << address.city() << Space
                                              << address.postalCode() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("TUR")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.postalCode() << Space << address.district() << Comma
                                              << address.city() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("VEN")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.city() << Space << address.postalCode() << Comma
                                              << address.state() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else if (address.countryCode() == QLatin1String("ZAF")) {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.district() << Comma << address.city() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    } else {
        text += addressLine(QStringList() << address.street() << newLine);
        text += addressLine(QStringList() << address.postalCode() << Space << address.city() << newLine);
        text += addressLine(QStringList() << address.country() << newLine);
    }

    text.chop(newLine.length());
    return text;
}

QGeoAddressPrivate::QGeoAddressPrivate()
        : QSharedData(),
          m_autoGeneratedText(false)
{
}

QGeoAddressPrivate::QGeoAddressPrivate(const QGeoAddressPrivate &other)
        : QSharedData(other),
        sCountry(other.sCountry),
        sCountryCode(other.sCountryCode),
        sState(other.sState),
        sCounty(other.sCounty),
        sCity(other.sCity),
        sDistrict(other.sDistrict),
        sStreet(other.sStreet),
        sPostalCode(other.sPostalCode),
        sText(other.sText),
        m_autoGeneratedText(false)
{
}

QGeoAddressPrivate::~QGeoAddressPrivate()
{
}

/*!
    \class QGeoAddress
    \inmodule QtPositioning
    \ingroup QtPositioning-positioning
    \ingroup QtLocation-places-data
    \ingroup QtLocation-places
    \since 5.2

    \brief The QGeoAddress class represents an address of a \l QGeoLocation.

    The address' attributes are normalized to US feature names and can be mapped
    to the local feature levels (for example State matches "Bundesland" in Germany).

    The address contains a \l text() for displaying purposes and additional
    properties to access the components of an address:

    \list
        \li QGeoAddress::country()
        \li QGeoAddress::countryCode()
        \li QGeoAddress::state()
        \li QGeoAddress::city()
        \li QGeoAddress::district()
        \li QGeoAddress::street()
        \li QGeoAddress::postalCode()
    \endlist
*/

/*!
    Default constructor.
*/
QGeoAddress::QGeoAddress()
        : d(new QGeoAddressPrivate)
{
}

/*!
    Constructs a copy of \a other.
*/
QGeoAddress::QGeoAddress(const QGeoAddress &other)
        : d(other.d)
{
}

/*!
    Destroys this address.
*/
QGeoAddress::~QGeoAddress()
{
}

/*!
    Assigns the given \a address to this address and
    returns a reference to this address.
*/
QGeoAddress &QGeoAddress::operator=(const QGeoAddress & address)
{
    if (this == &address)
        return *this;

    d = address.d;
    return *this;
}

/*!
    Returns true if this address is equal to \a other,
    otherwise returns false.
*/
bool QGeoAddress::operator==(const QGeoAddress &other) const
{
#ifdef QGEOADDRESS_DEBUG
    qDebug() << "country" << (d->sCountry == other.country());
    qDebug() << "countryCode" << (d->sCountryCode == other.countryCode());
    qDebug() << "state:" <<  (d->sState == other.state());
    qDebug() << "county:" << (d->sCounty == other.county());
    qDebug() << "city:" << (d->sCity == other.city());
    qDebug() << "district:" << (d->sDistrict == other.district());
    qDebug() << "street:" << (d->sStreet == other.street());
    qDebug() << "postalCode:" << (d->sPostalCode == other.postalCode());
    qDebug() << "text:" << (text() == other.text());
#endif

    return d->sCountry == other.country() &&
           d->sCountryCode == other.countryCode() &&
           d->sState == other.state() &&
           d->sCounty == other.county() &&
           d->sCity == other.city() &&
           d->sDistrict == other.district() &&
           d->sStreet == other.street() &&
           d->sPostalCode == other.postalCode() &&
           this->text() == other.text();
}

/*!
    \fn bool QGeoAddress::operator!=(const QGeoAddress &other) const

    Returns true if this address is not equal to \a other,
    otherwise returns false.
*/

/*!
    Returns the address as a single formatted string. It is the recommended string
    to use to display the address to the user. It typically takes the format of
    an address as found on an envelope, but this is not always necessarily the case.

    The address text is either automatically generated or explicitly assigned.
    This can be determined by checking \l {QGeoAddress::isTextGenerated()} {isTextGenerated}.

    If an empty string is provided to setText(), then isTextGenerated() will be set
    to true and text() will return a string which is locally formatted according to
    countryCode() and based on the elements of the address such as street, city and so on.
    Because the text string is generated from the address elements, a sequence
    of calls such as text(), setStreet(), text() may return different strings for each
    invocation of text().

    If a non-empty string is provided to setText(), then isTextGenerated() will be
    set to false and text() will always return the explicitly assigned string.
    Calls to modify other elements such as setStreet(), setCity() and so on will not
    affect the resultant string from text().
*/
QString QGeoAddress::text() const
{
    if (d->sText.isEmpty())
        return formattedAddress(*this);
    else
        return d->sText;
}

/*!
    If \a text is not empty, explicitly assigns \a text as the string to be returned by
    text().  isTextGenerated() will return false.

    If \a text is empty, indicates that text() should be automatically generated
    from the address elements.  isTextGenerated() will return true.
*/
void QGeoAddress::setText(const QString &text)
{
    d->sText = text;
}

/*!
    Returns the country name.
*/
QString QGeoAddress::country() const
{
    return d->sCountry;
}

/*!
    Sets the \a country name.
*/
void QGeoAddress::setCountry(const QString &country)
{
    d->sCountry = country;
}

/*!
    Returns the country code according to ISO 3166-1 alpha-3
*/
QString QGeoAddress::countryCode() const
{
    return d->sCountryCode;
}

/*!
    Sets the \a countryCode according to ISO 3166-1 alpha-3
*/
void QGeoAddress::setCountryCode(const QString &countryCode)
{
    d->sCountryCode = countryCode;
}

/*!
    Returns the state.  The state is considered the first subdivision below country.
*/
QString QGeoAddress::state() const
{
    return d->sState;
}

/*!
    Sets the \a state.
*/
void QGeoAddress::setState(const QString &state)
{
    d->sState = state;
}

/*!
    Returns the county.  The county is considered the second subdivision below country.
*/
QString QGeoAddress::county() const
{
    return d->sCounty;
}

/*!
    Sets the \a county.
*/
void QGeoAddress::setCounty(const QString &county)
{
    d->sCounty = county;
}

/*!
    Returns the city.
*/
QString QGeoAddress::city() const
{
    return d->sCity;
}

/*!
    Sets the \a city.
*/
void QGeoAddress::setCity(const QString &city)
{
    d->sCity = city;
}

/*!
    Returns the district.  The district is considered the subdivison below city.
*/
QString QGeoAddress::district() const
{
    return d->sDistrict;
}

/*!
    Sets the \a district.
*/
void QGeoAddress::setDistrict(const QString &district)
{
    d->sDistrict = district;
}

/*!
    Returns the street-level component of the address.

    This typically includes a street number and street name
    but may also contain things like a unit number, a building
    name, or anything else that might be used to
    distinguish one address from another.
*/
QString QGeoAddress::street() const
{
    return d->sStreet;
}

/*!
    Sets the street-level component of the address to \a street.

    This typically includes a street number and street name
    but may also contain things like a unit number, a building
    name, or anything else that might be used to
    distinguish one address from another.
*/
void QGeoAddress::setStreet(const QString &street)
{
    d->sStreet = street;
}

/*!
    Returns the postal code.
*/
QString QGeoAddress::postalCode() const
{
    return d->sPostalCode;
}

/*!
    Sets the \a postalCode.
*/
void QGeoAddress::setPostalCode(const QString &postalCode)
{
    d->sPostalCode = postalCode;
}

/*!
    Returns whether this address is empty. An address is considered empty
    if \e all of its fields are empty.
*/
bool QGeoAddress::isEmpty() const
{
    return d->sCountry.isEmpty() &&
           d->sCountryCode.isEmpty() &&
           d->sState.isEmpty() &&
           d->sCounty.isEmpty() &&
           d->sCity.isEmpty() &&
           d->sDistrict.isEmpty() &&
           d->sStreet.isEmpty() &&
           d->sPostalCode.isEmpty() &&
           d->sText.isEmpty();

}

/*!
    Clears all of the address' data fields.
*/
void QGeoAddress::clear()
{
    d->sCountry.clear();
    d->sCountryCode.clear();
    d->sState.clear();
    d->sCounty.clear();
    d->sCity.clear();
    d->sDistrict.clear();
    d->sStreet.clear();
    d->sPostalCode.clear();
    d->sText.clear();
}

/*!
    Returns true if QGeoAddress::text() is automatically generated from address elements,
    otherwise returns false if text() has been explicitly assigned.

    \sa text(), setText()
*/
bool QGeoAddress::isTextGenerated() const
{
    return d->sText.isEmpty();
}

QT_END_NAMESPACE

