// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_CONFIGURATOR_H_
#define COMPONENTS_UPDATE_CLIENT_CONFIGURATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"

class GURL;
class PrefService;

namespace base {
class SequencedTaskRunner;
class Version;
}

namespace net {
class URLRequestContextGetter;
}

namespace update_client {

class OutOfProcessPatcher;

// Controls the component updater behavior.
// TODO(sorin): this class will be split soon in two. One class controls
// the behavior of the update client, and the other class controls the
// behavior of the component updater.
class Configurator : public base::RefCountedThreadSafe<Configurator> {
 public:
  // Delay in seconds from calling Start() to the first update check.
  virtual int InitialDelay() const = 0;

  // Delay in seconds to every subsequent update check. 0 means don't check.
  virtual int NextCheckDelay() const = 0;

  // Delay in seconds from each task step. Used to smooth out CPU/IO usage.
  virtual int StepDelay() const = 0;

  // Minimum delta time in seconds before an on-demand check is allowed
  // for the same component.
  virtual int OnDemandDelay() const = 0;

  // The time delay in seconds between applying updates for different
  // components.
  virtual int UpdateDelay() const = 0;

  // The URLs for the update checks. The URLs are tried in order, the first one
  // that succeeds wins.
  virtual std::vector<GURL> UpdateUrl() const = 0;

  // The URLs for pings. Returns an empty vector if and only if pings are
  // disabled. Similarly, these URLs have a fall back behavior too.
  virtual std::vector<GURL> PingUrl() const = 0;

  // The ProdId is used as a prefix in some of the version strings which appear
  // in the protocol requests. Possible values include "chrome", "chromecrx",
  // "chromiumcrx", and "unknown".
  virtual std::string GetProdId() const = 0;

  // Version of the application. Used to compare the component manifests.
  virtual base::Version GetBrowserVersion() const = 0;

  // Returns the value we use for the "updaterchannel=" and "prodchannel="
  // parameters. Possible return values include: "canary", "dev", "beta", and
  // "stable".
  virtual std::string GetChannel() const = 0;

  // Returns the brand code or distribution tag that has been assigned to
  // a partner. A brand code is a 4-character string used to identify
  // installations that took place as a result of partner deals or website
  // promotions.
  virtual std::string GetBrand() const = 0;

  // Returns the language for the present locale. Possible return values are
  // standard tags for languages, such as "en", "en-US", "de", "fr", "af", etc.
  virtual std::string GetLang() const = 0;

  // Returns the OS's long name like "Windows", "Mac OS X", etc.
  virtual std::string GetOSLongName() const = 0;

  // Parameters added to each url request. It can be empty if none are needed.
  // The return string must be safe for insertion as an attribute in an
  // XML element.
  virtual std::string ExtraRequestParams() const = 0;

  // Provides a hint for the server to control the order in which multiple
  // download urls are returned. The hint may or may not be honored in the
  // response returned by the server.
  // Returns an empty string if no policy is in effect.
  virtual std::string GetDownloadPreference() const = 0;

  // The source of contexts for all the url requests.
  virtual net::URLRequestContextGetter* RequestContext() const = 0;

  // Returns a new out of process patcher. May be NULL for implementations
  // that patch in-process.
  virtual scoped_refptr<update_client::OutOfProcessPatcher>
  CreateOutOfProcessPatcher() const = 0;

  // True means that this client can handle delta updates.
  virtual bool EnabledDeltas() const = 0;

  // True if component updates are enabled. Updates for all components are
  // enabled by default. This method allows enabling or disabling
  // updates for certain components such as the plugins. Updates for some
  // components are always enabled and can't be disabled programatically.
  virtual bool EnabledComponentUpdates() const = 0;

  // True means that the background downloader can be used for downloading
  // non on-demand components.
  virtual bool EnabledBackgroundDownloader() const = 0;

  // True if signing of update checks is enabled.
  virtual bool EnabledCupSigning() const = 0;

  // Gets a task runner to a blocking pool of threads suitable for worker jobs.
  virtual scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner()
      const = 0;

  // Returns a PrefService that the update_client can use to store persistent
  // update information. The PrefService must outlive the entire update_client,
  // and be safe to access from the thread the update_client is constructed
  // on.
  // Returning null is safe and will disable any functionality that requires
  // persistent storage.
  virtual PrefService* GetPrefService() const = 0;

  // Returns true if the Chrome is installed for the current user only, or false
  // if Chrome is installed for all users on the machine. This function must be
  // called only from a blocking pool thread, as it may access the file system.
  virtual bool IsPerUserInstall() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<Configurator>;

  virtual ~Configurator() {}
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_CONFIGURATOR_H_
