/*
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Gabor Loki <loki@webkit.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ParallelJobs_h
#define ParallelJobs_h

#include "platform/Task.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "wtf/Assertions.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

// Usage:
//
//     // Initialize parallel jobs
//     ParallelJobs<TypeOfParameter> parallelJobs(&worker, requestedNumberOfJobs);
//
//     // Fill the parameter array
//     for(i = 0; i < parallelJobs.numberOfJobs(); ++i) {
//       TypeOfParameter& params = parallelJobs.parameter(i);
//       params.attr1 = localVars ...
//       ...
//     }
//
//     // Execute parallel jobs
//     parallelJobs.execute();
//

namespace WebCore {

template<typename Type>
class ParallelJobs {
    WTF_MAKE_NONCOPYABLE(ParallelJobs);
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef void (*WorkerFunction)(Type*);

    ParallelJobs(WorkerFunction func, size_t requestedJobNumber)
        : m_func(func)
    {
        size_t numberOfJobs = std::max(static_cast<size_t>(2), std::min(requestedJobNumber, blink::Platform::current()->numberOfProcessors()));
        m_parameters.grow(numberOfJobs);
        // The main thread can execute one job, so only create requestJobNumber - 1 threads.
        for (size_t i = 0; i < numberOfJobs - 1; ++i) {
            OwnPtr<blink::WebThread> thread = adoptPtr(blink::Platform::current()->createThread("Unfortunate parallel worker"));
            m_threads.append(thread.release());
        }
    }

    size_t numberOfJobs()
    {
        return m_parameters.size();
    }

    Type& parameter(size_t i)
    {
        return m_parameters[i];
    }

    void execute()
    {
        for (size_t i = 0; i < numberOfJobs() - 1; ++i)
            m_threads[i]->postTask(new Task(WTF::bind(m_func, &parameter(i))));
        m_func(&parameter(numberOfJobs() - 1));
        m_threads.clear();
    }

private:
    WorkerFunction m_func;
    Vector<OwnPtr<blink::WebThread> > m_threads;
    Vector<Type> m_parameters;
};

} // namespace WebCore

#endif // ParallelJobs_h
