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

Q_DECLARE_METATYPE(QVector<QRectF>)
Q_DECLARE_METATYPE(QT_CHARTS_NAMESPACE::CandlestickData)
Q_DECLARE_METATYPE(qreal)

QT_CHARTS_BEGIN_NAMESPACE

CandlestickBodyWicksAnimation::CandlestickBodyWicksAnimation(Candlestick *candlestick,
                                                             CandlestickAnimation *animation,
                                                             int duration, QEasingCurve &curve)
    : ChartAnimation(candlestick),
      m_candlestick(candlestick),
      m_candlestickAnimation(animation),
      m_changeAnimation(false)
{
    setDuration(duration);
    setEasingCurve(curve);
}

CandlestickBodyWicksAnimation::~CandlestickBodyWicksAnimation()
{
    if (m_candlestickAnimation)
        m_candlestickAnimation->removeCandlestickAnimation(m_candlestick);
}

void CandlestickBodyWicksAnimation::setup(const CandlestickData &startData,
                                          const CandlestickData &endData)
{
    setKeyValueAt(0.0, qVariantFromValue(startData));
    setKeyValueAt(1.0, qVariantFromValue(endData));
}

void CandlestickBodyWicksAnimation::setStartData(const CandlestickData &startData)
{
    if (state() != QAbstractAnimation::Stopped)
        stop();

    setStartValue(qVariantFromValue(startData));
}

void CandlestickBodyWicksAnimation::setEndData(const CandlestickData &endData)
{
    if (state() != QAbstractAnimation::Stopped)
        stop();

    setEndValue(qVariantFromValue(endData));
}

void CandlestickBodyWicksAnimation::updateCurrentValue(const QVariant &value)
{
    CandlestickData data = qvariant_cast<CandlestickData>(value);
    m_candlestick->setLayout(data);
}

QVariant CandlestickBodyWicksAnimation::interpolated(const QVariant &from, const QVariant &to,
                                                     qreal progress) const
{
    CandlestickData startData = qvariant_cast<CandlestickData>(from);
    CandlestickData endData = qvariant_cast<CandlestickData>(to);
    CandlestickData result = endData;

    if (m_changeAnimation) {
        result.m_open = startData.m_open + progress * (endData.m_open - startData.m_open);
        result.m_high = startData.m_high + progress * (endData.m_high - startData.m_high);
        result.m_low = startData.m_low + progress * (endData.m_low - startData.m_low);
        result.m_close = startData.m_close + progress * (endData.m_close - startData.m_close);
    } else {
        const qreal median = (endData.m_open + endData.m_close) / 2;
        result.m_low = median + progress * (endData.m_low - median);
        result.m_close = median + progress * (endData.m_close - median);
        result.m_open = median + progress * (endData.m_open - median);
        result.m_high = median + progress * (endData.m_high - median);
    }

    return qVariantFromValue(result);
}

#include "moc_candlestickbodywicksanimation_p.cpp"

QT_CHARTS_END_NAMESPACE
