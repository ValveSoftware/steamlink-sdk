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
#include <QtPositioning/private/qwebmercator_p.h>
#include <QtCore/private/qobject_p.h>
#include <QtQuick/QSGImageNode>
#include <QtQuick/QQuickWindow>
#include <QtQuick/private/qsgdefaultimagenode_p.h>
#include <QtGui/QVector3D>
#include <cmath>
#include <QtPositioning/private/qlocationutils_p.h>
#include <QtPositioning/private/qdoublematrix4x4_p.h>
#include <QtPositioning/private/qwebmercator_p.h>

static QVector3D toVector3D(const QDoubleVector3D& in)
{
    return QVector3D(in.x(), in.y(), in.z());
}

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
    double m_mapEdgeSize;

    QHash<QGeoTileSpec, QSharedPointer<QGeoTileTexture> > m_textures;
    QVector<QGeoTileSpec> m_updatedTextures;

    // tilesToGrid transform
    int m_minTileX; // the minimum tile index, i.e. 0 to sideLength which is 1<< zoomLevel
    int m_minTileY;
    int m_maxTileX;
    int m_maxTileY;
    int m_tileXWrapsBelow; // the wrap point as a tile index

    bool m_linearScaling;

    bool m_dropTextures;

    void addTile(const QGeoTileSpec &spec, QSharedPointer<QGeoTileTexture> texture);

    void setVisibleTiles(const QSet<QGeoTileSpec> &visibleTiles);
    void removeTiles(const QSet<QGeoTileSpec> &oldTiles);
    bool buildGeometry(const QGeoTileSpec &spec, QSGImageNode *imageNode, bool &overzooming);
    void updateTileBounds(const QSet<QGeoTileSpec> &tiles);
    void setupCamera();
    inline bool isTiltedOrRotated() { return (m_cameraData.tilt() > 0.0) || (m_cameraData.bearing() > 0.0); }
};

QGeoTiledMapScene::QGeoTiledMapScene(QObject *parent)
    : QObject(*new QGeoTiledMapScenePrivate(),parent)
{
}

QGeoTiledMapScene::~QGeoTiledMapScene()
{
}

void QGeoTiledMapScene::setScreenSize(const QSize &size)
{
    Q_D(QGeoTiledMapScene);
    d->m_screenSize = size;
}

void QGeoTiledMapScene::updateSceneParameters()
{
    Q_D(QGeoTiledMapScene);
    d->m_intZoomLevel = static_cast<int>(std::floor(d->m_cameraData.zoomLevel()));
    const float delta = d->m_cameraData.zoomLevel() - d->m_intZoomLevel;
    d->m_linearScaling = qAbs(delta) > 0.05 || d->isTiltedOrRotated();
    d->m_sideLength = 1 << d->m_intZoomLevel;
    d->m_mapEdgeSize = std::pow(2.0, d->m_cameraData.zoomLevel()) * d->m_tileSize;
}

void QGeoTiledMapScene::setTileSize(int tileSize)
{
    Q_D(QGeoTiledMapScene);
    if (d->m_tileSize == tileSize)
        return;

    d->m_tileSize = tileSize;
    updateSceneParameters();
}

void QGeoTiledMapScene::setCameraData(const QGeoCameraData &cameraData)
{
    Q_D(QGeoTiledMapScene);
    d->m_cameraData = cameraData;
    updateSceneParameters();
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

QSet<QGeoTileSpec> QGeoTiledMapScene::texturedTiles()
{
    Q_D(QGeoTiledMapScene);
    QSet<QGeoTileSpec> textured;
    for (auto it = d->m_textures.cbegin(); it != d->m_textures.cend(); ++it)
        textured += it.value()->spec;

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
      m_linearScaling(false),
      m_dropTextures(false)
{
}

QGeoTiledMapScenePrivate::~QGeoTiledMapScenePrivate()
{
}

bool QGeoTiledMapScenePrivate::buildGeometry(const QGeoTileSpec &spec, QSGImageNode *imageNode, bool &overzooming)
{
    overzooming = false;
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

    // Calculate the texture mapping, in case we are magnifying some lower ZL tile
    const auto it = m_textures.find(spec); // This should be always found, but apparently sometimes it isn't, possibly due to memory shortage
    if (it != m_textures.end()) {
        if (it.value()->spec.zoom() < spec.zoom()) {
            // Currently only using lower ZL tiles for the overzoom.
            const int tilesPerTexture = 1 << (spec.zoom() - it.value()->spec.zoom());
            const int mappedSize = imageNode->texture()->textureSize().width() / tilesPerTexture;
            const int x = (spec.x() % tilesPerTexture) * mappedSize;
            const int y = (spec.y() % tilesPerTexture) * mappedSize;
            imageNode->setSourceRect(QRectF(x, y, mappedSize, mappedSize));
            overzooming = true;
        } else {
            imageNode->setSourceRect(QRectF(QPointF(0,0), imageNode->texture()->textureSize()));
        }
    } else {
        qWarning() << "!! buildGeometry: tileSpec not present in m_textures !!";
        imageNode->setSourceRect(QRectF(QPointF(0,0), imageNode->texture()->textureSize()));
    }

    return true;
}

void QGeoTiledMapScenePrivate::addTile(const QGeoTileSpec &spec, QSharedPointer<QGeoTileTexture> texture)
{
    if (!m_visibleTiles.contains(spec)) // Don't add the geometry if it isn't visible
        return;

    if (m_textures.contains(spec))
        m_updatedTextures.append(spec);
    m_textures.insert(spec, texture);
}

void QGeoTiledMapScenePrivate::setVisibleTiles(const QSet<QGeoTileSpec> &visibleTiles)
{
    // work out the tile bounds for the new scene
    updateTileBounds(visibleTiles);

    // set up the gl camera for the new scene
    setupCamera();

    QSet<QGeoTileSpec> toRemove = m_visibleTiles - visibleTiles;
    if (!toRemove.isEmpty())
        removeTiles(toRemove);

    m_visibleTiles = visibleTiles;
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

void QGeoTiledMapScenePrivate::updateTileBounds(const QSet<QGeoTileSpec> &tiles)
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
    // NOTE: The following instruction is correct only because WebMercator is a square projection!
    double f = m_screenSize.height();

    // Using fraction of zoom level, z varies between [ m_tileSize , 2 * m_tileSize [
    double z = std::pow(2.0, m_cameraData.zoomLevel() - m_intZoomLevel) * m_tileSize;

    // calculate altitude that allows the visible map tiles
    // to fit in the screen correctly (note that a larger f will cause
    // the camera be higher, resulting in gray areas displayed around
    // the tiles)
    double altitude = f / (2.0 * z);

    // calculate center
    double edge = m_scaleFactor * m_tileSize;

    // first calculate the camera center in map space in the range of 0 <-> sideLength (2^z)
    QDoubleVector2D camCenterMercator = QWebMercator::coordToMercator(m_cameraData.center());
    QDoubleVector3D center = (m_sideLength * camCenterMercator);

    // wrap the center if necessary (due to dateline crossing)
    if (center.x() < m_tileXWrapsBelow)
        center.setX(center.x() + 1.0 * m_sideLength);

    // work out where the camera center is w.r.t minimum tile bounds
    center.setX(center.x() - 1.0 * m_minTileX);
    center.setY(1.0 * m_minTileY - center.y());

    // apply necessary scaling to the camera center
    center *= edge;

    // calculate eye
    double apertureSize = 1.0;
    if (m_cameraData.fieldOfView() != 90.0) //aperture(90 / 2) = 1
        apertureSize = tan(QLocationUtils::radians(m_cameraData.fieldOfView()) * 0.5);
    QDoubleVector3D eye = center;
    eye.setZ(altitude * edge / apertureSize);

    // calculate up

    QDoubleVector3D view = eye - center;
    QDoubleVector3D side = QDoubleVector3D::normal(view, QDoubleVector3D(0.0, 1.0, 0.0));
    QDoubleVector3D up = QDoubleVector3D::normal(side, view);

    // old bearing, tilt and roll code.
    // Now using double matrices until distilling the transformation to QMatrix4x4
    QDoubleMatrix4x4 mBearing;
    // -1.0 * bearing removed, now map north goes in the bearing direction
    mBearing.rotate(-1.0 * m_cameraData.bearing(), view);
    up = mBearing * up;

    QDoubleVector3D side2 = QDoubleVector3D::normal(up, view);
    if (m_cameraData.tilt() > 0.01) {
        QDoubleMatrix4x4 mTilt;
        mTilt.rotate(m_cameraData.tilt(), side2);
        eye = mTilt * view + center;
    }

    view = eye - center;
    view.normalize();
    side = QDoubleVector3D::normal(view, QDoubleVector3D(0.0, 1.0, 0.0));
    up = QDoubleVector3D::normal(view, side2);

    //    QMatrix4x4 mRoll;
    //    mRoll.rotate(camera.roll(), view);
    //    up = mRoll * up;

    // near plane and far plane

    double nearPlane = 1.0;
    // Clip plane. Used to be (altitude + 1.0) * edge. This does not affect the perspective. minimum value would be > 0.0
    // Since, for some reasons possibly related to how QSG works, this clipping plane is unable to clip part of tiles,
    // Instead of farPlane =  (altitude + m_cameraData.clipDistance()) * edge , we use a fixed large clipDistance, and
    // leave the clipping only in QGeoCameraTiles::createFrustum
    double farPlane =  (altitude + 10000.0) * edge;

    m_cameraUp = up;
    m_cameraCenter = center;
    m_cameraEye = eye;

    double aspectRatio = 1.0 * m_screenSize.width() / m_screenSize.height();
    float halfWidth = 1 * apertureSize;
    float halfHeight = 1 * apertureSize;
    halfWidth *= aspectRatio;

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
                     QQuickWindow *window,
                     bool ogl);

    bool isTextureLinear;

    QSGGeometry geometry;
    QRect clipRect;

    QSGTransformNode *root;

    QGeoTiledMapTileContainerNode *tiles;        // The majority of the tiles
    QGeoTiledMapTileContainerNode *wrapLeft;     // When zoomed out, the tiles that wrap around on the left.
    QGeoTiledMapTileContainerNode *wrapRight;    // When zoomed out, the tiles that wrap around on the right

    QHash<QGeoTileSpec, QSGTexture *> textures;
};

static bool qgeotiledmapscene_isTileInViewport_Straight(const QRectF &tileRect, const QMatrix4x4 &matrix)
{
    const QRectF boundingRect = QRectF(matrix * tileRect.topLeft(), matrix * tileRect.bottomRight());
    return QRectF(-1, -1, 2, 2).intersects(boundingRect);
}

static bool qgeotiledmapscene_isTileInViewport_rotationTilt(const QRectF &tileRect, const QMatrix4x4 &matrix)
{
    // Transformed corners
    const QPointF tlt = matrix * tileRect.topLeft();
    const QPointF trt = matrix * tileRect.topRight();
    const QPointF blt = matrix * tileRect.bottomLeft();
    const QPointF brt = matrix * tileRect.bottomRight();

    const QRectF boundingRect = QRectF(QPointF(qMin(qMin(qMin(tlt.x(), trt.x()), blt.x()), brt.x())
                                              ,qMax(qMax(qMax(tlt.y(), trt.y()), blt.y()), brt.y()))
                                      ,QPointF(qMax(qMax(qMax(tlt.x(), trt.x()), blt.x()), brt.x())
                                              ,qMin(qMin(qMin(tlt.y(), trt.y()), blt.y()), brt.y()))
                                       );
    return QRectF(-1, -1, 2, 2).intersects(boundingRect);
}

static bool qgeotiledmapscene_isTileInViewport(const QRectF &tileRect, const QMatrix4x4 &matrix, const bool straight)
{
    if (straight)
        return qgeotiledmapscene_isTileInViewport_Straight(tileRect, matrix);
    return qgeotiledmapscene_isTileInViewport_rotationTilt(tileRect, matrix);
}

void QGeoTiledMapRootNode::updateTiles(QGeoTiledMapTileContainerNode *root,
                                       QGeoTiledMapScenePrivate *d,
                                       double camAdjust,
                                       QQuickWindow *window,
                                       bool ogl)
{
    // Set up the matrix...
    QDoubleVector3D eye = d->m_cameraEye;
    eye.setX(eye.x() + camAdjust);
    QDoubleVector3D center = d->m_cameraCenter;
    center.setX(center.x() + camAdjust);
    QMatrix4x4 cameraMatrix;
    cameraMatrix.lookAt(toVector3D(eye), toVector3D(center), toVector3D(d->m_cameraUp));
    root->setMatrix(d->m_projectionMatrix * cameraMatrix);

    const QSet<QGeoTileSpec> tilesInSG = QSet<QGeoTileSpec>::fromList(root->tiles.keys());
    const QSet<QGeoTileSpec> toRemove = tilesInSG - d->m_visibleTiles;
    const QSet<QGeoTileSpec> toAdd = d->m_visibleTiles - tilesInSG;

    for (const QGeoTileSpec &s : toRemove)
        delete root->tiles.take(s);
    bool straight = !d->isTiltedOrRotated();
    bool overzooming;
    qreal pixelRatio = window->effectiveDevicePixelRatio();
    for (QHash<QGeoTileSpec, QSGImageNode *>::iterator it = root->tiles.begin();
         it != root->tiles.end(); ) {
        QSGImageNode *node = it.value();
        bool ok = d->buildGeometry(it.key(), node, overzooming)
                && qgeotiledmapscene_isTileInViewport(node->rect(), root->matrix(), straight);

        QSGNode::DirtyState dirtyBits = 0;

        if (!ok) {
            it = root->tiles.erase(it);
            delete node;
        } else {
            if (isTextureLinear != d->m_linearScaling) {
                if (node->texture()->textureSize().width() > d->m_tileSize * pixelRatio) {
                    node->setFiltering(QSGTexture::Linear); // With mipmapping QSGTexture::Nearest generates artifacts
                    node->setMipmapFiltering(QSGTexture::Linear);
                } else {
                    node->setFiltering((d->m_linearScaling || overzooming) ? QSGTexture::Linear : QSGTexture::Nearest);
                }
#if QT_CONFIG(opengl)
                if (ogl)
                    static_cast<QSGDefaultImageNode *>(node)->setAnisotropyLevel(QSGTexture::Anisotropy16x);
#else
    Q_UNUSED(ogl)
#endif
                dirtyBits |= QSGNode::DirtyMaterial;
            }
            if (dirtyBits != 0)
                node->markDirty(dirtyBits);
            it++;
        }
    }

    for (const QGeoTileSpec &s : toAdd) {
        QGeoTileTexture *tileTexture = d->m_textures.value(s).data();
        if (!tileTexture || tileTexture->image.isNull())
            continue;
        QSGImageNode *tileNode = window->createImageNode();
        // note: setTexture will update coordinates so do it here, before we buildGeometry
        tileNode->setTexture(textures.value(s));
        if (d->buildGeometry(s, tileNode, overzooming)
                && qgeotiledmapscene_isTileInViewport(tileNode->rect(), root->matrix(), straight)) {
            if (tileNode->texture()->textureSize().width() > d->m_tileSize * pixelRatio) {
                tileNode->setFiltering(QSGTexture::Linear); // with mipmapping QSGTexture::Nearest generates artifacts
                tileNode->setMipmapFiltering(QSGTexture::Linear);
            } else {
                tileNode->setFiltering((d->m_linearScaling || overzooming) ? QSGTexture::Linear : QSGTexture::Nearest);
            }
#if QT_CONFIG(opengl)
            if (ogl)
                static_cast<QSGDefaultImageNode *>(tileNode)->setAnisotropyLevel(QSGTexture::Anisotropy16x);
#endif
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

    bool isOpenGL = (window->rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL);
    QGeoTiledMapRootNode *mapRoot = static_cast<QGeoTiledMapRootNode *>(oldNode);
    if (!mapRoot)
        mapRoot = new QGeoTiledMapRootNode();

    // Setting clip rect to fullscreen, as now the map can never be smaller than the viewport.
    mapRoot->setClipRect(QRect(0, 0, w, h));

    QMatrix4x4 itemSpaceMatrix;
    itemSpaceMatrix.scale(w / 2, h / 2);
    itemSpaceMatrix.translate(1, 1);
    itemSpaceMatrix.scale(1, -1);
    mapRoot->root->setMatrix(itemSpaceMatrix);

    if (d->m_dropTextures) {
        for (const QGeoTileSpec &s : mapRoot->tiles->tiles.keys())
            delete mapRoot->tiles->tiles.take(s);
        for (const QGeoTileSpec &s : mapRoot->wrapLeft->tiles.keys())
            delete mapRoot->wrapLeft->tiles.take(s);
        for (const QGeoTileSpec &s : mapRoot->wrapRight->tiles.keys())
            delete mapRoot->wrapRight->tiles.take(s);
        for (const QGeoTileSpec &spec : mapRoot->textures.keys())
            mapRoot->textures.take(spec)->deleteLater();
        d->m_dropTextures = false;
    }

    // Evicting loZL tiles temporarily used in place of hiZL ones
    if (d->m_updatedTextures.size()) {
        const QVector<QGeoTileSpec> &toRemove = d->m_updatedTextures;
        for (const QGeoTileSpec &s : toRemove) {
            if (mapRoot->tiles->tiles.contains(s))
                delete mapRoot->tiles->tiles.take(s);

            if (mapRoot->wrapLeft->tiles.contains(s))
                delete mapRoot->wrapLeft->tiles.take(s);

            if (mapRoot->wrapRight->tiles.contains(s))
                delete mapRoot->wrapRight->tiles.take(s);

            if (mapRoot->textures.contains(s))
                mapRoot->textures.take(s)->deleteLater();
        }
        d->m_updatedTextures.clear();
    }

    const QSet<QGeoTileSpec> textures = QSet<QGeoTileSpec>::fromList(mapRoot->textures.keys());
    const QSet<QGeoTileSpec> toRemove = textures - d->m_visibleTiles;
    const QSet<QGeoTileSpec> toAdd = d->m_visibleTiles - textures;

    for (const QGeoTileSpec &spec : toRemove)
        mapRoot->textures.take(spec)->deleteLater();
    for (const QGeoTileSpec &spec : toAdd) {
        QGeoTileTexture *tileTexture = d->m_textures.value(spec).data();
        if (!tileTexture || tileTexture->image.isNull())
            continue;
        mapRoot->textures.insert(spec, window->createTextureFromImage(tileTexture->image));
    }

    double sideLength = d->m_scaleFactor * d->m_tileSize * d->m_sideLength;
    mapRoot->updateTiles(mapRoot->tiles, d, 0, window, isOpenGL);
    mapRoot->updateTiles(mapRoot->wrapLeft, d, +sideLength, window, isOpenGL);
    mapRoot->updateTiles(mapRoot->wrapRight, d, -sideLength, window, isOpenGL);

    mapRoot->isTextureLinear = d->m_linearScaling;

    return mapRoot;
}

QT_END_NAMESPACE
