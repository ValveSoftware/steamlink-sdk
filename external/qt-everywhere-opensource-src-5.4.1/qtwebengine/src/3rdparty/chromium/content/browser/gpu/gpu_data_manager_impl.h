// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_
#define CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/process/kill.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/gpu_memory_stats.h"
#include "content/public/common/three_d_api_types.h"
#include "gpu/config/gpu_info.h"

class GURL;
struct WebPreferences;

namespace base {
class CommandLine;
}

namespace content {

class GpuDataManagerImplPrivate;

class CONTENT_EXPORT GpuDataManagerImpl
    : public NON_EXPORTED_BASE(GpuDataManager) {
 public:
  // Indicates the guilt level of a domain which caused a GPU reset.
  // If a domain is 100% known to be guilty of resetting the GPU, then
  // it will generally not cause other domains' use of 3D APIs to be
  // blocked, unless system stability would be compromised.
  enum DomainGuilt {
    DOMAIN_GUILT_KNOWN,
    DOMAIN_GUILT_UNKNOWN
  };

  // Indicates the reason that access to a given client API (like
  // WebGL or Pepper 3D) was blocked or not. This state is distinct
  // from blacklisting of an entire feature.
  enum DomainBlockStatus {
    DOMAIN_BLOCK_STATUS_BLOCKED,
    DOMAIN_BLOCK_STATUS_ALL_DOMAINS_BLOCKED,
    DOMAIN_BLOCK_STATUS_NOT_BLOCKED
  };

  // Getter for the singleton. This will return NULL on failure.
  static GpuDataManagerImpl* GetInstance();

  // GpuDataManager implementation.
  virtual void InitializeForTesting(
      const std::string& gpu_blacklist_json,
      const gpu::GPUInfo& gpu_info) OVERRIDE;
  virtual bool IsFeatureBlacklisted(int feature) const OVERRIDE;
  virtual gpu::GPUInfo GetGPUInfo() const OVERRIDE;
  virtual void GetGpuProcessHandles(
      const GetGpuProcessHandlesCallback& callback) const OVERRIDE;
  virtual bool GpuAccessAllowed(std::string* reason) const OVERRIDE;
  virtual void RequestCompleteGpuInfoIfNeeded() OVERRIDE;
  virtual bool IsCompleteGpuInfoAvailable() const OVERRIDE;
  virtual void RequestVideoMemoryUsageStatsUpdate() const OVERRIDE;
  virtual bool ShouldUseSwiftShader() const OVERRIDE;
  virtual void RegisterSwiftShaderPath(const base::FilePath& path) OVERRIDE;
  virtual void AddObserver(GpuDataManagerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(GpuDataManagerObserver* observer) OVERRIDE;
  virtual void UnblockDomainFrom3DAPIs(const GURL& url) OVERRIDE;
  virtual void DisableGpuWatchdog() OVERRIDE;
  virtual void SetGLStrings(const std::string& gl_vendor,
                            const std::string& gl_renderer,
                            const std::string& gl_version) OVERRIDE;
  virtual void GetGLStrings(std::string* gl_vendor,
                            std::string* gl_renderer,
                            std::string* gl_version) OVERRIDE;
  virtual void DisableHardwareAcceleration() OVERRIDE;
  virtual bool CanUseGpuBrowserCompositor() const OVERRIDE;

  // This collects preliminary GPU info, load GpuBlacklist, and compute the
  // preliminary blacklisted features; it should only be called at browser
  // startup time in UI thread before the IO restriction is turned on.
  void Initialize();

  // Only update if the current GPUInfo is not finalized.  If blacklist is
  // loaded, run through blacklist and update blacklisted features.
  void UpdateGpuInfo(const gpu::GPUInfo& gpu_info);

  void UpdateVideoMemoryUsageStats(
      const GPUVideoMemoryUsageStats& video_memory_usage_stats);

  // Insert disable-feature switches corresponding to preliminary gpu feature
  // flags into the renderer process command line.
  void AppendRendererCommandLine(base::CommandLine* command_line) const;

  // Insert switches into gpu process command line: kUseGL, etc.
  void AppendGpuCommandLine(base::CommandLine* command_line) const;

  // Insert switches into plugin process command line:
  // kDisableCoreAnimationPlugins.
  void AppendPluginCommandLine(base::CommandLine* command_line) const;

  // Update WebPreferences for renderer based on blacklisting decisions.
  void UpdateRendererWebPrefs(WebPreferences* prefs) const;

  std::string GetBlacklistVersion() const;
  std::string GetDriverBugListVersion() const;

  // Returns the reasons for the latest run of blacklisting decisions.
  // For the structure of returned value, see documentation for
  // GpuBlacklist::GetBlacklistedReasons().
  void GetBlacklistReasons(base::ListValue* reasons) const;

  // Returns the workarounds that are applied to the current system as
  // a list of strings.
  void GetDriverBugWorkarounds(base::ListValue* workarounds) const;

  void AddLogMessage(int level,
                     const std::string& header,
                     const std::string& message);

  void ProcessCrashed(base::TerminationStatus exit_code);

  // Returns a new copy of the ListValue.  Caller is responsible to release
  // the returned value.
  base::ListValue* GetLogMessages() const;

  // Called when switching gpu.
  void HandleGpuSwitch();

  // Maintenance of domains requiring explicit user permission before
  // using client-facing 3D APIs (WebGL, Pepper 3D), either because
  // the domain has caused the GPU to reset, or because too many GPU
  // resets have been observed globally recently, and system stability
  // might be compromised.
  //
  // The given URL may be a partial URL (including at least the host)
  // or a full URL to a page.
  //
  // Note that the unblocking API must be part of the content API
  // because it is called from Chrome side code.
  void BlockDomainFrom3DAPIs(const GURL& url, DomainGuilt guilt);
  bool Are3DAPIsBlocked(const GURL& url,
                        int render_process_id,
                        int render_view_id,
                        ThreeDAPIType requester);

  // Disables domain blocking for 3D APIs. For use only in tests.
  void DisableDomainBlockingFor3DAPIsForTesting();

  void Notify3DAPIBlocked(const GURL& url,
                          int render_process_id,
                          int render_view_id,
                          ThreeDAPIType requester);

  // Get number of features being blacklisted.
  size_t GetBlacklistedFeatureCount() const;

  void SetDisplayCount(unsigned int display_count);
  unsigned int GetDisplayCount() const;

  // Set the active gpu.
  // Return true if it's a different GPU from the previous active one.
  bool UpdateActiveGpu(uint32 vendor_id, uint32 device_id);

  // Called when GPU process initialization failed.
  void OnGpuProcessInitFailure();

  bool IsDriverBugWorkaroundActive(int feature) const;

 private:
  friend class GpuDataManagerImplPrivate;
  friend class GpuDataManagerImplPrivateTest;
  friend struct DefaultSingletonTraits<GpuDataManagerImpl>;

  // It's similar to AutoUnlock, but we want to make it a no-op
  // if the owner GpuDataManagerImpl is null.
  // This should only be used by GpuDataManagerImplPrivate where
  // callbacks are called, during which re-entering
  // GpuDataManagerImpl is possible.
  class UnlockedSession {
   public:
    explicit UnlockedSession(GpuDataManagerImpl* owner)
        : owner_(owner) {
      DCHECK(owner_);
      owner_->lock_.AssertAcquired();
      owner_->lock_.Release();
    }

    ~UnlockedSession() {
      DCHECK(owner_);
      owner_->lock_.Acquire();
    }

   private:
    GpuDataManagerImpl* owner_;
    DISALLOW_COPY_AND_ASSIGN(UnlockedSession);
  };

  GpuDataManagerImpl();
  virtual ~GpuDataManagerImpl();

  mutable base::Lock lock_;
  scoped_ptr<GpuDataManagerImplPrivate> private_;

  DISALLOW_COPY_AND_ASSIGN(GpuDataManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_
