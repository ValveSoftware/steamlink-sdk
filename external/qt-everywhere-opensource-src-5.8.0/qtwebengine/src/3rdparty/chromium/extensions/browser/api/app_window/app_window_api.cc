// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/app_window/app_window_api.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_client.h"
#include "extensions/browser/app_window/app_window_contents.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/api/app_window.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace app_window = extensions::api::app_window;
namespace Create = app_window::Create;

namespace extensions {

namespace app_window_constants {
const char kInvalidWindowId[] =
    "The window id can not be more than 256 characters long.";
const char kInvalidColorSpecification[] =
    "The color specification could not be parsed.";
const char kColorWithFrameNone[] = "Windows with no frame cannot have a color.";
const char kInactiveColorWithoutColor[] =
    "frame.inactiveColor must be used with frame.color.";
const char kConflictingBoundsOptions[] =
    "The $1 property cannot be specified for both inner and outer bounds.";
const char kAlwaysOnTopPermission[] =
    "The \"app.window.alwaysOnTop\" permission is required.";
const char kInvalidUrlParameter[] =
    "The URL used for window creation must be local for security reasons.";
const char kAlphaEnabledWrongChannel[] =
    "The alphaEnabled option requires dev channel or newer.";
const char kAlphaEnabledMissingPermission[] =
    "The alphaEnabled option requires app.window.alpha permission.";
const char kAlphaEnabledNeedsFrameNone[] =
    "The alphaEnabled option can only be used with \"frame: 'none'\".";
const char kImeWindowMissingPermission[] =
    "Extensions require the \"app.window.ime\" permission to create windows.";
const char kImeOptionIsNotSupported[] =
    "The \"ime\" option is not supported for platform app.";
#if !defined(OS_CHROMEOS)
const char kImeWindowUnsupportedPlatform[] =
    "The \"ime\" option can only be used on ChromeOS.";
#else
const char kImeWindowMustBeImeWindowOrPanel[] =
    "IME extensions must create ime window ( with \"ime: true\" and "
    "\"frame: 'none'\") or panel window (with \"type: panel\").";
#endif
}  // namespace app_window_constants

const char kNoneFrameOption[] = "none";

namespace {

// If the same property is specified for the inner and outer bounds, raise an
// error.
bool CheckBoundsConflict(const std::unique_ptr<int>& inner_property,
                         const std::unique_ptr<int>& outer_property,
                         const std::string& property_name,
                         std::string* error) {
  if (inner_property.get() && outer_property.get()) {
    std::vector<std::string> subst;
    subst.push_back(property_name);
    *error = base::ReplaceStringPlaceholders(
        app_window_constants::kConflictingBoundsOptions, subst, NULL);
    return false;
  }

  return true;
}

// Copy over the bounds specification properties from the API to the
// AppWindow::CreateParams.
void CopyBoundsSpec(const app_window::BoundsSpecification* input_spec,
                    AppWindow::BoundsSpecification* create_spec) {
  if (!input_spec)
    return;

  if (input_spec->left.get())
    create_spec->bounds.set_x(*input_spec->left);
  if (input_spec->top.get())
    create_spec->bounds.set_y(*input_spec->top);
  if (input_spec->width.get())
    create_spec->bounds.set_width(*input_spec->width);
  if (input_spec->height.get())
    create_spec->bounds.set_height(*input_spec->height);
  if (input_spec->min_width.get())
    create_spec->minimum_size.set_width(*input_spec->min_width);
  if (input_spec->min_height.get())
    create_spec->minimum_size.set_height(*input_spec->min_height);
  if (input_spec->max_width.get())
    create_spec->maximum_size.set_width(*input_spec->max_width);
  if (input_spec->max_height.get())
    create_spec->maximum_size.set_height(*input_spec->max_height);
}

}  // namespace

AppWindowCreateFunction::AppWindowCreateFunction() {}

bool AppWindowCreateFunction::RunAsync() {
  // Don't create app window if the system is shutting down.
  if (ExtensionsBrowserClient::Get()->IsShuttingDown())
    return false;

  std::unique_ptr<Create::Params> params(Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url = extension()->GetResourceURL(params->url);
  // Allow absolute URLs for component apps, otherwise prepend the extension
  // path.
  // TODO(devlin): Investigate if this is still used. If not, kill it dead!
  GURL absolute = GURL(params->url);
  if (absolute.has_scheme()) {
    if (extension()->location() == Manifest::COMPONENT) {
      url = absolute;
    } else {
      // Show error when url passed isn't local.
      error_ = app_window_constants::kInvalidUrlParameter;
      return false;
    }
  }

  // TODO(jeremya): figure out a way to pass the opening WebContents through to
  // AppWindow::Create so we can set the opener at create time rather than
  // with a hack in AppWindowCustomBindings::GetView().
  AppWindow::CreateParams create_params;
  app_window::CreateWindowOptions* options = params->options.get();
  if (options) {
    if (options->id.get()) {
      // TODO(mek): use URL if no id specified?
      // Limit length of id to 256 characters.
      if (options->id->length() > 256) {
        error_ = app_window_constants::kInvalidWindowId;
        return false;
      }

      create_params.window_key = *options->id;

      if (options->singleton && *options->singleton == false) {
        WriteToConsole(
          content::CONSOLE_MESSAGE_LEVEL_WARNING,
          "The 'singleton' option in chrome.apps.window.create() is deprecated!"
          " Change your code to no longer rely on this.");
      }

      if (!options->singleton || *options->singleton) {
        AppWindow* existing_window =
            AppWindowRegistry::Get(browser_context())
                ->GetAppWindowForAppAndKey(extension_id(),
                                           create_params.window_key);
        if (existing_window) {
          content::RenderFrameHost* existing_frame =
              existing_window->web_contents()->GetMainFrame();
          int frame_id = MSG_ROUTING_NONE;
          if (render_frame_host()->GetProcess()->GetID() ==
              existing_frame->GetProcess()->GetID()) {
            frame_id = existing_frame->GetRoutingID();
          }

          if (!options->hidden.get() || !*options->hidden.get()) {
            if (options->focused.get() && !*options->focused.get())
              existing_window->Show(AppWindow::SHOW_INACTIVE);
            else
              existing_window->Show(AppWindow::SHOW_ACTIVE);
          }

          std::unique_ptr<base::DictionaryValue> result(
              new base::DictionaryValue);
          result->Set("frameId", new base::FundamentalValue(frame_id));
          existing_window->GetSerializedState(result.get());
          result->SetBoolean("existingWindow", true);
          SetResult(std::move(result));
          SendResponse(true);
          return true;
        }
      }
    }

    if (!GetBoundsSpec(*options, &create_params, &error_))
      return false;

    if (options->type == app_window::WINDOW_TYPE_PANEL) {
#if defined(OS_CHROMEOS)
      // Panels for v2 apps are only supported on Chrome OS.
      create_params.window_type = AppWindow::WINDOW_TYPE_PANEL;
#else
      WriteToConsole(content::CONSOLE_MESSAGE_LEVEL_WARNING,
                     "Panels are not supported on this platform");
#endif
    }

    if (!GetFrameOptions(*options, &create_params))
      return false;

    if (extension()->GetType() == Manifest::TYPE_EXTENSION) {
      // Whitelisted IME extensions are allowed to use this API to create IME
      // specific windows to show accented characters or suggestions.
      if (!extension()->permissions_data()->HasAPIPermission(
              APIPermission::kImeWindowEnabled)) {
        error_ = app_window_constants::kImeWindowMissingPermission;
        return false;
      }

#if !defined(OS_CHROMEOS)
      // IME window is only supported on ChromeOS.
      error_ = app_window_constants::kImeWindowUnsupportedPlatform;
      return false;
#else
      // IME extensions must create ime window (with "ime: true" and
      // "frame: none") or panel window (with "type: panel").
      if (options->ime.get() && *options->ime.get() &&
          create_params.frame == AppWindow::FRAME_NONE) {
        create_params.is_ime_window = true;
      } else if (options->type == app_window::WINDOW_TYPE_PANEL) {
        create_params.window_type = AppWindow::WINDOW_TYPE_PANEL;
      } else {
        error_ = app_window_constants::kImeWindowMustBeImeWindowOrPanel;
        return false;
      }
#endif  // OS_CHROMEOS
    } else {
      if (options->ime.get()) {
        error_ = app_window_constants::kImeOptionIsNotSupported;
        return false;
      }
    }

    if (options->alpha_enabled.get()) {
      const char* const kWhitelist[] = {
#if defined(OS_CHROMEOS)
        "B58B99751225318C7EB8CF4688B5434661083E07",  // http://crbug.com/410550
        "06BE211D5F014BAB34BC22D9DDA09C63A81D828E",  // http://crbug.com/425539
        "F94EE6AB36D6C6588670B2B01EB65212D9C64E33",
        "B9EF10DDFEA11EF77873CC5009809E5037FC4C7A",  // http://crbug.com/435380
#endif
        "0F42756099D914A026DADFA182871C015735DD95",  // http://crbug.com/323773
        "2D22CDB6583FD0A13758AEBE8B15E45208B4E9A7",
        "E7E2461CE072DF036CF9592740196159E2D7C089",  // http://crbug.com/356200
        "A74A4D44C7CFCD8844830E6140C8D763E12DD8F3",
        "312745D9BF916161191143F6490085EEA0434997",
        "53041A2FA309EECED01FFC751E7399186E860B2C",
        "A07A5B743CD82A1C2579DB77D353C98A23201EEF",  // http://crbug.com/413748
        "F16F23C83C5F6DAD9B65A120448B34056DD80691",
        "0F585FB1D0FDFBEBCE1FEB5E9DFFB6DA476B8C9B"
      };
      if (AppWindowClient::Get()->IsCurrentChannelOlderThanDev() &&
          !SimpleFeature::IsIdInArray(
              extension_id(), kWhitelist, arraysize(kWhitelist))) {
        error_ = app_window_constants::kAlphaEnabledWrongChannel;
        return false;
      }
      if (!extension()->permissions_data()->HasAPIPermission(
              APIPermission::kAlphaEnabled)) {
        error_ = app_window_constants::kAlphaEnabledMissingPermission;
        return false;
      }
      if (create_params.frame != AppWindow::FRAME_NONE) {
        error_ = app_window_constants::kAlphaEnabledNeedsFrameNone;
        return false;
      }
#if defined(USE_AURA)
      create_params.alpha_enabled = *options->alpha_enabled;
#else
      // Transparency is only supported on Aura.
      // Fallback to creating an opaque window (by ignoring alphaEnabled).
#endif
    }

    if (options->hidden.get())
      create_params.hidden = *options->hidden.get();

    if (options->resizable.get())
      create_params.resizable = *options->resizable.get();

    if (options->always_on_top.get()) {
      create_params.always_on_top = *options->always_on_top.get();

      if (create_params.always_on_top &&
          !extension()->permissions_data()->HasAPIPermission(
              APIPermission::kAlwaysOnTopWindows)) {
        error_ = app_window_constants::kAlwaysOnTopPermission;
        return false;
      }
    }

    if (options->focused.get())
      create_params.focused = *options->focused.get();

    if (options->visible_on_all_workspaces.get()) {
      create_params.visible_on_all_workspaces =
          *options->visible_on_all_workspaces.get();
    }

    if (options->type != app_window::WINDOW_TYPE_PANEL) {
      switch (options->state) {
        case app_window::STATE_NONE:
        case app_window::STATE_NORMAL:
          break;
        case app_window::STATE_FULLSCREEN:
          create_params.state = ui::SHOW_STATE_FULLSCREEN;
          break;
        case app_window::STATE_MAXIMIZED:
          create_params.state = ui::SHOW_STATE_MAXIMIZED;
          break;
        case app_window::STATE_MINIMIZED:
          create_params.state = ui::SHOW_STATE_MINIMIZED;
          break;
      }
    }
  }

  create_params.creator_process_id =
      render_frame_host()->GetProcess()->GetID();

  AppWindow* app_window =
      AppWindowClient::Get()->CreateAppWindow(browser_context(), extension());
  app_window->Init(url, new AppWindowContentsImpl(app_window),
                   render_frame_host(), create_params);

  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode() &&
      !app_window->is_ime_window()) {
    app_window->ForcedFullscreen();
  }

  content::RenderFrameHost* created_frame =
      app_window->web_contents()->GetMainFrame();
  int frame_id = MSG_ROUTING_NONE;
  if (create_params.creator_process_id == created_frame->GetProcess()->GetID())
    frame_id = created_frame->GetRoutingID();

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  result->Set("frameId", new base::FundamentalValue(frame_id));
  result->Set("id", new base::StringValue(app_window->window_key()));
  app_window->GetSerializedState(result.get());
  SetResult(std::move(result));

  if (AppWindowRegistry::Get(browser_context())
          ->HadDevToolsAttached(app_window->web_contents())) {
    AppWindowClient::Get()->OpenDevToolsWindow(
        app_window->web_contents(),
        base::Bind(&AppWindowCreateFunction::SendResponse, this, true));
    return true;
  }

  // PlzNavigate: delay sending the response until the newly created window has
  // been told to navigate, and blink has been correctly initialized in the
  // renderer.
  if (content::IsBrowserSideNavigationEnabled()) {
    app_window->SetOnFirstCommitCallback(
        base::Bind(&AppWindowCreateFunction::SendResponse, this, true));
    return true;
  }

  SendResponse(true);
  app_window->WindowEventsReady();

  return true;
}

bool AppWindowCreateFunction::GetBoundsSpec(
    const app_window::CreateWindowOptions& options,
    AppWindow::CreateParams* params,
    std::string* error) {
  DCHECK(params);
  DCHECK(error);

  if (options.inner_bounds.get() || options.outer_bounds.get()) {
    // Parse the inner and outer bounds specifications. If developers use the
    // new API, the deprecated fields will be ignored - do not attempt to merge
    // them.

    const app_window::BoundsSpecification* inner_bounds =
        options.inner_bounds.get();
    const app_window::BoundsSpecification* outer_bounds =
        options.outer_bounds.get();
    if (inner_bounds && outer_bounds) {
      if (!CheckBoundsConflict(
               inner_bounds->left, outer_bounds->left, "left", error)) {
        return false;
      }
      if (!CheckBoundsConflict(
               inner_bounds->top, outer_bounds->top, "top", error)) {
        return false;
      }
      if (!CheckBoundsConflict(
               inner_bounds->width, outer_bounds->width, "width", error)) {
        return false;
      }
      if (!CheckBoundsConflict(
               inner_bounds->height, outer_bounds->height, "height", error)) {
        return false;
      }
      if (!CheckBoundsConflict(inner_bounds->min_width,
                               outer_bounds->min_width,
                               "minWidth",
                               error)) {
        return false;
      }
      if (!CheckBoundsConflict(inner_bounds->min_height,
                               outer_bounds->min_height,
                               "minHeight",
                               error)) {
        return false;
      }
      if (!CheckBoundsConflict(inner_bounds->max_width,
                               outer_bounds->max_width,
                               "maxWidth",
                               error)) {
        return false;
      }
      if (!CheckBoundsConflict(inner_bounds->max_height,
                               outer_bounds->max_height,
                               "maxHeight",
                               error)) {
        return false;
      }
    }

    CopyBoundsSpec(inner_bounds, &(params->content_spec));
    CopyBoundsSpec(outer_bounds, &(params->window_spec));
  } else {
    // Parse deprecated fields.
    // Due to a bug in NativeAppWindow::GetFrameInsets() on Windows and ChromeOS
    // the bounds set the position of the window and the size of the content.
    // This will be preserved as apps may be relying on this behavior.

    if (options.default_width.get())
      params->content_spec.bounds.set_width(*options.default_width.get());
    if (options.default_height.get())
      params->content_spec.bounds.set_height(*options.default_height.get());
    if (options.default_left.get())
      params->window_spec.bounds.set_x(*options.default_left.get());
    if (options.default_top.get())
      params->window_spec.bounds.set_y(*options.default_top.get());

    if (options.width.get())
      params->content_spec.bounds.set_width(*options.width.get());
    if (options.height.get())
      params->content_spec.bounds.set_height(*options.height.get());
    if (options.left.get())
      params->window_spec.bounds.set_x(*options.left.get());
    if (options.top.get())
      params->window_spec.bounds.set_y(*options.top.get());

    if (options.bounds.get()) {
      app_window::ContentBounds* bounds = options.bounds.get();
      if (bounds->width.get())
        params->content_spec.bounds.set_width(*bounds->width.get());
      if (bounds->height.get())
        params->content_spec.bounds.set_height(*bounds->height.get());
      if (bounds->left.get())
        params->window_spec.bounds.set_x(*bounds->left.get());
      if (bounds->top.get())
        params->window_spec.bounds.set_y(*bounds->top.get());
    }

    gfx::Size& minimum_size = params->content_spec.minimum_size;
    if (options.min_width.get())
      minimum_size.set_width(*options.min_width);
    if (options.min_height.get())
      minimum_size.set_height(*options.min_height);
    gfx::Size& maximum_size = params->content_spec.maximum_size;
    if (options.max_width.get())
      maximum_size.set_width(*options.max_width);
    if (options.max_height.get())
      maximum_size.set_height(*options.max_height);
  }

  return true;
}

AppWindow::Frame AppWindowCreateFunction::GetFrameFromString(
    const std::string& frame_string) {
  if (frame_string == kNoneFrameOption)
    return AppWindow::FRAME_NONE;

  return AppWindow::FRAME_CHROME;
}

bool AppWindowCreateFunction::GetFrameOptions(
    const app_window::CreateWindowOptions& options,
    AppWindow::CreateParams* create_params) {
  if (!options.frame)
    return true;

  DCHECK(options.frame->as_string || options.frame->as_frame_options);
  if (options.frame->as_string) {
    create_params->frame = GetFrameFromString(*options.frame->as_string);
    return true;
  }

  if (options.frame->as_frame_options->type)
    create_params->frame =
        GetFrameFromString(*options.frame->as_frame_options->type);

  if (options.frame->as_frame_options->color.get()) {
    if (create_params->frame != AppWindow::FRAME_CHROME) {
      error_ = app_window_constants::kColorWithFrameNone;
      return false;
    }

    if (!image_util::ParseHexColorString(
            *options.frame->as_frame_options->color,
            &create_params->active_frame_color)) {
      error_ = app_window_constants::kInvalidColorSpecification;
      return false;
    }

    create_params->has_frame_color = true;
    create_params->inactive_frame_color = create_params->active_frame_color;

    if (options.frame->as_frame_options->inactive_color.get()) {
      if (!image_util::ParseHexColorString(
              *options.frame->as_frame_options->inactive_color,
              &create_params->inactive_frame_color)) {
        error_ = app_window_constants::kInvalidColorSpecification;
        return false;
      }
    }

    return true;
  }

  if (options.frame->as_frame_options->inactive_color.get()) {
    error_ = app_window_constants::kInactiveColorWithoutColor;
    return false;
  }

  return true;
}

}  // namespace extensions
