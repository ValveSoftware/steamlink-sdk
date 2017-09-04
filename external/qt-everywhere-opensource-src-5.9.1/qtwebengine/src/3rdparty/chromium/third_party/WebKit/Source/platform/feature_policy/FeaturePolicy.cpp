// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/feature_policy/FeaturePolicy.h"

#include "platform/json/JSONValues.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

// Given a string name, return the matching feature struct, or nullptr if it is
// not the name of a policy-controlled feature.
const FeaturePolicy::Feature* featureForName(
    const String& featureName,
    FeaturePolicy::FeatureList& features) {
  for (const FeaturePolicy::Feature* feature : features) {
    if (featureName == feature->featureName)
      return feature;
  }
  return nullptr;
}

// Converts a list of JSON feature policy items into a mapping of features to
// whitelists. For future compatibility, unrecognized features are simply
// ignored, as are unparseable origins. Any errors in the input will cause an
// error message appended to |messages|.
HashMap<const FeaturePolicy::Feature*,
        std::unique_ptr<FeaturePolicy::Whitelist>>
parseFeaturePolicyFromJson(std::unique_ptr<JSONArray> policyItems,
                           RefPtr<SecurityOrigin> origin,
                           FeaturePolicy::FeatureList& features,
                           Vector<String>& messages) {
  HashMap<const FeaturePolicy::Feature*,
          std::unique_ptr<FeaturePolicy::Whitelist>>
      whitelists;

  for (size_t i = 0; i < policyItems->size(); ++i) {
    JSONObject* item = JSONObject::cast(policyItems->at(i));
    if (!item) {
      messages.append("Policy is not an object");
      continue;  // Array element is not an object; skip
    }

    for (size_t j = 0; j < item->size(); ++j) {
      JSONObject::Entry entry = item->at(j);
      String featureName = entry.first;
      JSONArray* targets = JSONArray::cast(entry.second);
      if (!targets) {
        messages.append("Whitelist is not an array of strings.");
        continue;
      }

      const FeaturePolicy::Feature* feature =
          featureForName(featureName, features);
      if (!feature)
        continue;  // Feature is not recognized; skip

      std::unique_ptr<FeaturePolicy::Whitelist> whitelist(
          new FeaturePolicy::Whitelist);
      String targetString;
      for (size_t j = 0; j < targets->size(); ++j) {
        if (targets->at(j)->asString(&targetString)) {
          if (equalIgnoringCase(targetString, "self")) {
            whitelist->add(origin);
          } else if (targetString == "*") {
            whitelist->addAll();
          } else {
            KURL originUrl = KURL(KURL(), targetString);
            if (originUrl.isValid()) {
              whitelist->add(SecurityOrigin::create(originUrl));
            }
          }
        } else {
          messages.append("Whitelist is not an array of strings.");
        }
      }
      whitelists.set(feature, std::move(whitelist));
    }
  }
  return whitelists;
}

}  // namespace

// Definitions of all features controlled by Feature Policy should appear here.
const FeaturePolicy::Feature kDocumentCookie{
    "cookie", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kDocumentDomain{
    "domain", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kDocumentWrite{
    "docwrite", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kGeolocationFeature{
    "geolocation", FeaturePolicy::FeatureDefault::EnableForSelf};
const FeaturePolicy::Feature kMidiFeature{
    "midi", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kNotificationsFeature{
    "notifications", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kPaymentFeature{
    "payment", FeaturePolicy::FeatureDefault::EnableForSelf};
const FeaturePolicy::Feature kPushFeature{
    "push", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kSyncScript{
    "sync-script", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kSyncXHR{
    "sync-xhr", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kUsermedia{
    "usermedia", FeaturePolicy::FeatureDefault::EnableForAll};
const FeaturePolicy::Feature kVibrateFeature{
    "vibrate", FeaturePolicy::FeatureDefault::EnableForSelf};
const FeaturePolicy::Feature kWebRTC{
    "webrtc", FeaturePolicy::FeatureDefault::EnableForAll};

FeaturePolicy::Whitelist::Whitelist() : m_matchesAllOrigins(false) {}

void FeaturePolicy::Whitelist::addAll() {
  m_matchesAllOrigins = true;
}

void FeaturePolicy::Whitelist::add(RefPtr<SecurityOrigin> origin) {
  m_origins.append(std::move(origin));
}

bool FeaturePolicy::Whitelist::contains(const SecurityOrigin& origin) const {
  if (m_matchesAllOrigins)
    return true;
  for (const auto& targetOrigin : m_origins) {
    if (targetOrigin->isSameSchemeHostPortAndSuborigin(&origin))
      return true;
  }
  return false;
}

String FeaturePolicy::Whitelist::toString() {
  StringBuilder sb;
  sb.append("[");
  if (m_matchesAllOrigins) {
    sb.append("*");
  } else {
    for (size_t i = 0; i < m_origins.size(); ++i) {
      if (i > 0) {
        sb.append(", ");
      }
      sb.append(m_origins[i]->toString());
    }
  }
  sb.append("]");
  return sb.toString();
}

// static
const FeaturePolicy::FeatureList& FeaturePolicy::getDefaultFeatureList() {
  DEFINE_STATIC_LOCAL(
      Vector<const FeaturePolicy::Feature*>, defaultFeatureList,
      ({&kDocumentCookie, &kDocumentDomain, &kDocumentWrite,
        &kGeolocationFeature, &kMidiFeature, &kNotificationsFeature,
        &kPaymentFeature, &kPushFeature, &kSyncScript, &kSyncXHR, &kUsermedia,
        &kVibrateFeature, &kWebRTC}));
  return defaultFeatureList;
}

// static
std::unique_ptr<FeaturePolicy> FeaturePolicy::createFromParentPolicy(
    const FeaturePolicy* parent,
    RefPtr<SecurityOrigin> currentOrigin,
    FeaturePolicy::FeatureList& features) {
  DCHECK(currentOrigin);
  std::unique_ptr<FeaturePolicy> newPolicy =
      wrapUnique(new FeaturePolicy(currentOrigin, features));
  for (const FeaturePolicy::Feature* feature : features) {
    if (!parent ||
        parent->isFeatureEnabledForOrigin(*feature, *currentOrigin)) {
      newPolicy->m_inheritedFeatures.set(feature, true);
    } else {
      newPolicy->m_inheritedFeatures.set(feature, false);
    }
  }
  return newPolicy;
}

// static
std::unique_ptr<FeaturePolicy> FeaturePolicy::createFromParentPolicy(
    const FeaturePolicy* parent,
    RefPtr<SecurityOrigin> currentOrigin) {
  return createFromParentPolicy(parent, std::move(currentOrigin),
                                getDefaultFeatureList());
}

void FeaturePolicy::setHeaderPolicy(const String& policy,
                                    Vector<String>& messages) {
  DCHECK(m_headerWhitelists.isEmpty());
  std::unique_ptr<JSONArray> policyJSON = parseJSONHeader(policy);
  if (!policyJSON) {
    messages.append("Unable to parse header");
    return;
  }
  m_headerWhitelists = parseFeaturePolicyFromJson(
      std::move(policyJSON), m_origin, m_features, messages);
}

bool FeaturePolicy::isFeatureEnabledForOrigin(
    const FeaturePolicy::Feature& feature,
    const SecurityOrigin& origin) const {
  DCHECK(m_inheritedFeatures.contains(&feature));
  if (!m_inheritedFeatures.get(&feature)) {
    return false;
  }
  if (m_headerWhitelists.contains(&feature)) {
    return m_headerWhitelists.get(&feature)->contains(origin);
  }
  if (feature.defaultPolicy == FeaturePolicy::FeatureDefault::EnableForAll) {
    return true;
  }
  if (feature.defaultPolicy == FeaturePolicy::FeatureDefault::EnableForSelf) {
    return m_origin->isSameSchemeHostPortAndSuborigin(&origin);
  }
  return false;
}

bool FeaturePolicy::isFeatureEnabled(
    const FeaturePolicy::Feature& feature) const {
  DCHECK(m_origin);
  return isFeatureEnabledForOrigin(feature, *m_origin);
}

FeaturePolicy::FeaturePolicy(RefPtr<SecurityOrigin> currentOrigin,
                             FeaturePolicy::FeatureList& features)
    : m_origin(std::move(currentOrigin)), m_features(features) {}

String FeaturePolicy::toString() {
  StringBuilder sb;
  sb.append("Feature Policy for frame in origin: ");
  sb.append(m_origin->toString());
  sb.append("\n");
  sb.append("Inherited features:\n");
  for (const auto& inheritedFeature : m_inheritedFeatures) {
    sb.append("  ");
    sb.append(inheritedFeature.key->featureName);
    sb.append(": ");
    sb.append(inheritedFeature.value ? "true" : "false");
    sb.append("\n");
  }
  sb.append("Header whitelists:\n");
  for (const auto& whitelist : m_headerWhitelists) {
    sb.append("  ");
    sb.append(whitelist.key->featureName);
    sb.append(": ");
    sb.append(whitelist.value->toString());
    sb.append("\n");
  }
  return sb.toString();
}

}  // namespace blink
