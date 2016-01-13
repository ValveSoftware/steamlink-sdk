/****************************************************************************
 **
 ** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QGEOMAPITEMGEOMETRY_H
#define QGEOMAPITEMGEOMETRY_H

#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QGeoCoordinate>
#include <QVector2D>
#include <QList>

QT_BEGIN_NAMESPACE

class QSGGeometry;
class QGeoMap;

class QGeoMapItemGeometry
{
public:
    QGeoMapItemGeometry();

    inline bool isSourceDirty() const { return sourceDirty_; }
    inline bool isScreenDirty() const { return screenDirty_; }
    inline void markSourceDirty() { sourceDirty_ = true; screenDirty_ = true; }
    inline void markScreenDirty() { screenDirty_ = true; clipToViewport_ = true; }
    inline void markFullScreenDirty() { screenDirty_ = true; clipToViewport_ = false;}
    inline void markClean() { screenDirty_ = (sourceDirty_ = false); clipToViewport_ = true;}

    inline void setPreserveGeometry(bool value, QGeoCoordinate geoLeftBound = QGeoCoordinate())
    {
        preserveGeometry_ = value;
        if (preserveGeometry_)
            geoLeftBound_ = geoLeftBound;
    }
    inline QGeoCoordinate geoLeftBound() { return geoLeftBound_; }

    inline QRectF sourceBoundingBox() const { return sourceBounds_; }
    inline QRectF screenBoundingBox() const { return screenBounds_; }

    inline QPointF firstPointOffset() const { return firstPointOffset_; }
    void translate(const QPointF &offset);

    inline const QGeoCoordinate &origin() const { return srcOrigin_; }

    inline bool contains(const QPointF &screenPoint) const {
        return screenOutline_.contains(screenPoint);
    }

    inline QVector2D vertex(quint32 index) const {
        return QVector2D(screenVertices_[index]);
    }

    inline QVector<QPointF> vertices() const { return screenVertices_; }
    inline QVector<quint32> indices() const { return screenIndices_; }

    inline bool isIndexed() const { return (!screenIndices_.isEmpty()); }

    /* Size is # of triangles */
    inline quint32 size() const
    {
        if (isIndexed())
            return screenIndices_.size() / 3;
        else
            return screenVertices_.size() / 3;
    }

    inline void clear() { firstPointOffset_ = QPointF(0,0);
                          screenVertices_.clear(); screenIndices_.clear(); }

    void allocateAndFill(QSGGeometry *geom) const;

    double geoDistanceToScreenWidth(const QGeoMap &map,
                                           const QGeoCoordinate &fromCoord,
                                           const QGeoCoordinate &toCoord);

    static QRectF translateToCommonOrigin(const QList<QGeoMapItemGeometry *> &geoms);


protected:
    bool sourceDirty_;
    bool screenDirty_;
    bool clipToViewport_;
    bool preserveGeometry_;
    QGeoCoordinate geoLeftBound_;

    QPointF firstPointOffset_;

    QPainterPath screenOutline_;

    QRectF sourceBounds_;
    QRectF screenBounds_;

    QGeoCoordinate srcOrigin_;

    QVector<QPointF> screenVertices_;
    QVector<quint32> screenIndices_;
};

QT_END_NAMESPACE

#endif // QGEOMAPITEMGEOMETRY_H
