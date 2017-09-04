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

#ifndef QOPENVGMATRIX_H
#define QOPENVGMATRIX_H

#include <QtCore/qdebug.h>
#include <QtCore/QDataStream>
#include <QtCore/QPointF>

QT_BEGIN_NAMESPACE

class QOpenVGMatrix
{
public:
    QOpenVGMatrix();
    explicit QOpenVGMatrix(const float *values);

    const float& operator()(int row, int column) const;
    float& operator()(int row, int column);

    bool isIdentity() const;
    void setToIdentity();

    bool isAffine() const;

    QPointF map(const QPointF& point) const;

    void fill(float value);

    QOpenVGMatrix transposed() const;

    QOpenVGMatrix& operator+=(const QOpenVGMatrix& other);
    QOpenVGMatrix& operator-=(const QOpenVGMatrix& other);
    QOpenVGMatrix& operator*=(const QOpenVGMatrix& other);
    QOpenVGMatrix& operator*=(float factor);
    QOpenVGMatrix& operator/=(float divisor);
    friend QOpenVGMatrix operator*(const QOpenVGMatrix& m1, const QOpenVGMatrix& m2);
    friend QPointF operator*(const QPointF& point, const QOpenVGMatrix& matrix);
    friend QPointF operator*(const QOpenVGMatrix& matrix, const QPointF& point);
#ifndef QT_NO_DEBUG_STREAM
    friend QDebug operator<<(QDebug dbg, const QOpenVGMatrix &m);
#endif
    bool operator==(const QOpenVGMatrix& other) const;
    bool operator!=(const QOpenVGMatrix& other) const;

    void copyDataTo(float *values) const;

    float *data() { return *m; }
    const float *data() const { return *m; }
    const float *constData() const { return *m; }

private:
    float m[3][3];
};

QOpenVGMatrix operator*(const QOpenVGMatrix& m1, const QOpenVGMatrix& m2);
QPointF operator*(const QPointF& point, const QOpenVGMatrix& matrix);
QPointF operator*(const QOpenVGMatrix& matrix, const QPointF& point);

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QOpenVGMatrix &m);
#endif

#ifndef QT_NO_DATASTREAM
QDataStream &operator<<(QDataStream &, const QOpenVGMatrix &);
QDataStream &operator>>(QDataStream &, QOpenVGMatrix &);
#endif

QT_END_NAMESPACE

#endif // QOPENVGMATRIX_H
