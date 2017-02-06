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

#include "qgeomaptype_p.h"
#include "qgeomaptype_p_p.h"

QT_BEGIN_NAMESPACE

QGeoMapType::QGeoMapType()
    : d_ptr(new QGeoMapTypePrivate()) {}

QGeoMapType::QGeoMapType(const QGeoMapType &other)
    : d_ptr(other.d_ptr) {}

QGeoMapType::QGeoMapType(QGeoMapType::MapStyle style, const QString &name,
                         const QString &description, bool mobile, bool night, int mapId)
:   d_ptr(new QGeoMapTypePrivate(style, name, description, mobile, night, mapId))
{
}

QGeoMapType::~QGeoMapType() {}

QGeoMapType &QGeoMapType::operator = (const QGeoMapType &other)
{
    if (this == &other)
        return *this;

    d_ptr = other.d_ptr;
    return *this;
}

bool QGeoMapType::operator == (const QGeoMapType &other) const
{
    return (*d_ptr.constData() == *other.d_ptr.constData());
}

bool QGeoMapType::operator != (const QGeoMapType &other) const
{
    return !(operator ==(other));
}

QGeoMapType::MapStyle QGeoMapType::style() const
{
    return d_ptr->style_;
}

QString QGeoMapType::name() const
{
    return d_ptr->name_;
}

QString QGeoMapType::description() const
{
    return d_ptr->description_;
}

bool QGeoMapType::mobile() const
{
    return d_ptr->mobile_;
}

bool QGeoMapType::night() const
{
    return d_ptr->night_;
}

int QGeoMapType::mapId() const
{
    return d_ptr->mapId_;
}

QGeoMapTypePrivate::QGeoMapTypePrivate()
:   style_(QGeoMapType::NoMap), mobile_(false), night_(false), mapId_(0)
{
}

QGeoMapTypePrivate::QGeoMapTypePrivate(const QGeoMapTypePrivate &other)
:   QSharedData(other), style_(other.style_), name_(other.name_), description_(other.description_),
    mobile_(other.mobile_), night_(other.night_), mapId_(other.mapId_)
{
}

QGeoMapTypePrivate::QGeoMapTypePrivate(QGeoMapType::MapStyle style, const QString &name,
                                       const QString &description, bool mobile, bool night,
                                       int mapId)
:   style_(style), name_(name), description_(description), mobile_(mobile), night_(night),
    mapId_(mapId)
{
}

QGeoMapTypePrivate::~QGeoMapTypePrivate()
{
}

bool QGeoMapTypePrivate::operator==(const QGeoMapTypePrivate &other) const
{
    return style_ == other.style_ && name_ == other.name_ && description_ == other.description_ &&
           mobile_ == other.mobile_ && night_ == other.night_ && mapId_ == other.mapId_;
}

QT_END_NAMESPACE
