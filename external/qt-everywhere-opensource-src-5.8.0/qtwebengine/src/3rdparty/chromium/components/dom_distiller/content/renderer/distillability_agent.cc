// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"

#include "components/dom_distiller/content/common/distillability_service.mojom.h"
#include "components/dom_distiller/content/renderer/distillability_agent.h"
#include "components/dom_distiller/core/distillable_page_detector.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/dom_distiller/core/page_features.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/renderer/render_frame.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebDistillability.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace dom_distiller {

using namespace blink;

namespace {

const char* const kBlacklist[] = {
  "www.reddit.com"
};

enum RejectionBuckets {
  NOT_ARTICLE = 0,
  MOBILE_FRIENDLY,
  BLACKLISTED,
  TOO_SHORT,
  NOT_REJECTED,
  REJECTION_BUCKET_BOUNDARY
};

// Returns whether it is necessary to send updates back to the browser.
// The number of updates can be from 0 to 2. See the tests in
// "distillable_page_utils_browsertest.cc".
// Most heuristics types only require one update after parsing.
// Adaboost is the only one doing the second update, which is after loading.
bool NeedToUpdate(bool is_loaded) {
  switch (GetDistillerHeuristicsType()) {
    case DistillerHeuristicsType::ALWAYS_TRUE:
      return !is_loaded;
    case DistillerHeuristicsType::OG_ARTICLE:
      return !is_loaded;
    case DistillerHeuristicsType::ADABOOST_MODEL:
      return true;
    case DistillerHeuristicsType::NONE:
    default:
      return false;
  }
}

// Returns whether this update is the last one for the page.
bool IsLast(bool is_loaded) {
  if (GetDistillerHeuristicsType() == DistillerHeuristicsType::ADABOOST_MODEL)
    return is_loaded;

  return true;
}

bool IsBlacklisted(const GURL& url) {
  for (size_t i = 0; i < arraysize(kBlacklist); ++i) {
    if (base::LowerCaseEqualsASCII(url.host(), kBlacklist[i])) {
      return true;
    }
  }
  return false;
}

bool IsDistillablePageAdaboost(WebDocument& doc,
                               const DistillablePageDetector* detector,
                               const DistillablePageDetector* long_page,
                               bool is_last) {
  WebDistillabilityFeatures features = doc.distillabilityFeatures();
  GURL parsed_url(doc.url());
  if (!parsed_url.is_valid()) {
    return false;
  }
  std::vector<double> derived = CalculateDerivedFeatures(
    features.openGraph,
    parsed_url,
    features.elementCount,
    features.anchorCount,
    features.formCount,
    features.mozScore,
    features.mozScoreAllSqrt,
    features.mozScoreAllLinear
  );
  double score = detector->Score(derived) - detector->GetThreshold();
  double long_score = long_page->Score(derived) - long_page->GetThreshold();
  bool distillable = score > 0;
  bool long_article = long_score > 0;
  bool blacklisted = IsBlacklisted(parsed_url);

  if (!features.isMobileFriendly) {
    int score_int = std::round(score * 100);
    if (score > 0) {
      UMA_HISTOGRAM_COUNTS_1000("DomDistiller.DistillabilityScoreNMF.Positive",
          score_int);
    } else {
      UMA_HISTOGRAM_COUNTS_1000("DomDistiller.DistillabilityScoreNMF.Negative",
          -score_int);
    }
    if (distillable) {
      // The long-article model is trained with pages that are
      // non-mobile-friendly, and distillable (deemed by the first model), so
      // only record on that type of pages.
      int long_score_int = std::round(long_score * 100);
      if (long_score > 0) {
        UMA_HISTOGRAM_COUNTS_1000("DomDistiller.LongArticleScoreNMF.Positive",
            long_score_int);
      } else {
        UMA_HISTOGRAM_COUNTS_1000("DomDistiller.LongArticleScoreNMF.Negative",
            -long_score_int);
      }
    }
  }

  int bucket = static_cast<unsigned>(features.isMobileFriendly) |
      (static_cast<unsigned>(distillable) << 1);
  if (is_last) {
    UMA_HISTOGRAM_ENUMERATION("DomDistiller.PageDistillableAfterLoading",
        bucket, 4);
  } else {
    UMA_HISTOGRAM_ENUMERATION("DomDistiller.PageDistillableAfterParsing",
        bucket, 4);
    if (!distillable) {
      UMA_HISTOGRAM_ENUMERATION("DomDistiller.DistillabilityRejection",
          NOT_ARTICLE, REJECTION_BUCKET_BOUNDARY);
    } else if (features.isMobileFriendly) {
      UMA_HISTOGRAM_ENUMERATION("DomDistiller.DistillabilityRejection",
          MOBILE_FRIENDLY, REJECTION_BUCKET_BOUNDARY);
    } else if (blacklisted) {
      UMA_HISTOGRAM_ENUMERATION("DomDistiller.DistillabilityRejection",
          BLACKLISTED, REJECTION_BUCKET_BOUNDARY);
    } else if (!long_article) {
      UMA_HISTOGRAM_ENUMERATION("DomDistiller.DistillabilityRejection",
          TOO_SHORT, REJECTION_BUCKET_BOUNDARY);
    } else {
      UMA_HISTOGRAM_ENUMERATION("DomDistiller.DistillabilityRejection",
          NOT_REJECTED, REJECTION_BUCKET_BOUNDARY);
    }
  }

  if (blacklisted) {
    return false;
  }
  if (features.isMobileFriendly) {
    return false;
  }
  return distillable && long_article;
}

bool IsDistillablePage(WebDocument& doc, bool is_last) {
  switch (GetDistillerHeuristicsType()) {
    case DistillerHeuristicsType::ALWAYS_TRUE:
      return true;
    case DistillerHeuristicsType::OG_ARTICLE:
      return doc.distillabilityFeatures().openGraph;
    case DistillerHeuristicsType::ADABOOST_MODEL:
      return IsDistillablePageAdaboost(doc,
          DistillablePageDetector::GetNewModel(),
          DistillablePageDetector::GetLongPageModel(), is_last);
    case DistillerHeuristicsType::NONE:
    default:
      return false;
  }
}

}  // namespace

DistillabilityAgent::DistillabilityAgent(
    content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
}

void DistillabilityAgent::DidMeaningfulLayout(
    WebMeaningfulLayout layout_type) {
  if (layout_type != WebMeaningfulLayout::FinishedParsing &&
      layout_type != WebMeaningfulLayout::FinishedLoading) {
    return;
  }

  DCHECK(render_frame());
  if (!render_frame()->IsMainFrame()) return;
  DCHECK(render_frame()->GetWebFrame());
  WebDocument doc = render_frame()->GetWebFrame()->document();
  if (doc.isNull() || doc.body().isNull()) return;
  if (!url_utils::IsUrlDistillable(doc.url())) return;

  bool is_loaded = layout_type == WebMeaningfulLayout::FinishedLoading;
  if (!NeedToUpdate(is_loaded)) return;

  bool is_last = IsLast(is_loaded);
  // Connect to Mojo service on browser to notify page distillability.
  mojom::DistillabilityServicePtr distillability_service;
  render_frame()->GetRemoteInterfaces()->GetInterface(
      &distillability_service);
  DCHECK(distillability_service);
  distillability_service->NotifyIsDistillable(
      IsDistillablePage(doc, is_last), is_last);
}

DistillabilityAgent::~DistillabilityAgent() {}

void DistillabilityAgent::OnDestruct() {
  delete this;
}

}  // namespace dom_distiller
