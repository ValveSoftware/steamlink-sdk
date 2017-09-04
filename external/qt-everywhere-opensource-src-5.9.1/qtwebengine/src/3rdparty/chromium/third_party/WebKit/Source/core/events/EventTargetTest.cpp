// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptController.h"
#include "core/layout/LayoutTestHelper.h"
#include "platform/testing/HistogramTester.h"

namespace blink {

enum PassiveForcedListenerResultType {
  PreventDefaultNotCalled,
  DocumentLevelTouchPreventDefaultCalled
};

class EventTargetTest : public RenderingTest {
 public:
  EventTargetTest() {}
  ~EventTargetTest() {}
};

TEST_F(EventTargetTest, PreventDefaultNotCalled) {
  document().settings()->setScriptEnabled(true);
  HistogramTester histogramTester;
  document().frame()->script().executeScriptInMainWorld(
      "window.addEventListener('touchstart', function(e) {}, {});"
      "window.dispatchEvent(new TouchEvent('touchstart', {cancelable: "
      "false}));");

  histogramTester.expectTotalCount("Event.PassiveForcedEventDispatchCancelled",
                                   1);
  histogramTester.expectUniqueSample(
      "Event.PassiveForcedEventDispatchCancelled", PreventDefaultNotCalled, 1);
}

TEST_F(EventTargetTest, PreventDefaultCalled) {
  document().settings()->setScriptEnabled(true);
  HistogramTester histogramTester;
  document().frame()->script().executeScriptInMainWorld(
      "window.addEventListener('touchstart', function(e) "
      "{e.preventDefault();}, {});"
      "window.dispatchEvent(new TouchEvent('touchstart', {cancelable: "
      "false}));");

  histogramTester.expectTotalCount("Event.PassiveForcedEventDispatchCancelled",
                                   1);
  histogramTester.expectUniqueSample(
      "Event.PassiveForcedEventDispatchCancelled",
      DocumentLevelTouchPreventDefaultCalled, 1);
}

}  // namespace blink
