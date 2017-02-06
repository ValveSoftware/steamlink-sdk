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

#include "qquickpointdirection_p.h"
#include <stdlib.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype PointDirection
    \instantiates QQuickPointDirection
    \inqmlmodule QtQuick.Particles
    \ingroup qtquick-particles
    \inherits Direction
    \brief For specifying a direction that varies in x and y components

    The PointDirection element allows both the specification of a direction by x and y components,
    as well as varying the parameters by x or y component.
*/
/*!
    \qmlproperty real QtQuick.Particles::PointDirection::x
*/
/*!
    \qmlproperty real QtQuick.Particles::PointDirection::y
*/
/*!
    \qmlproperty real QtQuick.Particles::PointDirection::xVariation
*/
/*!
    \qmlproperty real QtQuick.Particles::PointDirection::yVariation
*/

QQuickPointDirection::QQuickPointDirection(QObject *parent) :
    QQuickDirection(parent)
  , m_x(0)
  , m_y(0)
  , m_xVariation(0)
  , m_yVariation(0)
{
}

const QPointF QQuickPointDirection::sample(const QPointF &)
{
    QPointF ret;
    ret.setX(m_x - m_xVariation + rand() / float(RAND_MAX) * m_xVariation * 2);
    ret.setY(m_y - m_yVariation + rand() / float(RAND_MAX) * m_yVariation * 2);
    return ret;
}

QT_END_NAMESPACE
