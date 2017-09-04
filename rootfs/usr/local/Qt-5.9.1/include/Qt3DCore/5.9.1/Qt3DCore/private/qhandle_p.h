/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#ifndef QT3DCORE_QHANDLE_P_H
#define QT3DCORE_QHANDLE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <Qt3DCore/qt3dcore_global.h>
#include <QtCore/QDebug>

class tst_Handle;  // needed for friend class declaration below

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

template <typename T, uint INDEXBITS>
class QHandleManager;

template <typename T, uint INDEXBITS = 16>
class QHandle
{
public:
    QHandle()
        : m_handle(0)
    {}


    quint32 index() const { return d.m_index; }
    quint32 counter() const { return d.m_counter; }
    quint32 handle() const { return m_handle; }
    bool isNull() const { return !m_handle; }

    operator quint32() const { return m_handle; }

    static quint32 maxIndex() { return MaxIndex; }
    static quint32 maxCounter() { return MaxCounter; }


private:
    enum {
        // Sizes to use for bit fields
        IndexBits = INDEXBITS,
        CounterBits = 32 - INDEXBITS - 2, // We use 2 bits for book-keeping in QHandleManager

        // Sizes to compare against for asserting dereferences
        MaxIndex = (1 << IndexBits) - 1,
        MaxCounter = (1 << CounterBits) - 1
    };

    QHandle(quint32 i, quint32 count)
    {
        d.m_index = i;
        d.m_counter = count;
        d.m_unused = 0;
        Q_ASSERT(i < MaxIndex);
        Q_ASSERT(count < MaxCounter);
    }


    friend class QHandleManager<T, INDEXBITS>;
    friend class ::tst_Handle;

    struct Data {
        quint32 m_index : IndexBits;
        quint32 m_counter : CounterBits;
        quint32 m_unused : 2;
    };
    union {
        Data d;
        quint32 m_handle;
    };
};

template <typename T, uint INDEXBITS>
QDebug operator<<(QDebug dbg, const QHandle<T, INDEXBITS> &h)
{
    QDebugStateSaver saver(dbg);
    QString binNumber = QString::number(h.handle(), 2).rightJustified(32, QChar::fromLatin1('0'));
    dbg.nospace() << "index = " << h.index()
                  << " magic/counter = " << h.counter()
                  << " m_handle = " << h.handle()
                  << " = " << binNumber;
    return dbg;
}

} // Qt3DCore

template <typename T, uint I>
class QTypeInfo<Qt3DCore::QHandle<T,I> > // simpler than fighting the Q_DECLARE_TYPEINFO macro
    : public QTypeInfoMerger<Qt3DCore::QHandle<T,I>, quint32> {};

QT_END_NAMESPACE

#endif // QT3DCORE_QRHANDLE_H
