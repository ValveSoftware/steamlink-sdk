// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distillable_page_utils.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/dom_distiller/content/browser/distillability_driver.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/core/distillable_page_detector.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/dom_distiller/core/page_features.h"
#include "content/public/browser/render_frame_host.h"
#include "grit/components_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace dom_distiller {
namespace {
void OnOGArticleJsResult(base::Callback<void(bool)> callback,
                         const base::Value* result) {
  bool success = false;
  if (result) {
    result->GetAsBoolean(&success);
  }
  callback.Run(success);
}

void OnExtractFeaturesJsResult(const DistillablePageDetector* detector,
                               base::Callback<void(bool)> callback,
                               const base::Value* result) {
  callback.Run(detector->Classify(CalculateDerivedFeaturesFromJSON(result)));
}
}  // namespace

void IsOpenGraphArticle(content::WebContents* web_contents,
                        base::Callback<void(bool)> callback) {
  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  if (!main_frame) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
    return;
  }
  std::string og_article_js = ResourceBundle::GetSharedInstance()
                                  .GetRawDataResource(IDR_IS_DISTILLABLE_JS)
                                  .as_string();
  RunIsolatedJavaScript(main_frame, og_article_js,
                        base::Bind(OnOGArticleJsResult, callback));
}

void IsDistillablePage(content::WebContents* web_contents,
                       bool is_mobile_optimized,
                       base::Callback<void(bool)> callback) {
  switch (GetDistillerHeuristicsType()) {
    case DistillerHeuristicsType::ALWAYS_TRUE:
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                    base::Bind(callback, true));
      return;
    case DistillerHeuristicsType::OG_ARTICLE:
      IsOpenGraphArticle(web_contents, callback);
      return;
    case DistillerHeuristicsType::ADABOOST_MODEL:
      // The adaboost model is only applied to non-mobile pages.
      if (is_mobile_optimized) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::Bind(callback, false));
        return;
      }
      IsDistillablePageForDetector(
          web_contents, DistillablePageDetector::GetDefault(), callback);
      return;
    case DistillerHeuristicsType::NONE:
    default:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(callback, false));
      return;
  }
}

void IsDistillablePageForDetector(content::WebContents* web_contents,
                                  const DistillablePageDetector* detector,
                                  base::Callback<void(bool)> callback) {
  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  if (!main_frame) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
    return;
  }
  std::string extract_features_js =
      ResourceBundle::GetSharedInstance()
          .GetRawDataResource(IDR_EXTRACT_PAGE_FEATURES_JS)
          .as_string();
  RunIsolatedJavaScript(
      main_frame, extract_features_js,
      base::Bind(OnExtractFeaturesJsResult, detector, callback));
}

void setDelegate(content::WebContents* web_contents,
                 DistillabilityDelegate delegate) {
  CHECK(web_contents);
  DistillabilityDriver::CreateForWebContents(web_contents);

  DistillabilityDriver *driver =
      DistillabilityDriver::FromWebContents(web_contents);
  CHECK(driver);
  driver->SetDelegate(delegate);
}

}  // namespace dom_distiller
