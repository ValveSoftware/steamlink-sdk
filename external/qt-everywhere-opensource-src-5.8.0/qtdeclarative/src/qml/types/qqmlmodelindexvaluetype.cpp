/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlmodelindexvaluetype_p.h"

QT_BEGIN_NAMESPACE

/*!
    \internal
*/
QString QQmlModelIndexValueType::propertiesString(const QModelIndex &idx)
{
    if (!idx.isValid())
        return QLatin1String("()");
    return QString(QLatin1String("(%1,%2,0x%3,%4(0x%5))"))
            .arg(idx.row()).arg(idx.column()).arg(idx.internalId(), 0, 16)
            .arg(QLatin1String(idx.model()->metaObject()->className())).arg(quintptr(idx.model()), 0, 16);
}

/*!
    \internal
*/
QString QQmlItemSelectionRangeValueType::toString() const
{
    return QString(QLatin1String("QItemSelectionRange(%1,%2)"))
            .arg(reinterpret_cast<const QQmlPersistentModelIndexValueType *>(&v.topLeft())->toString())
            .arg(reinterpret_cast<const QQmlPersistentModelIndexValueType *>(&v.bottomRight())->toString());
}

QT_END_NAMESPACE
