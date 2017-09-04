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

#include "colorgradient_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

ColorGradientStop::ColorGradientStop(QObject *parent)
    : QObject(parent)
{
}

qreal ColorGradientStop::position() const
{
    return m_position;
}

void ColorGradientStop::setPosition(qreal position)
{
    m_position = position;
    updateGradient();
    emit positionChanged(position);
}

QColor ColorGradientStop::color() const
{
    return m_color;
}

void ColorGradientStop::setColor(const QColor &color)
{
    m_color = color;
    updateGradient();
    emit colorChanged(color);
}

void ColorGradientStop::updateGradient()
{
    if (ColorGradient *grad = qobject_cast<ColorGradient*>(parent()))
        grad->doUpdate();
}

ColorGradient::ColorGradient(QObject *parent)
: QObject(parent)
{
}

ColorGradient::~ColorGradient()
{
}

QQmlListProperty<ColorGradientStop> ColorGradient::stops()
{
    return QQmlListProperty<ColorGradientStop>(this, m_stops);
}

void ColorGradient::doUpdate()
{
    emit updated();
}

QT_END_NAMESPACE_DATAVISUALIZATION
