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

#include "qv4memberdata_p.h"
#include <private/qv4mm_p.h>
#include "qv4value_p.h"

using namespace QV4;

DEFINE_MANAGED_VTABLE(MemberData);

void MemberData::markObjects(Heap::Base *that, ExecutionEngine *e)
{
    Heap::MemberData *m = static_cast<Heap::MemberData *>(that);
    for (uint i = 0; i < m->size; ++i)
        m->data[i].mark(e);
}

static Heap::MemberData *reallocateHelper(ExecutionEngine *e, Heap::MemberData *old, uint n)
{
    uint alloc = sizeof(Heap::MemberData) + (n)*sizeof(Value);
    Scope scope(e);
    Scoped<MemberData> newMemberData(scope, e->memoryManager->allocManaged<MemberData>(alloc));
    if (old)
        memcpy(newMemberData->d_unchecked(), old, sizeof(Heap::MemberData) + old->size * sizeof(Value));
    else
        newMemberData->d_unchecked()->init();
    newMemberData->d()->size = n;
    return newMemberData->d();
}

Heap::MemberData *MemberData::allocate(ExecutionEngine *e, uint n)
{
    return reallocateHelper(e, 0, n);
}

Heap::MemberData *MemberData::reallocate(ExecutionEngine *e, Heap::MemberData *old, uint n)
{
    uint s = old ? old->size : 0;
    if (n < s)
        return old;

    // n is multiplied by two to leave room for growth
    return reallocateHelper(e, old, qMax((uint)4, 2*n));
}
