// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PAGE_TRANSITION_TYPES_H_
#define CONTENT_PUBLIC_COMMON_PAGE_TRANSITION_TYPES_H_

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace content {

enum PageTransition {

#define PAGE_TRANSITION(label, value) PAGE_TRANSITION_ ## label = value,
#include "content/public/common/page_transition_types_list.h"
#undef PAGE_TRANSITION

};

// Compares two PageTransition types ignoring qualifiers. |rhs| is taken to
// be a compile time constant, and hence must not contain any qualifiers.
CONTENT_EXPORT bool PageTransitionCoreTypeIs(PageTransition lhs,
                                             PageTransition rhs);

// Simplifies the provided transition by removing any qualifier
CONTENT_EXPORT PageTransition PageTransitionStripQualifier(
    PageTransition type);

bool PageTransitionIsValidType(int32 type);

CONTENT_EXPORT PageTransition PageTransitionFromInt(int32 type);

// Returns true if the given transition is a top-level frame transition, or
// false if the transition was for a subframe.
CONTENT_EXPORT bool PageTransitionIsMainFrame(PageTransition type);

// Returns whether a transition involves a redirection
CONTENT_EXPORT bool PageTransitionIsRedirect(PageTransition type);

// Returns whether a transition is a new navigation (rather than a return
// to a previously committed navigation).
CONTENT_EXPORT bool PageTransitionIsNewNavigation(PageTransition type);

// Return the qualifier
CONTENT_EXPORT int32 PageTransitionGetQualifier(PageTransition type);

// Returns true if the transition can be triggered by the web instead of
// through UI or similar.
CONTENT_EXPORT bool PageTransitionIsWebTriggerable(PageTransition type);

// Return a string version of the core type values.
CONTENT_EXPORT const char* PageTransitionGetCoreTransitionString(
    PageTransition type);

// TODO(joth): Remove the #if guard here; requires all chrome layer code to
// be fixed up not to use operator==
#if defined(CONTENT_IMPLEMENTATION)
// Declare a dummy class that is intentionally never defined.
class DontUseOperatorEquals;

// Ban operator== as it's way too easy to forget to strip the qualifiers. Use
// PageTransitionCoreTypeIs() instead.
DontUseOperatorEquals operator==(PageTransition, PageTransition);
DontUseOperatorEquals operator==(PageTransition, int);
DontUseOperatorEquals operator==(int, PageTransition);
#endif  // defined(CONTENT_IMPLEMENTATION)

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PAGE_TRANSITION_TYPES_H_
