/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2014 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "qgeomapscene_p.h"

#include "qgeocameradata_p.h"

#include "qgeotilecache_p.h"
#include "qgeotilespec_p.h"

#include <QtPositioning/private/qgeoprojection_p.h>
#include <QtPositioning/private/qdoublevector2d_p.h>
#include <QtPositioning/private/qdoublevector3d_p.h>

#include <QtQuick/QSGSimpleTextureNode>
#include <QtQuick/QQuickWindow>

#include <QHash>

#include <QPointF>

#include <cmath>

QT_BEGIN_NAMESPACE

class QGeoMapScenePrivate
{
public:
    explicit QGeoMapScenePrivate(QGeoMapScene *scene);
    ~QGeoMapScenePrivate();

    QSize screenSize_; // in pixels
    int tileSize_; // the pixel resolution for each tile
    QGeoCameraData cameraData_;
    QSet<QGeoTileSpec> visibleTiles_;

    QDoubleVector3D cameraUp_;
    QDoubleVector3D cameraEye_;
    QDoubleVector3D cameraCenter_;
    QMatrix4x4 projectionMatrix_;

    // scales up the tile geometry and the camera altitude, resulting in no visible effect
    // other than to control the accuracy of the render by keeping the values in a sensible range
    double scaleFactor_;

    // rounded down, positive zoom is zooming in, corresponding to reduced altitude
    int intZoomLevel_;

    // mercatorToGrid transform
    // the number of tiles in each direction for the whole map (earth) at the current zoom level.
    // it is 1<<zoomLevel
    int sideLength_;

    QHash<QGeoTileSpec, QSharedPointer<QGeoTileTexture> > textures_;

    // tilesToGrid transform
    int minTileX_; // the minimum tile index, i.e. 0 to sideLength which is 1<< zoomLevel
    int minTileY_;
    int maxTileX_;
    int maxTileY_;
    int tileZ_;    // caches the zoom level for this tile set
    int tileXWrapsBelow_; // the wrap point as a tile index

    // cameraToGrid transform
    double mercatorCenterX_; // centre of camera in grid space (0 to sideLength)
    double mercatorCenterY_;
    double mercatorWidth_;   // width of camera in grid space (0 to sideLength)
    double mercatorHeight_;

    // screenToWindow transform
    double screenOffsetX_; // in pixels
    double screenOffsetY_; // in pixels
    // cameraToScreen transform
    double screenWidth_; // in pixels
    double screenHeight_; // in pixels

    bool useVerticalLock_;
    bool verticalLock_;
    bool linearScaling_;

    void addTile(const QGeoTileSpec &spec, QSharedPointer<QGeoTileTexture> texture);

    QDoubleVector2D screenPositionToMercator(const QDoubleVector2D &pos) const;
    QDoubleVector2D mercatorToScreenPosition(const QDoubleVector2D &mercator) const;

    void setVisibleTiles(const QSet<QGeoTileSpec> &tiles);
    void removeTiles(const QSet<QGeoTileSpec> &oldTiles);
    bool buildGeometry(const QGeoTileSpec &spec, QSGGeometry::TexturedPoint2D *vertices);
    void setTileBounds(const QSet<QGeoTileSpec> &tiles);
    void setupCamera();

private:
    QGeoMapScene *q_ptr;
    Q_DECLARE_PUBLIC(QGeoMapScene)
};

QGeoMapScene::QGeoMapScene()
    : QObject(),
      d_ptr(new QGeoMapScenePrivate(this)) {}

QGeoMapScene::~QGeoMapScene()
{
    delete d_ptr;
}

void QGeoMapScene::setUseVerticalLock(bool lock)
{
    Q_D(QGeoMapScene);
    d->useVerticalLock_ = lock;
}

void QGeoMapScene::setScreenSize(const QSize &size)
{
    Q_D(QGeoMapScene);
    d->screenSize_ = size;
}

void QGeoMapScene::setTileSize(int tileSize)
{
    Q_D(QGeoMapScene);
    d->tileSize_ = tileSize;
}

void QGeoMapScene::setCameraData(const QGeoCameraData &cameraData)
{
    Q_D(QGeoMapScene);
    d->cameraData_ = cameraData;
    d->intZoomLevel_ = static_cast<int>(std::floor(d->cameraData_.zoomLevel()));
    float delta = cameraData.zoomLevel() - d->intZoomLevel_;
    d->linearScaling_ = qAbs(delta) > 0.05;
    d->sideLength_ = 1 << d->intZoomLevel_;
}

void QGeoMapScene::setVisibleTiles(const QSet<QGeoTileSpec> &tiles)
{
    Q_D(QGeoMapScene);
    d->setVisibleTiles(tiles);
}

void QGeoMapScene::addTile(const QGeoTileSpec &spec, QSharedPointer<QGeoTileTexture> texture)
{
    Q_D(QGeoMapScene);
    d->addTile(spec, texture);
}

QDoubleVector2D QGeoMapScene::screenPositionToMercator(const QDoubleVector2D &pos) const
{
    Q_D(const QGeoMapScene);
    return d->screenPositionToMercator(pos);
}

QDoubleVector2D QGeoMapScene::mercatorToScreenPosition(const QDoubleVector2D &mercator) const
{
    Q_D(const QGeoMapScene);
    return d->mercatorToScreenPosition(mercator);
}

bool QGeoMapScene::verticalLock() const
{
    Q_D(const QGeoMapScene);
    return d->verticalLock_;
}

QSet<QGeoTileSpec> QGeoMapScene::texturedTiles()
{
    Q_D(QGeoMapScene);
    QSet<QGeoTileSpec> textured;
    foreach (const QGeoTileSpec &tile, d->textures_.keys()) {
        textured += tile;
    }
    return textured;
}

QGeoMapScenePrivate::QGeoMapScenePrivate(QGeoMapScene *scene)
    : tileSize_(0),
      scaleFactor_(10.0),
      intZoomLevel_(0),
      sideLength_(0),
      minTileX_(-1),
      minTileY_(-1),
      maxTileX_(-1),
      maxTileY_(-1),
      tileZ_(0),
      tileXWrapsBelow_(0),
      mercatorCenterX_(0.0),
      mercatorCenterY_(0.0),
      mercatorWidth_(0.0),
      mercatorHeight_(0.0),
      screenOffsetX_(0.0),
      screenOffsetY_(0.0),
      screenWidth_(0.0),
      screenHeight_(0.0),
      useVerticalLock_(false),
      verticalLock_(false),
      linearScaling_(false),
      q_ptr(scene) {}

QGeoMapScenePrivate::~QGeoMapScenePrivate()
{
}

QDoubleVector2D QGeoMapScenePrivate::screenPositionToMercator(const QDoubleVector2D &pos) const
{
    double x = mercatorWidth_ * (((pos.x() - screenOffsetX_) / screenWidth_) - 0.5);
    x += mercatorCenterX_;

    if (x > 1.0 * sideLength_)
        x -= 1.0 * sideLength_;
    if (x < 0.0)
        x += 1.0 * sideLength_;

    x /= 1.0 * sideLength_;

    double y = mercatorHeight_ * (((pos.y() - screenOffsetY_) / screenHeight_) - 0.5);
    y += mercatorCenterY_;
    y /= 1.0 * sideLength_;

    return QDoubleVector2D(x, y);
}

QDoubleVector2D QGeoMapScenePrivate::mercatorToScreenPosition(const QDoubleVector2D &mercator) const
{
    double mx = sideLength_ * mercator.x();

    double lb = mercatorCenterX_ - mercatorWidth_ / 2.0;
    if (lb < 0.0)
        lb += sideLength_;
    double ub = mercatorCenterX_ + mercatorWidth_ / 2.0;
    if (sideLength_ < ub)
        ub -= sideLength_;

    double m = (mx - mercatorCenterX_) / mercatorWidth_;

    double mWrapLower = (mx - mercatorCenterX_ - sideLength_) / mercatorWidth_;
    double mWrapUpper = (mx - mercatorCenterX_ + sideLength_) / mercatorWidth_;

    // correct for crossing dateline
    if (qFuzzyCompare(ub - lb + 1.0, 1.0) || (ub < lb) ) {
        if (mercatorCenterX_ < ub) {
            if (lb < mx) {
                 m = mWrapLower;
            }
        } else if (lb < mercatorCenterX_) {
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

    double x = screenWidth_ * (0.5 + m);
    double y = screenHeight_ * (0.5 + (sideLength_ * mercator.y() - mercatorCenterY_) / mercatorHeight_);

    return QDoubleVector2D(x + screenOffsetX_, y + screenOffsetY_);
}

bool QGeoMapScenePrivate::buildGeometry(const QGeoTileSpec &spec, QSGGeometry::TexturedPoint2D *vertices)
{
    int x = spec.x();

    if (x < tileXWrapsBelow_)
        x += sideLength_;

    if ((x < minTileX_)
            || (maxTileX_ < x)
            || (spec.y() < minTileY_)
            || (maxTileY_ < spec.y())
            || (spec.zoom() != tileZ_)) {
        return false;
    }

    double edge = scaleFactor_ * tileSize_;

    double x1 = (x - minTileX_);
    double x2 = x1 + 1.0;

    double y1 = (minTileY_ - spec.y());
    double y2 = y1 - 1.0;

    x1 *= edge;
    x2 *= edge;
    y1 *= edge;
    y2 *= edge;

    //Texture coordinate order for veritcal flip of texture
    vertices[0].set(x1, y1, 0, 0);
    vertices[1].set(x1, y2, 0, 1);
    vertices[2].set(x2, y1, 1, 0);
    vertices[3].set(x2, y2, 1, 1);

    return true;
}

void QGeoMapScenePrivate::addTile(const QGeoTileSpec &spec, QSharedPointer<QGeoTileTexture> texture)
{
    if (!visibleTiles_.contains(spec)) // Don't add the geometry if it isn't visible
        return;

    textures_.insert(spec, texture);
}

// return true if new tiles introduced in [tiles]
void QGeoMapScenePrivate::setVisibleTiles(const QSet<QGeoTileSpec> &tiles)
{
    Q_Q(QGeoMapScene);

    // detect if new tiles introduced
    bool newTilesIntroduced = !visibleTiles_.contains(tiles);

    // work out the tile bounds for the new scene
    setTileBounds(tiles);

    // set up the gl camera for the new scene
    setupCamera();

    QSet<QGeoTileSpec> toRemove = visibleTiles_ - tiles;
    QSet<QGeoTileSpec> toUpdate = visibleTiles_ - toRemove;
    if (!toRemove.isEmpty())
        removeTiles(toRemove);

    visibleTiles_ = tiles;
    if (newTilesIntroduced)
        emit q->newTilesVisible(visibleTiles_);
}

void QGeoMapScenePrivate::removeTiles(const QSet<QGeoTileSpec> &oldTiles)
{
    typedef QSet<QGeoTileSpec>::const_iterator iter;
    iter i = oldTiles.constBegin();
    iter end = oldTiles.constEnd();

    for (; i != end; ++i) {
        QGeoTileSpec tile = *i;
        textures_.remove(tile);
    }
}

void QGeoMapScenePrivate::setTileBounds(const QSet<QGeoTileSpec> &tiles)
{
    if (tiles.isEmpty()) {
        minTileX_ = -1;
        minTileY_ = -1;
        maxTileX_ = -1;
        maxTileY_ = -1;
        tileZ_ = -1;
        return;
    }

    typedef QSet<QGeoTileSpec>::const_iterator iter;
    iter i = tiles.constBegin();
    iter end = tiles.constEnd();

    tileZ_ = i->zoom();

    // determine whether the set of map tiles crosses the dateline.
    // A gap in the tiles indicates dateline crossing
    bool hasFarLeft = false;
    bool hasFarRight = false;
    bool hasMidLeft = false;
    bool hasMidRight = false;

    for (; i != end; ++i) {
        int x = (*i).x();
        if (x == 0)
            hasFarLeft = true;
        else if (x == (sideLength_ - 1))
            hasFarRight = true;
        else if (x == ((sideLength_ / 2) - 1)) {
            hasMidLeft = true;
        } else if (x == (sideLength_ / 2)) {
            hasMidRight = true;
        }
    }

    // if dateline crossing is detected we wrap all x pos of tiles
    // that are in the left half of the map.
    tileXWrapsBelow_ = 0;

    if (hasFarLeft && hasFarRight) {
        if (!hasMidRight) {
            tileXWrapsBelow_ = sideLength_ / 2;
        } else if (!hasMidLeft) {
            tileXWrapsBelow_ = (sideLength_ / 2) - 1;
        }
    }

    // finally, determine the min and max bounds
    i = tiles.constBegin();

    QGeoTileSpec tile = *i;

    int x = tile.x();
    if (tile.x() < tileXWrapsBelow_)
        x += sideLength_;

    minTileX_ = x;
    maxTileX_ = x;
    minTileY_ = tile.y();
    maxTileY_ = tile.y();

    ++i;

    for (; i != end; ++i) {
        tile = *i;
        int x = tile.x();
        if (tile.x() < tileXWrapsBelow_)
            x += sideLength_;

        minTileX_ = qMin(minTileX_, x);
        maxTileX_ = qMax(maxTileX_, x);
        minTileY_ = qMin(minTileY_, tile.y());
        maxTileY_ = qMax(maxTileY_, tile.y());
    }
}

void QGeoMapScenePrivate::setupCamera()
{
    double f = 1.0 * qMin(screenSize_.width(), screenSize_.height());

    // fraction of zoom level
    double z = std::pow(2.0, cameraData_.zoomLevel() - intZoomLevel_);

    // calculate altitdue that allows the visible map tiles
    // to fit in the screen correctly (note that a larger f will cause
    // the camera be higher, resulting in gray areas displayed around
    // the tiles)
    double altitude = scaleFactor_ * f / (2.0 * z);

    double aspectRatio = 1.0 * screenSize_.width() / screenSize_.height();

    double a = f / (z * tileSize_);

    // mercatorWidth_ and mercatorHeight_ define the ratio for
    // mercator and screen coordinate conversion,
    // see mercatorToScreenPosition() and screenPositionToMercator()
    if (aspectRatio > 1.0) {
        mercatorHeight_ = a;
        mercatorWidth_ = a * aspectRatio;
    } else {
        mercatorWidth_ = a;
        mercatorHeight_ = a / aspectRatio;
    }

    // calculate center

    double edge = scaleFactor_ * tileSize_;

    // first calculate the camera center in map space in the range of 0 <-> sideLength (2^z)
    QDoubleVector3D center = (sideLength_ * QGeoProjection::coordToMercator(cameraData_.center()));

    // wrap the center if necessary (due to dateline crossing)
    if (center.x() < tileXWrapsBelow_)
        center.setX(center.x() + 1.0 * sideLength_);

    mercatorCenterX_ = center.x();
    mercatorCenterY_ = center.y();

    // work out where the camera center is w.r.t minimum tile bounds
    center.setX(center.x() - 1.0 * minTileX_);
    center.setY(1.0 * minTileY_ - center.y());

    // letter box vertically
    if (useVerticalLock_ && (mercatorHeight_ > 1.0 * sideLength_)) {
        center.setY(-1.0 * sideLength_ / 2.0);
        mercatorCenterY_ = sideLength_ / 2.0;
        screenOffsetY_ = screenSize_.height() * (0.5 - sideLength_ / (2 * mercatorHeight_));
        screenHeight_ = screenSize_.height() - 2 * screenOffsetY_;
        mercatorHeight_ = 1.0 * sideLength_;
        verticalLock_ = true;
    } else {
        screenOffsetY_ = 0.0;
        screenHeight_ = screenSize_.height();
        verticalLock_ = false;
    }

    if (mercatorWidth_ > 1.0 * sideLength_) {
        screenOffsetX_ = screenSize_.width() * (0.5 - (sideLength_ / (2 * mercatorWidth_)));
        screenWidth_ = screenSize_.width() - 2 * screenOffsetX_;
        mercatorWidth_ = 1.0 * sideLength_;
    } else {
        screenOffsetX_ = 0.0;
        screenWidth_ = screenSize_.width();
    }

    // apply necessary scaling to the camera center
    center *= edge;

    // calculate eye

    QDoubleVector3D eye = center;
    eye.setZ(altitude);

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
    double farPlane = 4.0 * edge;

    cameraUp_ = up;
    cameraCenter_ = center;
    cameraEye_ = eye;

    float halfWidth = 1;
    float halfHeight = 1;
    if (aspectRatio > 1.0) {
        halfWidth *= aspectRatio;
    } else if (aspectRatio > 0.0f && aspectRatio < 1.0f) {
        halfHeight /= aspectRatio;
    }
    projectionMatrix_.setToIdentity();
    projectionMatrix_.frustum(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
}

class QGeoMapTileContainerNode : public QSGTransformNode
{
public:
    void addChild(const QGeoTileSpec &spec, QSGSimpleTextureNode *node)
    {
        tiles.insert(spec, node);
        appendChildNode(node);
    }
    QHash<QGeoTileSpec, QSGSimpleTextureNode *> tiles;
};

class QGeoMapRootNode : public QSGClipNode
{
public:
    QGeoMapRootNode()
        : isTextureLinear(false)
        , geometry(QSGGeometry::defaultAttributes_Point2D(), 4)
        , root(new QSGTransformNode())
        , tiles(new QGeoMapTileContainerNode())
        , wrapLeft(new QGeoMapTileContainerNode())
        , wrapRight(new QGeoMapTileContainerNode())
    {
        setIsRectangular(true);
        setGeometry(&geometry);
        root->appendChildNode(tiles);
        root->appendChildNode(wrapLeft);
        root->appendChildNode(wrapRight);
        appendChildNode(root);
    }

    ~QGeoMapRootNode()
    {
        qDeleteAll(textures.values());
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

    void updateTiles(QGeoMapTileContainerNode *root, QGeoMapScenePrivate *d, double camAdjust);

    bool isTextureLinear;

    QSGGeometry geometry;
    QRect clipRect;

    QSGTransformNode *root;

    QGeoMapTileContainerNode *tiles;        // The majority of the tiles
    QGeoMapTileContainerNode *wrapLeft;     // When zoomed out, the tiles that wrap around on the left.
    QGeoMapTileContainerNode *wrapRight;    // When zoomed out, the tiles that wrap around on the right

    QHash<QGeoTileSpec, QSGTexture *> textures;
};

static bool qgeomapscene_isTileInViewport(const QSGGeometry::TexturedPoint2D *tp, const QMatrix4x4 &matrix) {
    QPolygonF polygon; polygon.reserve(4);
    for (int i=0; i<4; ++i)
        polygon << matrix * QPointF(tp[i].x, tp[i].y);
    return QRectF(-1, -1, 2, 2).intersects(polygon.boundingRect());
}

static QVector3D toVector3D(const QDoubleVector3D& in)
{
    return QVector3D(in.x(), in.y(), in.z());
}

void QGeoMapRootNode::updateTiles(QGeoMapTileContainerNode *root,
                                  QGeoMapScenePrivate *d,
                                  double camAdjust)
{
    // Set up the matrix...
    QDoubleVector3D eye = d->cameraEye_;
    eye.setX(eye.x() + camAdjust);
    QDoubleVector3D center = d->cameraCenter_;
    center.setX(center.x() + camAdjust);
    QMatrix4x4 cameraMatrix;
    cameraMatrix.lookAt(toVector3D(eye), toVector3D(center), toVector3D(d->cameraUp_));
    root->setMatrix(d->projectionMatrix_ * cameraMatrix);

    QSet<QGeoTileSpec> tilesInSG = QSet<QGeoTileSpec>::fromList(root->tiles.keys());
    QSet<QGeoTileSpec> toRemove = tilesInSG - d->visibleTiles_;
    QSet<QGeoTileSpec> toAdd = d->visibleTiles_ - tilesInSG;

    foreach (const QGeoTileSpec &s, toRemove)
        delete root->tiles.take(s);

    for (QHash<QGeoTileSpec, QSGSimpleTextureNode *>::iterator it = root->tiles.begin();
         it != root->tiles.end(); ) {
        QSGGeometry visualGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
        QSGGeometry::TexturedPoint2D *v = visualGeometry.vertexDataAsTexturedPoint2D();
        bool ok = d->buildGeometry(it.key(), v) && qgeomapscene_isTileInViewport(v, root->matrix());
        QSGSimpleTextureNode *node = it.value();
        QSGNode::DirtyState dirtyBits = 0;

        // Check and handle changes to vertex data.
        if (ok && memcmp(node->geometry()->vertexData(), v, 4 * sizeof(QSGGeometry::TexturedPoint2D)) != 0) {
            if (v[0].x == v[3].x || v[0].y == v[3].y) { // top-left == bottom-right => invalid => remove
                ok = false;
            } else {
                memcpy(node->geometry()->vertexData(), v, 4 * sizeof(QSGGeometry::TexturedPoint2D));
                dirtyBits |= QSGNode::DirtyGeometry;
            }
        }

        if (!ok) {
            it = root->tiles.erase(it);
            delete node;
        } else {
            if (isTextureLinear != d->linearScaling_) {
                node->setFiltering(d->linearScaling_ ? QSGTexture::Linear : QSGTexture::Nearest);
                dirtyBits |= QSGNode::DirtyMaterial;
            }
            if (dirtyBits != 0)
                node->markDirty(dirtyBits);
            it++;
        }
    }

    foreach (const QGeoTileSpec &s, toAdd) {
        QGeoTileTexture *tileTexture = d->textures_.value(s).data();
        if (!tileTexture || tileTexture->image.isNull())
            continue;
        QSGSimpleTextureNode *tileNode = new QSGSimpleTextureNode();
        // note: setTexture will update coordinates so do it here, before we buildGeometry
        tileNode->setTexture(textures.value(s));
        Q_ASSERT(tileNode->geometry());
        Q_ASSERT(tileNode->geometry()->attributes() == QSGGeometry::defaultAttributes_TexturedPoint2D().attributes);
        Q_ASSERT(tileNode->geometry()->vertexCount() == 4);
        if (d->buildGeometry(s, tileNode->geometry()->vertexDataAsTexturedPoint2D())
                && qgeomapscene_isTileInViewport(tileNode->geometry()->vertexDataAsTexturedPoint2D(), root->matrix())) {
            tileNode->setFiltering(d->linearScaling_ ? QSGTexture::Linear : QSGTexture::Nearest);
            root->addChild(s, tileNode);
        } else {
            delete tileNode;
        }
    }
}

QSGNode *QGeoMapScene::updateSceneGraph(QSGNode *oldNode, QQuickWindow *window)
{
    Q_D(QGeoMapScene);
    float w = d->screenSize_.width();
    float h = d->screenSize_.height();
    if (w <= 0 || h <= 0) {
        delete oldNode;
        return 0;
    }

    QGeoMapRootNode *mapRoot = static_cast<QGeoMapRootNode *>(oldNode);
    if (!mapRoot)
        mapRoot = new QGeoMapRootNode();

    mapRoot->setClipRect(QRect(d->screenOffsetX_, d->screenOffsetY_, d->screenWidth_, d->screenHeight_));

    QMatrix4x4 itemSpaceMatrix;
    itemSpaceMatrix.scale(w / 2, h / 2);
    itemSpaceMatrix.translate(1, 1);
    itemSpaceMatrix.scale(1, -1);
    mapRoot->root->setMatrix(itemSpaceMatrix);

    QSet<QGeoTileSpec> textures = QSet<QGeoTileSpec>::fromList(mapRoot->textures.keys());
    QSet<QGeoTileSpec> toRemove = textures - d->visibleTiles_;
    QSet<QGeoTileSpec> toAdd = d->visibleTiles_ - textures;

    foreach (const QGeoTileSpec &spec, toRemove)
        mapRoot->textures.take(spec)->deleteLater();
    foreach (const QGeoTileSpec &spec, toAdd) {
        QGeoTileTexture *tileTexture = d->textures_.value(spec).data();
        if (!tileTexture || tileTexture->image.isNull())
            continue;
        mapRoot->textures.insert(spec, window->createTextureFromImage(tileTexture->image));
    }

    double sideLength = d->scaleFactor_ * d->tileSize_ * d->sideLength_;
    mapRoot->updateTiles(mapRoot->tiles, d, 0);
    mapRoot->updateTiles(mapRoot->wrapLeft, d, +sideLength);
    mapRoot->updateTiles(mapRoot->wrapRight, d, -sideLength);

    mapRoot->isTextureLinear = d->linearScaling_;

    return mapRoot;
}

QT_END_NAMESPACE
