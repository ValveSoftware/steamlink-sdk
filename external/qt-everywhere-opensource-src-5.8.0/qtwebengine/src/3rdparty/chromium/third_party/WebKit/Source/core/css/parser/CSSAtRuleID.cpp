// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSAtRuleID.h"

#include "core/frame/UseCounter.h"

namespace blink {

CSSAtRuleID cssAtRuleID(StringView name)
{
    if (equalIgnoringASCIICase(name, "charset"))
        return CSSAtRuleCharset;
    if (equalIgnoringASCIICase(name, "font-face"))
        return CSSAtRuleFontFace;
    if (equalIgnoringASCIICase(name, "import"))
        return CSSAtRuleImport;
    if (equalIgnoringASCIICase(name, "keyframes"))
        return CSSAtRuleKeyframes;
    if (equalIgnoringASCIICase(name, "media"))
        return CSSAtRuleMedia;
    if (equalIgnoringASCIICase(name, "namespace"))
        return CSSAtRuleNamespace;
    if (equalIgnoringASCIICase(name, "page"))
        return CSSAtRulePage;
    if (equalIgnoringASCIICase(name, "supports"))
        return CSSAtRuleSupports;
    if (equalIgnoringASCIICase(name, "viewport"))
        return CSSAtRuleViewport;
    if (equalIgnoringASCIICase(name, "-webkit-keyframes"))
        return CSSAtRuleWebkitKeyframes;
    if (equalIgnoringASCIICase(name, "apply"))
        return CSSAtRuleApply;
    return CSSAtRuleInvalid;
}

void countAtRule(UseCounter* useCounter, CSSAtRuleID ruleId)
{
    ASSERT(useCounter);
    UseCounter::Feature feature;

    switch (ruleId) {
    case CSSAtRuleCharset:
        feature = UseCounter::CSSAtRuleCharset;
        break;
    case CSSAtRuleFontFace:
        feature = UseCounter::CSSAtRuleFontFace;
        break;
    case CSSAtRuleImport:
        feature = UseCounter::CSSAtRuleImport;
        break;
    case CSSAtRuleKeyframes:
        feature = UseCounter::CSSAtRuleKeyframes;
        break;
    case CSSAtRuleMedia:
        feature = UseCounter::CSSAtRuleMedia;
        break;
    case CSSAtRuleNamespace:
        feature = UseCounter::CSSAtRuleNamespace;
        break;
    case CSSAtRulePage:
        feature = UseCounter::CSSAtRulePage;
        break;
    case CSSAtRuleSupports:
        feature = UseCounter::CSSAtRuleSupports;
        break;
    case CSSAtRuleViewport:
        feature = UseCounter::CSSAtRuleViewport;
        break;

    case CSSAtRuleWebkitKeyframes:
        feature = UseCounter::CSSAtRuleWebkitKeyframes;
        break;

    case CSSAtRuleApply:
        feature = UseCounter::CSSAtRuleApply;
        break;

    case CSSAtRuleInvalid:
        // fallthrough
    default:
        ASSERT_NOT_REACHED();
        return;
    }
    useCounter->count(feature);
}

} // namespace blink

