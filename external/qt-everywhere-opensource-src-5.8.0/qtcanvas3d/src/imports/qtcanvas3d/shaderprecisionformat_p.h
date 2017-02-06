/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtCanvas3D API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#ifndef SHADERPRECISIONFORMAT_P_H
#define SHADERPRECISIONFORMAT_P_H

#include "abstractobject3d_p.h"

#include <QObject>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

class CanvasShaderPrecisionFormat : public CanvasAbstractObject
{
    Q_OBJECT
    Q_PROPERTY(int rangeMin READ rangeMin NOTIFY rangeMinChanged)
    Q_PROPERTY(int rangeMax READ rangeMax NOTIFY rangeMaxChanged)
    Q_PROPERTY(int precision READ precision NOTIFY precisionChanged)

public:
    explicit CanvasShaderPrecisionFormat(QObject *parent = 0);

    inline void setRangeMin(int min) { m_rangeMin = min; }
    inline void setRangeMax(int max) { m_rangeMax = max; }
    inline void setPrecision(int precision) { m_precision = precision; }

    inline int rangeMin() { return m_rangeMin; }
    inline int rangeMax() { return m_rangeMax; }
    inline int precision() { return m_precision; }

signals:
    void rangeMinChanged(int rangeMin);
    void rangeMaxChanged(int rangeMax);
    void precisionChanged(int precision);

private:
    int m_rangeMin;
    int m_rangeMax;
    int m_precision;
};

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE

#endif // SHADERPRECISIONFORMAT_P_H
