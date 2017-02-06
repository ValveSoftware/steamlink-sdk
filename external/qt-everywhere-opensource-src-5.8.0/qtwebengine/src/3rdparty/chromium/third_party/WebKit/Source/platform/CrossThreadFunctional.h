// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CrossThreadFunctional_h
#define CrossThreadFunctional_h

#include "base/bind.h"
#include "platform/CrossThreadCopier.h"
#include "wtf/Functional.h"
#include <type_traits>

namespace blink {

// crossThreadBind() is bind() for cross-thread task posting.
// crossThreadBind() applies CrossThreadCopier to the arguments.
//
// Example:
//     void func1(int, const String&);
//     f = crossThreadBind(func1, 42, str);
// func1(42, str2) will be called when |f()| is executed,
// where |str2| is a deep copy of |str| (created by str.isolatedCopy()).
//
// crossThreadBind(str) is similar to bind(str.isolatedCopy()), but the latter
// is NOT thread-safe due to temporary objects (https://crbug.com/390851).
//
// Don't (if you pass the task across threads):
//     bind(func1, 42, str);
//     bind(func1, 42, str.isolatedCopy());

template<typename FunctionType, typename... Ps>
std::unique_ptr<Function<base::MakeUnboundRunType<FunctionType, Ps...>, WTF::CrossThreadAffinity>> crossThreadBind(
    FunctionType function,
    Ps&&... parameters)
{
    return WTF::bindInternal<WTF::CrossThreadAffinity>(
        function,
        CrossThreadCopier<typename std::decay<Ps>::type>::copy(std::forward<Ps>(parameters))...);
}

} // namespace blink

#endif // CrossThreadFunctional_h
