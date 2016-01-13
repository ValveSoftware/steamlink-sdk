/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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
#include "qquicklineextruder_p.h"
#include <stdlib.h>
#include <cmath>

/*!
    \qmltype LineShape
    \instantiates QQuickLineExtruder
    \inqmlmodule QtQuick.Particles
    \inherits Shape
    \brief Represents a line for affectors and emitters
    \ingroup qtquick-particles

*/

/*!
    \qmlproperty bool QtQuick.Particles::LineShape::mirrored

    By default, the line goes from (0,0) to (width, height) of the item that
    this shape is being applied to.

    If mirrored is set to true, this will be mirrored along the y axis.
    The line will then go from (0,height) to (width, 0).
*/

QQuickLineExtruder::QQuickLineExtruder(QObject *parent) :
    QQuickParticleExtruder(parent), m_mirrored(false)
{
}

QPointF QQuickLineExtruder::extrude(const QRectF &r)
{
    qreal x,y;
    if (!r.height()){
        x = r.width() * ((qreal)rand())/RAND_MAX;
        y = 0;
    }else{
        y = r.height() * ((qreal)rand())/RAND_MAX;
        if (!r.width()){
            x = 0;
        }else{
            x = r.width()/r.height() * y;
            if (m_mirrored)
                x = r.width() - x;
        }
    }
    return QPointF(x,y);
}
