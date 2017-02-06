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
#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <qdebug.h>
#include <qbytearray.h> // qvsnprintf
#include <FilePrintStream.h>

namespace WTF {

void* fastMalloc(size_t size)
{
    return malloc(size);
}

void* fastRealloc(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

void fastFree(void* ptr)
{
    free(ptr);
}

uint32_t cryptographicallyRandomNumber()
{
    return 0;
}

static FilePrintStream* s_dataFile;

void setDataFile(FilePrintStream *ps)
{
    delete s_dataFile;
    s_dataFile = ps;
}

void setDataFile(FILE* f)
{
    delete s_dataFile;
    s_dataFile = new FilePrintStream(f, FilePrintStream::Borrow);
}

FilePrintStream& dataFile()
{
    if (!s_dataFile)
        s_dataFile = new FilePrintStream(stderr, FilePrintStream::Borrow);
    return *s_dataFile;
}

void dataLogFV(const char* format, va_list args)
{
    char buffer[1024];
    qvsnprintf(buffer, sizeof(buffer), format, args);
    qDebug("%s", buffer);
}

void dataLogF(const char* format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    qvsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    qDebug("%s", buffer);
}

void dataLogFString(const char* str)
{
    qDebug("%s", str);
}

}

extern "C" {

void WTFReportAssertionFailure(const char* file, int line, const char* function, const char*assertion)
{
    fprintf(stderr, "WTF failing assertion in %s, line %d, function %s: %s\n", file, line, function, assertion);
}

void WTFReportAssertionFailureWithMessage(const char* file, int line, const char* function, const char* assertion, const char* format, ...)
{
    // TODO: show the message, or remove this function completely. (The latter would probably be best.)
    Q_UNUSED(format);
    fprintf(stderr, "WTF failing assertion in %s, line %d, function %s: %s\n", file, line, function, assertion);
}

void WTFReportBacktrace()
{
}

void WTFInvokeCrashHook()
{
}

}


#if ENABLE(ASSEMBLER) && CPU(X86) && !OS(MAC_OS_X)
#include <MacroAssemblerX86Common.h>

JSC::MacroAssemblerX86Common::SSE2CheckState JSC::MacroAssemblerX86Common::s_sse2CheckState = JSC::MacroAssemblerX86Common::NotCheckedSSE2;
#endif

