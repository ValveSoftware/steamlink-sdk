/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qdoublematrix4x4_p.h"
#include <QtCore/qmath.h>
//#include <QtCore/qvariant.h>
#include <QtCore/qdatastream.h>
#include <cmath>

QT_BEGIN_NAMESPACE

static const double inv_dist_to_plane = 1.0 / 1024.0;

QDoubleMatrix4x4::QDoubleMatrix4x4(const double *values)
{
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            m[col][row] = values[row * 4 + col];
    flagBits = General;
}

QDoubleMatrix4x4::QDoubleMatrix4x4(const double *values, int cols, int rows)
{
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            if (col < cols && row < rows)
                m[col][row] = values[col * rows + row];
            else if (col == row)
                m[col][row] = 1.0;
            else
                m[col][row] = 0.0;
        }
    }
    flagBits = General;
}

static inline double matrixDet2(const double m[4][4], int col0, int col1, int row0, int row1)
{
    return m[col0][row0] * m[col1][row1] - m[col0][row1] * m[col1][row0];
}

static inline double matrixDet3
    (const double m[4][4], int col0, int col1, int col2,
     int row0, int row1, int row2)
{
    return m[col0][row0] * matrixDet2(m, col1, col2, row1, row2)
            - m[col1][row0] * matrixDet2(m, col0, col2, row1, row2)
            + m[col2][row0] * matrixDet2(m, col0, col1, row1, row2);
}

static inline double matrixDet4(const double m[4][4])
{
    double det;
    det  = m[0][0] * matrixDet3(m, 1, 2, 3, 1, 2, 3);
    det -= m[1][0] * matrixDet3(m, 0, 2, 3, 1, 2, 3);
    det += m[2][0] * matrixDet3(m, 0, 1, 3, 1, 2, 3);
    det -= m[3][0] * matrixDet3(m, 0, 1, 2, 1, 2, 3);
    return det;
}

double QDoubleMatrix4x4::determinant() const
{
    if ((flagBits & ~(Translation | Rotation2D | Rotation)) == Identity)
        return 1.0;

    if (flagBits < Rotation2D)
        return m[0][0] * m[1][1] * m[2][2]; // Translation | Scale
    if (flagBits < Perspective)
        return matrixDet3(m, 0, 1, 2, 0, 1, 2);
    return matrixDet4(m);
}

QDoubleMatrix4x4 QDoubleMatrix4x4::inverted(bool *invertible) const
{
    // Handle some of the easy cases first.
    if (flagBits == Identity) {
        if (invertible)
            *invertible = true;
        return QDoubleMatrix4x4();
    } else if (flagBits == Translation) {
        QDoubleMatrix4x4 inv;
        inv.m[3][0] = -m[3][0];
        inv.m[3][1] = -m[3][1];
        inv.m[3][2] = -m[3][2];
        inv.flagBits = Translation;
        if (invertible)
            *invertible = true;
        return inv;
    } else if (flagBits < Rotation2D) {
        // Translation | Scale
        if (m[0][0] == 0 || m[1][1] == 0 || m[2][2] == 0) {
            if (invertible)
                *invertible = false;
            return QDoubleMatrix4x4();
        }
        QDoubleMatrix4x4 inv;
        inv.m[0][0] = 1.0 / m[0][0];
        inv.m[1][1] = 1.0 / m[1][1];
        inv.m[2][2] = 1.0 / m[2][2];
        inv.m[3][0] = -m[3][0] * inv.m[0][0];
        inv.m[3][1] = -m[3][1] * inv.m[1][1];
        inv.m[3][2] = -m[3][2] * inv.m[2][2];
        inv.flagBits = flagBits;

        if (invertible)
            *invertible = true;
        return inv;
    } else if ((flagBits & ~(Translation | Rotation2D | Rotation)) == Identity) {
        if (invertible)
            *invertible = true;
        return orthonormalInverse();
    } else if (flagBits < Perspective) {
        QDoubleMatrix4x4 inv(1); // The "1" says to not load the identity.

        double det = matrixDet3(m, 0, 1, 2, 0, 1, 2);
        if (det == 0.0) {
            if (invertible)
                *invertible = false;
            return QDoubleMatrix4x4();
        }
        det = 1.0 / det;

        inv.m[0][0] =  matrixDet2(m, 1, 2, 1, 2) * det;
        inv.m[0][1] = -matrixDet2(m, 0, 2, 1, 2) * det;
        inv.m[0][2] =  matrixDet2(m, 0, 1, 1, 2) * det;
        inv.m[0][3] = 0;
        inv.m[1][0] = -matrixDet2(m, 1, 2, 0, 2) * det;
        inv.m[1][1] =  matrixDet2(m, 0, 2, 0, 2) * det;
        inv.m[1][2] = -matrixDet2(m, 0, 1, 0, 2) * det;
        inv.m[1][3] = 0;
        inv.m[2][0] =  matrixDet2(m, 1, 2, 0, 1) * det;
        inv.m[2][1] = -matrixDet2(m, 0, 2, 0, 1) * det;
        inv.m[2][2] =  matrixDet2(m, 0, 1, 0, 1) * det;
        inv.m[2][3] = 0;
        inv.m[3][0] = -inv.m[0][0] * m[3][0] - inv.m[1][0] * m[3][1] - inv.m[2][0] * m[3][2];
        inv.m[3][1] = -inv.m[0][1] * m[3][0] - inv.m[1][1] * m[3][1] - inv.m[2][1] * m[3][2];
        inv.m[3][2] = -inv.m[0][2] * m[3][0] - inv.m[1][2] * m[3][1] - inv.m[2][2] * m[3][2];
        inv.m[3][3] = 1;
        inv.flagBits = flagBits;

        if (invertible)
            *invertible = true;
        return inv;
    }

    QDoubleMatrix4x4 inv(1); // The "1" says to not load the identity.

    double det = matrixDet4(m);
    if (det == 0.0) {
        if (invertible)
            *invertible = false;
        return QDoubleMatrix4x4();
    }
    det = 1.0 / det;

    inv.m[0][0] =  matrixDet3(m, 1, 2, 3, 1, 2, 3) * det;
    inv.m[0][1] = -matrixDet3(m, 0, 2, 3, 1, 2, 3) * det;
    inv.m[0][2] =  matrixDet3(m, 0, 1, 3, 1, 2, 3) * det;
    inv.m[0][3] = -matrixDet3(m, 0, 1, 2, 1, 2, 3) * det;
    inv.m[1][0] = -matrixDet3(m, 1, 2, 3, 0, 2, 3) * det;
    inv.m[1][1] =  matrixDet3(m, 0, 2, 3, 0, 2, 3) * det;
    inv.m[1][2] = -matrixDet3(m, 0, 1, 3, 0, 2, 3) * det;
    inv.m[1][3] =  matrixDet3(m, 0, 1, 2, 0, 2, 3) * det;
    inv.m[2][0] =  matrixDet3(m, 1, 2, 3, 0, 1, 3) * det;
    inv.m[2][1] = -matrixDet3(m, 0, 2, 3, 0, 1, 3) * det;
    inv.m[2][2] =  matrixDet3(m, 0, 1, 3, 0, 1, 3) * det;
    inv.m[2][3] = -matrixDet3(m, 0, 1, 2, 0, 1, 3) * det;
    inv.m[3][0] = -matrixDet3(m, 1, 2, 3, 0, 1, 2) * det;
    inv.m[3][1] =  matrixDet3(m, 0, 2, 3, 0, 1, 2) * det;
    inv.m[3][2] = -matrixDet3(m, 0, 1, 3, 0, 1, 2) * det;
    inv.m[3][3] =  matrixDet3(m, 0, 1, 2, 0, 1, 2) * det;
    inv.flagBits = flagBits;

    if (invertible)
        *invertible = true;
    return inv;
}

QDoubleMatrix4x4 QDoubleMatrix4x4::transposed() const
{
    QDoubleMatrix4x4 result(1); // The "1" says to not load the identity.
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result.m[col][row] = m[row][col];
        }
    }
    // When a translation is transposed, it becomes a perspective transformation.
    result.flagBits = (flagBits & Translation ? General : flagBits);
    return result;
}

QDoubleMatrix4x4& QDoubleMatrix4x4::operator/=(double divisor)
{
    m[0][0] /= divisor;
    m[0][1] /= divisor;
    m[0][2] /= divisor;
    m[0][3] /= divisor;
    m[1][0] /= divisor;
    m[1][1] /= divisor;
    m[1][2] /= divisor;
    m[1][3] /= divisor;
    m[2][0] /= divisor;
    m[2][1] /= divisor;
    m[2][2] /= divisor;
    m[2][3] /= divisor;
    m[3][0] /= divisor;
    m[3][1] /= divisor;
    m[3][2] /= divisor;
    m[3][3] /= divisor;
    flagBits = General;
    return *this;
}

QDoubleMatrix4x4 operator/(const QDoubleMatrix4x4& matrix, double divisor)
{
    QDoubleMatrix4x4 m(1); // The "1" says to not load the identity.
    m.m[0][0] = matrix.m[0][0] / divisor;
    m.m[0][1] = matrix.m[0][1] / divisor;
    m.m[0][2] = matrix.m[0][2] / divisor;
    m.m[0][3] = matrix.m[0][3] / divisor;
    m.m[1][0] = matrix.m[1][0] / divisor;
    m.m[1][1] = matrix.m[1][1] / divisor;
    m.m[1][2] = matrix.m[1][2] / divisor;
    m.m[1][3] = matrix.m[1][3] / divisor;
    m.m[2][0] = matrix.m[2][0] / divisor;
    m.m[2][1] = matrix.m[2][1] / divisor;
    m.m[2][2] = matrix.m[2][2] / divisor;
    m.m[2][3] = matrix.m[2][3] / divisor;
    m.m[3][0] = matrix.m[3][0] / divisor;
    m.m[3][1] = matrix.m[3][1] / divisor;
    m.m[3][2] = matrix.m[3][2] / divisor;
    m.m[3][3] = matrix.m[3][3] / divisor;
    m.flagBits = QDoubleMatrix4x4::General;
    return m;
}

void QDoubleMatrix4x4::scale(const QDoubleVector3D& vector)
{
    double vx = vector.x();
    double vy = vector.y();
    double vz = vector.z();
    if (flagBits < Scale) {
        m[0][0] = vx;
        m[1][1] = vy;
        m[2][2] = vz;
    } else if (flagBits < Rotation2D) {
        m[0][0] *= vx;
        m[1][1] *= vy;
        m[2][2] *= vz;
    } else if (flagBits < Rotation) {
        m[0][0] *= vx;
        m[0][1] *= vx;
        m[1][0] *= vy;
        m[1][1] *= vy;
        m[2][2] *= vz;
    } else {
        m[0][0] *= vx;
        m[0][1] *= vx;
        m[0][2] *= vx;
        m[0][3] *= vx;
        m[1][0] *= vy;
        m[1][1] *= vy;
        m[1][2] *= vy;
        m[1][3] *= vy;
        m[2][0] *= vz;
        m[2][1] *= vz;
        m[2][2] *= vz;
        m[2][3] *= vz;
    }
    flagBits |= Scale;
}

void QDoubleMatrix4x4::scale(double x, double y)
{
    if (flagBits < Scale) {
        m[0][0] = x;
        m[1][1] = y;
    } else if (flagBits < Rotation2D) {
        m[0][0] *= x;
        m[1][1] *= y;
    } else if (flagBits < Rotation) {
        m[0][0] *= x;
        m[0][1] *= x;
        m[1][0] *= y;
        m[1][1] *= y;
    } else {
        m[0][0] *= x;
        m[0][1] *= x;
        m[0][2] *= x;
        m[0][3] *= x;
        m[1][0] *= y;
        m[1][1] *= y;
        m[1][2] *= y;
        m[1][3] *= y;
    }
    flagBits |= Scale;
}

void QDoubleMatrix4x4::scale(double x, double y, double z)
{
    if (flagBits < Scale) {
        m[0][0] = x;
        m[1][1] = y;
        m[2][2] = z;
    } else if (flagBits < Rotation2D) {
        m[0][0] *= x;
        m[1][1] *= y;
        m[2][2] *= z;
    } else if (flagBits < Rotation) {
        m[0][0] *= x;
        m[0][1] *= x;
        m[1][0] *= y;
        m[1][1] *= y;
        m[2][2] *= z;
    } else {
        m[0][0] *= x;
        m[0][1] *= x;
        m[0][2] *= x;
        m[0][3] *= x;
        m[1][0] *= y;
        m[1][1] *= y;
        m[1][2] *= y;
        m[1][3] *= y;
        m[2][0] *= z;
        m[2][1] *= z;
        m[2][2] *= z;
        m[2][3] *= z;
    }
    flagBits |= Scale;
}

void QDoubleMatrix4x4::scale(double factor)
{
    if (flagBits < Scale) {
        m[0][0] = factor;
        m[1][1] = factor;
        m[2][2] = factor;
    } else if (flagBits < Rotation2D) {
        m[0][0] *= factor;
        m[1][1] *= factor;
        m[2][2] *= factor;
    } else if (flagBits < Rotation) {
        m[0][0] *= factor;
        m[0][1] *= factor;
        m[1][0] *= factor;
        m[1][1] *= factor;
        m[2][2] *= factor;
    } else {
        m[0][0] *= factor;
        m[0][1] *= factor;
        m[0][2] *= factor;
        m[0][3] *= factor;
        m[1][0] *= factor;
        m[1][1] *= factor;
        m[1][2] *= factor;
        m[1][3] *= factor;
        m[2][0] *= factor;
        m[2][1] *= factor;
        m[2][2] *= factor;
        m[2][3] *= factor;
    }
    flagBits |= Scale;
}

void QDoubleMatrix4x4::translate(const QDoubleVector3D& vector)
{
    double vx = vector.x();
    double vy = vector.y();
    double vz = vector.z();
    if (flagBits == Identity) {
        m[3][0] = vx;
        m[3][1] = vy;
        m[3][2] = vz;
    } else if (flagBits == Translation) {
        m[3][0] += vx;
        m[3][1] += vy;
        m[3][2] += vz;
    } else if (flagBits == Scale) {
        m[3][0] = m[0][0] * vx;
        m[3][1] = m[1][1] * vy;
        m[3][2] = m[2][2] * vz;
    } else if (flagBits == (Translation | Scale)) {
        m[3][0] += m[0][0] * vx;
        m[3][1] += m[1][1] * vy;
        m[3][2] += m[2][2] * vz;
    } else if (flagBits < Rotation) {
        m[3][0] += m[0][0] * vx + m[1][0] * vy;
        m[3][1] += m[0][1] * vx + m[1][1] * vy;
        m[3][2] += m[2][2] * vz;
    } else {
        m[3][0] += m[0][0] * vx + m[1][0] * vy + m[2][0] * vz;
        m[3][1] += m[0][1] * vx + m[1][1] * vy + m[2][1] * vz;
        m[3][2] += m[0][2] * vx + m[1][2] * vy + m[2][2] * vz;
        m[3][3] += m[0][3] * vx + m[1][3] * vy + m[2][3] * vz;
    }
    flagBits |= Translation;
}

void QDoubleMatrix4x4::translate(double x, double y)
{
    if (flagBits == Identity) {
        m[3][0] = x;
        m[3][1] = y;
    } else if (flagBits == Translation) {
        m[3][0] += x;
        m[3][1] += y;
    } else if (flagBits == Scale) {
        m[3][0] = m[0][0] * x;
        m[3][1] = m[1][1] * y;
    } else if (flagBits == (Translation | Scale)) {
        m[3][0] += m[0][0] * x;
        m[3][1] += m[1][1] * y;
    } else if (flagBits < Rotation) {
        m[3][0] += m[0][0] * x + m[1][0] * y;
        m[3][1] += m[0][1] * x + m[1][1] * y;
    } else {
        m[3][0] += m[0][0] * x + m[1][0] * y;
        m[3][1] += m[0][1] * x + m[1][1] * y;
        m[3][2] += m[0][2] * x + m[1][2] * y;
        m[3][3] += m[0][3] * x + m[1][3] * y;
    }
    flagBits |= Translation;
}

void QDoubleMatrix4x4::translate(double x, double y, double z)
{
    if (flagBits == Identity) {
        m[3][0] = x;
        m[3][1] = y;
        m[3][2] = z;
    } else if (flagBits == Translation) {
        m[3][0] += x;
        m[3][1] += y;
        m[3][2] += z;
    } else if (flagBits == Scale) {
        m[3][0] = m[0][0] * x;
        m[3][1] = m[1][1] * y;
        m[3][2] = m[2][2] * z;
    } else if (flagBits == (Translation | Scale)) {
        m[3][0] += m[0][0] * x;
        m[3][1] += m[1][1] * y;
        m[3][2] += m[2][2] * z;
    } else if (flagBits < Rotation) {
        m[3][0] += m[0][0] * x + m[1][0] * y;
        m[3][1] += m[0][1] * x + m[1][1] * y;
        m[3][2] += m[2][2] * z;
    } else {
        m[3][0] += m[0][0] * x + m[1][0] * y + m[2][0] * z;
        m[3][1] += m[0][1] * x + m[1][1] * y + m[2][1] * z;
        m[3][2] += m[0][2] * x + m[1][2] * y + m[2][2] * z;
        m[3][3] += m[0][3] * x + m[1][3] * y + m[2][3] * z;
    }
    flagBits |= Translation;
}

void QDoubleMatrix4x4::rotate(double angle, const QDoubleVector3D& vector)
{
    rotate(angle, vector.x(), vector.y(), vector.z());
}

void QDoubleMatrix4x4::rotate(double angle, double x, double y, double z)
{
    if (angle == 0.0)
        return;
    double c, s;
    if (angle == 90.0 || angle == -270.0) {
        s = 1.0;
        c = 0.0;
    } else if (angle == -90.0 || angle == 270.0) {
        s = -1.0;
        c = 0.0;
    } else if (angle == 180.0 || angle == -180.0) {
        s = 0.0;
        c = -1.0;
    } else {
        double a = qDegreesToRadians(angle);
        c = std::cos(a);
        s = std::sin(a);
    }
    if (x == 0.0) {
        if (y == 0.0) {
            if (z != 0.0) {
                // Rotate around the Z axis.
                if (z < 0)
                    s = -s;
                double tmp;
                m[0][0] = (tmp = m[0][0]) * c + m[1][0] * s;
                m[1][0] = m[1][0] * c - tmp * s;
                m[0][1] = (tmp = m[0][1]) * c + m[1][1] * s;
                m[1][1] = m[1][1] * c - tmp * s;
                m[0][2] = (tmp = m[0][2]) * c + m[1][2] * s;
                m[1][2] = m[1][2] * c - tmp * s;
                m[0][3] = (tmp = m[0][3]) * c + m[1][3] * s;
                m[1][3] = m[1][3] * c - tmp * s;

                flagBits |= Rotation2D;
                return;
            }
        } else if (z == 0.0) {
            // Rotate around the Y axis.
            if (y < 0)
                s = -s;
            double tmp;
            m[2][0] = (tmp = m[2][0]) * c + m[0][0] * s;
            m[0][0] = m[0][0] * c - tmp * s;
            m[2][1] = (tmp = m[2][1]) * c + m[0][1] * s;
            m[0][1] = m[0][1] * c - tmp * s;
            m[2][2] = (tmp = m[2][2]) * c + m[0][2] * s;
            m[0][2] = m[0][2] * c - tmp * s;
            m[2][3] = (tmp = m[2][3]) * c + m[0][3] * s;
            m[0][3] = m[0][3] * c - tmp * s;

            flagBits |= Rotation;
            return;
        }
    } else if (y == 0.0 && z == 0.0) {
        // Rotate around the X axis.
        if (x < 0)
            s = -s;
        double tmp;
        m[1][0] = (tmp = m[1][0]) * c + m[2][0] * s;
        m[2][0] = m[2][0] * c - tmp * s;
        m[1][1] = (tmp = m[1][1]) * c + m[2][1] * s;
        m[2][1] = m[2][1] * c - tmp * s;
        m[1][2] = (tmp = m[1][2]) * c + m[2][2] * s;
        m[2][2] = m[2][2] * c - tmp * s;
        m[1][3] = (tmp = m[1][3]) * c + m[2][3] * s;
        m[2][3] = m[2][3] * c - tmp * s;

        flagBits |= Rotation;
        return;
    }

    double len = double(x) * double(x) +
                 double(y) * double(y) +
                 double(z) * double(z);
    if (!qFuzzyCompare(len, 1.0) && !qFuzzyIsNull(len)) {
        len = std::sqrt(len);
        x = double(double(x) / len);
        y = double(double(y) / len);
        z = double(double(z) / len);
    }
    double ic = 1.0 - c;
    QDoubleMatrix4x4 rot(1); // The "1" says to not load the identity.
    rot.m[0][0] = x * x * ic + c;
    rot.m[1][0] = x * y * ic - z * s;
    rot.m[2][0] = x * z * ic + y * s;
    rot.m[3][0] = 0.0;
    rot.m[0][1] = y * x * ic + z * s;
    rot.m[1][1] = y * y * ic + c;
    rot.m[2][1] = y * z * ic - x * s;
    rot.m[3][1] = 0.0;
    rot.m[0][2] = x * z * ic - y * s;
    rot.m[1][2] = y * z * ic + x * s;
    rot.m[2][2] = z * z * ic + c;
    rot.m[3][2] = 0.0;
    rot.m[0][3] = 0.0;
    rot.m[1][3] = 0.0;
    rot.m[2][3] = 0.0;
    rot.m[3][3] = 1.0;
    rot.flagBits = Rotation;
    *this *= rot;
}

void QDoubleMatrix4x4::projectedRotate(double angle, double x, double y, double z)
{
    // Used by QGraphicsRotation::applyTo() to perform a rotation
    // and projection back to 2D in a single step.
    if (angle == 0.0)
        return;
    double c, s;
    if (angle == 90.0 || angle == -270.0) {
        s = 1.0;
        c = 0.0;
    } else if (angle == -90.0 || angle == 270.0) {
        s = -1.0;
        c = 0.0;
    } else if (angle == 180.0 || angle == -180.0) {
        s = 0.0;
        c = -1.0;
    } else {
        double a = qDegreesToRadians(angle);
        c = std::cos(a);
        s = std::sin(a);
    }
    if (x == 0.0) {
        if (y == 0.0) {
            if (z != 0.0) {
                // Rotate around the Z axis.
                if (z < 0)
                    s = -s;
                double tmp;
                m[0][0] = (tmp = m[0][0]) * c + m[1][0] * s;
                m[1][0] = m[1][0] * c - tmp * s;
                m[0][1] = (tmp = m[0][1]) * c + m[1][1] * s;
                m[1][1] = m[1][1] * c - tmp * s;
                m[0][2] = (tmp = m[0][2]) * c + m[1][2] * s;
                m[1][2] = m[1][2] * c - tmp * s;
                m[0][3] = (tmp = m[0][3]) * c + m[1][3] * s;
                m[1][3] = m[1][3] * c - tmp * s;

                flagBits |= Rotation2D;
                return;
            }
        } else if (z == 0.0) {
            // Rotate around the Y axis.
            if (y < 0)
                s = -s;
            m[0][0] = m[0][0] * c + m[3][0] * s * inv_dist_to_plane;
            m[0][1] = m[0][1] * c + m[3][1] * s * inv_dist_to_plane;
            m[0][2] = m[0][2] * c + m[3][2] * s * inv_dist_to_plane;
            m[0][3] = m[0][3] * c + m[3][3] * s * inv_dist_to_plane;
            flagBits = General;
            return;
        }
    } else if (y == 0.0 && z == 0.0) {
        // Rotate around the X axis.
        if (x < 0)
            s = -s;
        m[1][0] = m[1][0] * c - m[3][0] * s * inv_dist_to_plane;
        m[1][1] = m[1][1] * c - m[3][1] * s * inv_dist_to_plane;
        m[1][2] = m[1][2] * c - m[3][2] * s * inv_dist_to_plane;
        m[1][3] = m[1][3] * c - m[3][3] * s * inv_dist_to_plane;
        flagBits = General;
        return;
    }
    double len = double(x) * double(x) +
                 double(y) * double(y) +
                 double(z) * double(z);
    if (!qFuzzyCompare(len, 1.0) && !qFuzzyIsNull(len)) {
        len = std::sqrt(len);
        x = double(double(x) / len);
        y = double(double(y) / len);
        z = double(double(z) / len);
    }
    double ic = 1.0 - c;
    QDoubleMatrix4x4 rot(1); // The "1" says to not load the identity.
    rot.m[0][0] = x * x * ic + c;
    rot.m[1][0] = x * y * ic - z * s;
    rot.m[2][0] = 0.0;
    rot.m[3][0] = 0.0;
    rot.m[0][1] = y * x * ic + z * s;
    rot.m[1][1] = y * y * ic + c;
    rot.m[2][1] = 0.0;
    rot.m[3][1] = 0.0;
    rot.m[0][2] = 0.0;
    rot.m[1][2] = 0.0;
    rot.m[2][2] = 1.0;
    rot.m[3][2] = 0.0;
    rot.m[0][3] = (x * z * ic - y * s) * -inv_dist_to_plane;
    rot.m[1][3] = (y * z * ic + x * s) * -inv_dist_to_plane;
    rot.m[2][3] = 0.0;
    rot.m[3][3] = 1.0;
    rot.flagBits = General;
    *this *= rot;
}

void QDoubleMatrix4x4::ortho(const QRect& rect)
{
    // Note: rect.right() and rect.bottom() subtract 1 in QRect,
    // which gives the location of a pixel within the rectangle,
    // instead of the extent of the rectangle.  We want the extent.
    // QRectF expresses the extent properly.
    ortho(rect.x(), rect.x() + rect.width(), rect.y() + rect.height(), rect.y(), -1.0, 1.0);
}

void QDoubleMatrix4x4::ortho(const QRectF& rect)
{
    ortho(rect.left(), rect.right(), rect.bottom(), rect.top(), -1.0, 1.0);
}

void QDoubleMatrix4x4::ortho(double left, double right, double bottom, double top, double nearPlane, double farPlane)
{
    // Bail out if the projection volume is zero-sized.
    if (left == right || bottom == top || nearPlane == farPlane)
        return;

    // Construct the projection.
    double width = right - left;
    double invheight = top - bottom;
    double clip = farPlane - nearPlane;
    QDoubleMatrix4x4 m(1);
    m.m[0][0] = 2.0 / width;
    m.m[1][0] = 0.0;
    m.m[2][0] = 0.0;
    m.m[3][0] = -(left + right) / width;
    m.m[0][1] = 0.0;
    m.m[1][1] = 2.0 / invheight;
    m.m[2][1] = 0.0;
    m.m[3][1] = -(top + bottom) / invheight;
    m.m[0][2] = 0.0;
    m.m[1][2] = 0.0;
    m.m[2][2] = -2.0 / clip;
    m.m[3][2] = -(nearPlane + farPlane) / clip;
    m.m[0][3] = 0.0;
    m.m[1][3] = 0.0;
    m.m[2][3] = 0.0;
    m.m[3][3] = 1.0;
    m.flagBits = Translation | Scale;

    // Apply the projection.
    *this *= m;
}

void QDoubleMatrix4x4::frustum(double left, double right, double bottom, double top, double nearPlane, double farPlane)
{
    // Bail out if the projection volume is zero-sized.
    if (left == right || bottom == top || nearPlane == farPlane)
        return;

    // Construct the projection.
    QDoubleMatrix4x4 m(1);
    double width = right - left;
    double invheight = top - bottom;
    double clip = farPlane - nearPlane;
    m.m[0][0] = 2.0 * nearPlane / width;
    m.m[1][0] = 0.0;
    m.m[2][0] = (left + right) / width;
    m.m[3][0] = 0.0;
    m.m[0][1] = 0.0;
    m.m[1][1] = 2.0 * nearPlane / invheight;
    m.m[2][1] = (top + bottom) / invheight;
    m.m[3][1] = 0.0;
    m.m[0][2] = 0.0;
    m.m[1][2] = 0.0;
    m.m[2][2] = -(nearPlane + farPlane) / clip;
    m.m[3][2] = -2.0 * nearPlane * farPlane / clip;
    m.m[0][3] = 0.0;
    m.m[1][3] = 0.0;
    m.m[2][3] = -1.0;
    m.m[3][3] = 0.0;
    m.flagBits = General;

    // Apply the projection.
    *this *= m;
}

void QDoubleMatrix4x4::perspective(double verticalAngle, double aspectRatio, double nearPlane, double farPlane)
{
    // Bail out if the projection volume is zero-sized.
    if (nearPlane == farPlane || aspectRatio == 0.0)
        return;

    // Construct the projection.
    QDoubleMatrix4x4 m(1);
    double radians = qDegreesToRadians(verticalAngle / 2.0);
    double sine = std::sin(radians);
    if (sine == 0.0)
        return;
    double cotan = std::cos(radians) / sine;
    double clip = farPlane - nearPlane;
    m.m[0][0] = cotan / aspectRatio;
    m.m[1][0] = 0.0;
    m.m[2][0] = 0.0;
    m.m[3][0] = 0.0;
    m.m[0][1] = 0.0;
    m.m[1][1] = cotan;
    m.m[2][1] = 0.0;
    m.m[3][1] = 0.0;
    m.m[0][2] = 0.0;
    m.m[1][2] = 0.0;
    m.m[2][2] = -(nearPlane + farPlane) / clip;
    m.m[3][2] = -(2.0 * nearPlane * farPlane) / clip;
    m.m[0][3] = 0.0;
    m.m[1][3] = 0.0;
    m.m[2][3] = -1.0;
    m.m[3][3] = 0.0;
    m.flagBits = General;

    // Apply the projection.
    *this *= m;
}

void QDoubleMatrix4x4::lookAt(const QDoubleVector3D& eye, const QDoubleVector3D& center, const QDoubleVector3D& up)
{
    QDoubleVector3D forward = center - eye;
    if (qFuzzyIsNull(forward.x()) && qFuzzyIsNull(forward.y()) && qFuzzyIsNull(forward.z()))
        return;

    forward.normalize();
    QDoubleVector3D side = QDoubleVector3D::crossProduct(forward, up).normalized();
    QDoubleVector3D upVector = QDoubleVector3D::crossProduct(side, forward);

    QDoubleMatrix4x4 m(1);
    m.m[0][0] = side.x();
    m.m[1][0] = side.y();
    m.m[2][0] = side.z();
    m.m[3][0] = 0.0;
    m.m[0][1] = upVector.x();
    m.m[1][1] = upVector.y();
    m.m[2][1] = upVector.z();
    m.m[3][1] = 0.0;
    m.m[0][2] = -forward.x();
    m.m[1][2] = -forward.y();
    m.m[2][2] = -forward.z();
    m.m[3][2] = 0.0;
    m.m[0][3] = 0.0;
    m.m[1][3] = 0.0;
    m.m[2][3] = 0.0;
    m.m[3][3] = 1.0;
    m.flagBits = Rotation;

    *this *= m;
    translate(-eye);
}

void QDoubleMatrix4x4::viewport(double left, double bottom, double width, double height, double nearPlane, double farPlane)
{
    const double w2 = width / 2.0;
    const double h2 = height / 2.0;

    QDoubleMatrix4x4 m(1);
    m.m[0][0] = w2;
    m.m[1][0] = 0.0;
    m.m[2][0] = 0.0;
    m.m[3][0] = left + w2;
    m.m[0][1] = 0.0;
    m.m[1][1] = h2;
    m.m[2][1] = 0.0;
    m.m[3][1] = bottom + h2;
    m.m[0][2] = 0.0;
    m.m[1][2] = 0.0;
    m.m[2][2] = (farPlane - nearPlane) / 2.0;
    m.m[3][2] = (nearPlane + farPlane) / 2.0;
    m.m[0][3] = 0.0;
    m.m[1][3] = 0.0;
    m.m[2][3] = 0.0;
    m.m[3][3] = 1.0;
    m.flagBits = General;

    *this *= m;
}

void QDoubleMatrix4x4::flipCoordinates()
{
    // Multiplying the y and z coordinates with -1 does NOT flip between right-handed and
    // left-handed coordinate systems, it just rotates 180 degrees around the x axis, so
    // I'm deprecating this function.
    if (flagBits < Rotation2D) {
        // Translation | Scale
        m[1][1] = -m[1][1];
        m[2][2] = -m[2][2];
    } else {
        m[1][0] = -m[1][0];
        m[1][1] = -m[1][1];
        m[1][2] = -m[1][2];
        m[1][3] = -m[1][3];
        m[2][0] = -m[2][0];
        m[2][1] = -m[2][1];
        m[2][2] = -m[2][2];
        m[2][3] = -m[2][3];
    }
    flagBits |= Scale;
}

void QDoubleMatrix4x4::copyDataTo(double *values) const
{
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            values[row * 4 + col] = double(m[col][row]);
}

QRect QDoubleMatrix4x4::mapRect(const QRect& rect) const
{
    if (flagBits < Scale) {
        // Translation
        return QRect(qRound(rect.x() + m[3][0]),
                     qRound(rect.y() + m[3][1]),
                     rect.width(), rect.height());
    } else if (flagBits < Rotation2D) {
        // Translation | Scale
        double x = rect.x() * m[0][0] + m[3][0];
        double y = rect.y() * m[1][1] + m[3][1];
        double w = rect.width() * m[0][0];
        double h = rect.height() * m[1][1];
        if (w < 0) {
            w = -w;
            x -= w;
        }
        if (h < 0) {
            h = -h;
            y -= h;
        }
        return QRect(qRound(x), qRound(y), qRound(w), qRound(h));
    }

    QPoint tl = map(rect.topLeft());
    QPoint tr = map(QPoint(rect.x() + rect.width(), rect.y()));
    QPoint bl = map(QPoint(rect.x(), rect.y() + rect.height()));
    QPoint br = map(QPoint(rect.x() + rect.width(),
                           rect.y() + rect.height()));

    int xmin = qMin(qMin(tl.x(), tr.x()), qMin(bl.x(), br.x()));
    int xmax = qMax(qMax(tl.x(), tr.x()), qMax(bl.x(), br.x()));
    int ymin = qMin(qMin(tl.y(), tr.y()), qMin(bl.y(), br.y()));
    int ymax = qMax(qMax(tl.y(), tr.y()), qMax(bl.y(), br.y()));

    return QRect(xmin, ymin, xmax - xmin, ymax - ymin);
}

QRectF QDoubleMatrix4x4::mapRect(const QRectF& rect) const
{
    if (flagBits < Scale) {
        // Translation
        return rect.translated(m[3][0], m[3][1]);
    } else if (flagBits < Rotation2D) {
        // Translation | Scale
        double x = rect.x() * m[0][0] + m[3][0];
        double y = rect.y() * m[1][1] + m[3][1];
        double w = rect.width() * m[0][0];
        double h = rect.height() * m[1][1];
        if (w < 0) {
            w = -w;
            x -= w;
        }
        if (h < 0) {
            h = -h;
            y -= h;
        }
        return QRectF(x, y, w, h);
    }

    QPointF tl = map(rect.topLeft()); QPointF tr = map(rect.topRight());
    QPointF bl = map(rect.bottomLeft()); QPointF br = map(rect.bottomRight());

    double xmin = qMin(qMin(tl.x(), tr.x()), qMin(bl.x(), br.x()));
    double xmax = qMax(qMax(tl.x(), tr.x()), qMax(bl.x(), br.x()));
    double ymin = qMin(qMin(tl.y(), tr.y()), qMin(bl.y(), br.y()));
    double ymax = qMax(qMax(tl.y(), tr.y()), qMax(bl.y(), br.y()));

    return QRectF(QPointF(xmin, ymin), QPointF(xmax, ymax));
}

QDoubleMatrix4x4 QDoubleMatrix4x4::orthonormalInverse() const
{
    QDoubleMatrix4x4 result(1);  // The '1' says not to load identity

    result.m[0][0] = m[0][0];
    result.m[1][0] = m[0][1];
    result.m[2][0] = m[0][2];

    result.m[0][1] = m[1][0];
    result.m[1][1] = m[1][1];
    result.m[2][1] = m[1][2];

    result.m[0][2] = m[2][0];
    result.m[1][2] = m[2][1];
    result.m[2][2] = m[2][2];

    result.m[0][3] = 0.0;
    result.m[1][3] = 0.0;
    result.m[2][3] = 0.0;

    result.m[3][0] = -(result.m[0][0] * m[3][0] + result.m[1][0] * m[3][1] + result.m[2][0] * m[3][2]);
    result.m[3][1] = -(result.m[0][1] * m[3][0] + result.m[1][1] * m[3][1] + result.m[2][1] * m[3][2]);
    result.m[3][2] = -(result.m[0][2] * m[3][0] + result.m[1][2] * m[3][1] + result.m[2][2] * m[3][2]);
    result.m[3][3] = 1.0;

    result.flagBits = flagBits;

    return result;
}

void QDoubleMatrix4x4::optimize()
{
    // If the last row is not (0, 0, 0, 1), the matrix is not a special type.
    flagBits = General;
    if (m[0][3] != 0 || m[1][3] != 0 || m[2][3] != 0 || m[3][3] != 1)
        return;

    flagBits &= ~Perspective;

    // If the last column is (0, 0, 0, 1), then there is no translation.
    if (m[3][0] == 0 && m[3][1] == 0 && m[3][2] == 0)
        flagBits &= ~Translation;

    // If the two first elements of row 3 and column 3 are 0, then any rotation must be about Z.
    if (!m[0][2] && !m[1][2] && !m[2][0] && !m[2][1]) {
        flagBits &= ~Rotation;
        // If the six non-diagonal elements in the top left 3x3 matrix are 0, there is no rotation.
        if (!m[0][1] && !m[1][0]) {
            flagBits &= ~Rotation2D;
            // Check for identity.
            if (m[0][0] == 1 && m[1][1] == 1 && m[2][2] == 1)
                flagBits &= ~Scale;
        } else {
            // If the columns are orthonormal and form a right-handed system, then there is no scale.
            double det = matrixDet2(m, 0, 1, 0, 1);
            double lenX = m[0][0] * m[0][0] + m[0][1] * m[0][1];
            double lenY = m[1][0] * m[1][0] + m[1][1] * m[1][1];
            double lenZ = m[2][2];
            if (qFuzzyCompare(det, 1.0) && qFuzzyCompare(lenX, 1.0)
                    && qFuzzyCompare(lenY, 1.0) && qFuzzyCompare(lenZ, 1.0))
            {
                flagBits &= ~Scale;
            }
        }
    } else {
        // If the columns are orthonormal and form a right-handed system, then there is no scale.
        double det = matrixDet3(m, 0, 1, 2, 0, 1, 2);
        double lenX = m[0][0] * m[0][0] + m[0][1] * m[0][1] + m[0][2] * m[0][2];
        double lenY = m[1][0] * m[1][0] + m[1][1] * m[1][1] + m[1][2] * m[1][2];
        double lenZ = m[2][0] * m[2][0] + m[2][1] * m[2][1] + m[2][2] * m[2][2];
        if (qFuzzyCompare(det, 1.0) && qFuzzyCompare(lenX, 1.0)
                && qFuzzyCompare(lenY, 1.0) && qFuzzyCompare(lenZ, 1.0))
        {
            flagBits &= ~Scale;
        }
    }
}

#ifndef QT_NO_DEBUG_STREAM

QDebug operator<<(QDebug dbg, const QDoubleMatrix4x4 &m)
{
    QDebugStateSaver saver(dbg);
    // Create a string that represents the matrix type.
    QByteArray bits;
    if (m.flagBits == QDoubleMatrix4x4::Identity) {
        bits = "Identity";
    } else if (m.flagBits == QDoubleMatrix4x4::General) {
        bits = "General";
    } else {
        if ((m.flagBits & QDoubleMatrix4x4::Translation) != 0)
            bits += "Translation,";
        if ((m.flagBits & QDoubleMatrix4x4::Scale) != 0)
            bits += "Scale,";
        if ((m.flagBits & QDoubleMatrix4x4::Rotation2D) != 0)
            bits += "Rotation2D,";
        if ((m.flagBits & QDoubleMatrix4x4::Rotation) != 0)
            bits += "Rotation,";
        if ((m.flagBits & QDoubleMatrix4x4::Perspective) != 0)
            bits += "Perspective,";
        if (bits.size() > 0)
            bits = bits.left(bits.size() - 1);
    }

    // Output in row-major order because it is more human-readable.
    dbg.nospace() << "QDoubleMatrix4x4(type:" << bits.constData() << endl
        << qSetFieldWidth(10)
        << m(0, 0) << m(0, 1) << m(0, 2) << m(0, 3) << endl
        << m(1, 0) << m(1, 1) << m(1, 2) << m(1, 3) << endl
        << m(2, 0) << m(2, 1) << m(2, 2) << m(2, 3) << endl
        << m(3, 0) << m(3, 1) << m(3, 2) << m(3, 3) << endl
        << qSetFieldWidth(0) << ')';
    return dbg;
}

#endif

#ifndef QT_NO_DATASTREAM

QDataStream &operator<<(QDataStream &stream, const QDoubleMatrix4x4 &matrix)
{
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            stream << matrix(row, col);
    return stream;
}

QDataStream &operator>>(QDataStream &stream, QDoubleMatrix4x4 &matrix)
{
    double x;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            stream >> x;
            matrix(row, col) = x;
        }
    }
    matrix.optimize();
    return stream;
}

#endif // QT_NO_DATASTREAM

QT_END_NAMESPACE
