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

static PrintStream* s_dataFile;

void setDataFile(PrintStream *ps)
{
    delete s_dataFile;
    s_dataFile = ps;
}

void setDataFile(FILE* f)
{
    delete s_dataFile;
    s_dataFile = new FilePrintStream(f, FilePrintStream::Borrow);
}

PrintStream& dataFile()
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

