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

#include <private/candlestick_p.h>
#include <private/candlestickanimation_p.h>
#include <private/candlestickbodywicksanimation_p.h>
#include <private/candlestickchartitem_p.h>
#include <private/candlestickdata_p.h>

QT_CHARTS_BEGIN_NAMESPACE

CandlestickAnimation::CandlestickAnimation(CandlestickChartItem *item, int duration,
                                           QEasingCurve &curve)
    : QObject(item),
      m_item(item),
      m_animationDuration(duration),
      m_animationCurve(curve)
{
}

CandlestickAnimation::~CandlestickAnimation()
{
}

void CandlestickAnimation::addCandlestick(Candlestick *candlestick)
{
    CandlestickBodyWicksAnimation *animation = m_animations.value(candlestick, 0);
    if (!animation) {
        animation = new CandlestickBodyWicksAnimation(candlestick, this, m_animationDuration,
                                                      m_animationCurve);
        m_animations.insert(candlestick, animation);

        qreal median = (candlestick->m_data.m_open + candlestick->m_data.m_close) / 2;
        CandlestickData start;
        start.m_open = median;
        start.m_high = median;
        start.m_low = median;
        start.m_close = median;
        animation->setup(start, candlestick->m_data);
    } else {
        animation->stop();
        animation->setEndData(candlestick->m_data);
    }
}

ChartAnimation *CandlestickAnimation::candlestickAnimation(Candlestick *candlestick)
{
    CandlestickBodyWicksAnimation *animation = m_animations.value(candlestick, 0);
    if (animation)
        animation->m_changeAnimation = false;

    return animation;
}

ChartAnimation *CandlestickAnimation::candlestickChangeAnimation(Candlestick *candlestick)
{
    CandlestickBodyWicksAnimation *animation = m_animations.value(candlestick, 0);
    if (animation) {
        animation->m_changeAnimation = true;
        animation->setEndData(candlestick->m_data);
    }

    return animation;
}

void CandlestickAnimation::setAnimationStart(Candlestick *candlestick)
{
    CandlestickBodyWicksAnimation *animation = m_animations.value(candlestick, 0);
    if (animation)
        animation->setStartData(candlestick->m_data);
}

void CandlestickAnimation::stopAll()
{
    foreach (Candlestick *candlestick, m_animations.keys()) {
        CandlestickBodyWicksAnimation *animation = m_animations.value(candlestick, 0);
        if (animation)
            animation->stopAndDestroyLater();
        m_animations.remove(candlestick);
    }
}

void CandlestickAnimation::removeCandlestickAnimation(Candlestick *candlestick)
{
    m_animations.remove(candlestick);
}

#include "moc_candlestickanimation_p.cpp"

QT_CHARTS_END_NAMESPACE
