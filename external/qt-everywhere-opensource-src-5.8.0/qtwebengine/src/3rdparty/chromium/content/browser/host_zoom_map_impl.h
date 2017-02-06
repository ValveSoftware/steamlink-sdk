// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HOST_ZOOM_MAP_IMPL_H_
#define CONTENT_BROWSER_HOST_ZOOM_MAP_IMPL_H_

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {

class WebContentsImpl;

// HostZoomMap needs to be deleted on the UI thread because it listens
// to notifications on there (and holds a NotificationRegistrar).
class CONTENT_EXPORT HostZoomMapImpl : public NON_EXPORTED_BASE(HostZoomMap),
                                       public NotificationObserver {
 public:
  HostZoomMapImpl();
  ~HostZoomMapImpl() override;

  // HostZoomMap implementation:
  void SetPageScaleFactorIsOneForView(
      int render_process_id, int render_view_id, bool is_one) override;
  void ClearPageScaleFactorIsOneForView(
      int render_process_id, int render_view_id) override;
  void CopyFrom(HostZoomMap* copy) override;
  double GetZoomLevelForHostAndScheme(const std::string& scheme,
                                      const std::string& host) const override;
  // TODO(wjmaclean) Should we use a GURL here? crbug.com/384486
  bool HasZoomLevel(const std::string& scheme,
                    const std::string& host) const override;
  ZoomLevelVector GetAllZoomLevels() const override;
  void SetZoomLevelForHost(const std::string& host, double level) override;
  void SetZoomLevelForHostAndScheme(const std::string& scheme,
                                    const std::string& host,
                                    double level) override;
  bool UsesTemporaryZoomLevel(int render_process_id,
                              int render_view_id) const override;
  void SetTemporaryZoomLevel(int render_process_id,
                             int render_view_id,
                             double level) override;

  void ClearTemporaryZoomLevel(int render_process_id,
                               int render_view_id) override;
  double GetDefaultZoomLevel() const override;
  void SetDefaultZoomLevel(double level) override;
  std::unique_ptr<Subscription> AddZoomLevelChangedCallback(
      const ZoomLevelChangedCallback& callback) override;

  // Returns the current zoom level for the specified WebContents. This may
  // be a temporary zoom level, depending on UsesTemporaryZoomLevel().
  double GetZoomLevelForWebContents(
      const WebContentsImpl& web_contents_impl) const;

  bool PageScaleFactorIsOneForWebContents(
      const WebContentsImpl& web_contents_impl) const;

  // Sets the zoom level for this WebContents. If this WebContents is using
  // a temporary zoom level, then level is only applied to this WebContents.
  // Otherwise, the level will be applied on a host level.
  void SetZoomLevelForWebContents(const WebContentsImpl& web_contents_impl,
                                  double level);

  // Sets the zoom level for the specified view. The level may be set for only
  // this view, or for the host, depending on UsesTemporaryZoomLevel().
  void SetZoomLevelForView(int render_process_id,
                           int render_view_id,
                           double level,
                           const std::string& host);

  // Returns the temporary zoom level that's only valid for the lifetime of
  // the given WebContents (i.e. isn't saved and doesn't affect other
  // WebContentses) if it exists, the default zoom level otherwise.
  //
  // This may be called on any thread.
  double GetTemporaryZoomLevel(int render_process_id,
                               int render_view_id) const;

  // Returns the zoom level regardless of whether it's temporary, host-keyed or
  // scheme+host-keyed.
  //
  // This may be called on any thread.
  double GetZoomLevelForView(const GURL& url,
                             int render_process_id,
                             int render_view_id) const;

  // NotificationObserver implementation.
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  void SendErrorPageZoomLevelRefresh();

 private:
  typedef std::map<std::string, double> HostZoomLevels;
  typedef std::map<std::string, HostZoomLevels> SchemeHostZoomLevels;

  struct RenderViewKey {
    int render_process_id;
    int render_view_id;
    RenderViewKey(int render_process_id, int render_view_id)
        : render_process_id(render_process_id),
          render_view_id(render_view_id) {}
    bool operator<(const RenderViewKey& other) const {
      return std::tie(render_process_id, render_view_id) <
             std::tie(other.render_process_id, other.render_view_id);
    }
  };

  typedef std::map<RenderViewKey, double> TemporaryZoomLevels;
  typedef std::map<RenderViewKey, bool> ViewPageScaleFactorsAreOne;

  double GetZoomLevelForHost(const std::string& host) const;

  // Non-locked versions for internal use. These should only be called within
  // a scope where a lock has been acquired.
  double GetZoomLevelForHostInternal(const std::string& host) const;
  double GetZoomLevelForHostAndSchemeInternal(const std::string& scheme,
                                              const std::string& host) const;

  // Notifies the renderers from this browser context to change the zoom level
  // for the specified host and scheme.
  // TODO(wjmaclean) Should we use a GURL here? crbug.com/384486
  void SendZoomLevelChange(const std::string& scheme,
                           const std::string& host,
                           double level);

  // Callbacks called when zoom level changes.
  base::CallbackList<void(const ZoomLevelChange&)>
      zoom_level_changed_callbacks_;

  // Copy of the pref data, so that we can read it on the IO thread.
  HostZoomLevels host_zoom_levels_;
  SchemeHostZoomLevels scheme_host_zoom_levels_;
  double default_zoom_level_;

  // Page scale factor data for each renderer.
  ViewPageScaleFactorsAreOne view_page_scale_factors_are_one_;

  // Don't expect more than a couple of tabs that are using a temporary zoom
  // level, so vector is fine for now.
  TemporaryZoomLevels temporary_zoom_levels_;

  // Used around accesses to |host_zoom_levels_|, |default_zoom_level_|,
  // |temporary_zoom_levels_|, and |view_page_scale_factors_are_one_| to
  // guarantee thread safety.
  mutable base::Lock lock_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(HostZoomMapImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_HOST_ZOOM_MAP_IMPL_H_
