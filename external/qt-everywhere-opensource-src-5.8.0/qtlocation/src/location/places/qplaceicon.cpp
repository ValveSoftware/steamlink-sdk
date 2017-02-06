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

#include "qplaceicon.h"
#include "qplaceicon_p.h"
#include "qplacemanager.h"
#include "qplacemanagerengine.h"

QT_USE_NAMESPACE

QPlaceIconPrivate::QPlaceIconPrivate()
    : QSharedData(), manager(0)
{
}

QPlaceIconPrivate::QPlaceIconPrivate(const QPlaceIconPrivate &other)
    : QSharedData(other),
      manager(other.manager),
      parameters(other.parameters)
{
}

QPlaceIconPrivate::~QPlaceIconPrivate()
{
}

QPlaceIconPrivate &QPlaceIconPrivate::operator=(const QPlaceIconPrivate &other)
{
    if (this == &other)
        return *this;

    manager = other.manager;
    parameters = other.parameters;

    return *this;
}

bool QPlaceIconPrivate::operator == (const QPlaceIconPrivate &other) const
{
    return manager == other.manager
            && parameters == other.parameters;
}

/*!
    \class QPlaceIcon
    \inmodule QtLocation
    \ingroup QtLocation-places
    \ingroup QtLocation-places-data
    \since 5.6

    \brief The QPlaceIcon class represents an icon.

    The typical usage of an icon is to use the url() function to specify
    a preferred icon size.

    \snippet places/requesthandler.h icon

    The icons are typically backend dependent, if a manager backend does not support a given size, the URL of the icon that most
    closely matches those parameters is returned.

    The icon class also has a key-value set of parameters.  The precise key one
    needs to use depends on the \l {Qt Location#Plugin References and Parameters}{plugin}
    being used.  These parameters influence which icon URL is returned by
    the manager and may also be used to specify icon URL locations when
    saving icons.

    If there is only ever one image for an icon, then QPlaceIcon::SingleUrl can be used as a parameter
    key with a QUrl as the associated value.  If this key is set, then the url() function will always return the specified URL
    and not defer to any manager.
*/

/*!
    \variable QPlaceIcon::SingleUrl
    \brief Parameter key for an icon that only has a single image URL.

    The parameter value to be used with this key is a QUrl.  An icon with this parameter set will
    always return the specified URL regardless of the requested size when url() is called.
*/
const QString QPlaceIcon::SingleUrl(QLatin1String("singleUrl"));

/*!
    Constructs an icon.
*/
QPlaceIcon::QPlaceIcon()
    : d(new QPlaceIconPrivate)
{
}

/*!
    Constructs a copy of \a other.
*/
QPlaceIcon::QPlaceIcon(const QPlaceIcon &other)
    : d(other.d)
{
}

/*!
    Destroys the icon.
*/
QPlaceIcon::~QPlaceIcon()
{
}

/*!
    Assigns \a other to this icon and returns a reference to this icon.
*/
QPlaceIcon &QPlaceIcon::operator=(const QPlaceIcon &other)
{
    if (this == &other)
        return *this;

    d = other.d;
    return *this;
}

/*!
    Returns true if this icon is equal to \a other, otherwise returns false.
*/
bool QPlaceIcon::operator==(const QPlaceIcon &other) const
{
    return *d == *(other.d);
}

/*!
    \fn QPlaceIcon::operator!=(const QPlaceIcon &other) const

    Returns true if \a other is not equal to this icon, otherwise returns false.
*/

/*!
    Returns an icon URL according to the given \a size.

    If no manager has been assigned to the icon, and the parameters do not contain the QPlaceIcon::SingleUrl key, a default constructed QUrl
    is returned.
*/
QUrl QPlaceIcon::url(const QSize &size) const
{
    if (d->parameters.contains(QPlaceIcon::SingleUrl)) {
        QVariant value = d->parameters.value(QPlaceIcon::SingleUrl);
        if (value.type() == QVariant::Url)
            return value.toUrl();
        else if (value.type() == QVariant::String)
            return QUrl::fromUserInput(value.toString());

        return QUrl();
    }

    if (!d->manager)
        return QUrl();

    return d->manager->d->constructIconUrl(*this, size);
}

/*!
    Returns a set of parameters for the icon that are manager/plugin specific.
    These parameters are used by the manager to return the appropriate
    URL when url() is called and to specify locations to save to
    when saving icons.

    Consult the \l {Qt Location#Plugin References and Parameters}{plugin documentation}
    for what parameters are supported and how they should be used.
*/
QVariantMap QPlaceIcon::parameters() const
{
    return d->parameters;
}

/*!
    Sets the parameters of the icon to \a parameters.
*/
void QPlaceIcon::setParameters(const QVariantMap &parameters)
{
    d->parameters = parameters;
}

/*!
    Returns the manager that this icon is associated with.
*/
QPlaceManager *QPlaceIcon::manager() const
{
    return d->manager;
}

/*!
    Sets the \a manager that this icon is associated with.  The icon does not take
    ownership of the pointer.
*/
void QPlaceIcon::setManager(QPlaceManager *manager)
{
    d->manager = manager;
}

/*!
    Returns a boolean indicating whether the all the fields of the icon are empty or not.
*/
bool QPlaceIcon::isEmpty() const
{
    return (d->manager == 0
            && d->parameters.isEmpty());
}
