/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include <new>
#include <unistd.h>

#if defined(__LP64__)
#  define SIZE_T_MANGLING  "m"
#else
#  define SIZE_T_MANGLING  "j"
#endif

#define SHIM_ALIAS_SYMBOL(fn) __attribute__((weak, alias(#fn)))
#define SHIM_HIDDEN __attribute__ ((visibility ("hidden")))

extern "C" {

__asm__(".symver __ShimCppNew, _Znw" SIZE_T_MANGLING "@Qt_5");
void* __ShimCppNew(size_t size)
    SHIM_ALIAS_SYMBOL(ShimCppNew);

__asm__(".symver __ShimCppDelete, _ZdlPv@Qt_5");
void __ShimCppDelete(void* address)
    SHIM_ALIAS_SYMBOL(ShimCppDelete);

__asm__(".symver __ShimCppNewArray, _Zna" SIZE_T_MANGLING "@Qt_5");
void* __ShimCppNewArray(size_t size)
    SHIM_ALIAS_SYMBOL(ShimCppNewArray);

__asm__(".symver __ShimCppDeleteArray, _ZdaPv@Qt_5");
void __ShimCppDeleteArray(void* address)
    SHIM_ALIAS_SYMBOL(ShimCppDeleteArray);

__asm__(".symver __ShimCppNewNoThrow, _Znw" SIZE_T_MANGLING "RKSt9nothrow_t@Qt_5");
void __ShimCppNewNoThrow(size_t size, const std::nothrow_t&) noexcept
    SHIM_ALIAS_SYMBOL(ShimCppNew);

__asm__(".symver __ShimCppNewArrayNoThrow, _Zna" SIZE_T_MANGLING "RKSt9nothrow_t@Qt_5");
void __ShimCppNewArrayNoThrow(size_t size, const std::nothrow_t&) noexcept
    SHIM_ALIAS_SYMBOL(ShimCppNewArray);

__asm__(".symver __ShimCppDeleteNoThrow, _ZdlPvRKSt9nothrow_t@Qt_5");
void __ShimCppDeleteNoThrow(void* address, const std::nothrow_t&) noexcept
    SHIM_ALIAS_SYMBOL(ShimCppDelete);

__asm__(".symver __ShimCppDeleteArrayNoThrow, _ZdaPvRKSt9nothrow_t@Qt_5");
void __ShimCppDeleteArrayNoThrow(void* address, const std::nothrow_t&) noexcept
    SHIM_ALIAS_SYMBOL(ShimCppDeleteArray);

static void* __shimCppNew(size_t size);
static void* __shimCppNewArray(size_t size);
static void __shimCppDelete(void *address);
static void __shimCppDeleteArray(void *address);

SHIM_HIDDEN void* ShimCppNew(size_t size) {
    return __shimCppNew(size);
}

SHIM_HIDDEN void* ShimCppNewArray(size_t size) {
    return __shimCppNewArray(size);
}

SHIM_HIDDEN void ShimCppDelete(void* address) {
    __shimCppDelete(address);
}

SHIM_HIDDEN void ShimCppDeleteArray(void* address) {
    __shimCppDeleteArray(address);
}
} // extern "C"

static void* __shimCppNew(size_t size) {
    return operator new(size);
}

static void* __shimCppNewArray(size_t size) {
    return operator new[](size);
}

static void __shimCppDelete(void* address) {
    operator delete(address);
}

static void __shimCppDeleteArray(void* address) {
    operator delete[](address);
}
