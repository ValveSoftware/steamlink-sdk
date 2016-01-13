/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QV4MEMBERDATA_H
#define QV4MEMBERDATA_H

#include "qv4global_p.h"
#include "qv4managed_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct MemberData : Managed
{
    struct Data : Managed::Data {
        union {
            uint size;
            double _dummy;
        };
        Value data[1];
    };
    V4_MANAGED(Managed)

    MemberData(QV4::InternalClass *ic) : Managed(ic) {}
    Value &operator[] (uint idx) { return d()->data[idx]; }

    static void markObjects(Managed *that, ExecutionEngine *e);
};

struct Members : Value
{
    void reset() { m = 0; }
    void ensureIndex(QV4::ExecutionEngine *e, uint idx);
    Value &operator[] (uint idx) const { return static_cast<MemberData *>(managed())->d()->data[idx]; }
    inline uint size() const { return d() ? d()->d()->size : 0; }
    inline MemberData *d() const { return static_cast<MemberData *>(managed()); }
    Value *data() const { return static_cast<MemberData *>(managed())->d()->data; }

    void mark(ExecutionEngine *e) const {
        MemberData *m = d();
        if (m)
            m->mark(e);
    }
};

}

QT_END_NAMESPACE

#endif
