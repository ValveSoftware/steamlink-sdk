// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_EXTERNAL_FEEDBACK_REPORTER_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_EXTERNAL_FEEDBACK_REPORTER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace dom_distiller {

// ExternalFeedbackReporter handles reporting distillation quality through an
// external source.
class ExternalFeedbackReporter {
 public:
  ExternalFeedbackReporter() {}
  virtual ~ExternalFeedbackReporter() {}

  // Start an external form to record user feedback.
  virtual void ReportExternalFeedback(content::WebContents* web_contents,
                                      const GURL& url,
                                      const bool good) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalFeedbackReporter);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_EXTERNAL_FEEDBACK_REPORTER_H_
