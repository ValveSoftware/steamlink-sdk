// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FeaturePolicy_h
#define FeaturePolicy_h

#include "platform/PlatformExport.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

#include <memory>

namespace blink {

// Feature Policy is a mechanism for controlling the availability of web
// platform features in a frame, including all embedded frames. It can be used
// to remove features, automatically refuse API permission requests, or modify
// the behaviour of features. (The specific changes which are made depend on the
// feature; see the specification for details).
//
// Policies can be defined in the HTTP header stream, with the |Feature-Policy|
// HTTP header, or can be set by |enable| and |disable| attributes on the iframe
// element which embeds the document.
//
// See https://wicg.github.io/FeaturePolicy/
//
// Key concepts:
//
// Features
// --------
// Features which can be controlled by policy are defined as instances of the
// FeaturePoliicy::Feature struct. The features are referenced by pointer, so
// only a single instance of each feature should be defined. The features which
// are declared in the feature policy specification are all defined in
// |FeaturePolicy.cpp|.
//
// Whitelists
// ----------
// Policies are defined as a mapping of feaure names to whitelists. Whitelists
// are collections of origins, although two special terms can be used when
// declaring them:
//   "self" refers to the orgin of the frame which is declaring the policy.
//   "*" refers to all origins; any origin will match a whitelist which contains
//     it.
//
// Defaults
// --------
// Each defined feature has a default policy, which determines whether the
// feature is available when no policy has been declared, ans determines how the
// feature is inherited across origin boundaries.
//
// If the default policy  is in effect for a frame, then it controls how the
// feature is inherited by any cross-origin iframes embedded by the frame. (See
// the comments below in FeaturePolicy::DefaultPolicy for specifics)
//
// Policy Inheritance
// ------------------
// Policies in effect for a frame are inherited by any child frames it embeds.
// Unless another policy is declared in the child, all same-origin children will
// receive the same set of enables features as the parent frame. Whether or not
// features are inherited by cross-origin iframes without an explicit policy is
// determined by the feature's default policy. (Again, see the comments in
// FeaturePolicy::DefaultPolicy for details)

class PLATFORM_EXPORT FeaturePolicy final {
 public:
  // Represents a collection of origins which make up a whitelist in a feature
  // policy. This collection may be set to match every origin (corresponding to
  // the "*" syntax in the policy string, in which case the contains() method
  // will always return true.
  class Whitelist final {
   public:
    Whitelist();

    // Adds a single origin to the whitelist.
    void add(RefPtr<SecurityOrigin>);

    // Adds all origins to the whitelist.
    void addAll();

    // Returns true if the given origin has been added to the whitelist.
    bool contains(const SecurityOrigin&) const;
    String toString();

   private:
    bool m_matchesAllOrigins;
    Vector<RefPtr<SecurityOrigin>> m_origins;
  };

  // The FeaturePolicy::FeatureDefault enum defines the default enable state for
  // a feature when neither it nor any parent frame have declared an explicit
  // policy. The three possibilities map directly to Feature Policy Whitelist
  // semantics.
  enum class FeatureDefault {
    // Equivalent to []. If this default policy is in effect for a frame, then
    // the feature will not be enabled for that frame or any of its children.
    DisableForAll,

    // Equivalent to ["self"]. If this default policy is in effect for a frame,
    // then the feature will be enabled for that frame, and any same-origin
    // child frames, but not for any cross-origin child frames.
    EnableForSelf,

    // Equivalent to ["*"]. If in effect for a frame, then the feature is
    // enabled for that frame and all of its children.
    EnableForAll
  };

  // The FeaturePolicy::Feature struct is used to define all features under
  // control of Feature Policy. There should only be one instance of this struct
  // for any given feature (declared below).
  struct Feature {
    // The name of the feature, as it should appear in a policy string
    const char* const featureName;

    // Controls whether the feature should be available in the platform by
    // default, in the absence of any declared policy.
    FeatureDefault defaultPolicy;
  };

  using FeatureList = const Vector<const FeaturePolicy::Feature*>;

  static std::unique_ptr<FeaturePolicy> createFromParentPolicy(
      const FeaturePolicy* parent,
      RefPtr<SecurityOrigin>);

  // Sets the declared policy from the Feature-Policy HTTP header. If the header
  // cannot be parsed, errors will be appended to the |messages| vector.
  void setHeaderPolicy(const String&, Vector<String>& messages);

  // Returns whether or not the given feature is enabled by this policy.
  bool isFeatureEnabledForOrigin(const Feature&, const SecurityOrigin&) const;

  // Returns whether or not the given feature is enabled for the frame that owns
  // the policy.
  bool isFeatureEnabled(const Feature&) const;

  // Returns the list of features which can be controlled by Feature Policy.
  static FeatureList& getDefaultFeatureList();

  String toString();

 private:
  friend class FeaturePolicyTest;
  friend class FeaturePolicyInFrameTest;

  FeaturePolicy(RefPtr<SecurityOrigin>, FeatureList& features);

  static std::unique_ptr<FeaturePolicy> createFromParentPolicy(
      const FeaturePolicy* parent,
      RefPtr<SecurityOrigin>,
      FeatureList& features);

  RefPtr<SecurityOrigin> m_origin;

  // Records whether or not each feature was enabled for this frame by its
  // parent frame.
  // TODO(iclelland): Generate, instead of this map, a set of bool flags, one
  // for each feature, as all features are supposed to be represented here.
  HashMap<const Feature*, bool> m_inheritedFeatures;

  // Map of feature names to declared whitelists. Any feature which is missing
  // from this map should use the inherited policy.
  HashMap<const Feature*, std::unique_ptr<Whitelist>> m_headerWhitelists;

  // Contains the set of all features which can be controlled by this policy.
  FeatureList& m_features;

  DISALLOW_COPY_AND_ASSIGN(FeaturePolicy);
};

// Declarations for all features currently under control of the Feature Policy
// mechanism should be placed here.
extern const PLATFORM_EXPORT FeaturePolicy::Feature kDocumentCookie;
extern const PLATFORM_EXPORT FeaturePolicy::Feature kDocumentDomain;
extern const PLATFORM_EXPORT FeaturePolicy::Feature kDocumentWrite;
extern const PLATFORM_EXPORT FeaturePolicy::Feature kGeolocationFeature;
extern const PLATFORM_EXPORT FeaturePolicy::Feature kMidiFeature;
extern const PLATFORM_EXPORT FeaturePolicy::Feature kNotificationsFeature;
extern const PLATFORM_EXPORT FeaturePolicy::Feature kPaymentFeature;
extern const PLATFORM_EXPORT FeaturePolicy::Feature kPushFeature;
extern const PLATFORM_EXPORT FeaturePolicy::Feature kSyncScript;
extern const PLATFORM_EXPORT FeaturePolicy::Feature kSyncXHR;
extern const PLATFORM_EXPORT FeaturePolicy::Feature kUsermedia;
extern const PLATFORM_EXPORT FeaturePolicy::Feature kVibrateFeature;
extern const PLATFORM_EXPORT FeaturePolicy::Feature kWebRTC;

}  // namespace blink

#endif  // FeaturePolicy_h
