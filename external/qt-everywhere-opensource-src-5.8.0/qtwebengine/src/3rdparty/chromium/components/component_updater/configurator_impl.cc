// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/configurator_impl.h"

#include <stddef.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/component_updater/component_updater_url_constants.h"
#include "components/update_client/utils.h"
#include "components/version_info/version_info.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif  // OS_WIN

namespace component_updater {

namespace {
// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

// Debug values you can pass to --component-updater=value1,value2.
// Speed up component checking.
const char kSwitchFastUpdate[] = "fast-update";

// Add "testrequest=1" attribute to the update check request.
const char kSwitchRequestParam[] = "test-request";

// Disables pings. Pings are the requests sent to the update server that report
// the success or the failure of component install or update attempts.
extern const char kSwitchDisablePings[] = "disable-pings";

// Sets the URL for updates.
const char kSwitchUrlSource[] = "url-source";

// Disables differential updates.
const char kSwitchDisableDeltaUpdates[] = "disable-delta-updates";

#if defined(OS_WIN)
// Disables background downloads.
const char kSwitchDisableBackgroundDownloads[] = "disable-background-downloads";
#endif  // defined(OS_WIN)

// Returns true if and only if |test| is contained in |vec|.
bool HasSwitchValue(const std::vector<std::string>& vec, const char* test) {
  if (vec.empty())
    return 0;
  return (std::find(vec.begin(), vec.end(), test) != vec.end());
}

// If there is an element of |vec| of the form |test|=.*, returns the right-
// hand side of that assignment. Otherwise, returns an empty string.
// The right-hand side may contain additional '=' characters, allowing for
// further nesting of switch arguments.
std::string GetSwitchArgument(const std::vector<std::string>& vec,
                              const char* test) {
  if (vec.empty())
    return std::string();
  for (std::vector<std::string>::const_iterator it = vec.begin();
       it != vec.end(); ++it) {
    const std::size_t found = it->find("=");
    if (found != std::string::npos) {
      if (it->substr(0, found) == test) {
        return it->substr(found + 1);
      }
    }
  }
  return std::string();
}

}  // namespace

ConfiguratorImpl::ConfiguratorImpl(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* url_request_getter,
    bool require_encryption)
    : url_request_getter_(url_request_getter),
      fast_update_(false),
      pings_enabled_(false),
      deltas_enabled_(false),
      background_downloads_enabled_(false),
      require_encryption_(require_encryption) {
  // Parse comma-delimited debug flags.
  std::vector<std::string> switch_values = base::SplitString(
      cmdline->GetSwitchValueASCII(switches::kComponentUpdater), ",",
      base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  fast_update_ = HasSwitchValue(switch_values, kSwitchFastUpdate);
  pings_enabled_ = !HasSwitchValue(switch_values, kSwitchDisablePings);
  deltas_enabled_ = !HasSwitchValue(switch_values, kSwitchDisableDeltaUpdates);

#if defined(OS_WIN)
  background_downloads_enabled_ =
      !HasSwitchValue(switch_values, kSwitchDisableBackgroundDownloads);
#else
  background_downloads_enabled_ = false;
#endif

  const std::string switch_url_source =
      GetSwitchArgument(switch_values, kSwitchUrlSource);
  if (!switch_url_source.empty()) {
    url_source_override_ = GURL(switch_url_source);
    DCHECK(url_source_override_.is_valid());
  }

  if (HasSwitchValue(switch_values, kSwitchRequestParam))
    extra_info_ += "testrequest=\"1\"";
}

ConfiguratorImpl::~ConfiguratorImpl() {}

int ConfiguratorImpl::InitialDelay() const {
  return fast_update_ ? 10 : (6 * kDelayOneMinute);
}

int ConfiguratorImpl::NextCheckDelay() const {
  return fast_update_ ? 60 : (6 * kDelayOneHour);
}

int ConfiguratorImpl::StepDelay() const {
  return fast_update_ ? 1 : 1;
}

int ConfiguratorImpl::OnDemandDelay() const {
  return fast_update_ ? 2 : (30 * kDelayOneMinute);
}

int ConfiguratorImpl::UpdateDelay() const {
  return fast_update_ ? 10 : (15 * kDelayOneMinute);
}

std::vector<GURL> ConfiguratorImpl::UpdateUrl() const {
  std::vector<GURL> urls;
  if (url_source_override_.is_valid()) {
    urls.push_back(GURL(url_source_override_));
    return urls;
  }

  urls.push_back(GURL(kUpdaterDefaultUrl));
  urls.push_back(GURL(kUpdaterFallbackUrl));
  if (require_encryption_)
    update_client::RemoveUnsecureUrls(&urls);

  return urls;
}

std::vector<GURL> ConfiguratorImpl::PingUrl() const {
  return pings_enabled_ ? UpdateUrl() : std::vector<GURL>();
}

base::Version ConfiguratorImpl::GetBrowserVersion() const {
  return base::Version(version_info::GetVersionNumber());
}

std::string ConfiguratorImpl::GetOSLongName() const {
  return version_info::GetOSType();
}

std::string ConfiguratorImpl::ExtraRequestParams() const {
  return extra_info_;
}

std::string ConfiguratorImpl::GetDownloadPreference() const {
  return std::string();
}

net::URLRequestContextGetter* ConfiguratorImpl::RequestContext() const {
  return url_request_getter_;
}

bool ConfiguratorImpl::DeltasEnabled() const {
  return deltas_enabled_;
}

bool ConfiguratorImpl::UseBackgroundDownloader() const {
  return background_downloads_enabled_;
}

bool ConfiguratorImpl::UseCupSigning() const {
  return true;
}

}  // namespace component_updater
