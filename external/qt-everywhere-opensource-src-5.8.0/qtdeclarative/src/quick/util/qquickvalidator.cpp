/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickvalidator_p.h"

QT_BEGIN_NAMESPACE

#ifndef QT_NO_VALIDATOR

/*!
    \qmltype IntValidator
    \instantiates QIntValidator
    \inqmlmodule QtQuick
    \ingroup qtquick-text-utility
    \brief Defines a validator for integer values

    The IntValidator type provides a validator for integer values.

    If no \l locale is set IntValidator uses the \l {QLocale::setDefault()}{default locale} to
    interpret the number and will accept locale specific digits, group separators, and positive
    and negative signs.  In addition, IntValidator is always guaranteed to accept a number
    formatted according to the "C" locale.
*/

QQuickIntValidator::QQuickIntValidator(QObject *parent)
    : QIntValidator(parent)
{
}

/*!
    \qmlproperty string QtQuick::IntValidator::locale

    This property holds the name of the locale used to interpret the number.

    \sa {QtQml::Qt::locale()}{Qt.locale()}
*/

QString QQuickIntValidator::localeName() const
{
    return locale().name();
}

void QQuickIntValidator::setLocaleName(const QString &name)
{
    if (locale().name() != name) {
        setLocale(QLocale(name));
        emit localeNameChanged();
    }
}

void QQuickIntValidator::resetLocaleName()
{
    QLocale defaultLocale;
    if (locale() != defaultLocale) {
        setLocale(defaultLocale);
        emit localeNameChanged();
    }
}

/*!
    \qmlproperty int QtQuick::IntValidator::top

    This property holds the validator's highest acceptable value.
    By default, this property's value is derived from the highest signed integer available (typically 2147483647).
*/
/*!
    \qmlproperty int QtQuick::IntValidator::bottom

    This property holds the validator's lowest acceptable value.
    By default, this property's value is derived from the lowest signed integer available (typically -2147483647).
*/

/*!
    \qmltype DoubleValidator
    \instantiates QDoubleValidator
    \inqmlmodule QtQuick
    \ingroup qtquick-text-utility
    \brief Defines a validator for non-integer numbers

    The DoubleValidator type provides a validator for non-integer numbers.

    Input is accepted if it contains a double that is within the valid range
    and is in the  correct format.

    Input is accepected but invalid if it contains a double that is outside
    the range or is in the wrong format; e.g. with too many digits after the
    decimal point or is empty.

    Input is rejected if it is not a double.

    Note: If the valid range consists of just positive doubles (e.g. 0.0 to
    100.0) and input is a negative double then it is rejected. If \l notation
    is set to DoubleValidator.StandardNotation, and  the input contains more
    digits before the decimal point than a double in the valid range may have,
    it is also rejected. If \l notation is DoubleValidator.ScientificNotation,
    and the input is not in the valid range, it is accecpted but invalid. The
    value may yet become valid by changing the exponent.
*/

QQuickDoubleValidator::QQuickDoubleValidator(QObject *parent)
    : QDoubleValidator(parent)
{
}

/*!
    \qmlproperty string QtQuick::DoubleValidator::locale

    This property holds the name of the locale used to interpret the number.

    \sa {QtQml::Qt::locale()}{Qt.locale()}
*/

QString QQuickDoubleValidator::localeName() const
{
    return locale().name();
}

void QQuickDoubleValidator::setLocaleName(const QString &name)
{
    if (locale().name() != name) {
        setLocale(QLocale(name));
        emit localeNameChanged();
    }
}

void QQuickDoubleValidator::resetLocaleName()
{
    QLocale defaultLocale;
    if (locale() != defaultLocale) {
        setLocale(defaultLocale);
        emit localeNameChanged();
    }
}

/*!
    \qmlproperty real QtQuick::DoubleValidator::top

    This property holds the validator's maximum acceptable value.
    By default, this property contains a value of infinity.
*/
/*!
    \qmlproperty real QtQuick::DoubleValidator::bottom

    This property holds the validator's minimum acceptable value.
    By default, this property contains a value of -infinity.
*/
/*!
    \qmlproperty int QtQuick::DoubleValidator::decimals

    This property holds the validator's maximum number of digits after the decimal point.
    By default, this property contains a value of 1000.
*/
/*!
    \qmlproperty enumeration QtQuick::DoubleValidator::notation
    This property holds the notation of how a string can describe a number.

    The possible values for this property are:

    \list
    \li DoubleValidator.StandardNotation
    \li DoubleValidator.ScientificNotation (default)
    \endlist

    If this property is set to DoubleValidator.ScientificNotation, the written number may have an exponent part (e.g. 1.5E-2).
*/

/*!
    \qmltype RegExpValidator
    \instantiates QRegExpValidator
    \inqmlmodule QtQuick
    \ingroup qtquick-text-utility
    \brief Provides a string validator

    The RegExpValidator type provides a validator, which counts as valid any string which
    matches a specified regular expression.
*/
/*!
   \qmlproperty regExp QtQuick::RegExpValidator::regExp

   This property holds the regular expression used for validation.

   Note that this property should be a regular expression in JS syntax, e.g /a/ for the regular expression
   matching "a".

   By default, this property contains a regular expression with the pattern .* that matches any string.
*/

#endif // QT_NO_VALIDATOR

QT_END_NAMESPACE

