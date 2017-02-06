// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLoadingBehaviorFlag_h
#define WebLoadingBehaviorFlag_h

namespace blink {

// This enum tracks certain behavior Blink exhibits when loading a page. This is
// for use in metrics collection by the loading team, to evaluate experimental
// features and potential areas of improvement in the loading stack. The main
// consumer is the page_load_metrics component, which sends bit flags to the
// browser process for histogram splitting.
enum WebLoadingBehaviorFlag {
    WebLoadingBehaviorNone = 0,
    // Indicates that the page used the document.write evaluator to preload scan for resources inserted via document.write.
    WebLoadingBehaviorDocumentWriteEvaluator = 1 << 0,
    // Indicates that the page is controlled by a Service Worker.
    WebLoadingBehaviorServiceWorkerControlled = 1 << 1,
    // Indicates that the page has a synchronous, cross-origin document.written
    // script.
    WebLoadingBehaviorDocumentWriteBlock = 1 << 2,
    // Indicates that the page is a reload and has a synchronous, cross-origin document.written
    // script.
    WebLoadingBehaviorDocumentWriteBlockReload = 1 << 3,
};

} // namespace blink

#endif // WebLoadingBehaviorFlag_h
