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

#ifndef QDECLARATIVEPOLYLINEMAPITEM
#define QDECLARATIVEPOLYLINEMAPITEM

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtLocation/private/qdeclarativegeomapitembase_p.h>
#include <QtLocation/private/qgeomapitemgeometry_p.h>

#include <QtPositioning/QGeoPath>
#include <QtPositioning/private/qdoublevector2d_p.h>
#include <QSGGeometryNode>
#include <QSGFlatColorMaterial>

QT_BEGIN_NAMESPACE

class MapPolylineNode;

class Q_LOCATION_PRIVATE_EXPORT QDeclarativeMapLineProperties : public QObject
{
    Q_OBJECT

    Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY widthChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

public:
    explicit QDeclarativeMapLineProperties(QObject *parent = 0);

    QColor color() const;
    void setColor(const QColor &color);

    qreal width() const;
    void setWidth(qreal width);

Q_SIGNALS:
    void widthChanged(qreal width);
    void colorChanged(const QColor &color);

private:
    qreal width_;
    QColor color_;
};

class QGeoMapPolylineGeometry : public QGeoMapItemGeometry
{
public:
    QGeoMapPolylineGeometry();

    void updateSourcePoints(const QGeoMap &map,
                            const QList<QDoubleVector2D> &path,
                            const QGeoCoordinate geoLeftBound);

    void updateScreenPoints(const QGeoMap &map,
                            qreal strokeWidth);

protected:
    QList<QList<QDoubleVector2D> > clipPath(const QGeoMap &map,
                    const QList<QDoubleVector2D> &path,
                    QDoubleVector2D &leftBoundWrapped);

    void pathToScreen(const QGeoMap &map,
                      const QList<QList<QDoubleVector2D> > &clippedPaths,
                      const QDoubleVector2D &leftBoundWrapped);

private:
    QVector<qreal> srcPoints_;
    QVector<QPainterPath::ElementType> srcPointTypes_;

    friend class QDeclarativeCircleMapItem;
    friend class QDeclarativePolygonMapItem;
    friend class QDeclarativeRectangleMapItem;
};

class Q_LOCATION_PRIVATE_EXPORT QDeclarativePolylineMapItem : public QDeclarativeGeoMapItemBase
{
    Q_OBJECT

    Q_PROPERTY(QJSValue path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(QDeclarativeMapLineProperties *line READ line CONSTANT)

public:
    explicit QDeclarativePolylineMapItem(QQuickItem *parent = 0);
    ~QDeclarativePolylineMapItem();

    virtual void setMap(QDeclarativeGeoMap *quickMap, QGeoMap *map) Q_DECL_OVERRIDE;
       //from QuickItem
    virtual QSGNode *updateMapItemPaintNode(QSGNode *, UpdatePaintNodeData *) Q_DECL_OVERRIDE;

    Q_INVOKABLE int pathLength() const;
    Q_INVOKABLE void addCoordinate(const QGeoCoordinate &coordinate);
    Q_INVOKABLE void insertCoordinate(int index, const QGeoCoordinate &coordinate);
    Q_INVOKABLE void replaceCoordinate(int index, const QGeoCoordinate &coordinate);
    Q_INVOKABLE QGeoCoordinate coordinateAt(int index) const;
    Q_INVOKABLE bool containsCoordinate(const QGeoCoordinate &coordinate);
    Q_INVOKABLE void removeCoordinate(const QGeoCoordinate &coordinate);
    Q_INVOKABLE void removeCoordinate(int index);

    QJSValue path() const;
    virtual void setPath(const QJSValue &value);

    bool contains(const QPointF &point) const Q_DECL_OVERRIDE;
    const QGeoShape &geoShape() const Q_DECL_OVERRIDE;
    QGeoMap::ItemType itemType() const Q_DECL_OVERRIDE;

    QDeclarativeMapLineProperties *line();

Q_SIGNALS:
    void pathChanged();

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) Q_DECL_OVERRIDE;
    void setPathFromGeoList(const QList<QGeoCoordinate> &path);
    void updatePolish() Q_DECL_OVERRIDE;

protected Q_SLOTS:
    void markSourceDirtyAndUpdate();
    void updateAfterLinePropertiesChanged();
    virtual void afterViewportChanged(const QGeoMapViewportChangeEvent &event) Q_DECL_OVERRIDE;

private:
    void regenerateCache();
    void updateCache();

    QGeoPath geopath_;
    QList<QDoubleVector2D> geopathProjected_;
    QDeclarativeMapLineProperties line_;
    QColor color_;
    bool dirtyMaterial_;
    QGeoMapPolylineGeometry geometry_;
    bool updatingGeometry_;
};

//////////////////////////////////////////////////////////////////////

class MapPolylineNode : public QSGGeometryNode
{

public:
    MapPolylineNode();
    ~MapPolylineNode();

    void update(const QColor &fillColor, const QGeoMapItemGeometry *shape);
    bool isSubtreeBlocked() const;

private:
    QSGFlatColorMaterial fill_material_;
    QSGGeometry geometry_;
    bool blocked_;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QDeclarativeMapLineProperties)
QML_DECLARE_TYPE(QDeclarativePolylineMapItem)

#endif /* QDECLARATIVEPOLYLINEMAPITEM_H_ */
