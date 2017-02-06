/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/testing/UnitTestHelpers.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "platform/SharedBuffer.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "public/platform/FilePathConversion.h"
#include "public/platform/Platform.h"
#include "public/platform/WebString.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/text/StringUTF8Adaptor.h"

namespace blink {
namespace testing {

void runPendingTasks()
{
    Platform::current()->currentThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, WTF::bind(&exitRunLoop));

    // We forbid GC in the tasks. Otherwise the registered GCTaskObserver tries
    // to run GC with NoHeapPointerOnStack.
    ThreadState::current()->enterGCForbiddenScope();
    enterRunLoop();
    ThreadState::current()->leaveGCForbiddenScope();
}

void runDelayedTasks(double delayMs)
{
    Platform::current()->currentThread()->getWebTaskRunner()->postDelayedTask(BLINK_FROM_HERE, WTF::bind(&exitRunLoop), delayMs);
    enterRunLoop();
}

String blinkRootDir()
{
    base::FilePath path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.Append(FILE_PATH_LITERAL("third_party/WebKit"));
    path = base::MakeAbsoluteFilePath(path);
    return String::fromUTF8(path.MaybeAsASCII().c_str());
}

PassRefPtr<SharedBuffer> readFromFile(const String& path)
{
    base::FilePath filePath = blink::WebStringToFilePath(path);
    std::string buffer;
    base::ReadFileToString(filePath, &buffer);
    return SharedBuffer::create(buffer.data(), buffer.size());
}

void enterRunLoop()
{
    base::RunLoop().Run();
}

void exitRunLoop()
{
    base::MessageLoop::current()->QuitWhenIdle();
}

void yieldCurrentThread()
{
    base::PlatformThread::YieldCurrentThread();
}

} // namespace testing
} // namespace blink
