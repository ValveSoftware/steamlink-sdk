/****************************************************************************
**
** Copyright (C) 1993-2009 NVIDIA Corporation.
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Quick 3D.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSSGRENDEREULERANGLES_H
#define QSSGRENDEREULERANGLES_H

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

#include <QtQuick3DUtils/private/qtquick3dutilsglobal_p.h>
#include <qmath.h>
#include <QVector3D>
#include <QMatrix3x3>
#include <QMatrix4x4>

QT_BEGIN_NAMESPACE
//==============================================================================
//	Description
//==============================================================================
// QuatTypes.h - Basic type declarations
// by Ken Shoemake, shoemake@graphics.cis.upenn.edu
// in "Graphics Gems IV", Academic Press, 1994

#define EulFrmS 0
#define EulFrmR 1
#define EulFrm(ord) ((unsigned)(ord)&1)
#define EulRepNo 0
#define EulRepYes 1
#define EulRep(ord) (((unsigned)(ord) >> 1) & 1)
#define EulParEven 0
#define EulParOdd 1
#define EulPar(ord) (((unsigned)(ord) >> 2) & 1)
#define EulSafe "\000\001\002\000"
#define EulNext "\001\002\000\001"
#define EulAxI(ord) ((int)(EulSafe[(((unsigned)(ord) >> 3) & 3)]))
#define EulAxJ(ord) ((int)(EulNext[EulAxI(ord) + (EulPar(ord) == EulParOdd)]))
#define EulAxK(ord) ((int)(EulNext[EulAxI(ord) + (EulPar(ord) != EulParOdd)]))
#define EulAxH(ord) ((EulRep(ord) == EulRepNo) ? EulAxK(ord) : EulAxI(ord))

// getEulerOrder creates an order value between 0 and 23 from 4-tuple choices.
constexpr uint getEulerOrder(uint i, uint p, uint r, uint f)
{
    return (((((((i) << 1) + (p)) << 1) + (r)) << 1) + (f));
}

enum EulerOrder {
    // Static axes
    // X = 0, Y = 1, Z = 2 ref QuatPart
    XYZs = getEulerOrder(0, EulParEven, EulRepNo, EulFrmS),
    XYXs = getEulerOrder(0, EulParEven, EulRepYes, EulFrmS),
    XZYs = getEulerOrder(0, EulParOdd, EulRepNo, EulFrmS),
    XZXs = getEulerOrder(0, EulParOdd, EulRepYes, EulFrmS),
    YZXs = getEulerOrder(1, EulParEven, EulRepNo, EulFrmS),
    YZYs = getEulerOrder(1, EulParEven, EulRepYes, EulFrmS),
    YXZs = getEulerOrder(1, EulParOdd, EulRepNo, EulFrmS),
    YXYs = getEulerOrder(1, EulParOdd, EulRepYes, EulFrmS),
    ZXYs = getEulerOrder(2, EulParEven, EulRepNo, EulFrmS),
    ZXZs = getEulerOrder(2, EulParEven, EulRepYes, EulFrmS),
    ZYXs = getEulerOrder(2, EulParOdd, EulRepNo, EulFrmS),
    ZYZs = getEulerOrder(2, EulParOdd, EulRepYes, EulFrmS),

    // Rotating axes
    ZYXr = getEulerOrder(0, EulParEven, EulRepNo, EulFrmR),
    XYXr = getEulerOrder(0, EulParEven, EulRepYes, EulFrmR),
    YZXr = getEulerOrder(0, EulParOdd, EulRepNo, EulFrmR),
    XZXr = getEulerOrder(0, EulParOdd, EulRepYes, EulFrmR),
    XZYr = getEulerOrder(1, EulParEven, EulRepNo, EulFrmR),
    YZYr = getEulerOrder(1, EulParEven, EulRepYes, EulFrmR),
    ZXYr = getEulerOrder(1, EulParOdd, EulRepNo, EulFrmR),
    YXYr = getEulerOrder(1, EulParOdd, EulRepYes, EulFrmR),
    YXZr = getEulerOrder(2, EulParEven, EulRepNo, EulFrmR),
    ZXZr = getEulerOrder(2, EulParEven, EulRepYes, EulFrmR),
    XYZr = getEulerOrder(2, EulParOdd, EulRepNo, EulFrmR),
    ZYZr = getEulerOrder(2, EulParOdd, EulRepYes, EulFrmR),
};

typedef struct
{
    float x, y, z;
    EulerOrder order;
} EulerAngles; /* (x,y,z)=angle 1,2,3, order=order code  */

class Q_QUICK3DUTILS_EXPORT QSSGEulerAngleConverter
{
private:
    QSSGEulerAngleConverter();
    ~QSSGEulerAngleConverter();

    typedef struct
    {
        float x, y, z, w;
    } Quat; /* Quaternion */
    typedef float HMatrix[4][4]; /* Right-handed, for column vectors */

    static EulerAngles euler(float ai, float aj, float ah, EulerOrder order);
    static Quat eulerToQuat(EulerAngles ea);
    static void eulerToHMatrix(EulerAngles ea, HMatrix M);
    static EulerAngles eulerFromHMatrix(HMatrix M, EulerOrder order);
    static EulerAngles eulerFromQuat(Quat q, EulerOrder order);

public:
    static EulerAngles calculateEulerAngles(const QVector3D &rotation, EulerOrder order);
    static QVector3D calculateRotationVector(const EulerAngles &angles);
    static QVector3D calculateRotationVector(const QMatrix3x3 &rotationMatrix,
                                             bool matrixIsLeftHanded,
                                             EulerOrder order);
    static QMatrix4x4 createRotationMatrix(const QVector3D &rotationAsRadians, EulerOrder order);
};
QT_END_NAMESPACE

#endif // QSSGRENDEREULERANGLES_H
