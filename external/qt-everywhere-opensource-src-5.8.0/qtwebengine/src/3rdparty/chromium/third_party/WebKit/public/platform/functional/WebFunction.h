// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFunction_h
#define WebFunction_h

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "public/platform/WebCommon.h"

#include <memory>
#include <utility>

#if BLINK_IMPLEMENTATION
#include "wtf/Functional.h"
#else
#include "base/logging.h"
#endif

namespace blink {

template <typename...>
class WebFunction;

// Conversion from WTF closures to base closures to pass a callback out of
// blink.
template <typename R, typename... Args>
class WebFunction<R(Args...)> {
private:
#if BLINK_IMPLEMENTATION
    using WTFFunction = WTF::Function<R(Args...), WTF::SameThreadAffinity>;
#endif

public:
#if BLINK_IMPLEMENTATION
    WebFunction()
    {
    }

    explicit WebFunction(std::unique_ptr<WTFFunction> c)
    {
        m_callback = static_cast<base::Callback<R(Args...)>>(*c);
    }
#endif

    WebFunction(WebFunction&& other)
    {
        *this = std::move(other);
    }
    WebFunction& operator=(WebFunction&& other)
    {
#if DCHECK_IS_ON()
        m_haveClosure = other.m_haveClosure;
        other.m_haveClosure = false;
#endif
        m_callback = std::move(other.m_callback);
        return *this;
    }

#if !BLINK_IMPLEMENTATION
    // TODO(danakj): This could be rvalue-ref-qualified.
    base::Callback<R(Args...)> TakeBaseCallback()
    {
#if DCHECK_IS_ON()
        // Don't call this more than once!
        DCHECK(m_haveClosure);
        m_haveClosure = false;
#endif
        return std::move(m_callback);
    }
#endif

private:
#if DCHECK_IS_ON()
    bool m_haveClosure = true;
#endif
    base::Callback<R(Args...)> m_callback;

    DISALLOW_COPY_AND_ASSIGN(WebFunction);
};

using WebClosure = WebFunction<void()>;

} // namespace blink

#endif // WebClosure_h
