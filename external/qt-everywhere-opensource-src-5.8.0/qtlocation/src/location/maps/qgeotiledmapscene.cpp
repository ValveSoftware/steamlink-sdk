/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2014 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
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
#include "qgeotiledmapscene_p.h"
#include "qgeocameradata_p.h"
#include "qabstractgeotilecache_p.h"
#include "qgeotilespec_p.h"
#include <QtPositioning/private/qdoublevector3d_p.h>
#include <QtCore/private/qobject_p.h>
#include <QtQuick/QSGImageNode>
#include <QtQuick/QQuickWindow>
#include <cmath>

QT_BEGIN_NAMESPACE

class QGeoTiledMapScenePrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QGeoTiledMapScene)
public:
    QGeoTiledMapScenePrivate();
    ~QGeoTiledMapScenePrivate();

    QSize m_screenSize; // in pixels
    int m_tileSize; // the pixel resolution for each tile
    QGeoCameraData m_cameraData;
    QSet<QGeoTileSpec> m_visibleTiles;

    QDoubleVector3D m_cameraUp;
    QDoubleVector3D m_cameraEye;
    QDoubleVector3D m_cameraCenter;
    QMatrix4x4 m_projectionMatrix;

    // scales up the tile geometry and the camera altitude, resulting in no visible effect
    // other than to control the accuracy of the render by keeping the values in a sensible range
    double m_scaleFactor;

    // rounded down, positive zoom is zooming in, corresponding to reduced altitude
    int m_intZoomLevel;

    // mercatorToGrid transform
    // the number of tiles in each direction for the whole map (earth) at the current zoom level.
    // it is 1<<zoomLevel
    int m_sideLength;

    QHash<QGeoTileSpec, QSharedPointer<QGeoTileTexture> > m_textures;

    // tilesToGrid transform
    int m_minTileX; // the minimum tile index, i.e. 0 to sideLength which is 1<< zoomLevel
    int m_minTileY;
    int m_maxTileX;
    int m_maxTileY;
    int m_tileXWrapsBelow; // the wrap point as a tile index

    // cameraToGrid transform
    double m_mercatorCenterX; // center of camera in grid space (0 to sideLength)
    double m_mercatorCenterY;
    double m_mercatorWidth;   // width of camera in grid space (0 to sideLength)
    double m_mercatorHeight;

    // screenToWindow transform
    double m_screenOffsetX; // in pixels
    double m_screenOffsetY; // in pixels
    // cameraToScreen transform
    double m_screenWidth; // in pixels
    double m_screenHeight; // in pixels

    bool m_useVerticalLock;
    bool m_verticalLock;
    bool m_linearScaling;

    bool m_dropTextures;

    void addTile(const QGeoTileSpec &spec, QSharedPointer<QGeoTileTexture> texture);

    QDoubleVector2D itemPositionToMercator(const QDoubleVector2D &pos) const;
    QDoubleVector2D mercatorToItemPosition(const QDoubleVector2D &mercator) const;

    void setVisibleTiles(const QSet<QGeoTileSpec> &tiles);
    void removeTiles(const QSet<QGeoTileSpec> &oldTiles);
    bool buildGeometry(const QGeoTileSpec &spec, QSGImageNode *imageNode);
    void setTileBounds(const QSet<QGeoTileSpec> &tiles);
    void setupCamera();
};

QGeoTiledMapScene::QGeoTiledMapScene(QObject *parent)
    : QObject(*new QGeoTiledMapScenePrivate(),parent)
{
}

QGeoTiledMapScene::~QGeoTiledMapScene()
{
}

void QGeoTiledMapScene::setUseVerticalLock(bool lock)
{
    Q_D(QGeoTiledMapScene);
    d->m_useVerticalLock = lock;
}

void QGeoTiledMapScene::setScreenSize(const QSize &size)
{
    Q_D(QGeoTiledMapScene);
    d->m_screenSize = size;
}

void QGeoTiledMapScene::setTileSize(int tileSize)
{
    Q_D(QGeoTiledMapScene);
    d->m_tileSize = tileSize;
}

void QGeoTiledMapScene::setCameraData(const QGeoCameraData &cameraData)
{
    Q_D(QGeoTiledMapScene);
    d->m_cameraData = cameraData;
    d->m_intZoomLevel = static_cast<int>(std::floor(d->m_cameraData.zoomLevel()));
    float delta = cameraData.zoomLevel() - d->m_intZoomLevel;
    d->m_linearScaling = qAbs(delta) > 0.05;
    d->m_sideLength = 1 << d->m_intZoomLevel;
}

void QGeoTiledMapScene::setVisibleTiles(const QSet<QGeoTileSpec> &tiles)
{
    Q_D(QGeoTiledMapScene);
    d->setVisibleTiles(tiles);
}

const QSet<QGeoTileSpec> &QGeoTiledMapScene::visibleTiles() const
{
    Q_D(const QGeoTiledMapScene);
    return d->m_visibleTiles;
}

void QGeoTiledMapScene::addTile(const QGeoTileSpec &spec, QSharedPointer<QGeoTileTexture> texture)
{
    Q_D(QGeoTiledMapScene);
    d->addTile(spec, texture);
}

QDoubleVector2D QGeoTiledMapScene::itemPositionToMercator(const QDoubleVector2D &pos) const
{
    Q_D(const QGeoTiledMapScene);
    return d->itemPositionToMercator(pos);
}

QDoubleVector2D QGeoTiledMapScene::mercatorToItemPosition(const QDoubleVector2D &mercator) const
{
    Q_D(const QGeoTiledMapScene);
    return d->mercatorToItemPosition(mercator);
}

bool QGeoTiledMapScene::verticalLock() const
{
    Q_D(const QGeoTiledMapScene);
    return d->m_verticalLock;
}

QSet<QGeoTileSpec> QGeoTiledMapScene::texturedTiles()
{
    Q_D(QGeoTiledMapScene);
    QSet<QGeoTileSpec> textured;
    foreach (const QGeoTileSpec &tile, d->m_textures.keys()) {
        textured += tile;
    }
    return textured;
}

void QGeoTiledMapScene::clearTexturedTiles()
{
    Q_D(QGeoTiledMapScene);
    d->m_textures.clear();
    d->m_dropTextures = true;
}

QGeoTiledMapScenePrivate::QGeoTiledMapScenePrivate()
    : QObjectPrivate(),
      m_tileSize(0),
      m_scaleFactor(10.0),
      m_intZoomLevel(0),
      m_sideLength(0),
      m_minTileX(-1),
      m_minTileY(-1),
      m_maxTileX(-1),
      m_maxTileY(-1),
      m_tileXWrapsBelow(0),
      m_mercatorCenterX(0.0),
      m_mercatorCenterY(0.0),
      m_mercatorWidth(0.0),
      m_mercatorHeight(0.0),
      m_screenOffsetX(0.0),
      m_screenOffsetY(0.0),
      m_screenWidth(0.0),
      m_screenHeight(0.0),
      m_useVerticalLock(false),
      m_verticalLock(false),
      m_linearScaling(false),
      m_dropTextures(false)
{
}

QGeoTiledMapScenePrivate::~QGeoTiledMapScenePrivate()
{
}

QDoubleVector2D QGeoTiledMapScenePrivate::itemPositionToMercator(const QDoubleVector2D &pos) const
{
    double x = m_mercatorWidth * (((pos.x() - m_screenOffsetX) / m_screenWidth) - 0.5);
    x += m_mercatorCenterX;

    if (x > 1.0 * m_sideLength)
        x -= 1.0 * m_sideLength;
    if (x < 0.0)
        x += 1.0 * m_sideLength;

    x /= 1.0 * m_sideLength;

    double y = m_mercatorHeight * (((pos.y() - m_screenOffsetY) / m_screenHeight) - 0.5);
    y += m_mercatorCenterY;
    y /= 1.0 * m_sideLength;

    return QDoubleVector2D(x, y);
}

QDoubleVector2D QGeoTiledMapScenePrivate::mercatorToItemPosition(const QDoubleVector2D &mercator) const
{
    double mx = m_sideLength * mercator.x();

    double lb = m_mercatorCenterX - m_mercatorWidth / 2.0;
    if (lb < 0.0)
        lb += m_sideLength;
    double ub = m_mercatorCenterX + m_mercatorWidth / 2.0;
    if (m_sideLength < ub)
        ub -= m_sideLength;

    double m = (mx - m_mercatorCenterX) / m_mercatorWidth;

    double mWrapLower = (mx - m_mercatorCenterX - m_sideLength) / m_mercatorWidth;
    double mWrapUpper = (mx - m_mercatorCenterX + m_sideLength) / m_mercatorWidth;

    // correct for crossing dateline
    if (qFuzzyCompare(ub - lb + 1.0, 1.0) || (ub < lb) ) {
        if (m_mercatorCenterX < ub) {
            if (lb < mx) {
                 m = mWrapLower;
            }
        } else if (lb < m_mercatorCenterX) {
            if (mx <= ub) {
                m = mWrapUpper;
            }
        }
    }

    // apply wrapping if necessary so we don't return unreasonably large pos/neg screen positions
    // also allows map items to be drawn properly if some of their coords are out of the screen
    if ( qAbs(mWrapLower) < qAbs(m) )
        m = mWrapLower;
    if ( qAbs(mWrapUpper) < qAbs(m) )
        m = mWrapUpper;

    double x = m_screenWidth * (0.5 + m);
    double y = m_screenHeight * (0.5 + (m_sideLength * mercator.y() - m_mercatorCenterY) / m_mercatorHeight);

    return QDoubleVector2D(x + m_screenOffsetX, y + m_screenOffsetY);
}

bool QGeoTiledMapScenePrivate::buildGeometry(const QGeoTileSpec &spec, QSGImageNode *imageNode)
{
    int x = spec.x();

    if (x < m_tileXWrapsBelow)
        x += m_sideLength;

    if ((x < m_minTileX)
            || (m_maxTileX < x)
            || (spec.y() < m_minTileY)
            || (m_maxTileY < spec.y())
            || (spec.zoom() != m_intZoomLevel)) {
        return false;
    }

    double edge = m_scaleFactor * m_tileSize;

    double x1 = (x - m_minTileX);
    double x2 = x1 + 1.0;

    double y1 = (m_minTileY - spec.y());
    double y2 = y1 - 1.0;

    x1 *= edge;
    x2 *= edge;
    y1 *= edge;
    y2 *= edge;

    imageNode->setRect(QRectF(QPointF(x1, y2), QPointF(x2, y1)));
    imageNode->setTextureCoordinatesTransform(QSGImageNode::MirrorVertically);
    imageNode->setSourceRect(QRectF(QPointF(0,0), imageNode->texture()->textureSize()));

    return true;
}

void QGeoTiledMapScenePrivate::addTile(const QGeoTileSpec &spec, QSharedPointer<QGeoTileTexture> texture)
{
    if (!m_visibleTiles.contains(spec)) // Don't add the geometry if it isn't visible
        return;

    m_textures.insert(spec, texture);
}

void QGeoTiledMapScenePrivate::setVisibleTiles(const QSet<QGeoTileSpec> &tiles)
{
    // work out the tile bounds for the new scene
    setTileBounds(tiles);

    // set up the gl camera for the new scene
    setupCamera();

    QSet<QGeoTileSpec> toRemove = m_visibleTiles - tiles;
    if (!toRemove.isEmpty())
        removeTiles(toRemove);

    m_visibleTiles = tiles;
}

void QGeoTiledMapScenePrivate::removeTiles(const QSet<QGeoTileSpec> &oldTiles)
{
    typedef QSet<QGeoTileSpec>::const_iterator iter;
    iter i = oldTiles.constBegin();
    iter end = oldTiles.constEnd();

    for (; i != end; ++i) {
        QGeoTileSpec tile = *i;
        m_textures.remove(tile);
    }
}

void QGeoTiledMapScenePrivate::setTileBounds(const QSet<QGeoTileSpec> &tiles)
{
    if (tiles.isEmpty()) {
        m_minTileX = -1;
        m_minTileY = -1;
        m_maxTileX = -1;
        m_maxTileY = -1;
        return;
    }

    typedef QSet<QGeoTileSpec>::const_iterator iter;
    iter i = tiles.constBegin();
    iter end = tiles.constEnd();

    // determine whether the set of map tiles crosses the dateline.
    // A gap in the tiles indicates dateline crossing
    bool hasFarLeft = false;
    bool hasFarRight = false;
    bool hasMidLeft = false;
    bool hasMidRight = false;

    for (; i != end; ++i) {
        if ((*i).zoom() != m_intZoomLevel)
            continue;
        int x = (*i).x();
        if (x == 0)
            hasFarLeft = true;
        else if (x == (m_sideLength - 1))
            hasFarRight = true;
        else if (x == ((m_sideLength / 2) - 1)) {
            hasMidLeft = true;
        } else if (x == (m_sideLength / 2)) {
            hasMidRight = true;
        }
    }

    // if dateline crossing is detected we wrap all x pos of tiles
    // that are in the left half of the map.
    m_tileXWrapsBelow = 0;

    if (hasFarLeft && hasFarRight) {
        if (!hasMidRight) {
            m_tileXWrapsBelow = m_sideLength / 2;
        } else if (!hasMidLeft) {
            m_tileXWrapsBelow = (m_sideLength / 2) - 1;
        }
    }

    // finally, determine the min and max bounds
    i = tiles.constBegin();

    QGeoTileSpec tile = *i;

    int x = tile.x();
    if (tile.x() < m_tileXWrapsBelow)
        x += m_sideLength;

    m_minTileX = x;
    m_maxTileX = x;
    m_minTileY = tile.y();
    m_maxTileY = tile.y();

    ++i;

    for (; i != end; ++i) {
        tile = *i;
        if (tile.zoom() != m_intZoomLevel)
            continue;

        int x = tile.x();
        if (tile.x() < m_tileXWrapsBelow)
            x += m_sideLength;

        m_minTileX = qMin(m_minTileX, x);
        m_maxTileX = qMax(m_maxTileX, x);
        m_minTileY = qMin(m_minTileY, tile.y());
        m_maxTileY = qMax(m_maxTileY, tile.y());
    }
}

void QGeoTiledMapScenePrivate::setupCamera()
{
    double f = 1.0 * qMin(m_screenSize.width(), m_screenSize.height());

    // fraction of zoom level
    double z = std::pow(2.0, m_cameraData.zoomLevel() - m_intZoomLevel) * m_tileSize;

    // calculate altitdue that allows the visible map tiles
    // to fit in the screen correctly (note that a larger f will cause
    // the camera be higher, resulting in gray areas displayed around
    // the tiles)
    double altitude = f / (2.0 * z) ;

    // mercatorWidth_ and mercatorHeight_ define the ratio for
    // mercator and screen coordinate conversion,
    // see mercatorToItemPosition() and itemPositionToMercator()
    m_mercatorHeight = m_screenSize.height() / z;
    m_mercatorWidth = m_screenSize.width() / z;

    // calculate center
    double edge = m_scaleFactor * m_tileSize;

    // first calculate the camera center in map space in the range of 0 <-> sideLength (2^z)
    QDoubleVector3D center = (m_sideLength * QGeoProjection::coordToMercator(m_cameraData.center()));

    // wrap the center if necessary (due to dateline crossing)
    if (center.x() < m_tileXWrapsBelow)
        center.setX(center.x() + 1.0 * m_sideLength);

    m_mercatorCenterX = center.x();
    m_mercatorCenterY = center.y();

    // work out where the camera center is w.r.t minimum tile bounds
    center.setX(center.x() - 1.0 * m_minTileX);
    center.setY(1.0 * m_minTileY - center.y());

    // letter box vertically
    if (m_useVerticalLock && (m_mercatorHeight > 1.0 * m_sideLength)) {
        center.setY(-1.0 * m_sideLength / 2.0);
        m_mercatorCenterY = m_sideLength / 2.0;
        m_screenOffsetY = m_screenSize.height() * (0.5 - m_sideLength / (2 * m_mercatorHeight));
        m_screenHeight = m_screenSize.height() - 2 * m_screenOffsetY;
        m_mercatorHeight = 1.0 * m_sideLength;
        m_verticalLock = true;
    } else {
        m_screenOffsetY = 0.0;
        m_screenHeight = m_screenSize.height();
        m_verticalLock = false;
    }

    if (m_mercatorWidth > 1.0 * m_sideLength) {
        m_screenOffsetX = m_screenSize.width() * (0.5 - (m_sideLength / (2 * m_mercatorWidth)));
        m_screenWidth = m_screenSize.width() - 2 * m_screenOffsetX;
        m_mercatorWidth = 1.0 * m_sideLength;
    } else {
        m_screenOffsetX = 0.0;
        m_screenWidth = m_screenSize.width();
    }

    // apply necessary scaling to the camera center
    center *= edge;

    // calculate eye

    QDoubleVector3D eye = center;
    eye.setZ(altitude * edge);

    // calculate up

    QDoubleVector3D view = eye - center;
    QDoubleVector3D side = QDoubleVector3D::normal(view, QDoubleVector3D(0.0, 1.0, 0.0));
    QDoubleVector3D up = QDoubleVector3D::normal(side, view);

    // old bearing, tilt and roll code
    //    QMatrix4x4 mBearing;
    //    mBearing.rotate(-1.0 * camera.bearing(), view);
    //    up = mBearing * up;

    //    QDoubleVector3D side2 = QDoubleVector3D::normal(up, view);
    //    QMatrix4x4 mTilt;
    //    mTilt.rotate(camera.tilt(), side2);
    //    eye = (mTilt * view) + center;

    //    view = eye - center;
    //    side = QDoubleVector3D::normal(view, QDoubleVector3D(0.0, 1.0, 0.0));
    //    up = QDoubleVector3D::normal(view, side2);

    //    QMatrix4x4 mRoll;
    //    mRoll.rotate(camera.roll(), view);
    //    up = mRoll * up;

    // near plane and far plane

    double nearPlane = 1.0;
    double farPlane = (altitude + 1.0) * edge;

    m_cameraUp = up;
    m_cameraCenter = center;
    m_cameraEye = eye;

    double aspectRatio = 1.0 * m_screenSize.width() / m_screenSize.height();
    float halfWidth = 1;
    float halfHeight = 1;
    if (aspectRatio > 1.0) {
        halfWidth *= aspectRatio;
    } else if (aspectRatio > 0.0f && aspectRatio < 1.0f) {
        halfHeight /= aspectRatio;
    }
    m_projectionMatrix.setToIdentity();
    m_projectionMatrix.frustum(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
}

class QGeoTiledMapTileContainerNode : public QSGTransformNode
{
public:
    void addChild(const QGeoTileSpec &spec, QSGImageNode *node)
    {
        tiles.insert(spec, node);
        appendChildNode(node);
    }
    QHash<QGeoTileSpec, QSGImageNode *> tiles;
};

class QGeoTiledMapRootNode : public QSGClipNode
{
public:
    QGeoTiledMapRootNode()
        : isTextureLinear(false)
        , geometry(QSGGeometry::defaultAttributes_Point2D(), 4)
        , root(new QSGTransformNode())
        , tiles(new QGeoTiledMapTileContainerNode())
        , wrapLeft(new QGeoTiledMapTileContainerNode())
        , wrapRight(new QGeoTiledMapTileContainerNode())
    {
        setIsRectangular(true);
        setGeometry(&geometry);
        root->appendChildNode(tiles);
        root->appendChildNode(wrapLeft);
        root->appendChildNode(wrapRight);
        appendChildNode(root);
    }

    ~QGeoTiledMapRootNode()
    {
        qDeleteAll(textures);
    }

    void setClipRect(const QRect &rect)
    {
        if (rect != clipRect) {
            QSGGeometry::updateRectGeometry(&geometry, rect);
            QSGClipNode::setClipRect(rect);
            clipRect = rect;
            markDirty(DirtyGeometry);
        }
    }

    void updateTiles(QGeoTiledMapTileContainerNode *root,
                     QGeoTiledMapScenePrivate *d,
                     double camAdjust,
                     QQuickWindow *window);

    bool isTextureLinear;

    QSGGeometry geometry;
    QRect clipRect;

    QSGTransformNode *root;

    QGeoTiledMapTileContainerNode *tiles;        // The majority of the tiles
    QGeoTiledMapTileContainerNode *wrapLeft;     // When zoomed out, the tiles that wrap around on the left.
    QGeoTiledMapTileContainerNode *wrapRight;    // When zoomed out, the tiles that wrap around on the right

    QHash<QGeoTileSpec, QSGTexture *> textures;
};

static bool qgeotiledmapscene_isTileInViewport(const QRectF &tileRect, const QMatrix4x4 &matrix) {
    const QRectF boundingRect = QRectF(matrix * tileRect.topLeft(), matrix * tileRect.bottomRight());
    return QRectF(-1, -1, 2, 2).intersects(boundingRect);
}

static QVector3D toVector3D(const QDoubleVector3D& in)
{
    return QVector3D(in.x(), in.y(), in.z());
}

void QGeoTiledMapRootNode::updateTiles(QGeoTiledMapTileContainerNode *root,
                                       QGeoTiledMapScenePrivate *d,
                                       double camAdjust,
                                       QQuickWindow *window)
{
    // Set up the matrix...
    QDoubleVector3D eye = d->m_cameraEye;
    eye.setX(eye.x() + camAdjust);
    QDoubleVector3D center = d->m_cameraCenter;
    center.setX(center.x() + camAdjust);
    QMatrix4x4 cameraMatrix;
    cameraMatrix.lookAt(toVector3D(eye), toVector3D(center), toVector3D(d->m_cameraUp));
    root->setMatrix(d->m_projectionMatrix * cameraMatrix);

    QSet<QGeoTileSpec> tilesInSG = QSet<QGeoTileSpec>::fromList(root->tiles.keys());
    QSet<QGeoTileSpec> toRemove = tilesInSG - d->m_visibleTiles;
    QSet<QGeoTileSpec> toAdd = d->m_visibleTiles - tilesInSG;

    foreach (const QGeoTileSpec &s, toRemove)
        delete root->tiles.take(s);

    for (QHash<QGeoTileSpec, QSGImageNode *>::iterator it = root->tiles.begin();
         it != root->tiles.end(); ) {
        QSGImageNode *node = it.value();
        bool ok = d->buildGeometry(it.key(), node) && qgeotiledmapscene_isTileInViewport(node->rect(), root->matrix());

        QSGNode::DirtyState dirtyBits = 0;

        if (!ok) {
            it = root->tiles.erase(it);
            delete node;
        } else {
            if (isTextureLinear != d->m_linearScaling) {
                if (node->texture()->textureSize().width() > d->m_tileSize) {
                    node->setFiltering(QSGTexture::Linear); // With mipmapping QSGTexture::Nearest generates artifacts
                    node->setMipmapFiltering(QSGTexture::Linear);
                } else {
                    node->setFiltering(d->m_linearScaling ? QSGTexture::Linear : QSGTexture::Nearest);
                }
                dirtyBits |= QSGNode::DirtyMaterial;
            }
            if (dirtyBits != 0)
                node->markDirty(dirtyBits);
            it++;
        }
    }

    foreach (const QGeoTileSpec &s, toAdd) {
        QGeoTileTexture *tileTexture = d->m_textures.value(s).data();
        if (!tileTexture || tileTexture->image.isNull())
            continue;
        QSGImageNode *tileNode = window->createImageNode();
        // note: setTexture will update coordinates so do it here, before we buildGeometry
        tileNode->setTexture(textures.value(s));
        if (d->buildGeometry(s, tileNode) && qgeotiledmapscene_isTileInViewport(tileNode->rect(), root->matrix())) {
            if (tileNode->texture()->textureSize().width() > d->m_tileSize) {
                tileNode->setFiltering(QSGTexture::Linear); // with mipmapping QSGTexture::Nearest generates artifacts
                tileNode->setMipmapFiltering(QSGTexture::Linear);
            } else {
                tileNode->setFiltering(d->m_linearScaling ? QSGTexture::Linear : QSGTexture::Nearest);
            }
            root->addChild(s, tileNode);
        } else {
            delete tileNode;
        }
    }
}

QSGNode *QGeoTiledMapScene::updateSceneGraph(QSGNode *oldNode, QQuickWindow *window)
{
    Q_D(QGeoTiledMapScene);
    float w = d->m_screenSize.width();
    float h = d->m_screenSize.height();
    if (w <= 0 || h <= 0) {
        delete oldNode;
        return 0;
    }

    QGeoTiledMapRootNode *mapRoot = static_cast<QGeoTiledMapRootNode *>(oldNode);
    if (!mapRoot)
        mapRoot = new QGeoTiledMapRootNode();

    mapRoot->setClipRect(QRect(d->m_screenOffsetX, d->m_screenOffsetY, d->m_screenWidth, d->m_screenHeight));

    QMatrix4x4 itemSpaceMatrix;
    itemSpaceMatrix.scale(w / 2, h / 2);
    itemSpaceMatrix.translate(1, 1);
    itemSpaceMatrix.scale(1, -1);
    mapRoot->root->setMatrix(itemSpaceMatrix);

    if (d->m_dropTextures) {
        foreach (const QGeoTileSpec &s, mapRoot->tiles->tiles.keys())
            delete mapRoot->tiles->tiles.take(s);
        foreach (const QGeoTileSpec &s, mapRoot->wrapLeft->tiles.keys())
            delete mapRoot->wrapLeft->tiles.take(s);
        foreach (const QGeoTileSpec &s, mapRoot->wrapRight->tiles.keys())
            delete mapRoot->wrapRight->tiles.take(s);
        foreach (const QGeoTileSpec &spec, mapRoot->textures.keys())
            mapRoot->textures.take(spec)->deleteLater();
        d->m_dropTextures = false;
    }

    QSet<QGeoTileSpec> textures = QSet<QGeoTileSpec>::fromList(mapRoot->textures.keys());
    QSet<QGeoTileSpec> toRemove = textures - d->m_visibleTiles;
    QSet<QGeoTileSpec> toAdd = d->m_visibleTiles - textures;

    foreach (const QGeoTileSpec &spec, toRemove)
        mapRoot->textures.take(spec)->deleteLater();
    foreach (const QGeoTileSpec &spec, toAdd) {
        QGeoTileTexture *tileTexture = d->m_textures.value(spec).data();
        if (!tileTexture || tileTexture->image.isNull())
            continue;
        mapRoot->textures.insert(spec, window->createTextureFromImage(tileTexture->image));
    }

    double sideLength = d->m_scaleFactor * d->m_tileSize * d->m_sideLength;
    mapRoot->updateTiles(mapRoot->tiles, d, 0, window);
    mapRoot->updateTiles(mapRoot->wrapLeft, d, +sideLength, window);
    mapRoot->updateTiles(mapRoot->wrapRight, d, -sideLength, window);

    mapRoot->isTextureLinear = d->m_linearScaling;

    return mapRoot;
}

QT_END_NAMESPACE
