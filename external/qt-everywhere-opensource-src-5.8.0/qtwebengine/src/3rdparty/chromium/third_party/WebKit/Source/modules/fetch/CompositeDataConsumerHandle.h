// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositeDataConsumerHandle_h
#define CompositeDataConsumerHandle_h

#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "wtf/Allocator.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class WebThread;

// This is a utility class to construct a composite data consumer handle. It
// owns a web data consumer handle and delegates methods. A user can update
// the handle by using |update| method.
class MODULES_EXPORT CompositeDataConsumerHandle final : public WebDataConsumerHandle {
    WTF_MAKE_NONCOPYABLE(CompositeDataConsumerHandle);
    USING_FAST_MALLOC(CompositeDataConsumerHandle);
    class Context;
public:
    // An Updater is bound to the creator thread.
    class MODULES_EXPORT Updater final : public GarbageCollectedFinalized<Updater> {
    public:
        explicit Updater(PassRefPtr<Context>);
        ~Updater();

        // |handle| must not be null and must not be locked.
        void update(std::unique_ptr<WebDataConsumerHandle> /* handle */);
        DEFINE_INLINE_TRACE() { }

    private:
        RefPtr<Context> m_context;
#if ENABLE(ASSERT)
        WebThread* const m_thread;
#endif
    };

    // Returns a handle and stores the associated updater to |*updater|. The
    // associated updater will be bound to the calling thread.
    // |handle| must not be null and must not be locked.
    template<typename T>
    static std::unique_ptr<WebDataConsumerHandle> create(std::unique_ptr<WebDataConsumerHandle> handle, T* updater)
    {
        ASSERT(handle);
        Updater* u = nullptr;
        std::unique_ptr<CompositeDataConsumerHandle> p = wrapUnique(new CompositeDataConsumerHandle(std::move(handle), &u));
        *updater = u;
        return std::move(p);
    }
    ~CompositeDataConsumerHandle() override;

private:
    class ReaderImpl;
    Reader* obtainReaderInternal(Client*) override;

    const char* debugName() const override { return "CompositeDataConsumerHandle"; }

    CompositeDataConsumerHandle(std::unique_ptr<WebDataConsumerHandle>, Updater**);

    RefPtr<Context> m_context;
};

} // namespace blink

#endif // CompositeDataConsumerHandle_h
