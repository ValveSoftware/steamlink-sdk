/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include "qclipperutils_p.h"

QT_BEGIN_NAMESPACE

static const double kClipperScaleFactor = 281474976710656.0;  // 48 bits of precision
static const double kClipperScaleFactorInv = 1.0 / kClipperScaleFactor;

double QClipperUtils::clipperScaleFactor()
{
    return kClipperScaleFactor;
}

QDoubleVector2D QClipperUtils::toVector2D(const IntPoint &p)
{
    return QDoubleVector2D(double(p.X) * kClipperScaleFactorInv, double(p.Y) * kClipperScaleFactorInv);
}

IntPoint QClipperUtils::toIntPoint(const QDoubleVector2D &p)
{
    return IntPoint(cInt(p.x() * kClipperScaleFactor), cInt(p.y() * kClipperScaleFactor));
}

QList<QDoubleVector2D> QClipperUtils::pathToQList(const Path &path)
{
    QList<QDoubleVector2D> res;
    res.reserve(int(path.size()));
    for (const IntPoint &ip: path)
        res.append(toVector2D(ip));
    return res;
}

QList<QList<QDoubleVector2D> > QClipperUtils::pathsToQList(const Paths &paths)
{
    QList<QList<QDoubleVector2D> > res;
    res.reserve(int(paths.size()));
    for (const Path &p: paths) {
        res.append(pathToQList(p));
    }
    return res;
}

Path QClipperUtils::qListToPath(const QList<QDoubleVector2D> &list)
{
    Path res;
    res.reserve(list.size());
    for (const QDoubleVector2D &p: list)
        res.push_back(toIntPoint(p));
    return res;
}

Paths QClipperUtils::qListToPaths(const QList<QList<QDoubleVector2D> > &lists)
{
    Paths res;
    res.reserve(lists.size());
    for (const QList<QDoubleVector2D> &l: lists) {
        res.push_back(qListToPath(l));
    }
    return res;
}

QT_END_NAMESPACE
