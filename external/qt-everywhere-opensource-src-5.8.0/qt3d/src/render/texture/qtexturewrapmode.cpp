/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qtexturewrapmode.h"
#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

class QTextureWrapModePrivate : public QObjectPrivate
{
public:
    QTextureWrapModePrivate()
        : QObjectPrivate()
        , m_x(QTextureWrapMode::ClampToEdge)
        , m_y(QTextureWrapMode::ClampToEdge)
        , m_z(QTextureWrapMode::ClampToEdge)
    {
    }

    Q_DECLARE_PUBLIC(QTextureWrapMode)
    QTextureWrapMode::WrapMode m_x;
    QTextureWrapMode::WrapMode m_y;
    QTextureWrapMode::WrapMode m_z;
};

/*!
    \class Qt3DRender::QTextureWrapMode
    \inmodule Qt3DRender
    \since 5.5

    \brief Defines the wrap mode a Qt3DRender::QAbstractTexture
    should apply to a texture.
 */

QTextureWrapMode::QTextureWrapMode(WrapMode wrapMode, QObject *parent)
    : QObject(*new QTextureWrapModePrivate, parent)
{
    d_func()->m_x = wrapMode;
    d_func()->m_y = wrapMode;
    d_func()->m_z = wrapMode;
}

/*!
    Contrusts a new Qt3DRender::QTextureWrapMode instance with the wrap mode to apply to
    each dimension \a x, \a y \a z of the texture and \a parent as parent.
 */
QTextureWrapMode::QTextureWrapMode(WrapMode x,WrapMode y, WrapMode z, QObject *parent)
    : QObject(*new QTextureWrapModePrivate, parent)
{
    d_func()->m_x = x;
    d_func()->m_y = y;
    d_func()->m_z = z;
}

/*! \internal */
QTextureWrapMode::~QTextureWrapMode()
{
}

/*!
    Sets the wrap mode of the x dimension to \a x.
 */
void QTextureWrapMode::setX(WrapMode x)
{
    Q_D(QTextureWrapMode);
    if (d->m_x != x) {
        d->m_x = x;
        emit xChanged(x);
    }
}

/*!
    Returns the wrap mode of the x dimension.
 */
QTextureWrapMode::WrapMode QTextureWrapMode::x() const
{
    Q_D(const QTextureWrapMode);
    return d->m_x;
}

/*!
    Sets the wrap mode of the y dimension to \a y.
    \note this is not available on 1D textures.
 */
void QTextureWrapMode::setY(WrapMode y)
{
    Q_D(QTextureWrapMode);
    if (d->m_y != y) {
        d->m_y = y;
        emit yChanged(y);
    }
}

/*!
    Returns the wrap mode of the y dimension.
 */
QTextureWrapMode::WrapMode QTextureWrapMode::y() const
{
    Q_D(const QTextureWrapMode);
    return d->m_y;
}

/*!
    Sets the wrap mode of the z dimension to \a z.
    \note this is only available on 3D textures.
 */
void QTextureWrapMode::setZ(WrapMode z)
{
    Q_D(QTextureWrapMode);
    if (d->m_z != z) {
        d->m_z = z;
        emit zChanged(z);
    }
}

/*!
    Returns the wrap mode of the z dimension.
 */
QTextureWrapMode::WrapMode QTextureWrapMode::z() const
{
    Q_D(const QTextureWrapMode);
    return d->m_z;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
