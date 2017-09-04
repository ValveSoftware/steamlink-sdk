// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_NOTIFICATION_TYPES_H_
#define EXTENSIONS_BROWSER_NOTIFICATION_TYPES_H_

#include <string>

#include "content/public/browser/notification_types.h"
#include "extensions/features/features.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS)
#error "Extensions must be enabled"
#endif

namespace extensions {

// Only notifications fired by the extensions module should be here. The
// extensions module should not listen to notifications fired by the
// embedder.
enum NotificationType {
  // WARNING: This need to match chrome/browser/chrome_notification_types.h.
  NOTIFICATION_EXTENSIONS_START = content::NOTIFICATION_CONTENT_END,

  // Sent when a CrxInstaller finishes. Source is the CrxInstaller that
  // finished. The details are the extension which was installed.
  NOTIFICATION_CRX_INSTALLER_DONE = NOTIFICATION_EXTENSIONS_START,

  // Sent when the known installed extensions have all been loaded.  In
  // testing scenarios this can happen multiple times if extensions are
  // unloaded and reloaded. The source is a BrowserContext*.
  //
  // DEPRECATED: Use ExtensionSystem::Get(browser_context)->ready().Post().
  NOTIFICATION_EXTENSIONS_READY_DEPRECATED,

  // Sent when a new extension is loaded. The details are an Extension, and
  // the source is a BrowserContext*.
  //
  // DEPRECATED: Use ExtensionRegistry::AddObserver instead.
  NOTIFICATION_EXTENSION_LOADED_DEPRECATED,

  // An error occured while attempting to load an extension. The details are a
  // string with details about why the load failed.
  NOTIFICATION_EXTENSION_LOAD_ERROR,

  // Sent when an extension is enabled. Under most circumstances, listeners will
  // want to use ExtensionRegistryObserver::OnExtensionLoaded(). This
  // notification is only fired when the "Enable" button is hit in the
  // extensions tab. The details are an Extension, and the source is a
  // BrowserContext*.
  NOTIFICATION_EXTENSION_ENABLED,

  // Sent when attempting to load a new extension, but they are disabled. The
  // details are an Extension, and the source is a BrowserContext*.
  NOTIFICATION_EXTENSION_UPDATE_DISABLED,

  // Sent when an extension's permissions change. The details are an
  // UpdatedExtensionPermissionsInfo, and the source is a BrowserContext*.
  NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,

  // An error occured during extension install. The details are a string with
  // details about why the install failed.
  NOTIFICATION_EXTENSION_INSTALL_ERROR,

  // Sent when an extension uninstall is not allowed because the extension is
  // not user manageable.  The details are an Extension, and the source is a
  // BrowserContext*.
  NOTIFICATION_EXTENSION_UNINSTALL_NOT_ALLOWED,

  // Sent when an extension is unloaded. This happens when an extension is
  // uninstalled or disabled. The details are an UnloadedExtensionInfo, and the
  // source is a BrowserContext*.
  //
  // Note that when this notification is sent, the ExtensionService and the
  // ExtensionRegistry have already removed the extension from their internal
  // state.
  //
  // DEPRECATED: Use ExtensionRegistry::AddObserver instead.
  NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,

  // Sent when an Extension object is removed from ExtensionService. This
  // can happen when an extension is uninstalled, upgraded, or blacklisted,
  // including all cases when the Extension is deleted. The details are an
  // Extension, and the source is a BrowserContext*.
  NOTIFICATION_EXTENSION_REMOVED,

  // Sent after a new ExtensionHost* is created. The details are
  // an ExtensionHost* and the source is a BrowserContext*.
  NOTIFICATION_EXTENSION_HOST_CREATED,

  // Sent before an ExtensionHost* is destroyed. The details are
  // an ExtensionHost* and the source is a BrowserContext*.
  NOTIFICATION_EXTENSION_HOST_DESTROYED,

  // Sent by an ExtensionHost* when it has finished its initial page load,
  // including any external resources.
  // The details are an ExtensionHost* and the source is a BrowserContext*.
  NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,

  // Sent by an ExtensionHost* when its render view requests closing through
  // window.close(). The details are an ExtensionHost* and the source is a
  // BrowserContext*.
  NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,

  // Sent when extension render process ends (whether it crashes or closes). The
  // details are an ExtensionHost* and the source is a BrowserContext*. Not sent
  // during browser shutdown.
  NOTIFICATION_EXTENSION_PROCESS_TERMINATED,

  // Sent when a background page is ready so other components can load.
  NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,

  // Sent when the count of page actions has changed. Note that some of them
  // may not apply to the current page. The source is a LocationBar*. There
  // are no details.
  NOTIFICATION_EXTENSION_PAGE_ACTION_COUNT_CHANGED,

  // Sent when an extension command has been removed. The source is the
  // BrowserContext* and the details is an ExtensionCommandRemovedDetails
  // consisting of std::strings representing an extension ID, the name of the
  // command being removed, and the accelerator associated with the command.
  NOTIFICATION_EXTENSION_COMMAND_REMOVED,

  // Sent when an extension command has been added. The source is the
  // BrowserContext* and the details is a std::pair of two std::string objects
  // (an extension ID and the name of the command being added).
  NOTIFICATION_EXTENSION_COMMAND_ADDED,

  // Sent when an extension command shortcut for a browser action is activated
  // on Mac. The source is the BrowserContext* and the details is a std::pair of
  // a std::string containing an extension ID and a gfx::NativeWindow for the
  // associated window.
  NOTIFICATION_EXTENSION_COMMAND_BROWSER_ACTION_MAC,

  // Sent when an extension command shortcut for a page action is activated
  // on Mac. The source is the BrowserContext* and the details is a std::pair of
  // a
  // std::string containing an extension ID and a gfx::NativeWindow for the
  // associated window.
  NOTIFICATION_EXTENSION_COMMAND_PAGE_ACTION_MAC,

  // Sent by an extension to notify the browser about the results of a unit
  // test.
  NOTIFICATION_EXTENSION_TEST_PASSED,
  NOTIFICATION_EXTENSION_TEST_FAILED,

  // Sent by extension test javascript code, typically in a browser test. The
  // sender is a std::string representing the extension id, and the details
  // are a std::string with some message. This is particularly useful when you
  // want to have C++ code wait for javascript code to do something.
  NOTIFICATION_EXTENSION_TEST_MESSAGE,

  // Sent when an bookmarks extensions API function was successfully invoked.
  // The source is the id of the extension that invoked the function, and the
  // details are a pointer to the const BookmarksFunction in question.
  NOTIFICATION_EXTENSION_BOOKMARKS_API_INVOKED,

  // Sent when a downloads extensions API event is fired. The source is an
  // ExtensionDownloadsEventRouter::NotificationSource, and the details is a
  // std::string containing json. Used for testing.
  NOTIFICATION_EXTENSION_DOWNLOADS_EVENT,

  // Sent when an omnibox extension has sent back omnibox suggestions. The
  // source is the BrowserContext*, and the details are an
  // extensions::api::omnibox::SendSuggestions::Params object.
  NOTIFICATION_EXTENSION_OMNIBOX_SUGGESTIONS_READY,

  // Sent when the user accepts the input in an extension omnibox keyword
  // session. The source is the BrowserContext*.
  NOTIFICATION_EXTENSION_OMNIBOX_INPUT_ENTERED,

  // Sent when an omnibox extension has updated the default suggestion. The
  // source is the BrowserContext*.
  NOTIFICATION_EXTENSION_OMNIBOX_DEFAULT_SUGGESTION_CHANGED,

  // Sent when the extension updater starts checking for updates to installed
  // extensions. The source is a BrowserContext*, and there are no details.
  NOTIFICATION_EXTENSION_UPDATING_STARTED,

  // The extension updater found an update and will attempt to download and
  // install it. The source is a BrowserContext*, and the details are an
  // extensions::UpdateDetails object with the extension id and version of the
  // found update.
  NOTIFICATION_EXTENSION_UPDATE_FOUND,

  // Sent when there are new user scripts available.  The details are a
  // pointer to SharedMemory containing the new scripts.
  NOTIFICATION_USER_SCRIPTS_UPDATED,
  NOTIFICATION_EXTENSIONS_END
};

struct ExtensionCommandRemovedDetails {
  ExtensionCommandRemovedDetails(const std::string& extension_id,
                                 const std::string& command_name,
                                 const std::string& accelerator);

  std::string extension_id;
  std::string command_name;
  std::string accelerator;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_NOTIFICATION_TYPES_H_
