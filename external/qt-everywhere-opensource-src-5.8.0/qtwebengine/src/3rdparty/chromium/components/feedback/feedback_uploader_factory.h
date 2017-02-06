// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_FACTORY_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template<typename T> struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

namespace feedback {

class FeedbackUploader;

// Singleton that owns the FeedbackUploaders and associates them with profiles;
class FeedbackUploaderFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns singleton instance of FeedbackUploaderFactory.
  static FeedbackUploaderFactory* GetInstance();

  // Returns the Feedback Uploader associated with |context|.
  static FeedbackUploader* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<FeedbackUploaderFactory>;

  FeedbackUploaderFactory();
  ~FeedbackUploaderFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(FeedbackUploaderFactory);
};

}  // namespace feedback

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_FACTORY_H_
