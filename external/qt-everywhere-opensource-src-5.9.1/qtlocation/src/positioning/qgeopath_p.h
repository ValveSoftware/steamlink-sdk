/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#ifndef QGEOPATH_P_H
#define QGEOPATH_P_H

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

#include "qgeoshape_p.h"
#include "qgeocoordinate.h"
#include "qlocationutils_p.h"

#include <QtCore/QVector>

QT_BEGIN_NAMESPACE

class QGeoPathPrivate : public QGeoShapePrivate
{
public:
    QGeoPathPrivate();
    QGeoPathPrivate(const QList<QGeoCoordinate> &path, const qreal width = 0.0);
    QGeoPathPrivate(const QGeoPathPrivate &other);
    ~QGeoPathPrivate();

    bool isValid() const Q_DECL_OVERRIDE;
    bool isEmpty() const Q_DECL_OVERRIDE;
    bool contains(const QGeoCoordinate &coordinate) const Q_DECL_OVERRIDE;

    QGeoCoordinate center() const Q_DECL_OVERRIDE;
    QGeoRectangle boundingGeoRectangle() const Q_DECL_OVERRIDE;
    void extendShape(const QGeoCoordinate &coordinate) Q_DECL_OVERRIDE;
    void translate(double degreesLatitude, double degreesLongitude);

    QGeoShapePrivate *clone() const Q_DECL_OVERRIDE;

    bool operator==(const QGeoShapePrivate &other) const Q_DECL_OVERRIDE;

    const QList<QGeoCoordinate> &path() const;
    void setPath(const QList<QGeoCoordinate> &path);
    qreal width() const;
    void setWidth(const qreal &width);
    double length(int indexFrom, int indexTo) const;
    void addCoordinate(const QGeoCoordinate &coordinate);
    void insertCoordinate(int index, const QGeoCoordinate &coordinate);
    void replaceCoordinate(int index, const QGeoCoordinate &coordinate);
    QGeoCoordinate coordinateAt(int index) const;
    bool containsCoordinate(const QGeoCoordinate &coordinate) const;
    void removeCoordinate(const QGeoCoordinate &coordinate);
    void removeCoordinate(int index);
    void computeBoundingBox();
    void updateBoundingBox();


    QList<QGeoCoordinate> m_path;
    QVector<double> m_deltaXs; // longitude deltas from m_path[0]
    double m_minX;             // minimum value inside deltaXs
    double m_maxX;             // maximum value inside deltaXs
    double m_minLati;             // minimum latitude. paths do not wrap around through the poles
    double m_maxLati;             // minimum latitude. paths do not wrap around through the poles
    QGeoRectangle m_bbox;
    qreal m_width;
};

QT_END_NAMESPACE

#endif // QGEOPATH_P_H
