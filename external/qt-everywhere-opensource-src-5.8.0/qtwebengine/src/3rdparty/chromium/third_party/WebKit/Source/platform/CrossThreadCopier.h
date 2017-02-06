/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
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

#ifndef CrossThreadCopier_h
#define CrossThreadCopier_h

#include "platform/PlatformExport.h"
#include "wtf/Assertions.h"
#include "wtf/Forward.h"
#include "wtf/Functional.h" // FunctionThreadAffinity
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/TypeTraits.h"
#include <memory>

class SkRefCnt;

namespace WTF {

template <typename T>
class PassedWrapper;

}

namespace blink {

class IntRect;
class IntSize;
class KURL;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
struct CrossThreadResourceResponseData;
struct CrossThreadResourceRequestData;
template<typename T>class CrossThreadPersistent;
template<typename T>class CrossThreadWeakPersistent;

template <typename T>
struct CrossThreadCopierPassThrough {
    STATIC_ONLY(CrossThreadCopierPassThrough);
    typedef T Type;
    static Type copy(const T& parameter)
    {
        return parameter;
    }
};

template <typename T, bool isArithmeticOrEnum>
struct CrossThreadCopierBase;

// Arithmetic values (integers or floats) and enums can be safely copied.
template <typename T>
struct CrossThreadCopierBase<T, true> : public CrossThreadCopierPassThrough<T> {
    STATIC_ONLY(CrossThreadCopierBase);
};

template <typename T>
struct CrossThreadCopier : public CrossThreadCopierBase<T, std::is_arithmetic<T>::value || std::is_enum<T>::value> {
    STATIC_ONLY(CrossThreadCopier);
};

// CrossThreadCopier specializations follow.
template <typename T>
struct CrossThreadCopier<PassRefPtr<T>> : public CrossThreadCopierPassThrough<PassRefPtr<T>> {
    STATIC_ONLY(CrossThreadCopier);
    static_assert(WTF::IsSubclassOfTemplate<T, ThreadSafeRefCounted>::value || std::is_base_of<SkRefCnt, T>::value, "PassRefPtr<T> can be passed across threads only if T is ThreadSafeRefCounted or SkRefCnt.");
};
template <typename T>
struct CrossThreadCopier<RefPtr<T>> : public CrossThreadCopierPassThrough<RefPtr<T>> {
    STATIC_ONLY(CrossThreadCopier);
    static_assert(WTF::IsSubclassOfTemplate<T, ThreadSafeRefCounted>::value || std::is_base_of<SkRefCnt, T>::value, "RefPtr<T> can be passed across threads only if T is ThreadSafeRefCounted or SkRefCnt.");
};

// nullptr_t can be passed through without any changes.
template <>
struct CrossThreadCopier<std::nullptr_t> : public CrossThreadCopierPassThrough<std::nullptr_t> {
    STATIC_ONLY(CrossThreadCopier);
};

// To allow a type to be passed across threads using its copy constructor, add a forward declaration of the type and
// provide a specialization of CrossThreadCopier<T> in this file, like IntRect below.
template <>
struct CrossThreadCopier<IntRect> : public CrossThreadCopierPassThrough<IntRect> {
    STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<IntSize> : public CrossThreadCopierPassThrough<IntSize> {
    STATIC_ONLY(CrossThreadCopier);
};

template <typename T, typename Deleter>
struct CrossThreadCopier<std::unique_ptr<T, Deleter>> {
    STATIC_ONLY(CrossThreadCopier);
    using Type = std::unique_ptr<T, Deleter>;
    static std::unique_ptr<T, Deleter> copy(std::unique_ptr<T, Deleter> pointer)
    {
        return pointer; // This is in fact a move.
    }
};

template<typename T>
struct CrossThreadCopier<CrossThreadPersistent<T>> : public CrossThreadCopierPassThrough<CrossThreadPersistent<T>> {
    STATIC_ONLY(CrossThreadCopier);
};

template<typename T>
struct CrossThreadCopier<CrossThreadWeakPersistent<T>> : public CrossThreadCopierPassThrough<CrossThreadWeakPersistent<T>> {
    STATIC_ONLY(CrossThreadCopier);
};

template<typename T>
struct CrossThreadCopier<WTF::UnretainedWrapper<T, WTF::CrossThreadAffinity>> : public CrossThreadCopierPassThrough<WTF::UnretainedWrapper<T, WTF::CrossThreadAffinity>> {
    STATIC_ONLY(CrossThreadCopier);
};

template<typename T>
struct CrossThreadCopier<WeakPtr<T>> : public CrossThreadCopierPassThrough<WeakPtr<T>> {
    STATIC_ONLY(CrossThreadCopier);
};

template <typename T>
struct CrossThreadCopier<WTF::PassedWrapper<T>> {
    STATIC_ONLY(CrossThreadCopier);
    using Type = WTF::PassedWrapper<typename CrossThreadCopier<T>::Type>;
    static Type copy(WTF::PassedWrapper<T>&& value) { return passed(CrossThreadCopier<T>::copy(value.moveOut())); }
};

template <>
struct CrossThreadCopier<KURL> {
    STATIC_ONLY(CrossThreadCopier);
    typedef KURL Type;
    PLATFORM_EXPORT static Type copy(const KURL&);
};

template <>
struct CrossThreadCopier<String> {
    STATIC_ONLY(CrossThreadCopier);
    typedef String Type;
    PLATFORM_EXPORT static Type copy(const String&);
};

template <>
struct CrossThreadCopier<ResourceError> {
    STATIC_ONLY(CrossThreadCopier);
    typedef ResourceError Type;
    PLATFORM_EXPORT static Type copy(const ResourceError&);
};

template <>
struct CrossThreadCopier<ResourceRequest> {
    STATIC_ONLY(CrossThreadCopier);
    typedef WTF::PassedWrapper<std::unique_ptr<CrossThreadResourceRequestData>> Type;
    PLATFORM_EXPORT static Type copy(const ResourceRequest&);
};

template <>
struct CrossThreadCopier<ResourceResponse> {
    STATIC_ONLY(CrossThreadCopier);
    typedef WTF::PassedWrapper<std::unique_ptr<CrossThreadResourceResponseData>> Type;
    PLATFORM_EXPORT static Type copy(const ResourceResponse&);
};

} // namespace blink

#endif // CrossThreadCopier_h
