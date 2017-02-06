// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/ruleset_service.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_proxy.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/core/browser/ruleset_distributor.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"

namespace subresource_filter {

// Constant definitions and helper functions ---------------------------------

namespace {

// The current binary format version of the serialized ruleset.
const int kCurrentRulesetFormatVersion = 10;

// Names of the preferences storing the most recent ruleset version that
// was successfully stored to disk.
const char kSubresourceFilterRulesetContentVersion[] =
    "subresource_filter.ruleset_version.content";
const char kSubresourceFilterRulesetFormatVersion[] =
    "subresource_filter.ruleset_version.format";

base::FilePath GetSubdirectoryPathForVersion(const base::FilePath& base_dir,
                                             const RulesetVersion& version) {
  return base_dir.AppendASCII(version.AsString());
}

base::FilePath GetRulesetDataFilePath(const base::FilePath& version_directory) {
  return version_directory.Append(kRulesetDataFileName);
}

// Returns a duplicate of the file wrapped by |file_proxy|. Temporary workaround
// because base::FileProxy exposes neither the underlying file nor a Duplicate()
// method.
base::File DuplicateHandle(base::FileProxy* file_proxy) {
  DCHECK(file_proxy);
  DCHECK(file_proxy->IsValid());
  base::File file = file_proxy->TakeFile();
  base::File duplicate = file.Duplicate();
  file_proxy->SetFile(std::move(file));
  return duplicate;
}

}  // namespace

// RulesetVersion ------------------------------------------------------------

RulesetVersion::RulesetVersion() = default;
RulesetVersion::RulesetVersion(const std::string& content_version,
                               int format_version)
    : content_version(content_version), format_version(format_version) {}
RulesetVersion::~RulesetVersion() = default;
RulesetVersion& RulesetVersion::operator=(const RulesetVersion&) = default;

// static
void RulesetVersion::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kSubresourceFilterRulesetContentVersion,
                               std::string());
  registry->RegisterIntegerPref(kSubresourceFilterRulesetFormatVersion, 0);
}

// static
int RulesetVersion::CurrentFormatVersion() {
  return kCurrentRulesetFormatVersion;
}

void RulesetVersion::ReadFromPrefs(PrefService* local_state) {
  format_version =
      local_state->GetInteger(kSubresourceFilterRulesetFormatVersion);
  content_version =
      local_state->GetString(kSubresourceFilterRulesetContentVersion);
}

bool RulesetVersion::IsValid() const {
  return format_version != 0 && !content_version.empty();
}

bool RulesetVersion::IsCurrentFormatVersion() const {
  return format_version == CurrentFormatVersion();
}

void RulesetVersion::SaveToPrefs(PrefService* local_state) const {
  DCHECK(IsValid());
  local_state->SetInteger(kSubresourceFilterRulesetFormatVersion,
                          format_version);
  local_state->SetString(kSubresourceFilterRulesetContentVersion,
                         content_version);
}

std::string RulesetVersion::AsString() const {
  DCHECK(IsValid());
  return base::IntToString(format_version) + "_" + content_version;
}

// RulesetService ------------------------------------------------------------

RulesetService::RulesetService(
    PrefService* local_state,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::FilePath& base_dir)
    : local_state_(local_state),
      blocking_task_runner_(blocking_task_runner),
      base_dir_(base_dir) {
  DCHECK_NE(local_state_->GetInitializationStatus(),
            PrefService::INITIALIZATION_STATUS_WAITING);
  RulesetVersion version_on_disk;
  version_on_disk.ReadFromPrefs(local_state_);
  if (version_on_disk.IsCurrentFormatVersion())
    OpenAndPublishRuleset(version_on_disk);
}

RulesetService::~RulesetService() {}

void RulesetService::StoreAndPublishUpdatedRuleset(
    std::vector<uint8_t> ruleset_data,
    const std::string& new_content_version) {
  // TODO(engedy): The |format_version| corresponds to the output of the
  // indexing step that will ultimately be performed here.
  RulesetVersion new_version(new_content_version,
                             RulesetVersion::CurrentFormatVersion());
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&WriteRuleset, base_dir_, new_version,
                 std::move(ruleset_data)),
      base::Bind(&RulesetService::OnWrittenRuleset, AsWeakPtr(), new_version));
}

void RulesetService::RegisterDistributor(
    std::unique_ptr<RulesetDistributor> distributor) {
  if (ruleset_data_ && ruleset_data_->IsValid())
    distributor->PublishNewVersion(DuplicateHandle(ruleset_data_.get()));
  distributors_.push_back(std::move(distributor));
}

void RulesetService::NotifyRulesetVersionAvailable(
    const std::string& rules,
    const base::Version& version) {}

// static
bool RulesetService::WriteRuleset(const base::FilePath& base_dir,
                                  const RulesetVersion& version,
                                  std::vector<uint8_t> data) {
  base::ThreadRestrictions::AssertIOAllowed();
  base::ScopedTempDir scratch_dir;
  if (!scratch_dir.CreateUniqueTempDirUnderPath(base_dir))
    return false;

  static_assert(sizeof(uint8_t) == sizeof(char), "Expected char = byte.");
  const int data_size_in_chars = base::checked_cast<int>(data.size());
  if (base::WriteFile(GetRulesetDataFilePath(scratch_dir.path()),
                      reinterpret_cast<const char*>(data.data()),
                      data_size_in_chars) != data_size_in_chars) {
    return false;
  }

  DCHECK(base::PathExists(base_dir));
  if (base::ReplaceFile(scratch_dir.path(),
                        GetSubdirectoryPathForVersion(base_dir, version),
                        nullptr)) {
    scratch_dir.Take();
    return true;
  }

  return false;
}

void RulesetService::OnWrittenRuleset(const RulesetVersion& version,
                                      bool success) {
  if (!success)
    return;

  version.SaveToPrefs(local_state_);
  OpenAndPublishRuleset(version);
}

void RulesetService::OpenAndPublishRuleset(const RulesetVersion& version) {
  ruleset_data_.reset(new base::FileProxy(blocking_task_runner_.get()));
  // On Windows, open the file with FLAG_SHARE_DELETE to allow deletion while
  // there are handles to it still open. Note that creating a new file at the
  // same path would still be impossible until after the last of those handles
  // is closed.
  ruleset_data_->CreateOrOpen(
      GetRulesetDataFilePath(GetSubdirectoryPathForVersion(base_dir_, version)),
      base::File::FLAG_OPEN | base::File::FLAG_READ |
          base::File::FLAG_SHARE_DELETE,
      base::Bind(&RulesetService::OnOpenedRuleset, AsWeakPtr()));
}

void RulesetService::OnOpenedRuleset(base::File::Error error) {
  // This should not happen unless |base_dir| has been tampered with.
  if (!ruleset_data_ || !ruleset_data_->IsValid())
    return;
  DCHECK_EQ(error, base::File::Error::FILE_OK);
  for (auto& distributor : distributors_)
    distributor->PublishNewVersion(DuplicateHandle(ruleset_data_.get()));
}

}  // namespace subresource_filter
