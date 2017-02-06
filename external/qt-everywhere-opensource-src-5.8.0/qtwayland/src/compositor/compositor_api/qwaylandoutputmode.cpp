/****************************************************************************
**
** Copyright (C) 2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
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

#include "qwaylandoutputmode.h"
#include "qwaylandoutputmode_p.h"

/*!
   \class QWaylandOutputMode
   \inmodule QtWaylandCompositor
   \since 5.8
   \brief The QWaylandOutputMode class holds the resolution and refresh rate of an output.

   QWaylandOutputMode holds the resolution and refresh rate of an output.
   Resolution is expressed in pixels and refresh rate is measured in mHz.

   \sa QWaylandOutput
*/

QWaylandOutputMode::QWaylandOutputMode()
    : d(new QWaylandOutputModePrivate)
{
}

QWaylandOutputMode::QWaylandOutputMode(const QSize &size, int refreshRate)
    : d(new QWaylandOutputModePrivate)
{
    d->size = size;
    d->refreshRate = refreshRate;
}

QWaylandOutputMode::QWaylandOutputMode(const QWaylandOutputMode &other)
    : d(new QWaylandOutputModePrivate)
{
    d->size = other.size();
    d->refreshRate = other.refreshRate();
}

QWaylandOutputMode::~QWaylandOutputMode()
{
    delete d;
}

QWaylandOutputMode &QWaylandOutputMode::operator=(const QWaylandOutputMode &other)
{
    d->size = other.size();
    d->refreshRate = other.refreshRate();
    return *this;
}

/*!
    Returns \c true if this mode is equal to \a other,
    otherwise returns \c false.
*/
bool QWaylandOutputMode::operator==(const QWaylandOutputMode &other) const
{
    return size() == other.size() && refreshRate() == refreshRate();
}

/*!
    Returns \c true if this mode is not equal to \a other,
    otherwise returns \c false.
*/
bool QWaylandOutputMode::operator!=(const QWaylandOutputMode &other) const
{
    return size() != other.size() || refreshRate() != refreshRate();
}

/*!
    Returns whether this mode contains a valid resolution and refresh rate.
*/
bool QWaylandOutputMode::isValid() const
{
    return !d->size.isEmpty() && d->refreshRate > 0;
}

/*!
    Returns the resolution in pixels.
*/
QSize QWaylandOutputMode::size() const
{
    return d->size;
}

/*!
    Returns the refresh rate in mHz.
*/
int QWaylandOutputMode::refreshRate() const
{
    return d->refreshRate;
}

/*!
 * \internal
 */
void QWaylandOutputMode::setWidth(int width)
{
    d->size.setWidth(width);
}

/*!
 * \internal
 */
void QWaylandOutputMode::setHeight(int height)
{
    d->size.setHeight(height);
}
