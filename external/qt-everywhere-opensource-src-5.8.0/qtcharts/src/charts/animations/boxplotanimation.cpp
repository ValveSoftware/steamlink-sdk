/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
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

#include <private/boxplotanimation_p.h>
#include <private/boxplotchartitem_p.h>
#include <private/boxwhiskersdata_p.h>
#include <private/boxwhiskersanimation_p.h>

QT_CHARTS_BEGIN_NAMESPACE

BoxPlotAnimation::BoxPlotAnimation(BoxPlotChartItem *item, int duration, QEasingCurve &curve)
    : QObject(item),
    m_item(item),
    m_animationDuration(duration),
    m_animationCurve(curve)
{
}

BoxPlotAnimation::~BoxPlotAnimation()
{
}

void BoxPlotAnimation::addBox(BoxWhiskers *box)
{
    BoxWhiskersAnimation *animation = m_animations.value(box);
    if (!animation) {
        animation = new BoxWhiskersAnimation(box, this, m_animationDuration, m_animationCurve);
        m_animations.insert(box, animation);
        BoxWhiskersData start;
        start.m_lowerExtreme = box->m_data.m_median;
        start.m_lowerQuartile = box->m_data.m_median;
        start.m_median = box->m_data.m_median;
        start.m_upperQuartile = box->m_data.m_median;
        start.m_upperExtreme = box->m_data.m_median;
        animation->setup(start, box->m_data);

    } else {
        animation->stop();
        animation->setEndData(box->m_data);
    }
}

ChartAnimation *BoxPlotAnimation::boxAnimation(BoxWhiskers *box)
{
    BoxWhiskersAnimation *animation = m_animations.value(box);
    if (animation)
        animation->m_changeAnimation = false;

    return animation;
}

ChartAnimation *BoxPlotAnimation::boxChangeAnimation(BoxWhiskers *box)
{
    BoxWhiskersAnimation *animation = m_animations.value(box);
    animation->m_changeAnimation = true;
    animation->setEndData(box->m_data);

    return animation;
}

void BoxPlotAnimation::setAnimationStart(BoxWhiskers *box)
{
    BoxWhiskersAnimation *animation = m_animations.value(box);
    animation->setStartData(box->m_data);
}

void BoxPlotAnimation::stopAll()
{
    foreach (BoxWhiskers *box, m_animations.keys()) {
        BoxWhiskersAnimation *animation = m_animations.value(box);
        animation->stopAndDestroyLater();
        m_animations.remove(box);
    }
}

void BoxPlotAnimation::removeBoxAnimation(BoxWhiskers *box)
{
    m_animations.remove(box);
}

#include "moc_boxplotanimation_p.cpp"

QT_CHARTS_END_NAMESPACE
