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

#include "qv4compilationunitmapper_p.h"

#include "qv4compileddata_p.h"
#include <private/qdeferredcleanup_p.h>
#include <QFileInfo>
#include <QDateTime>
#include <qt_windows.h>

QT_BEGIN_NAMESPACE

using namespace QV4;

CompiledData::Unit *CompilationUnitMapper::open(const QString &cacheFileName, const QString &sourcePath, QString *errorString)
{
    close();

    // ### TODO: fix up file encoding/normalization/unc handling once QFileSystemEntry
    // is exported from QtCore.
    HANDLE handle =
#if defined(Q_OS_WINRT)
        CreateFile2(reinterpret_cast<const wchar_t*>(cacheFileName.constData()),
                   GENERIC_READ | GENERIC_EXECUTE, FILE_SHARE_READ,
                   OPEN_EXISTING, nullptr);
#else
        CreateFile(reinterpret_cast<const wchar_t*>(cacheFileName.constData()),
                   GENERIC_READ | GENERIC_EXECUTE, FILE_SHARE_READ,
                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                   nullptr);
#endif
    if (handle == INVALID_HANDLE_VALUE) {
        *errorString = qt_error_string(GetLastError());
        return nullptr;
    }

    QDeferredCleanup fileHandleCleanup([handle]{
        CloseHandle(handle);
    });

#if !defined(Q_OS_WINRT) || _MSC_VER >= 1900
    CompiledData::Unit header;
    DWORD bytesRead;
    if (!ReadFile(handle, reinterpret_cast<char *>(&header), sizeof(header), &bytesRead, nullptr)) {
        *errorString = qt_error_string(GetLastError());
        return nullptr;
    }

    if (bytesRead != sizeof(header)) {
        *errorString = QStringLiteral("File too small for the header fields");
        return nullptr;
    }

    if (!verifyHeader(&header, sourcePath, errorString))
        return nullptr;

    const uint mappingFlags = header.flags & QV4::CompiledData::Unit::ContainsMachineCode
                              ? PAGE_EXECUTE_READ : PAGE_READONLY;
    const uint viewFlags = header.flags & QV4::CompiledData::Unit::ContainsMachineCode
                           ? (FILE_MAP_READ | FILE_MAP_EXECUTE) : FILE_MAP_READ;

    // Data structure and qt version matched, so now we can access the rest of the file safely.

    HANDLE fileMappingHandle = CreateFileMapping(handle, 0, mappingFlags, 0, 0, 0);
    if (!fileMappingHandle) {
        *errorString = qt_error_string(GetLastError());
        return nullptr;
    }

    QDeferredCleanup mappingCleanup([fileMappingHandle]{
        CloseHandle(fileMappingHandle);
    });

    dataPtr = MapViewOfFile(fileMappingHandle, viewFlags, 0, 0, 0);
    if (!dataPtr) {
        *errorString = qt_error_string(GetLastError());
        return nullptr;
    }

    return reinterpret_cast<CompiledData::Unit*>(dataPtr);
#else
    Q_UNUSED(sourcePath);
    *errorString = QStringLiteral("Compilation unit mapping not supported on WinRT 8.1");
    return nullptr;
#endif
}

void CompilationUnitMapper::close()
{
#if !defined(Q_OS_WINRT) || _MSC_VER >= 1900
    if (dataPtr != nullptr)
        UnmapViewOfFile(dataPtr);
#endif
    dataPtr = nullptr;
}

QT_END_NAMESPACE
