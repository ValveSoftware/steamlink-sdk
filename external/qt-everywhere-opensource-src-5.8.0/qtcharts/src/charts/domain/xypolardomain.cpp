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

#include <private/xypolardomain_p.h>
#include <private/qabstractaxis_p.h>
#include <QtCore/QtMath>

QT_CHARTS_BEGIN_NAMESPACE

XYPolarDomain::XYPolarDomain(QObject *parent)
    : PolarDomain(parent)
{
}

XYPolarDomain::~XYPolarDomain()
{
}

void XYPolarDomain::setRange(qreal minX, qreal maxX, qreal minY, qreal maxY)
{
    bool axisXChanged = false;
    bool axisYChanged = false;

    if (!qFuzzyCompare(m_minX, minX) || !qFuzzyCompare(m_maxX, maxX)) {
        m_minX = minX;
        m_maxX = maxX;
        axisXChanged = true;
        if (!m_signalsBlocked)
            emit rangeHorizontalChanged(m_minX, m_maxX);
    }

    if (!qFuzzyCompare(m_minY, minY) || !qFuzzyCompare(m_maxY, maxY)) {
        m_minY = minY;
        m_maxY = maxY;
        axisYChanged = true;
        if (!m_signalsBlocked)
            emit rangeVerticalChanged(m_minY, m_maxY);
    }

    if (axisXChanged || axisYChanged)
        emit updated();
}


void XYPolarDomain::zoomIn(const QRectF &rect)
{
    storeZoomReset();
    qreal dx = spanX() / m_size.width();
    qreal dy = spanY() / m_size.height();

    qreal maxX = m_maxX;
    qreal minX = m_minX;
    qreal minY = m_minY;
    qreal maxY = m_maxY;

    maxX = minX + dx * rect.right();
    minX = minX + dx * rect.left();
    minY = maxY - dy * rect.bottom();
    maxY = maxY - dy * rect.top();

    setRange(minX, maxX, minY, maxY);
}

void XYPolarDomain::zoomOut(const QRectF &rect)
{
    storeZoomReset();
    qreal dx = spanX() / rect.width();
    qreal dy = spanY() / rect.height();

    qreal maxX = m_maxX;
    qreal minX = m_minX;
    qreal minY = m_minY;
    qreal maxY = m_maxY;

    minX = maxX - dx * rect.right();
    maxX = minX + dx * m_size.width();
    maxY = minY + dy * rect.bottom();
    minY = maxY - dy * m_size.height();

    setRange(minX, maxX, minY, maxY);
}

void XYPolarDomain::move(qreal dx, qreal dy)
{
    // One unit scrolls one degree angular and one pixel radial
    qreal x = spanX() / 360.0;
    qreal y = spanY() / m_radius;

    qreal maxX = m_maxX;
    qreal minX = m_minX;
    qreal minY = m_minY;
    qreal maxY = m_maxY;

    if (dx != 0) {
        minX = minX + x * dx;
        maxX = maxX + x * dx;
    }
    if (dy != 0) {
        minY = minY + y * dy;
        maxY = maxY + y * dy;
    }
    setRange(minX, maxX, minY, maxY);
}

QPointF XYPolarDomain::calculateDomainPoint(const QPointF &point) const
{
    if (point == m_center)
        return QPointF(0.0, m_minX);

    QLineF line(m_center, point);
    qreal a = 90.0 - line.angle();
    if (a < 0.0)
        a += 360.0;
    a = ((a / 360.0) * (m_maxX - m_minX)) + m_minX;
    qreal r = m_minY + ((m_maxY - m_minY) * (line.length() / m_radius));
    return QPointF(a, r);
}

qreal XYPolarDomain::toAngularCoordinate(qreal value, bool &ok) const
{
    ok = true;
    qreal f = (value - m_minX) / (m_maxX - m_minX);
    return f * 360.0;
}

qreal XYPolarDomain::toRadialCoordinate(qreal value, bool &ok) const
{
    ok = true;
    if (value < m_minY)
        value = m_minY;

    // Dont limit the max. The drawing should clip the stuff that goes out of the grid
    qreal f = (value - m_minY) / (m_maxY - m_minY);

    return f * m_radius;
}

// operators

bool Q_AUTOTEST_EXPORT operator== (const XYPolarDomain &domain1, const XYPolarDomain &domain2)
{
    return (qFuzzyCompare(domain1.m_maxX, domain2.m_maxX)
            && qFuzzyCompare(domain1.m_maxY, domain2.m_maxY)
            && qFuzzyCompare(domain1.m_minX, domain2.m_minX)
            && qFuzzyCompare(domain1.m_minY, domain2.m_minY));
}


bool Q_AUTOTEST_EXPORT operator!= (const XYPolarDomain &domain1, const XYPolarDomain &domain2)
{
    return !(domain1 == domain2);
}


QDebug Q_AUTOTEST_EXPORT operator<<(QDebug dbg, const XYPolarDomain &domain)
{
#ifdef QT_NO_TEXTSTREAM
    Q_UNUSED(domain)
#else
    dbg.nospace() << "AbstractDomain(" << domain.m_minX << ',' << domain.m_maxX << ',' << domain.m_minY << ',' << domain.m_maxY << ')' << domain.m_size;
#endif
    return dbg.maybeSpace();
}

#include "moc_xypolardomain_p.cpp"

QT_CHARTS_END_NAMESPACE
