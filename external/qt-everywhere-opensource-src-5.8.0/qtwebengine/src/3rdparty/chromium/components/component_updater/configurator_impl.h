// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_CONFIGURATOR_IMPL_H_
#define COMPONENTS_COMPONENT_UPDATER_CONFIGURATOR_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
class Version;
}

namespace net {
class URLRequestContextGetter;
}

namespace component_updater {

// Helper class for the implementations of update_client::Configurator.
// Can be used both on iOS and other platforms.
class ConfiguratorImpl {
 public:
  ConfiguratorImpl(const base::CommandLine* cmdline,
                   net::URLRequestContextGetter* url_request_getter,
                   bool require_encryption);

  ~ConfiguratorImpl();

  // Delay in seconds from calling Start() to the first update check.
  int InitialDelay() const;

  // Delay in seconds to every subsequent update check. 0 means don't check.
  int NextCheckDelay() const;

  // Delay in seconds from each task step. Used to smooth out CPU/IO usage.
  int StepDelay() const;

  // Minimum delta time in seconds before an on-demand check is allowed for the
  // same component.
  int OnDemandDelay() const;

  // The time delay in seconds between applying updates for different
  // components.
  int UpdateDelay() const;

  // The URLs for the update checks. The URLs are tried in order, the first one
  // that succeeds wins.
  std::vector<GURL> UpdateUrl() const;

  // The URLs for pings. Returns an empty vector if and only if pings are
  // disabled. Similarly, these URLs have a fall back behavior too.
  std::vector<GURL> PingUrl() const;

  // Version of the application. Used to compare the component manifests.
  base::Version GetBrowserVersion() const;

  // Returns the OS's long name like "Windows", "Mac OS X", etc.
  std::string GetOSLongName() const;

  // Parameters added to each url request. It can be empty if none are needed.
  // The return string must be safe for insertion as an attribute in an
  // XML element.
  std::string ExtraRequestParams() const;

  // Provides a hint for the server to control the order in which multiple
  // download urls are returned.
  std::string GetDownloadPreference() const;

  // The source of contexts for all the url requests.
  net::URLRequestContextGetter* RequestContext() const;

  // True means that this client can handle delta updates.
  bool DeltasEnabled() const;

  // True means that the background downloader can be used for downloading
  // non on-demand components.
  bool UseBackgroundDownloader() const;

  // True if signing of update checks is enabled.
  bool UseCupSigning() const;

 private:
  net::URLRequestContextGetter* url_request_getter_;
  std::string extra_info_;
  GURL url_source_override_;
  bool fast_update_;
  bool pings_enabled_;
  bool deltas_enabled_;
  bool background_downloads_enabled_;
  bool require_encryption_;

  DISALLOW_COPY_AND_ASSIGN(ConfiguratorImpl);
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_CONFIGURATOR_IMPL_H_
