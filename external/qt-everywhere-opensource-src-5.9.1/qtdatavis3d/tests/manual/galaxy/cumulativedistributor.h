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

#ifndef CUMULATIVEDISTRIBUTOR_H
#define CUMULATIVEDISTRIBUTOR_H

#include <QtCore/qglobal.h>
#include <QtGui/QVector2D>
#include <QtCore/QVector>

class CumulativeDistributor
{
public:
    typedef qreal (CumulativeDistributor::*dist_fun_t)(qreal x);

    CumulativeDistributor();
    virtual ~CumulativeDistributor();

    qreal PropFromVal(qreal fVal);
    qreal ValFromProp(qreal fVal);

    void setupRealistic(qreal I0, qreal k, qreal a, qreal RBulge, qreal min,
                        qreal max, int nSteps);
    qreal valFromProp(qreal fVal);

private:
    dist_fun_t m_pDistFun;
    qreal m_fMin;
    qreal m_fMax;
    qreal m_fWidth;
    int m_nSteps;

    // parameters for realistic star distribution
    qreal m_I0;
    qreal m_k;
    qreal m_a;
    qreal m_RBulge;

    QVector<qreal> m_vM1;
    QVector<qreal> m_vY1;
    QVector<qreal> m_vX1;

    QVector<qreal> m_vM2;
    QVector<qreal> m_vY2;
    QVector<qreal> m_vX2;

    void buildCDF(int nSteps);

    qreal IntensityBulge(qreal R, qreal I0, qreal k);
    qreal IntensityDisc(qreal R, qreal I0, qreal a);
    qreal Intensity(qreal x);
};

#endif // CUMULATIVEDISTRIBUTOR_H
