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
#ifndef MASM_EXECUTABLEALLOCATOR_H
#define MASM_EXECUTABLEALLOCATOR_H

// Defined via mkspec
#if _MSC_VER >= 1900
#include <windows.h>
#endif

#include <RefPtr.h>
#include <RefCounted.h>
#include <wtf/PageBlock.h>

#include <private/qv4executableallocator_p.h>

#if OS(WINDOWS)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifdef __QNXNTO__
using std::perror;
#endif

namespace JSC {

class JSGlobalData;

struct ExecutableMemoryHandle : public RefCounted<ExecutableMemoryHandle> {
    ExecutableMemoryHandle(QV4::ExecutableAllocator *allocator, int size)
        : m_allocator(allocator)
        , m_size(size)
    {
        m_allocation = allocator->allocate(size);
    }
    ~ExecutableMemoryHandle()
    {
        m_allocation->deallocate(m_allocator);
    }

    inline void shrink(size_t) {
        // ### TODO.
    }

    inline bool isManaged() const { return true; }

    void* start() { return m_allocation->start(); }
    int sizeInBytes() { return m_size; }

    QV4::ExecutableAllocator::ChunkOfPages *chunk() const
    { return m_allocator->chunkForAllocation(m_allocation); }

    QV4::ExecutableAllocator *m_allocator;
    QV4::ExecutableAllocator::Allocation *m_allocation;
    int m_size;
};

struct ExecutableAllocator {
    ExecutableAllocator(QV4::ExecutableAllocator *alloc)
        : realAllocator(alloc)
    {}

    PassRefPtr<ExecutableMemoryHandle> allocate(JSGlobalData&, int size, void*, int)
    {
        return adoptRef(new ExecutableMemoryHandle(realAllocator, size));
    }

    static void makeWritable(void* addr, size_t size)
    {
        quintptr pageSize = WTF::pageSize();
        quintptr iaddr = reinterpret_cast<quintptr>(addr);
        quintptr roundAddr = iaddr & ~(pageSize - 1);
        size = size + (iaddr - roundAddr);
        addr = reinterpret_cast<void*>(roundAddr);

#if ENABLE(ASSEMBLER_WX_EXCLUSIVE)
#  if OS(WINDOWS)
        DWORD oldProtect;
#    if !OS(WINRT)
        VirtualProtect(addr, size, PAGE_READWRITE, &oldProtect);
#    elif _MSC_VER >= 1900
        bool hr = VirtualProtectFromApp(addr, size, PAGE_READWRITE, &oldProtect);
        if (!hr) {
            Q_UNREACHABLE();
        }
#    else
        (void)oldProtect;
#    endif
#  else
        int mode = PROT_READ | PROT_WRITE;
        if (mprotect(addr, size, mode) != 0) {
            perror("mprotect failed in ExecutableAllocator::makeWritable");
            Q_UNREACHABLE();
        }
#  endif
#else
        // We assume we already have RWX
        (void)addr; // suppress unused parameter warning
        (void)size; // suppress unused parameter warning
#endif
    }

    static void makeExecutable(void* addr, size_t size)
    {
        quintptr pageSize = WTF::pageSize();
        quintptr iaddr = reinterpret_cast<quintptr>(addr);
        quintptr roundAddr = iaddr & ~(pageSize - 1);
        size = size + (iaddr - roundAddr);
        addr = reinterpret_cast<void*>(roundAddr);

#if ENABLE(ASSEMBLER_WX_EXCLUSIVE)
#  if OS(WINDOWS)
        DWORD oldProtect;
#    if !OS(WINRT)
        VirtualProtect(addr, size, PAGE_EXECUTE_READ, &oldProtect);
#    elif _MSC_VER >= 1900
        bool hr = VirtualProtectFromApp(addr, size, PAGE_EXECUTE_READ, &oldProtect);
        if (!hr) {
            Q_UNREACHABLE();
        }
#    else
        (void)oldProtect;
#    endif
#  else
        int mode = PROT_READ | PROT_EXEC;
        if (mprotect(addr, size, mode) != 0) {
            perror("mprotect failed in ExecutableAllocator::makeExecutable");
            Q_UNREACHABLE();
        }
#  endif
#else
#  error "Only W^X is supported"
#endif
    }

    QV4::ExecutableAllocator *realAllocator;
};

}

#endif // MASM_EXECUTABLEALLOCATOR_H
