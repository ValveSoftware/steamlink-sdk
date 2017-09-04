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

#include "qsgsoftwarespritenode_p.h"
#include "qsgsoftwarepixmaptexture_p.h"
#include <QtGui/QPainter>

QT_BEGIN_NAMESPACE

QSGSoftwareSpriteNode::QSGSoftwareSpriteNode()
{
    setMaterial((QSGMaterial*)1);
    setGeometry((QSGGeometry*)1);
}

void QSGSoftwareSpriteNode::setTexture(QSGTexture *texture)
{
    m_texture = qobject_cast<QSGSoftwarePixmapTexture*>(texture);
    markDirty(DirtyMaterial);
}

void QSGSoftwareSpriteNode::setTime(float time)
{
    if (m_time != time) {
        m_time = time;
        markDirty(DirtyMaterial);
    }
}

void QSGSoftwareSpriteNode::setSourceA(const QPoint &source)
{
    if (m_sourceA != source) {
        m_sourceA = source;
        markDirty(DirtyMaterial);
    }
}

void QSGSoftwareSpriteNode::setSourceB(const QPoint &source)
{
    if (m_sourceB != source) {
        m_sourceB = source;
        markDirty(DirtyMaterial);
    }
}

void QSGSoftwareSpriteNode::setSpriteSize(const QSize &size)
{
    if (m_spriteSize != size) {
        m_spriteSize = size;
        markDirty(DirtyMaterial);
    }
}

void QSGSoftwareSpriteNode::setSheetSize(const QSize &size)
{
    if (m_sheetSize != size) {
        m_sheetSize = size;
        markDirty(DirtyMaterial);
    }
}

void QSGSoftwareSpriteNode::setSize(const QSizeF &size)
{
    if (m_size != size) {
        m_size = size;
        markDirty(DirtyGeometry);
    }
}

void QSGSoftwareSpriteNode::setFiltering(QSGTexture::Filtering filtering)
{
    Q_UNUSED(filtering);
}

void QSGSoftwareSpriteNode::update()
{
}

void QSGSoftwareSpriteNode::paint(QPainter *painter)
{
    //Get the pixmap handle from the texture
    if (!m_texture)
        return;

    const QPixmap &pixmap = m_texture->pixmap();

    // XXX try to do some kind of interpolation between sourceA and sourceB using time
    painter->drawPixmap(QRectF(0, 0, m_size.width(), m_size.height()),
                        pixmap,
                        QRectF(m_sourceA, m_spriteSize));
}

bool QSGSoftwareSpriteNode::isOpaque() const
{
    return false;
}

QRectF QSGSoftwareSpriteNode::rect() const
{
    return QRectF(0, 0, m_size.width(), m_size.height());
}

QT_END_NAMESPACE
