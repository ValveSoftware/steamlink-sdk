/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#ifndef ThreadableLoader_h
#define ThreadableLoader_h

#include "core/CoreExport.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "platform/CrossThreadCopier.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

class ResourceRequest;
class ExecutionContext;
class ThreadableLoaderClient;

enum CrossOriginRequestPolicy {
    DenyCrossOriginRequests,
    UseAccessControl,
    AllowCrossOriginRequests
};

enum PreflightPolicy {
    ConsiderPreflight,
    ForcePreflight,
    PreventPreflight
};

enum ContentSecurityPolicyEnforcement {
    EnforceContentSecurityPolicy,
    DoNotEnforceContentSecurityPolicy,
};

struct ThreadableLoaderOptions {
    DISALLOW_NEW();
    ThreadableLoaderOptions()
        : preflightPolicy(ConsiderPreflight)
        , crossOriginRequestPolicy(DenyCrossOriginRequests)
        , contentSecurityPolicyEnforcement(EnforceContentSecurityPolicy)
        , timeoutMilliseconds(0) { }

    // When adding members, CrossThreadThreadableLoaderOptionsData should
    // be updated.
    PreflightPolicy preflightPolicy; // If AccessControl is used, how to determine if a preflight is needed.
    CrossOriginRequestPolicy crossOriginRequestPolicy;
    AtomicString initiator;
    ContentSecurityPolicyEnforcement contentSecurityPolicyEnforcement;
    unsigned long timeoutMilliseconds;
};

// Encode AtomicString as String to cross threads.
struct CrossThreadThreadableLoaderOptionsData {
    STACK_ALLOCATED();
    explicit CrossThreadThreadableLoaderOptionsData(const ThreadableLoaderOptions& options)
        : preflightPolicy(options.preflightPolicy)
        , crossOriginRequestPolicy(options.crossOriginRequestPolicy)
        , initiator(options.initiator.getString().isolatedCopy())
        , contentSecurityPolicyEnforcement(options.contentSecurityPolicyEnforcement)
        , timeoutMilliseconds(options.timeoutMilliseconds) { }

    operator ThreadableLoaderOptions() const
    {
        ThreadableLoaderOptions options;
        options.preflightPolicy = preflightPolicy;
        options.crossOriginRequestPolicy = crossOriginRequestPolicy;
        options.initiator = AtomicString(initiator);
        options.contentSecurityPolicyEnforcement = contentSecurityPolicyEnforcement;
        options.timeoutMilliseconds = timeoutMilliseconds;
        return options;
    }

    PreflightPolicy preflightPolicy;
    CrossOriginRequestPolicy crossOriginRequestPolicy;
    String initiator;
    ContentSecurityPolicyEnforcement contentSecurityPolicyEnforcement;
    unsigned long timeoutMilliseconds;
};

template <>
struct CrossThreadCopier<ThreadableLoaderOptions> {
    typedef CrossThreadThreadableLoaderOptionsData Type;
    static Type copy(const ThreadableLoaderOptions& options)
    {
        return CrossThreadThreadableLoaderOptionsData(options);
    }
};

// Useful for doing loader operations from any thread (not threadsafe,
// just able to run on threads other than the main thread).
//
// Arguments common to both loadResourceSynchronously() and create():
//
// - ThreadableLoaderOptions argument configures this ThreadableLoader's
//   behavior.
//
// - ResourceLoaderOptions argument will be passed to the FetchRequest
//   that this ThreadableLoader creates. It can be altered e.g. when
//   redirect happens.
class CORE_EXPORT ThreadableLoader {
    WTF_MAKE_NONCOPYABLE(ThreadableLoader);
public:
    // ThreadableLoaderClient methods may not destroy the ThreadableLoader
    // instance in them.
    static void loadResourceSynchronously(ExecutionContext&, const ResourceRequest&, ThreadableLoaderClient&, const ThreadableLoaderOptions&, const ResourceLoaderOptions&);

    // This method never returns nullptr.
    //
    // This method must always be followed by start() call.
    // ThreadableLoaderClient methods are never called before start() call.
    //
    // The async loading feature is separated into the create() method and
    // and the start() method in order to:
    // - reduce work done in a constructor
    // - not to ask the users to handle failures in the constructor and other
    //   async failures separately
    //
    // Loading completes when one of the following methods are called:
    // - didFinishLoading()
    // - didFail()
    // - didFailAccessControlCheck()
    // - didFailRedirectCheck()
    // After any of these methods is called, the loader won't call any of the
    // ThreadableLoaderClient methods.
    //
    // When a ThreadableLoader is destructed, any of the
    // ThreadableLoaderClient methods is NOT called in response to the
    // destruction either synchronously or after destruction.
    //
    // When ThreadableLoader::cancel() is called,
    // ThreadableLoaderClient::didFail() is called with a ResourceError
    // with isCancellation() returning true, if any of didFinishLoading()
    // or didFail.*() methods have not been called yet. (didFail() may be
    // called with a ResourceError with isCancellation() returning true
    // also for cancellation happened inside the loader.)
    //
    // ThreadableLoaderClient methods:
    // - may call cancel()
    // - can destroy the ThreadableLoader instance in them (by clearing
    //   std::unique_ptr<ThreadableLoader>).
    static std::unique_ptr<ThreadableLoader> create(ExecutionContext&, ThreadableLoaderClient*, const ThreadableLoaderOptions&, const ResourceLoaderOptions&);

    // The methods on the ThreadableLoaderClient passed on create() call
    // may be called synchronous to start() call.
    virtual void start(const ResourceRequest&) = 0;

    // A ThreadableLoader may have a timeout specified. It is possible, in some cases, for
    // the timeout to be overridden after the request is sent (for example, XMLHttpRequests
    // may override their timeout setting after sending).
    //
    // Set a new timeout relative to the time the request started, in milliseconds.
    virtual void overrideTimeout(unsigned long timeoutMilliseconds) = 0;

    virtual void cancel() = 0;

    virtual ~ThreadableLoader() { }

protected:
    ThreadableLoader() { }
};

} // namespace blink

#endif // ThreadableLoader_h
