// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABLE_PAGE_UTILS_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABLE_PAGE_UTILS_H_

#include "base/callback.h"
#include "content/public/browser/web_contents.h"

namespace dom_distiller {

class DistillablePageDetector;

// Checks if the page appears to be distillable based on whichever heuristics
// are configured to be used (see dom_distiller::GetDistillerHeuristicsType).
void IsDistillablePage(content::WebContents* web_contents,
                       bool is_mobile_optimized,
                       base::Callback<void(bool)> callback);

// Checks if the web_contents is has opengraph type=article markup.
void IsOpenGraphArticle(content::WebContents* web_contents,
                        base::Callback<void(bool)> callback);

// Uses the provided DistillablePageDetector to detect if the page is
// distillable. The passed detector must be alive until after the callback is
// called.
void IsDistillablePageForDetector(content::WebContents* web_contents,
                                  const DistillablePageDetector* detector,
                                  base::Callback<void(bool)> callback);

typedef base::Callback<void(bool, bool)> DistillabilityDelegate;

// Set the delegate to receive the result of whether the page is distillable.
void setDelegate(content::WebContents* web_contents,
                 DistillabilityDelegate delegate);
}

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABLE_PAGE_UTILS_H_
