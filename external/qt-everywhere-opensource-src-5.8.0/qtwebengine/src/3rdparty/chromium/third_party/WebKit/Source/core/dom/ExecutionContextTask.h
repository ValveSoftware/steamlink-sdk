/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ExecutionContextTask_h
#define ExecutionContextTask_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;

class CORE_EXPORT ExecutionContextTask {
    WTF_MAKE_NONCOPYABLE(ExecutionContextTask);
    USING_FAST_MALLOC(ExecutionContextTask);
public:
    ExecutionContextTask() { }
    virtual ~ExecutionContextTask() { }
    virtual void performTask(ExecutionContext*) = 0;
};

namespace internal {

template<WTF::FunctionThreadAffinity threadAffinity>
void runCallClosureTask(std::unique_ptr<Function<void(), threadAffinity>> closure, ExecutionContext*)
{
    (*closure)();
}

template<WTF::FunctionThreadAffinity threadAffinity>
void runCallClosureTask(std::unique_ptr<Function<void(ExecutionContext*), threadAffinity>> closure, ExecutionContext* executionContext)
{
    (*closure)(executionContext);
}

template<typename T, WTF::FunctionThreadAffinity threadAffinity>
class CallClosureTask final : public ExecutionContextTask {
public:
    static std::unique_ptr<CallClosureTask> create(std::unique_ptr<Function<T, threadAffinity>> closure)
    {
        return wrapUnique(new CallClosureTask(std::move(closure)));
    }

private:
    explicit CallClosureTask(std::unique_ptr<Function<T, threadAffinity>> closure)
        : m_closure(std::move(closure))
    {
    }

    void performTask(ExecutionContext* executionContext) override
    {
        runCallClosureTask(std::move(m_closure), executionContext);
    }

    std::unique_ptr<Function<T, threadAffinity>> m_closure;
};

// Do not use |create| other than in createCrossThreadTask and
// createSameThreadTask.
// See http://crbug.com/390851
template<typename T, WTF::FunctionThreadAffinity threadAffinity>
std::unique_ptr<CallClosureTask<T, threadAffinity>> createCallClosureTask(std::unique_ptr<Function<T, threadAffinity>> closure)
{
    return CallClosureTask<T, threadAffinity>::create(std::move(closure));
}

} // namespace internal

// Create tasks passed within a single thread.
// When posting tasks within a thread, use |createSameThreadTask| instead
// of using |bind| directly to state explicitly that there is no need to care
// about thread safety when posting the task.
// When posting tasks across threads, use |createCrossThreadTask|.
template<typename FunctionType, typename... P>
std::unique_ptr<ExecutionContextTask> createSameThreadTask(
    FunctionType function, P&&... parameters)
{
    return internal::createCallClosureTask(WTF::bind(function, std::forward<P>(parameters)...));
}

} // namespace blink

#endif
