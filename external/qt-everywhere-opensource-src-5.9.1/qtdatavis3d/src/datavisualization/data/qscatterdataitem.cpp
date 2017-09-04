/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qscatterdataitem_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QScatterDataItem
 * \inmodule QtDataVisualization
 * \brief The QScatterDataItem class provides a container for resolved data to be added to scatter
 * graphs.
 * \since QtDataVisualization 1.0
 *
 * A scatter data item holds the data for a single rendered item in a scatter
 * graph. Scatter data proxies parse data into QScatterDataItem instances for
 * visualization.
 *
 * \sa QScatterDataProxy, {Qt Data Visualization C++ Classes}
 */

/*!
 * Constructs a scatter data item.
 */
QScatterDataItem::QScatterDataItem()
    : d_ptr(0) // private data doesn't exist by default (optimization)

{
}

/*!
 * Constructs a scatter data item at the position \a position.
 */
QScatterDataItem::QScatterDataItem(const QVector3D &position)
    : d_ptr(0),
      m_position(position)
{
}

/*!
 * Constructs a scatter data item at the position \a position with the rotation
 * \a rotation.
 */
QScatterDataItem::QScatterDataItem(const QVector3D &position, const QQuaternion &rotation)
    : d_ptr(0),
      m_position(position),
      m_rotation(rotation)
{
}

/*!
 * Constructs a copy of \a other.
 */
QScatterDataItem::QScatterDataItem(const QScatterDataItem &other)
{
    operator=(other);
}

/*!
 * Deletes a scatter data item.
 */
QScatterDataItem::~QScatterDataItem()
{
}

/*!
 *  Assigns a copy of \a other to this object.
 */
QScatterDataItem &QScatterDataItem::operator=(const QScatterDataItem &other)
{
    m_position = other.m_position;
    m_rotation = other.m_rotation;

    if (other.d_ptr)
        createExtraData();
    else
        d_ptr = 0;

    return *this;
}

/*!
 * \fn void QScatterDataItem::setPosition(const QVector3D &pos)
 * Sets the position \a pos for this data item.
 */

/*!
 * \fn QVector3D QScatterDataItem::position() const
 * Returns the position of this data item.
 */

/*!
 * \fn void QScatterDataItem::setRotation(const QQuaternion &rot)
 * Sets the rotation \a rot for this data item.
 * The value of \a rot should be a normalized QQuaternion.
 * If the series also has rotation, item rotation is multiplied by it.
 * Defaults to no rotation.
 */

/*!
 * \fn QQuaternion QScatterDataItem::rotation() const
 * Returns the rotation of this data item.
 * \sa setRotation()
 */

/*!
 * \fn void QScatterDataItem::setX(float value)
 * Sets the x-coordinate of the item position to the value \a value.
 */

/*!
 * \fn void QScatterDataItem::setY(float value)
 * Sets the y-coordinate of the item position to the value \a value.
 */

/*!
 * \fn void QScatterDataItem::setZ(float value)
 * Sets the z-coordinate of the item position to the value \a value.
 */

/*!
 * \fn float QScatterDataItem::x() const
 * Returns the x-coordinate of the position of this data item.
 */

/*!
 * \fn float QScatterDataItem::y() const
 * Returns the y-coordinate of the position of this data item.
 */

/*!
 * \fn float QScatterDataItem::z() const
 * Returns the z-coordinate of the position of this data item.
 */

/*!
 * \internal
 */
void QScatterDataItem::createExtraData()
{
    if (!d_ptr)
        d_ptr = new QScatterDataItemPrivate;
}

QScatterDataItemPrivate::QScatterDataItemPrivate()
{
}

QScatterDataItemPrivate::~QScatterDataItemPrivate()
{
}

QT_END_NAMESPACE_DATAVISUALIZATION
