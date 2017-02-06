// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_SERVICE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/version.h"

class PrefService;
class PrefRegistrySimple;

namespace base {
class FileProxy;
class SequencedTaskRunner;
}  // namespace base

namespace subresource_filter {

class RulesetDistributor;

// Encapsulates the combination of the binary format version of the ruleset, and
// the version of the ruleset's contents.
struct RulesetVersion {
  RulesetVersion();
  RulesetVersion(const std::string& content_version, int format_version);
  ~RulesetVersion();
  RulesetVersion& operator=(const RulesetVersion&);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  static int CurrentFormatVersion();

  bool IsValid() const;
  bool IsCurrentFormatVersion() const;

  void SaveToPrefs(PrefService* local_state) const;
  void ReadFromPrefs(PrefService* local_state);
  std::string AsString() const;

  std::string content_version;
  int format_version = 0;
};

// Responsible for versioned storage of subresource filtering rules, and for
// supplying the most up-to-date version to the registered RulesetDistributors
// for distribution.
//
// Files corresponding to each version of the ruleset are stored in a separate
// subdirectory inside |ruleset_base_dir| named after the version. The version
// information of the most recent successfully stored ruleset is written into
// |local_state|. The invariant is maintained that the version pointed to by
// preferences will exist on disk at any point in time. All file operations are
// posted to |blocking_task_runner|.
class RulesetService : public base::SupportsWeakPtr<RulesetService> {
 public:
  RulesetService(PrefService* local_state,
                 scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
                 const base::FilePath& base_dir);
  virtual ~RulesetService();

  // Notifies that new data has arrived from the component updater.
  virtual void NotifyRulesetVersionAvailable(const std::string& rules,
                                             const base::Version& version);

  // Persists a new |content_version| of the |ruleset_data|, then, on success,
  // publishes it through the registered distributors. The |ruleset_data| must
  // be a function of the |content_version| in the mathematical sense, i.e.
  // different |ruleset_data| contents should have different |content_versions|.
  //
  // Trying to store a ruleset with the same version for a second time will
  // silenty fail on Windows if the previously stored copy of the rules is
  // still in use. This, however, means that the new rules must have been
  // successfully stored on the first try, otherwise they would not have been
  // published. Therefore, the operation would have been idempotent anyway.
  void StoreAndPublishUpdatedRuleset(std::vector<uint8_t> ruleset_data,
                                     const std::string& content_version);

  // Registers a |distributor| that will be notified each time a new version of
  // the ruleset becomes avalable. The |distributor| will be destroyed along
  // with |this|.
  void RegisterDistributor(std::unique_ptr<RulesetDistributor> distributor);

 private:
  friend class SubresourceFilteringRulesetServiceTest;

  // Writes a new |version| of the ruleset |data| into the correspondingly named
  // subdirectory under |base_dir|, and returns true on success.
  //
  // It will attempt to overwrite the previously stored ruleset with the same
  // version, if any. Doing so is needed in case the earlier write was
  // interrupted, but will silently fail on Windows in case the earlier write
  // was successful and the ruleset is in use. See the comment above regarding
  // why this is not an issue in practice.
  static bool WriteRuleset(const base::FilePath& base_dir,
                           const RulesetVersion& version,
                           std::vector<uint8_t> data);

  void OnWrittenRuleset(const RulesetVersion& version, bool success);
  void OpenAndPublishRuleset(const RulesetVersion& version);
  void OnOpenedRuleset(base::File::Error error);

  PrefService* const local_state_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  const base::FilePath base_dir_;
  std::unique_ptr<base::FileProxy> ruleset_data_;
  std::vector<std::unique_ptr<RulesetDistributor>> distributors_;

  DISALLOW_COPY_AND_ASSIGN(RulesetService);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_SERVICE_H_
