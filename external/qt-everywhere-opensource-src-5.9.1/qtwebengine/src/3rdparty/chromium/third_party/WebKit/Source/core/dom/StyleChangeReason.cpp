// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/StyleChangeReason.h"

#include "platform/tracing/TraceEvent.h"
#include "wtf/StaticConstructors.h"

namespace blink {

namespace StyleChangeReason {
const char ActiveStylesheetsUpdate[] = "ActiveStylesheetsUpdate";
const char Animation[] = "Animation";
const char Attribute[] = "Attribute";
const char CleanupPlaceholderStyles[] = "CleanupPlaceholderStyles";
const char CompositorProxy[] = "CompositorProxy";
const char ControlValue[] = "ControlValue";
const char Control[] = "Control";
const char DeclarativeContent[] = "Extension declarativeContent.css";
const char DesignMode[] = "DesignMode";
const char FontSizeChange[] = "FontSizeChange";
const char Fonts[] = "Fonts";
const char FullScreen[] = "FullScreen";
const char Inline[] = "Inline";
const char InlineCSSStyleMutated[] = "Inline CSS style declaration was mutated";
const char Inspector[] = "Inspector";
const char Language[] = "Language";
const char LinkColorChange[] = "LinkColorChange";
const char PlatformColorChange[] = "PlatformColorChange";
const char PropagateInheritChangeToDistributedNodes[] =
    "PropagateInheritChangeToDistributedNodes";
const char PropertyRegistration[] = "PropertyRegistration";
const char PropertyUnregistration[] = "PropertyUnregistration";
const char PseudoClass[] = "PseudoClass";
const char SVGContainerSizeChange[] = "SVGContainerSizeChange";
const char SVGCursor[] = "SVGCursor";
const char Settings[] = "Settings";
const char Shadow[] = "Shadow";
const char StyleInvalidator[] = "StyleInvalidator";
const char StyleSheetChange[] = "StyleSheetChange";
const char ViewportUnits[] = "ViewportUnits";
const char VisitedLink[] = "VisitedLink";
const char VisuallyOrdered[] = "VisuallyOrdered";
const char WritingModeChange[] = "WritingModeChange";
const char Zoom[] = "Zoom";
}  // namespace StyleChangeReason

namespace StyleChangeExtraData {
DEFINE_GLOBAL(AtomicString, Active)
DEFINE_GLOBAL(AtomicString, Disabled)
DEFINE_GLOBAL(AtomicString, Drag)
DEFINE_GLOBAL(AtomicString, Focus)
DEFINE_GLOBAL(AtomicString, Hover)
DEFINE_GLOBAL(AtomicString, Past)
DEFINE_GLOBAL(AtomicString, Unresolved)

void init() {
  DCHECK(isMainThread());

  new (NotNull, (void*)&Active) AtomicString(":active");
  new (NotNull, (void*)&Disabled) AtomicString(":disabled");
  new (NotNull, (void*)&Drag) AtomicString(":-webkit-drag");
  new (NotNull, (void*)&Focus) AtomicString(":focus");
  new (NotNull, (void*)&Hover) AtomicString(":hover");
  new (NotNull, (void*)&Past) AtomicString(":past");
  new (NotNull, (void*)&Unresolved) AtomicString(":unresolved");
}

}  // namespace StyleChangeExtraData

}  // namespace blink
