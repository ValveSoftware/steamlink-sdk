// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_ASSOCIATED_DATA_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_ASSOCIATED_DATA_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/metrics/field_trial.h"
#include "components/variations/active_field_trials.h"

// This file provides various helpers that extend the functionality around
// base::FieldTrial.
//
// This includes several simple APIs to handle getting and setting additional
// data related to Chrome variations, such as parameters and Google variation
// IDs. These APIs are meant to extend the base::FieldTrial APIs to offer extra
// functionality that is not offered by the simpler base::FieldTrial APIs.
//
// The AssociateGoogleVariationID and AssociateVariationParams functions are
// generally meant to be called by the VariationsService based on server-side
// variation configs, but may also be used for client-only field trials by
// invoking them directly after appending all the groups to a FieldTrial.
//
// Experiment code can then use the getter APIs to retrieve variation parameters
// or IDs:
//
//  std::map<std::string, std::string> params;
//  if (GetVariationParams("trial", &params)) {
//    // use |params|
//  }
//
//  std::string value = GetVariationParamValue("trial", "param_x");
//  // use |value|, which will be "" if it does not exist
//
// VariationID id = GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, "trial",
//                                       "group1");
// if (id != variations::kEmptyID) {
//   // use |id|
// }

namespace base {
struct Feature;
}

namespace variations {

typedef int VariationID;
class VariationsHttpHeaderProvider;

const VariationID EMPTY_ID = 0;

// A key into the Associate/Get methods for VariationIDs. This is used to create
// separate ID associations for separate parties interested in VariationIDs.
enum IDCollectionKey {
  // This collection is used by Google web properties, transmitted through the
  // X-Client-Data header.
  GOOGLE_WEB_PROPERTIES,
  // This collection is used by Google web properties for signed in users only,
  // transmitted through the X-Client-Data header.
  GOOGLE_WEB_PROPERTIES_SIGNED_IN,
  // This collection is used by Google web properties for IDs that trigger
  // server side experimental behavior, transmitted through the
  // X-Client-Data header.
  GOOGLE_WEB_PROPERTIES_TRIGGER,
  // This collection is used by Chrome Sync services, transmitted through the
  // Chrome Sync experiment labels.
  CHROME_SYNC_SERVICE,
  // The total count of collections.
  ID_COLLECTION_COUNT,
};

// Associate a variations::VariationID value with a FieldTrial group for
// collection |key|. If an id was previously set for |trial_name| and
// |group_name|, this does nothing. The group is denoted by |trial_name| and
// |group_name|. This must be called whenever a FieldTrial is prepared (create
// the trial and append groups) and needs to have a variations::VariationID
// associated with it so Google servers can recognize the FieldTrial.
// Thread safe.
void AssociateGoogleVariationID(IDCollectionKey key,
                                const std::string& trial_name,
                                const std::string& group_name,
                                VariationID id);

// As above, but overwrites any previously set id. Thread safe.
void AssociateGoogleVariationIDForce(IDCollectionKey key,
                                     const std::string& trial_name,
                                     const std::string& group_name,
                                     VariationID id);

// As above, but takes an ActiveGroupId hash pair, rather than the string names.
void AssociateGoogleVariationIDForceHashes(IDCollectionKey key,
                                           const ActiveGroupId& active_group,
                                           VariationID id);

// Retrieve the variations::VariationID associated with a FieldTrial group for
// collection |key|. The group is denoted by |trial_name| and |group_name|.
// This will return variations::kEmptyID if there is currently no associated ID
// for the named group. This API can be nicely combined with
// FieldTrial::GetActiveFieldTrialGroups() to enumerate the variation IDs for
// all active FieldTrial groups. Thread safe.
VariationID GetGoogleVariationID(IDCollectionKey key,
                                 const std::string& trial_name,
                                 const std::string& group_name);

// Same as GetGoogleVariationID(), but takes in a hashed |active_group| rather
// than the string trial and group name.
VariationID GetGoogleVariationIDFromHashes(IDCollectionKey key,
                                           const ActiveGroupId& active_group);

// Associates the specified set of key-value |params| with the variation
// specified by |trial_name| and |group_name|. Fails and returns false if the
// specified variation already has params associated with it or the field trial
// is already active (group() has been called on it). Thread safe.
bool AssociateVariationParams(const std::string& trial_name,
                              const std::string& group_name,
                              const std::map<std::string, std::string>& params);

// Retrieves the set of key-value |params| for the variation associated with
// the specified field trial, based on its selected group. If the field trial
// does not exist or its selected group does not have any parameters associated
// with it, returns false and does not modify |params|. Calling this function
// will result in the field trial being marked as active if found (i.e. group()
// will be called on it), if it wasn't already. Currently, this information is
// only available from the browser process. Thread safe.
bool GetVariationParams(const std::string& trial_name,
                        std::map<std::string, std::string>* params);

// Retrieves the set of key-value |params| for the variation associated with the
// specified |feature|. A feature is associated with at most one variation,
// through the variation's associated field trial, and selected group. See
// base/feature_list.h for more information on features. If the feature is not
// enabled, or if there's no associated variation params, returns false and does
// not modify |params|. Calling this function will result in the associated
// field trial being marked as active if found (i.e. group() will be called on
// it), if it wasn't already. Currently, this information is only available from
// the browser process. Thread safe.
bool GetVariationParamsByFeature(const base::Feature& feature,
                                 std::map<std::string, std::string>* params);

// Retrieves a specific parameter value corresponding to |param_name| for the
// variation associated with the specified field trial, based on its selected
// group. If the field trial does not exist or the specified parameter does not
// exist, returns an empty string. Calling this function will result in the
// field trial being marked as active if found (i.e. group() will be called on
// it), if it wasn't already. Currently, this information is only available from
// the browser process. Thread safe.
std::string GetVariationParamValue(const std::string& trial_name,
                                   const std::string& param_name);

// Retrieves a specific parameter value corresponding to |param_name| for the
// variation associated with the specified |feature|. A feature is associated
// with at most one variation, through the variation's associated field trial,
// and selected group. See base/feature_list.h for more information on
// features. If the feature is not enabled, or the specified parameter does not
// exist, returns an empty string.Calling this function will result in the
// associated field trial being marked as active if found (i.e. group() will be
// called on it), if it wasn't already. Currently, this information is only
// available from the browser process. Thread safe.
std::string GetVariationParamValueByFeature(const base::Feature& feature,
                                            const std::string& param_name);

// Expose some functions for testing.
namespace testing {

// Use this class as a member in your test class to set variation params for
// your tests. You can directly set the parameters in the constructor (if they
// are used by other members upon construction). You can change them later
// arbitrarily many times using the SetVariationParams function. Internally, it
// creates a FieldTrialList as a member. It works well for multiple tests of a
// given test class, as it clears the parameters when this class is destructed.
// Note that it clears all parameters (not just those registered here).
class VariationParamsManager {
 public:
  VariationParamsManager(const std::string& trial_name,
                         const std::map<std::string, std::string>& params);
  ~VariationParamsManager();

  // Associates |params| with the given |trial_name|. It creates a new group,
  // used only for testing. Between two calls of this function,
  // ClearAllVariationParams() has to be called.
  void SetVariationParams(const std::string& trial_name,
                          const std::map<std::string, std::string>& params);

 private:
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(VariationParamsManager);
};

// Clears all of the mapped associations.
void ClearAllVariationIDs();

// Clears all of the associated params.
void ClearAllVariationParams();

}  // namespace testing

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_ASSOCIATED_DATA_H_
