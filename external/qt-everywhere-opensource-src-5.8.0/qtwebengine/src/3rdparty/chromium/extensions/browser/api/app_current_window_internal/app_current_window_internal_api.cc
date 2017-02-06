// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/app_current_window_internal/app_current_window_internal_api.h"

#include <stdint.h>

#include <utility>

#include "base/command_line.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_client.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/app_window/size_constraints.h"
#include "extensions/common/api/app_current_window_internal.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace app_current_window_internal =
    extensions::api::app_current_window_internal;

namespace Show = app_current_window_internal::Show;
namespace SetBounds = app_current_window_internal::SetBounds;
namespace SetSizeConstraints = app_current_window_internal::SetSizeConstraints;
namespace SetIcon = app_current_window_internal::SetIcon;
namespace SetShape = app_current_window_internal::SetShape;
namespace SetAlwaysOnTop = app_current_window_internal::SetAlwaysOnTop;
namespace SetVisibleOnAllWorkspaces =
    app_current_window_internal::SetVisibleOnAllWorkspaces;

using app_current_window_internal::Bounds;
using app_current_window_internal::Region;
using app_current_window_internal::RegionRect;
using app_current_window_internal::SizeConstraints;

namespace extensions {

namespace {

const char kNoAssociatedAppWindow[] =
    "The context from which the function was called did not have an "
    "associated app window.";

const char kDevChannelOnly[] =
    "This function is currently only available in the Dev channel.";

const char kRequiresFramelessWindow[] =
    "This function requires a frameless window (frame:none).";

const char kAlwaysOnTopPermission[] =
    "The \"app.window.alwaysOnTop\" permission is required.";

const char kInvalidParameters[] = "Invalid parameters.";

const int kUnboundedSize = SizeConstraints::kUnboundedSize;

void GetBoundsFields(const Bounds& bounds_spec, gfx::Rect* bounds) {
  if (bounds_spec.left)
    bounds->set_x(*bounds_spec.left);
  if (bounds_spec.top)
    bounds->set_y(*bounds_spec.top);
  if (bounds_spec.width)
    bounds->set_width(*bounds_spec.width);
  if (bounds_spec.height)
    bounds->set_height(*bounds_spec.height);
}

// Copy the constraint value from the API to our internal representation of
// content size constraints. A value of zero resets the constraints. The insets
// are used to transform window constraints to content constraints.
void GetConstraintWidth(const std::unique_ptr<int>& width,
                        const gfx::Insets& insets,
                        gfx::Size* size) {
  if (!width.get())
    return;

  size->set_width(*width > 0 ? std::max(0, *width - insets.width())
                             : kUnboundedSize);
}

void GetConstraintHeight(const std::unique_ptr<int>& height,
                         const gfx::Insets& insets,
                         gfx::Size* size) {
  if (!height.get())
    return;

  size->set_height(*height > 0 ? std::max(0, *height - insets.height())
                               : kUnboundedSize);
}

}  // namespace

namespace bounds {

enum BoundsType {
  INNER_BOUNDS,
  OUTER_BOUNDS,
  DEPRECATED_BOUNDS,
  INVALID_TYPE
};

const char kInnerBoundsType[] = "innerBounds";
const char kOuterBoundsType[] = "outerBounds";
const char kDeprecatedBoundsType[] = "bounds";

BoundsType GetBoundsType(const std::string& type_as_string) {
  if (type_as_string == kInnerBoundsType)
    return INNER_BOUNDS;
  else if (type_as_string == kOuterBoundsType)
    return OUTER_BOUNDS;
  else if (type_as_string == kDeprecatedBoundsType)
    return DEPRECATED_BOUNDS;
  else
    return INVALID_TYPE;
}

}  // namespace bounds

bool AppCurrentWindowInternalExtensionFunction::RunSync() {
  AppWindowRegistry* registry = AppWindowRegistry::Get(browser_context());
  DCHECK(registry);
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents)
    // No need to set an error, since we won't return to the caller anyway if
    // there's no RVH.
    return false;
  AppWindow* window = registry->GetAppWindowForWebContents(web_contents);
  if (!window) {
    error_ = kNoAssociatedAppWindow;
    return false;
  }
  return RunWithWindow(window);
}

bool AppCurrentWindowInternalFocusFunction::RunWithWindow(AppWindow* window) {
  window->GetBaseWindow()->Activate();
  return true;
}

bool AppCurrentWindowInternalFullscreenFunction::RunWithWindow(
    AppWindow* window) {
  window->Fullscreen();
  return true;
}

bool AppCurrentWindowInternalMaximizeFunction::RunWithWindow(
    AppWindow* window) {
  window->Maximize();
  return true;
}

bool AppCurrentWindowInternalMinimizeFunction::RunWithWindow(
    AppWindow* window) {
  window->Minimize();
  return true;
}

bool AppCurrentWindowInternalRestoreFunction::RunWithWindow(AppWindow* window) {
  window->Restore();
  return true;
}

bool AppCurrentWindowInternalDrawAttentionFunction::RunWithWindow(
    AppWindow* window) {
  window->GetBaseWindow()->FlashFrame(true);
  return true;
}

bool AppCurrentWindowInternalClearAttentionFunction::RunWithWindow(
    AppWindow* window) {
  window->GetBaseWindow()->FlashFrame(false);
  return true;
}

bool AppCurrentWindowInternalShowFunction::RunWithWindow(AppWindow* window) {
  std::unique_ptr<Show::Params> params(Show::Params::Create(*args_));
  CHECK(params.get());
  if (params->focused && !*params->focused)
    window->Show(AppWindow::SHOW_INACTIVE);
  else
    window->Show(AppWindow::SHOW_ACTIVE);
  return true;
}

bool AppCurrentWindowInternalHideFunction::RunWithWindow(AppWindow* window) {
  window->Hide();
  return true;
}

bool AppCurrentWindowInternalSetBoundsFunction::RunWithWindow(
    AppWindow* window) {
  std::unique_ptr<SetBounds::Params> params(SetBounds::Params::Create(*args_));
  CHECK(params.get());

  bounds::BoundsType bounds_type = bounds::GetBoundsType(params->bounds_type);
  if (bounds_type == bounds::INVALID_TYPE) {
    NOTREACHED();
    error_ = kInvalidParameters;
    return false;
  }

  // Start with the current bounds, and change any values that are specified in
  // the incoming parameters.
  gfx::Rect original_window_bounds = window->GetBaseWindow()->GetBounds();
  gfx::Rect window_bounds = original_window_bounds;
  gfx::Insets frame_insets = window->GetBaseWindow()->GetFrameInsets();
  const Bounds& bounds_spec = params->bounds;

  switch (bounds_type) {
    case bounds::DEPRECATED_BOUNDS: {
      // We need to maintain backcompatibility with a bug on Windows and
      // ChromeOS, which sets the position of the window but the size of the
      // content.
      if (bounds_spec.left)
        window_bounds.set_x(*bounds_spec.left);
      if (bounds_spec.top)
        window_bounds.set_y(*bounds_spec.top);
      if (bounds_spec.width)
        window_bounds.set_width(*bounds_spec.width + frame_insets.width());
      if (bounds_spec.height)
        window_bounds.set_height(*bounds_spec.height + frame_insets.height());
      break;
    }
    case bounds::OUTER_BOUNDS: {
      GetBoundsFields(bounds_spec, &window_bounds);
      break;
    }
    case bounds::INNER_BOUNDS: {
      window_bounds.Inset(frame_insets);
      GetBoundsFields(bounds_spec, &window_bounds);
      window_bounds.Inset(-frame_insets);
      break;
    }
    default:
      NOTREACHED();
  }

  if (original_window_bounds != window_bounds) {
    if (original_window_bounds.size() != window_bounds.size()) {
      SizeConstraints constraints(
          SizeConstraints::AddFrameToConstraints(
              window->GetBaseWindow()->GetContentMinimumSize(), frame_insets),
          SizeConstraints::AddFrameToConstraints(
              window->GetBaseWindow()->GetContentMaximumSize(), frame_insets));

      window_bounds.set_size(constraints.ClampSize(window_bounds.size()));
    }

    window->GetBaseWindow()->SetBounds(window_bounds);
  }

  return true;
}

bool AppCurrentWindowInternalSetSizeConstraintsFunction::RunWithWindow(
    AppWindow* window) {
  std::unique_ptr<SetSizeConstraints::Params> params(
      SetSizeConstraints::Params::Create(*args_));
  CHECK(params.get());

  bounds::BoundsType bounds_type = bounds::GetBoundsType(params->bounds_type);
  if (bounds_type != bounds::INNER_BOUNDS &&
      bounds_type != bounds::OUTER_BOUNDS) {
    NOTREACHED();
    error_ = kInvalidParameters;
    return false;
  }

  gfx::Size original_min_size =
      window->GetBaseWindow()->GetContentMinimumSize();
  gfx::Size original_max_size =
      window->GetBaseWindow()->GetContentMaximumSize();
  gfx::Size min_size = original_min_size;
  gfx::Size max_size = original_max_size;
  const app_current_window_internal::SizeConstraints& constraints =
      params->constraints;

  // Use the frame insets to convert window size constraints to content size
  // constraints.
  gfx::Insets insets;
  if (bounds_type == bounds::OUTER_BOUNDS)
    insets = window->GetBaseWindow()->GetFrameInsets();

  GetConstraintWidth(constraints.min_width, insets, &min_size);
  GetConstraintWidth(constraints.max_width, insets, &max_size);
  GetConstraintHeight(constraints.min_height, insets, &min_size);
  GetConstraintHeight(constraints.max_height, insets, &max_size);

  if (min_size != original_min_size || max_size != original_max_size)
    window->SetContentSizeConstraints(min_size, max_size);

  return true;
}

bool AppCurrentWindowInternalSetIconFunction::RunWithWindow(AppWindow* window) {
  if (AppWindowClient::Get()->IsCurrentChannelOlderThanDev() &&
      extension()->location() != extensions::Manifest::COMPONENT) {
    error_ = kDevChannelOnly;
    return false;
  }

  std::unique_ptr<SetIcon::Params> params(SetIcon::Params::Create(*args_));
  CHECK(params.get());
  // The |icon_url| parameter may be a blob url (e.g. an image fetched with an
  // XMLHttpRequest) or a resource url.
  GURL url(params->icon_url);
  if (!url.is_valid())
    url = extension()->GetResourceURL(params->icon_url);

  window->SetAppIconUrl(url);
  return true;
}

bool AppCurrentWindowInternalSetShapeFunction::RunWithWindow(
    AppWindow* window) {

  if (!window->GetBaseWindow()->IsFrameless()) {
    error_ = kRequiresFramelessWindow;
    return false;
  }

  std::unique_ptr<SetShape::Params> params(SetShape::Params::Create(*args_));
  const Region& shape = params->region;

  // Build a region from the supplied list of rects.
  // If |rects| is missing, then the input region is removed. This clears the
  // input region so that the entire window accepts input events.
  // To specify an empty input region (so the window ignores all input),
  // |rects| should be an empty list.
  std::unique_ptr<SkRegion> region(new SkRegion);
  if (shape.rects) {
    for (const RegionRect& input_rect : *shape.rects) {
      int32_t x = input_rect.left;
      int32_t y = input_rect.top;
      int32_t width = input_rect.width;
      int32_t height = input_rect.height;

      SkIRect rect = SkIRect::MakeXYWH(x, y, width, height);
      region->op(rect, SkRegion::kUnion_Op);
    }
  } else {
    region.reset(NULL);
  }

  window->UpdateShape(std::move(region));

  return true;
}

bool AppCurrentWindowInternalSetAlwaysOnTopFunction::RunWithWindow(
    AppWindow* window) {
  if (!extension()->permissions_data()->HasAPIPermission(
          extensions::APIPermission::kAlwaysOnTopWindows)) {
    error_ = kAlwaysOnTopPermission;
    return false;
  }

  std::unique_ptr<SetAlwaysOnTop::Params> params(
      SetAlwaysOnTop::Params::Create(*args_));
  CHECK(params.get());
  window->SetAlwaysOnTop(params->always_on_top);
  return true;
}

bool AppCurrentWindowInternalSetVisibleOnAllWorkspacesFunction::RunWithWindow(
    AppWindow* window) {
  std::unique_ptr<SetVisibleOnAllWorkspaces::Params> params(
      SetVisibleOnAllWorkspaces::Params::Create(*args_));
  CHECK(params.get());
  window->GetBaseWindow()->SetVisibleOnAllWorkspaces(params->always_visible);
  return true;
}

}  // namespace extensions
