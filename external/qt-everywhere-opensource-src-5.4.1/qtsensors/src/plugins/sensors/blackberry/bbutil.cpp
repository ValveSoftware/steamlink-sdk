/****************************************************************************
**
** Copyright (C) 2012 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
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
#include "bbutil.h"

#include <QtCore/qmath.h>

namespace BbUtil {

static float getMatrixElement(const float matrix[3*3], int index0, int index1)
{
    return matrix[index0 * 3 + index1];
}

void matrixToEulerZXY(const float matrix[3*3],
                      float &thetaX, float &thetaY, float& thetaZ)
{
    thetaX = asin( getMatrixElement(matrix, 2, 1));
    if ( thetaX < M_PI_2 ) {
        if ( thetaX > -M_PI_2 ) {
            thetaZ = atan2( -getMatrixElement(matrix, 0, 1),
                             getMatrixElement(matrix, 1, 1) );
            thetaY = atan2( -getMatrixElement(matrix, 2, 0),
                             getMatrixElement(matrix, 2, 2) );
        } else {
            // Not a unique solution
            thetaZ = -atan2( getMatrixElement(matrix, 0, 2),
                             getMatrixElement(matrix, 0, 0) );
            thetaY = 0.0;
        }
    } else {
        // Not a unique solution
        thetaZ = atan2( getMatrixElement(matrix, 0, 2),
                        getMatrixElement(matrix, 0, 0) );
        thetaY = 0.0;
    }
}

qreal radiansToDegrees(qreal radians)
{
    static const qreal radToDeg = 180.0f / M_PI;
    return radians * radToDeg;
}

}
