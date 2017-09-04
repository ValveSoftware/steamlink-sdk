/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

#include <QtGlobal>

#include "qcommonsequencetypes_p.h"

#include "qitemtype_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

ItemType::~ItemType()
{
}

const ItemType &ItemType::operator|(const ItemType &other) const
{
    const ItemType *ca = this;

    if(other == *CommonSequenceTypes::None)
        return *ca;

    if(*ca == *CommonSequenceTypes::Empty)
        return other;
    else if(other == *CommonSequenceTypes::Empty)
        return *ca;

    do
    {
        const ItemType *cb = &other;
        do
        {
            if(*ca == *cb)
                return *ca;

            cb = cb->xdtSuperType().data();
        }
        while(cb);

        ca = ca->xdtSuperType().data();
    }
    while(ca);

    Q_ASSERT_X(false, Q_FUNC_INFO, "We should never reach this line.");
    return *this;
}

ItemType::Category ItemType::itemTypeCategory() const
{
    return Other;
}

bool ItemType::operator==(const ItemType &other) const
{
    return this == &other;
}

ItemType::InstanceOf ItemType::instanceOf() const
{
    return ClassOther;
}

QT_END_NAMESPACE
