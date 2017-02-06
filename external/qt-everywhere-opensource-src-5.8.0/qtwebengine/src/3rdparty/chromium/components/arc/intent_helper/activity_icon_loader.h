// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_ACTIVITY_ICON_LOADER_H_
#define COMPONENTS_ARC_INTENT_HELPER_ACTIVITY_ICON_LOADER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "ui/base/layout.h"
#include "ui/gfx/image/image.h"

namespace arc {

// A class which retrieves an activity icon from ARC.
class ActivityIconLoader : public base::RefCounted<ActivityIconLoader> {
 public:
  struct Icons {
    Icons(const gfx::Image& icon16, const gfx::Image& icon20);
    const gfx::Image icon16;  // 16 dip
    const gfx::Image icon20;  // 20 dip
  };

  struct ActivityName {
    ActivityName(const std::string& package_name,
                 const std::string& activity_name);
    bool operator<(const ActivityName& other) const;

    // TODO(yusukes): Add const to these variables later. At this point,
    // doing so seems to confuse g++ 4.6 on builders.
    std::string package_name;

    // Can be empty. When |activity_name| is empty, the loader tries to fetch
    // the package's default icon.
    std::string activity_name;
  };

  enum class GetResult {
    // Succeeded. The callback will be called asynchronously.
    SUCCEEDED_ASYNC,
    // Succeeded. The callback has already been called synchronously.
    SUCCEEDED_SYNC,
    // Failed. The intent_helper instance is not yet ready. This is a temporary
    // error.
    FAILED_ARC_NOT_READY,
    // Failed. Either ARC is not supported at all or intent_helper instance
    // version is too old.
    FAILED_ARC_NOT_SUPPORTED,
  };

  using ActivityToIconsMap = std::map<ActivityName, Icons>;
  using OnIconsReadyCallback =
      base::Callback<void(std::unique_ptr<ActivityToIconsMap>)>;

  ActivityIconLoader();

  // Removes icons associated with |package_name| from the cache.
  void InvalidateIcons(const std::string& package_name);

  // Retrieves icons for the |activities| and calls |cb|. The |cb| is called
  // back exactly once, either synchronously in the GetActivityIcons() when
  // the result is _not_ SUCCEEDED_ASYNC (i.e. all icons are already cached
  // locally or ARC is not ready/supported). Otherwise, the callback is run
  // later asynchronously with icons fetched from ARC side.
  GetResult GetActivityIcons(const std::vector<ActivityName>& activities,
                             const OnIconsReadyCallback& cb);

  void OnIconsResizedForTesting(const OnIconsReadyCallback& cb,
                                std::unique_ptr<ActivityToIconsMap> result);
  void AddIconToCacheForTesting(const ActivityName& activity,
                                const gfx::Image& image);

  // Returns true if |result| indicates that the |cb| object passed to
  // GetActivityIcons() has already called.
  static bool HasIconsReadyCallbackRun(GetResult result);

  const ActivityToIconsMap& cached_icons_for_testing() { return cached_icons_; }

 private:
  friend class base::RefCounted<ActivityIconLoader>;
  ~ActivityIconLoader();

  // A function called when the mojo IPC returns.
  void OnIconsReady(std::unique_ptr<ActivityToIconsMap> cached_result,
                    const OnIconsReadyCallback& cb,
                    mojo::Array<mojom::ActivityIconPtr> icons);

  // Resize |icons| and returns the results as ActivityToIconsMap.
  std::unique_ptr<ActivityToIconsMap> ResizeIcons(
      mojo::Array<mojom::ActivityIconPtr> icons);

  // A function called when ResizeIcons finishes. Append items in |result| to
  // |cached_icons_|.
  void OnIconsResized(std::unique_ptr<ActivityToIconsMap> cached_result,
                      const OnIconsReadyCallback& cb,
                      std::unique_ptr<ActivityToIconsMap> result);

  // The maximum scale factor the current platform supports.
  const ui::ScaleFactor scale_factor_;
  // A map which holds icons in a scale-factor independent form (gfx::Image).
  ActivityToIconsMap cached_icons_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ActivityIconLoader);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_ACTIVITY_ICON_LOADER_H_
