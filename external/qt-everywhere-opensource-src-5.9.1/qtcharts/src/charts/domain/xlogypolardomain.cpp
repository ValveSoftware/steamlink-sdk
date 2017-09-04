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

#include <private/xlogypolardomain_p.h>
#include <private/qabstractaxis_p.h>
#include <QtCharts/QLogValueAxis>
#include <QtCore/QtMath>
#include <cmath>

QT_CHARTS_BEGIN_NAMESPACE

XLogYPolarDomain::XLogYPolarDomain(QObject *parent)
    : PolarDomain(parent),
      m_logInnerY(0),
      m_logOuterY(1),
      m_logBaseY(10)
{
}

XLogYPolarDomain::~XLogYPolarDomain()
{
}

void XLogYPolarDomain::setRange(qreal minX, qreal maxX, qreal minY, qreal maxY)
{
    bool axisXChanged = false;
    bool axisYChanged = false;

    adjustLogDomainRanges(minY, maxY);

    if (!qFuzzyIsNull(m_minX - minX) || !qFuzzyIsNull(m_maxX - maxX)) {
        m_minX = minX;
        m_maxX = maxX;
        axisXChanged = true;
        if (!m_signalsBlocked)
            emit rangeHorizontalChanged(m_minX, m_maxX);
    }

    if (!qFuzzyIsNull(m_minY - minY) || !qFuzzyIsNull(m_maxY - maxY)) {
        m_minY = minY;
        m_maxY = maxY;
        axisYChanged = true;
        qreal logMinY = std::log10(m_minY) / std::log10(m_logBaseY);
        qreal logMaxY = std::log10(m_maxY) / std::log10(m_logBaseY);
        m_logInnerY = logMinY < logMaxY ? logMinY : logMaxY;
        m_logOuterY = logMinY > logMaxY ? logMinY : logMaxY;
        if (!m_signalsBlocked)
            emit rangeVerticalChanged(m_minY, m_maxY);
    }

    if (axisXChanged || axisYChanged)
        emit updated();
}

void XLogYPolarDomain::zoomIn(const QRectF &rect)
{
    storeZoomReset();
    qreal dx = spanX() / m_size.width();
    qreal maxX = m_maxX;
    qreal minX = m_minX;

    maxX = minX + dx * rect.right();
    minX = minX + dx * rect.left();

    qreal logLeftY = m_logOuterY - rect.bottom() * (m_logOuterY - m_logInnerY) / m_size.height();
    qreal logRightY = m_logOuterY - rect.top() * (m_logOuterY - m_logInnerY) / m_size.height();
    qreal leftY = qPow(m_logBaseY, logLeftY);
    qreal rightY = qPow(m_logBaseY, logRightY);
    qreal minY = leftY < rightY ? leftY : rightY;
    qreal maxY = leftY > rightY ? leftY : rightY;

    setRange(minX, maxX, minY, maxY);
}

void XLogYPolarDomain::zoomOut(const QRectF &rect)
{
    storeZoomReset();
    qreal dx = spanX() / rect.width();
    qreal maxX = m_maxX;
    qreal minX = m_minX;

    minX = maxX - dx * rect.right();
    maxX = minX + dx * m_size.width();

    const qreal factorY = m_size.height() / rect.height();
    qreal newLogMinY = m_logInnerY + (m_logOuterY - m_logInnerY) / 2.0 * (1.0 - factorY);
    qreal newLogMaxY = m_logInnerY + (m_logOuterY - m_logInnerY) / 2.0 * (1.0 + factorY);
    qreal leftY = qPow(m_logBaseY, newLogMinY);
    qreal rightY = qPow(m_logBaseY, newLogMaxY);
    qreal minY = leftY < rightY ? leftY : rightY;
    qreal maxY = leftY > rightY ? leftY : rightY;

    setRange(minX, maxX, minY, maxY);
}

void XLogYPolarDomain::move(qreal dx, qreal dy)
{
    qreal x = spanX() / 360.0;

    qreal maxX = m_maxX;
    qreal minX = m_minX;

    if (dx != 0) {
        minX = minX + x * dx;
        maxX = maxX + x * dx;
    }

    qreal stepY = dy * (m_logOuterY - m_logInnerY) / m_radius;
    qreal leftY = qPow(m_logBaseY, m_logInnerY + stepY);
    qreal rightY = qPow(m_logBaseY, m_logOuterY + stepY);
    qreal minY = leftY < rightY ? leftY : rightY;
    qreal maxY = leftY > rightY ? leftY : rightY;

    setRange(minX, maxX, minY, maxY);
}

qreal XLogYPolarDomain::toAngularCoordinate(qreal value, bool &ok) const
{
    ok = true;
    qreal f = (value - m_minX) / (m_maxX - m_minX);
    return f * 360.0;
}

qreal XLogYPolarDomain::toRadialCoordinate(qreal value, bool &ok) const
{
    qreal retVal;
    if (value <= 0) {
        ok = false;
        retVal = 0.0;
    } else {
        ok =  true;
        const qreal tickSpan = m_radius / qAbs(m_logOuterY - m_logInnerY);
        const qreal logValue = std::log10(value) / std::log10(m_logBaseY);
        const qreal valueDelta = logValue - m_logInnerY;

        retVal = valueDelta * tickSpan;

        if (retVal < 0.0)
            retVal = 0.0;
    }
    return retVal;
}

QPointF XLogYPolarDomain::calculateDomainPoint(const QPointF &point) const
{
    if (point == m_center)
        return QPointF(0.0, m_minY);

    QLineF line(m_center, point);
    qreal a = 90.0 - line.angle();
    if (a < 0.0)
        a += 360.0;
    a = ((a / 360.0) * (m_maxX - m_minX)) + m_minX;

    const qreal deltaY = m_radius / qAbs(m_logOuterY - m_logInnerY);
    qreal r = qPow(m_logBaseY, m_logInnerY + (line.length() / deltaY));

    return QPointF(a, r);
}

bool XLogYPolarDomain::attachAxis(QAbstractAxis *axis)
{
    QLogValueAxis *logAxis = qobject_cast<QLogValueAxis *>(axis);

    if (logAxis && logAxis->orientation() == Qt::Vertical) {
        QObject::connect(logAxis, SIGNAL(baseChanged(qreal)), this, SLOT(handleVerticalAxisBaseChanged(qreal)));
        handleVerticalAxisBaseChanged(logAxis->base());
    }
    return  AbstractDomain::attachAxis(axis);
}

bool XLogYPolarDomain::detachAxis(QAbstractAxis *axis)
{
    QLogValueAxis *logAxis = qobject_cast<QLogValueAxis *>(axis);

    if (logAxis && logAxis->orientation() == Qt::Vertical)
        QObject::disconnect(logAxis, SIGNAL(baseChanged(qreal)), this, SLOT(handleVerticalAxisBaseChanged(qreal)));

    return AbstractDomain::detachAxis(axis);
}

void XLogYPolarDomain::handleVerticalAxisBaseChanged(qreal baseY)
{
    m_logBaseY = baseY;
    qreal logMinY = std::log10(m_minY) / std::log10(m_logBaseY);
    qreal logMaxY = std::log10(m_maxY) / std::log10(m_logBaseY);
    m_logInnerY = logMinY < logMaxY ? logMinY : logMaxY;
    m_logOuterY = logMinY > logMaxY ? logMinY : logMaxY;
    emit updated();
}

// operators

bool Q_AUTOTEST_EXPORT operator== (const XLogYPolarDomain &domain1, const XLogYPolarDomain &domain2)
{
    return (qFuzzyIsNull(domain1.m_maxX - domain2.m_maxX)
            && qFuzzyIsNull(domain1.m_maxY - domain2.m_maxY)
            && qFuzzyIsNull(domain1.m_minX - domain2.m_minX)
            && qFuzzyIsNull(domain1.m_minY - domain2.m_minY));
}


bool Q_AUTOTEST_EXPORT operator!= (const XLogYPolarDomain &domain1, const XLogYPolarDomain &domain2)
{
    return !(domain1 == domain2);
}


QDebug Q_AUTOTEST_EXPORT operator<<(QDebug dbg, const XLogYPolarDomain &domain)
{
#ifdef QT_NO_TEXTSTREAM
    Q_UNUSED(domain)
#else
    dbg.nospace() << "AbstractDomain(" << domain.m_minX << ',' << domain.m_maxX << ',' << domain.m_minY << ',' << domain.m_maxY << ')' << domain.m_size;
#endif
    return dbg.maybeSpace();
}

#include "moc_xlogypolardomain_p.cpp"

QT_CHARTS_END_NAMESPACE
