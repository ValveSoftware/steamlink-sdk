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

/*
 * Galaxy creation code obtained from http://beltoforion.de/galaxy/galaxy_en.html
 * Thanks to Ingo Berg, great work.
 * Licensed under a  Creative Commons Attribution 3.0 License
 * http://creativecommons.org/licenses/by/3.0/
 */

#include "cumulativedistributor.h"

#include <QDebug>

CumulativeDistributor::CumulativeDistributor()
    : m_pDistFun(NULL),
      m_vM1(),
      m_vY1(),
      m_vX1(),
      m_vM2(),
      m_vY2(),
      m_vX2()
{
}

CumulativeDistributor::~CumulativeDistributor()
{
}

void CumulativeDistributor::setupRealistic(qreal I0, qreal k, qreal a,
                                           qreal RBulge, qreal min, qreal max,
                                           int nSteps)
{
    m_fMin = min;
    m_fMax = max;
    m_nSteps = nSteps;

    m_I0 = I0;
    m_k  = k;
    m_a  = a;
    m_RBulge = RBulge;

    m_pDistFun = &CumulativeDistributor::Intensity;

    // build the distribution function
    buildCDF(m_nSteps);
}

void CumulativeDistributor::buildCDF(int nSteps)
{
    qreal h = (m_fMax - m_fMin) / nSteps;
    qreal x = 0, y = 0;

    m_vX1.clear();
    m_vY1.clear();
    m_vX2.clear();
    m_vM1.clear();
    m_vM1.clear();

    // Simpson rule for integration of the distribution function
    m_vY1.push_back(0.0);
    m_vX1.push_back(0.0);
    for (int i = 0; i < nSteps; i += 2) {
        x = (i + 2) * h;
        y += h/3 * ((this->*m_pDistFun)(m_fMin + i*h) + 4*(this->*m_pDistFun)(m_fMin + (i+1)*h) + (this->*m_pDistFun)(m_fMin + (i+2)*h) );

        m_vM1.push_back((y - m_vY1.back()) / (2*h));
        m_vX1.push_back(x);
        m_vY1.push_back(y);
    }

    // normieren
    for (int i = 0; i < m_vM1.size(); i++) {
        m_vY1[i] /= m_vY1.back();
        m_vM1[i] /= m_vY1.back();
    }

    m_vY2.clear();
    m_vM2.clear();
    m_vY2.push_back(0.0);

    qreal p = 0;
    h = 1.0 / nSteps;
    for (int i = 1, k = 0; i < nSteps; ++i) {
        p = (qreal)i * h;

        for (; m_vY1[k+1] <= p; ++k) {
        }

        y = m_vX1[k] + (p - m_vY1[k]) / m_vM1[k];

        m_vM2.push_back( (y - m_vY2.back()) / h);
        m_vX2.push_back(p);
        m_vY2.push_back(y);
    }
}

qreal CumulativeDistributor::valFromProp(qreal fVal)
{
  if (fVal < 0.0 || fVal > 1.0)
    throw std::runtime_error("out of range");

  qreal h = 1.0 / m_vY2.size();

  int i = int(fVal / h);
  qreal remainder = fVal - i*h;

  int min = qMin(m_vY2.size(), m_vM2.size());
  if (i >= min)
      i = min - 1;

  return (m_vY2[i] + m_vM2[i] * remainder);
}

qreal CumulativeDistributor::IntensityBulge(qreal R, qreal I0, qreal k)
{
    return I0 * exp(-k * pow(R, 0.25));
}

qreal CumulativeDistributor::IntensityDisc(qreal R, qreal I0, qreal a)
{
  return I0 * exp(-R/a);
}

qreal CumulativeDistributor::Intensity(qreal x)
{
    return (x < m_RBulge) ? IntensityBulge(x, m_I0, m_k) : IntensityDisc(x - m_RBulge, IntensityBulge(m_RBulge, m_I0, m_k), m_a);
}
