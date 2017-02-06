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

#include "qsgsoftwareinternalimagenode_p.h"

#include "qsgsoftwarepixmaptexture_p.h"
#include "qsgsoftwarelayer_p.h"
#include <QPainter>
#include <qmath.h>

QT_BEGIN_NAMESPACE

namespace QSGSoftwareHelpers {
// Helper from widgets/styles/qdrawutil.cpp

static inline QMargins normalizedMargins(const QMargins &m)
{
    return QMargins(qMax(m.left(), 0), qMax(m.top(), 0), qMax(m.right(), 0), qMax(m.bottom(), 0));
}

void qDrawBorderPixmap(QPainter *painter, const QRect &targetRect, const QMargins &targetMarginsIn,
                       const QPixmap &pixmap, const QRect &sourceRect, const QMargins &sourceMarginsIn,
                       const QTileRules &rules, QDrawBorderPixmap::DrawingHints hints)
{
    QPainter::PixmapFragment d;
    d.opacity = 1.0;
    d.rotation = 0.0;

    QPixmapFragmentsArray opaqueData;
    QPixmapFragmentsArray translucentData;

    QMargins sourceMargins = normalizedMargins(sourceMarginsIn);
    QMargins targetMargins = normalizedMargins(targetMarginsIn);

    // source center
    const int sourceCenterTop = sourceRect.top() + sourceMargins.top();
    const int sourceCenterLeft = sourceRect.left() + sourceMargins.left();
    const int sourceCenterBottom = sourceRect.bottom() - sourceMargins.bottom() + 1;
    const int sourceCenterRight = sourceRect.right() - sourceMargins.right() + 1;
    const int sourceCenterWidth = sourceCenterRight - sourceCenterLeft;
    const int sourceCenterHeight = sourceCenterBottom - sourceCenterTop;
    // target center
    const int targetCenterTop = targetRect.top() + targetMargins.top();
    const int targetCenterLeft = targetRect.left() + targetMargins.left();
    const int targetCenterBottom = targetRect.bottom() - targetMargins.bottom() + 1;
    const int targetCenterRight = targetRect.right() - targetMargins.right() + 1;
    const int targetCenterWidth = targetCenterRight - targetCenterLeft;
    const int targetCenterHeight = targetCenterBottom - targetCenterTop;

    QVarLengthArray<qreal, 16> xTarget; // x-coordinates of target rectangles
    QVarLengthArray<qreal, 16> yTarget; // y-coordinates of target rectangles

    int columns = 3;
    int rows = 3;
    if (rules.horizontal != Qt::StretchTile && sourceCenterWidth != 0)
        columns = qMax(3, 2 + qCeil(targetCenterWidth / qreal(sourceCenterWidth)));
    if (rules.vertical != Qt::StretchTile && sourceCenterHeight != 0)
        rows = qMax(3, 2 + qCeil(targetCenterHeight / qreal(sourceCenterHeight)));

    xTarget.resize(columns + 1);
    yTarget.resize(rows + 1);

    bool oldAA = painter->testRenderHint(QPainter::Antialiasing);
    if (painter->paintEngine()->type() != QPaintEngine::OpenGL
        && painter->paintEngine()->type() != QPaintEngine::OpenGL2
        && oldAA && painter->combinedTransform().type() != QTransform::TxNone) {
        painter->setRenderHint(QPainter::Antialiasing, false);
    }

    xTarget[0] = targetRect.left();
    xTarget[1] = targetCenterLeft;
    xTarget[columns - 1] = targetCenterRight;
    xTarget[columns] = targetRect.left() + targetRect.width();

    yTarget[0] = targetRect.top();
    yTarget[1] = targetCenterTop;
    yTarget[rows - 1] = targetCenterBottom;
    yTarget[rows] = targetRect.top() + targetRect.height();

    qreal dx = targetCenterWidth;
    qreal dy = targetCenterHeight;

    switch (rules.horizontal) {
    case Qt::StretchTile:
        dx = targetCenterWidth;
        break;
    case Qt::RepeatTile:
        dx = sourceCenterWidth;
        break;
    case Qt::RoundTile:
        dx = targetCenterWidth / qreal(columns - 2);
        break;
    }

    for (int i = 2; i < columns - 1; ++i)
        xTarget[i] = xTarget[i - 1] + dx;

    switch (rules.vertical) {
    case Qt::StretchTile:
        dy = targetCenterHeight;
        break;
    case Qt::RepeatTile:
        dy = sourceCenterHeight;
        break;
    case Qt::RoundTile:
        dy = targetCenterHeight / qreal(rows - 2);
        break;
    }

    for (int i = 2; i < rows - 1; ++i)
        yTarget[i] = yTarget[i - 1] + dy;

    // corners
    if (targetMargins.top() > 0 && targetMargins.left() > 0 && sourceMargins.top() > 0 && sourceMargins.left() > 0) { // top left
        d.x = (0.5 * (xTarget[1] + xTarget[0]));
        d.y = (0.5 * (yTarget[1] + yTarget[0]));
        d.sourceLeft = sourceRect.left();
        d.sourceTop = sourceRect.top();
        d.width = sourceMargins.left();
        d.height = sourceMargins.top();
        d.scaleX = qreal(xTarget[1] - xTarget[0]) / d.width;
        d.scaleY = qreal(yTarget[1] - yTarget[0]) / d.height;
        if (hints & QDrawBorderPixmap::OpaqueTopLeft)
            opaqueData.append(d);
        else
            translucentData.append(d);
    }
    if (targetMargins.top() > 0 && targetMargins.right() > 0 && sourceMargins.top() > 0 && sourceMargins.right() > 0) { // top right
        d.x = (0.5 * (xTarget[columns] + xTarget[columns - 1]));
        d.y = (0.5 * (yTarget[1] + yTarget[0]));
        d.sourceLeft = sourceCenterRight;
        d.sourceTop = sourceRect.top();
        d.width = sourceMargins.right();
        d.height = sourceMargins.top();
        d.scaleX = qreal(xTarget[columns] - xTarget[columns - 1]) / d.width;
        d.scaleY = qreal(yTarget[1] - yTarget[0]) / d.height;
        if (hints & QDrawBorderPixmap::OpaqueTopRight)
            opaqueData.append(d);
        else
            translucentData.append(d);
    }
    if (targetMargins.bottom() > 0 && targetMargins.left() > 0 && sourceMargins.bottom() > 0 && sourceMargins.left() > 0) { // bottom left
        d.x = (0.5 * (xTarget[1] + xTarget[0]));
        d.y =(0.5 * (yTarget[rows] + yTarget[rows - 1]));
        d.sourceLeft = sourceRect.left();
        d.sourceTop = sourceCenterBottom;
        d.width = sourceMargins.left();
        d.height = sourceMargins.bottom();
        d.scaleX = qreal(xTarget[1] - xTarget[0]) / d.width;
        d.scaleY = qreal(yTarget[rows] - yTarget[rows - 1]) / d.height;
        if (hints & QDrawBorderPixmap::OpaqueBottomLeft)
            opaqueData.append(d);
        else
            translucentData.append(d);
    }
    if (targetMargins.bottom() > 0 && targetMargins.right() > 0 && sourceMargins.bottom() > 0 && sourceMargins.right() > 0) { // bottom right
        d.x = (0.5 * (xTarget[columns] + xTarget[columns - 1]));
        d.y = (0.5 * (yTarget[rows] + yTarget[rows - 1]));
        d.sourceLeft = sourceCenterRight;
        d.sourceTop = sourceCenterBottom;
        d.width = sourceMargins.right();
        d.height = sourceMargins.bottom();
        d.scaleX = qreal(xTarget[columns] - xTarget[columns - 1]) / d.width;
        d.scaleY = qreal(yTarget[rows] - yTarget[rows - 1]) / d.height;
        if (hints & QDrawBorderPixmap::OpaqueBottomRight)
            opaqueData.append(d);
        else
            translucentData.append(d);
    }

    // horizontal edges
    if (targetCenterWidth > 0 && sourceCenterWidth > 0) {
        if (targetMargins.top() > 0 && sourceMargins.top() > 0) { // top
            QPixmapFragmentsArray &data = hints & QDrawBorderPixmap::OpaqueTop ? opaqueData : translucentData;
            d.sourceLeft = sourceCenterLeft;
            d.sourceTop = sourceRect.top();
            d.width = sourceCenterWidth;
            d.height = sourceMargins.top();
            d.y = (0.5 * (yTarget[1] + yTarget[0]));
            d.scaleX = dx / d.width;
            d.scaleY = qreal(yTarget[1] - yTarget[0]) / d.height;
            for (int i = 1; i < columns - 1; ++i) {
                d.x = (0.5 * (xTarget[i + 1] + xTarget[i]));
                data.append(d);
            }
            if (rules.horizontal == Qt::RepeatTile)
                data[data.size() - 1].width = ((xTarget[columns - 1] - xTarget[columns - 2]) / d.scaleX);
        }
        if (targetMargins.bottom() > 0 && sourceMargins.bottom() > 0) { // bottom
            QPixmapFragmentsArray &data = hints & QDrawBorderPixmap::OpaqueBottom ? opaqueData : translucentData;
            d.sourceLeft = sourceCenterLeft;
            d.sourceTop = sourceCenterBottom;
            d.width = sourceCenterWidth;
            d.height = sourceMargins.bottom();
            d.y = (0.5 * (yTarget[rows] + yTarget[rows - 1]));
            d.scaleX = dx / d.width;
            d.scaleY = qreal(yTarget[rows] - yTarget[rows - 1]) / d.height;
            for (int i = 1; i < columns - 1; ++i) {
                d.x = (0.5 * (xTarget[i + 1] + xTarget[i]));
                data.append(d);
            }
            if (rules.horizontal == Qt::RepeatTile)
                data[data.size() - 1].width = ((xTarget[columns - 1] - xTarget[columns - 2]) / d.scaleX);
        }
    }

    // vertical edges
    if (targetCenterHeight > 0 && sourceCenterHeight > 0) {
        if (targetMargins.left() > 0 && sourceMargins.left() > 0) { // left
            QPixmapFragmentsArray &data = hints & QDrawBorderPixmap::OpaqueLeft ? opaqueData : translucentData;
            d.sourceLeft = sourceRect.left();
            d.sourceTop = sourceCenterTop;
            d.width = sourceMargins.left();
            d.height = sourceCenterHeight;
            d.x = (0.5 * (xTarget[1] + xTarget[0]));
            d.scaleX = qreal(xTarget[1] - xTarget[0]) / d.width;
            d.scaleY = dy / d.height;
            for (int i = 1; i < rows - 1; ++i) {
                d.y = (0.5 * (yTarget[i + 1] + yTarget[i]));
                data.append(d);
            }
            if (rules.vertical == Qt::RepeatTile)
                data[data.size() - 1].height = ((yTarget[rows - 1] - yTarget[rows - 2]) / d.scaleY);
        }
        if (targetMargins.right() > 0 && sourceMargins.right() > 0) { // right
            QPixmapFragmentsArray &data = hints & QDrawBorderPixmap::OpaqueRight ? opaqueData : translucentData;
            d.sourceLeft = sourceCenterRight;
            d.sourceTop = sourceCenterTop;
            d.width = sourceMargins.right();
            d.height = sourceCenterHeight;
            d.x = (0.5 * (xTarget[columns] + xTarget[columns - 1]));
            d.scaleX = qreal(xTarget[columns] - xTarget[columns - 1]) / d.width;
            d.scaleY = dy / d.height;
            for (int i = 1; i < rows - 1; ++i) {
                d.y = (0.5 * (yTarget[i + 1] + yTarget[i]));
                data.append(d);
            }
            if (rules.vertical == Qt::RepeatTile)
                data[data.size() - 1].height = ((yTarget[rows - 1] - yTarget[rows - 2]) / d.scaleY);
        }
    }

    // center
    if (targetCenterWidth > 0 && targetCenterHeight > 0 && sourceCenterWidth > 0 && sourceCenterHeight > 0) {
        QPixmapFragmentsArray &data = hints & QDrawBorderPixmap::OpaqueCenter ? opaqueData : translucentData;
        d.sourceLeft = sourceCenterLeft;
        d.sourceTop = sourceCenterTop;
        d.width = sourceCenterWidth;
        d.height = sourceCenterHeight;
        d.scaleX = dx / d.width;
        d.scaleY = dy / d.height;

        qreal repeatWidth = (xTarget[columns - 1] - xTarget[columns - 2]) / d.scaleX;
        qreal repeatHeight = (yTarget[rows - 1] - yTarget[rows - 2]) / d.scaleY;

        for (int j = 1; j < rows - 1; ++j) {
            d.y = (0.5 * (yTarget[j + 1] + yTarget[j]));
            for (int i = 1; i < columns - 1; ++i) {
                d.x = (0.5 * (xTarget[i + 1] + xTarget[i]));
                data.append(d);
            }
            if (rules.horizontal == Qt::RepeatTile)
                data[data.size() - 1].width = repeatWidth;
        }
        if (rules.vertical == Qt::RepeatTile) {
            for (int i = 1; i < columns - 1; ++i)
                data[data.size() - i].height = repeatHeight;
        }
    }

    if (opaqueData.size())
        painter->drawPixmapFragments(opaqueData.data(), opaqueData.size(), pixmap, QPainter::OpaqueHint);
    if (translucentData.size())
        painter->drawPixmapFragments(translucentData.data(), translucentData.size(), pixmap);

    if (oldAA)
        painter->setRenderHint(QPainter::Antialiasing, true);
}

} // QSGSoftwareHelpers namespace

QSGSoftwareInternalImageNode::QSGSoftwareInternalImageNode()
    : m_innerSourceRect(0, 0, 1, 1)
    , m_subSourceRect(0, 0, 1, 1)
    , m_texture(0)
    , m_mirror(false)
    , m_smooth(true)
    , m_tileHorizontal(false)
    , m_tileVertical(false)
    , m_cachedMirroredPixmapIsDirty(false)
{
    setMaterial((QSGMaterial*)1);
    setGeometry((QSGGeometry*)1);
}


void QSGSoftwareInternalImageNode::setTargetRect(const QRectF &rect)
{
    if (rect == m_targetRect)
        return;
    m_targetRect = rect;
    markDirty(DirtyGeometry);
}

void QSGSoftwareInternalImageNode::setInnerTargetRect(const QRectF &rect)
{
    if (rect == m_innerTargetRect)
        return;
    m_innerTargetRect = rect;
    markDirty(DirtyGeometry);
}

void QSGSoftwareInternalImageNode::setInnerSourceRect(const QRectF &rect)
{
    if (rect == m_innerSourceRect)
        return;
    m_innerSourceRect = rect;
    markDirty(DirtyGeometry);
}

void QSGSoftwareInternalImageNode::setSubSourceRect(const QRectF &rect)
{
    if (rect == m_subSourceRect)
        return;
    m_subSourceRect = rect;
    markDirty(DirtyGeometry);
}

void QSGSoftwareInternalImageNode::setTexture(QSGTexture *texture)
{
    m_texture = texture;
    m_cachedMirroredPixmapIsDirty = true;
    markDirty(DirtyMaterial);
}

void QSGSoftwareInternalImageNode::setMirror(bool mirror)
{
    if (m_mirror != mirror) {
        m_mirror = mirror;
        m_cachedMirroredPixmapIsDirty = true;
        markDirty(DirtyMaterial);
    }
}

void QSGSoftwareInternalImageNode::setMipmapFiltering(QSGTexture::Filtering /*filtering*/)
{
}

void QSGSoftwareInternalImageNode::setFiltering(QSGTexture::Filtering filtering)
{
    bool smooth = (filtering == QSGTexture::Linear);
    if (smooth == m_smooth)
        return;

    m_smooth = smooth;
    markDirty(DirtyMaterial);
}

void QSGSoftwareInternalImageNode::setHorizontalWrapMode(QSGTexture::WrapMode wrapMode)
{
    bool tileHorizontal = (wrapMode == QSGTexture::Repeat);
    if (tileHorizontal == m_tileHorizontal)
        return;

    m_tileHorizontal = tileHorizontal;
    markDirty(DirtyMaterial);
}

void QSGSoftwareInternalImageNode::setVerticalWrapMode(QSGTexture::WrapMode wrapMode)
{
    bool tileVertical = (wrapMode == QSGTexture::Repeat);
    if (tileVertical == m_tileVertical)
        return;

    m_tileVertical = (wrapMode == QSGTexture::Repeat);
    markDirty(DirtyMaterial);
}

void QSGSoftwareInternalImageNode::update()
{
    if (m_cachedMirroredPixmapIsDirty) {
        if (m_mirror) {
            m_cachedMirroredPixmap = pixmap().transformed(QTransform(-1, 0, 0, 1, 0, 0));
        } else {
            //Cleanup cached pixmap if necessary
            if (!m_cachedMirroredPixmap.isNull())
                m_cachedMirroredPixmap = QPixmap();
        }
        m_cachedMirroredPixmapIsDirty = false;
    }
}

void QSGSoftwareInternalImageNode::preprocess()
{
    bool doDirty = false;
    QSGLayer *t = qobject_cast<QSGLayer *>(m_texture);
    if (t) {
        doDirty = t->updateTexture();
        markDirty(DirtyGeometry);
    }
    if (doDirty)
        markDirty(DirtyMaterial);
}

static Qt::TileRule getTileRule(qreal factor)
{
    int ifactor = qRound(factor);
    if (qFuzzyCompare(factor, ifactor )) {
        if (ifactor  == 1 || ifactor  == 0)
            return Qt::StretchTile;
        return Qt::RoundTile;
    }
    return Qt::RepeatTile;
}


void QSGSoftwareInternalImageNode::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::SmoothPixmapTransform, m_smooth);

    const QPixmap &pm = m_mirror ? m_cachedMirroredPixmap : pixmap();

    if (m_innerTargetRect != m_targetRect) {
        // border image
        QMargins margins(m_innerTargetRect.left() - m_targetRect.left(), m_innerTargetRect.top() - m_targetRect.top(),
                         m_targetRect.right() - m_innerTargetRect.right(), m_targetRect.bottom() - m_innerTargetRect.bottom());
        QSGSoftwareHelpers::QTileRules tilerules(getTileRule(m_subSourceRect.width()), getTileRule(m_subSourceRect.height()));
        QSGSoftwareHelpers::qDrawBorderPixmap(painter, m_targetRect.toRect(), margins, pm, QRect(0, 0, pm.width(), pm.height()),
                                              margins, tilerules, QSGSoftwareHelpers::QDrawBorderPixmap::DrawingHints(0));
        return;
    }

    if (m_tileHorizontal || m_tileVertical) {
        painter->save();
        qreal sx = m_targetRect.width()/(m_subSourceRect.width()*pm.width());
        qreal sy = m_targetRect.height()/(m_subSourceRect.height()*pm.height());
        QMatrix transform(sx, 0, 0, sy, 0, 0);
        painter->setMatrix(transform, true);
        painter->drawTiledPixmap(QRectF(m_targetRect.x()/sx, m_targetRect.y()/sy, m_targetRect.width()/sx, m_targetRect.height()/sy),
                                 pm,
                                 QPointF(m_subSourceRect.left()*pm.width(), m_subSourceRect.top()*pm.height()));
        painter->restore();
    } else {
        QRectF sr(m_subSourceRect.left()*pm.width(), m_subSourceRect.top()*pm.height(),
                  m_subSourceRect.width()*pm.width(), m_subSourceRect.height()*pm.height());
        painter->drawPixmap(m_targetRect, pm, sr);
    }
}

QRectF QSGSoftwareInternalImageNode::rect() const
{
    return m_targetRect;
}

const QPixmap &QSGSoftwareInternalImageNode::pixmap() const
{
    if (QSGSoftwarePixmapTexture *pt = qobject_cast<QSGSoftwarePixmapTexture*>(m_texture)) {
        return pt->pixmap();
    } else {
        QSGSoftwareLayer *layer = qobject_cast<QSGSoftwareLayer*>(m_texture);
        return layer->pixmap();
    }
}

QT_END_NAMESPACE
