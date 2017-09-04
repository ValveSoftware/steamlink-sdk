/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "functionrangefinder_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {
namespace Animation {

/*!
    \internal
    \class FunctionRangeFinder finds the lower bound index of a range that encloses a function value

    Given a vector of function values (typically abscissa values of some other function), this
    class can find the lower bound index of a range that encloses the requested value. This is
    very useful for finding the two points that sandwich a value to which you later wish to
    interpolate for example.
 */

/*!
    \internal
    \fn findLowerBound

    Finds the lower bound index of a range that encloses the requested value.

    We use a technique which tries to be better than a simple bisection. Often when
    performing interpolations, subsequent points are correlated with earlier calls.
    This is especially true with time based lookups. If two calls are determined to
    be correlated, then the next subsequent call will use the hunt function to search
    close to the last returned value first. The hunt algorithms searches outward in
    increasing step sizes until a sandwiching range is found. Traditional bisection
    is then used to refine this result.

    If the previous results are uncorrelated, a simple bisection is used.
 */

FunctionRangeFinder::FunctionRangeFinder(const QVector<float> &x)
    : m_x(x)
    , m_previousLowerBound(0)
    , m_correlated(0)
    , m_rangeSize(2)
    , m_correlationThreshold(1)
    , m_ascending(true)
{
    updateAutomaticCorrelationThreshold();
    if (!m_x.isEmpty())
        m_ascending = (m_x.last() >= m_x.first());
}

/*!
    \internal
    Locates the lower bound of a range that encloses \a x by a bisection method.
*/
int FunctionRangeFinder::locate(float x) const
{
    if (m_x.size() < 2 || m_rangeSize < 2 || m_rangeSize > m_x.size())
        return -1;

    int jLower = 0;
    int jUpper = m_x.size() - 1;
    while (jUpper - jLower > 1) {
        int jMid = (jUpper + jLower) >> 1;
        if ((x >= m_x[jMid]) == m_ascending)
            jLower = jMid;
        else
            jUpper = jMid;
    }

    m_correlated = std::abs(jLower - m_previousLowerBound) <= m_correlationThreshold;
    m_previousLowerBound = jLower;

    return std::max(0, std::min(m_x.size() - m_rangeSize, jLower - ((m_rangeSize - 2) >> 1)));
}

/*!
    \internal
    Hunts outward from the previous result in increasing step sizes then refines via bisection.
 */
int FunctionRangeFinder::hunt(float x) const
{
    if (m_x.size() < 2 || m_rangeSize < 2 || m_rangeSize > m_x.size())
        return -1;

    int jLower = m_previousLowerBound;
    int jMid;
    int jUpper;
    if (jLower < 0 || jLower > (m_x.size() - 1)) {
        jLower = 0;
        jUpper = m_x.size() - 1;
    } else {
        int increment = 1;
        if ((x >= m_x[jLower]) == m_ascending) {
            for (;;) {
                jUpper = jLower + increment;
                if (jUpper >= m_x.size() - 1) {
                    jUpper = m_x.size() - 1;
                    break;
                } else if ((x < m_x[jUpper]) == m_ascending) {
                    break;
                } else {
                    jLower = jUpper;
                    increment += increment;
                }
            }
        } else {
            jUpper = jLower;
            for (;;) {
                jLower = jLower - increment;
                if (jLower <= 0) {
                    jLower = 0;
                    break;
                } else if ((x >= m_x[jLower]) == m_ascending) {
                    break;
                } else {
                    jUpper = jLower;
                    increment += increment;
                }
            }
        }
    }

    while (jUpper - jLower > 1) {
        jMid = (jUpper + jLower) >> 1;
        if ((x >= m_x[jMid]) == m_ascending)
            jLower = jMid;
        else
            jUpper = jMid;
    }

    m_correlated = std::abs(jLower - m_previousLowerBound) <= m_correlationThreshold;
    m_previousLowerBound = jLower;

    return std::max(0, std::min(m_x.size() - m_rangeSize, jLower - ((m_rangeSize - 2) >> 1)));
}

} // namespace Animation
} // namespace Qt3DAnimation

QT_END_NAMESPACE
