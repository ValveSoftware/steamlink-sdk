/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
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

#include "star.h"

#include <QtCore/qmath.h>

static const double DEG_TO_RAD = M_PI / 180.0;

Star::Star()
    : m_theta(0),
      m_a(0),
      m_b(0),
      m_center(QVector2D(0.0f, 0.0f))
{
}

const void Star::calcXY()
{
    qreal &a = m_a;
    qreal &b = m_b;
    qreal &theta = m_theta;
    const QVector2D &p = m_center;

    qreal beta  = -m_angle;
    qreal alpha = theta * DEG_TO_RAD;

    // temporaries to save cpu time
    qreal cosalpha = qCos(alpha);
    qreal sinalpha = qSin(alpha);
    qreal cosbeta  = qCos(beta);
    qreal sinbeta  = qSin(beta);

    m_pos = QVector2D(p.x() + (a * cosalpha * cosbeta - b * sinalpha * sinbeta),
                      p.y() + (a * cosalpha * sinbeta + b * sinalpha * cosbeta));
}
