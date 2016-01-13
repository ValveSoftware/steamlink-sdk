// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/plugin_list.h"

#import <Carbon/Carbon.h>
#import <Foundation/Foundation.h>

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "base/native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"

using base::ScopedCFTypeRef;

namespace content {

namespace {

void GetPluginCommonDirectory(std::vector<base::FilePath>* plugin_dirs,
                              bool user) {
  // Note that there are no NSSearchPathDirectory constants for these
  // directories so we can't use Cocoa's NSSearchPathForDirectoriesInDomains().
  // Interestingly, Safari hard-codes the location (see
  // WebKit/WebKit/mac/Plugins/WebPluginDatabase.mm's +_defaultPlugInPaths).
  FSRef ref;
  OSErr err = FSFindFolder(user ? kUserDomain : kLocalDomain,
                           kInternetPlugInFolderType, false, &ref);

  if (err)
    return;

  plugin_dirs->push_back(base::FilePath(base::mac::PathFromFSRef(ref)));
}

// Returns true if the plugin should be prevented from loading.
bool IsBlacklistedPlugin(const WebPluginInfo& info) {
  // We blacklist Gears by included MIME type, since that is more stable than
  // its name. Be careful about adding any more plugins to this list though,
  // since it's easy to accidentally blacklist plugins that support lots of
  // MIME types.
  for (std::vector<WebPluginMimeType>::const_iterator i =
           info.mime_types.begin(); i != info.mime_types.end(); ++i) {
    // The Gears plugin is Safari-specific, so don't load it.
    if (i->mime_type == "application/x-googlegears")
      return true;
  }

  // Versions of Flip4Mac 2.3 before 2.3.6 often hang the renderer, so don't
  // load them.
  if (StartsWith(info.name,
                 base::ASCIIToUTF16("Flip4Mac Windows Media"), false) &&
      StartsWith(info.version, base::ASCIIToUTF16("2.3"), false)) {
    std::vector<base::string16> components;
    base::SplitString(info.version, '.', &components);
    int bugfix_version = 0;
    return (components.size() >= 3 &&
            base::StringToInt(components[2], &bugfix_version) &&
            bugfix_version < 6);
  }

  return false;
}

NSDictionary* GetMIMETypes(CFBundleRef bundle) {
  NSString* mime_filename =
      (NSString*)CFBundleGetValueForInfoDictionaryKey(bundle,
                     CFSTR("WebPluginMIMETypesFilename"));

  if (mime_filename) {

    // get the file

    NSString* mime_path =
        [NSString stringWithFormat:@"%@/Library/Preferences/%@",
         NSHomeDirectory(), mime_filename];
    NSDictionary* mime_file_dict =
        [NSDictionary dictionaryWithContentsOfFile:mime_path];

    // is it valid?

    bool valid_file = false;
    if (mime_file_dict) {
      NSString* l10n_name =
          [mime_file_dict objectForKey:@"WebPluginLocalizationName"];
      NSString* preferred_l10n = [[NSLocale currentLocale] localeIdentifier];
      if ([l10n_name isEqualToString:preferred_l10n])
        valid_file = true;
    }

    if (valid_file)
      return [mime_file_dict objectForKey:@"WebPluginMIMETypes"];

    // dammit, I didn't want to have to do this

    typedef void (*CreateMIMETypesPrefsPtr)(void);
    CreateMIMETypesPrefsPtr create_prefs_file =
        (CreateMIMETypesPrefsPtr)CFBundleGetFunctionPointerForName(
        bundle, CFSTR("BP_CreatePluginMIMETypesPreferences"));
    if (!create_prefs_file)
      return nil;
    create_prefs_file();

    // one more time

    mime_file_dict = [NSDictionary dictionaryWithContentsOfFile:mime_path];
    if (mime_file_dict)
      return [mime_file_dict objectForKey:@"WebPluginMIMETypes"];
    else
      return nil;

  } else {
    return (NSDictionary*)CFBundleGetValueForInfoDictionaryKey(bundle,
                              CFSTR("WebPluginMIMETypes"));
  }
}

bool ReadPlistPluginInfo(const base::FilePath& filename, CFBundleRef bundle,
                         WebPluginInfo* info) {
  NSDictionary* mime_types = GetMIMETypes(bundle);
  if (!mime_types)
    return false;  // no type info here; try elsewhere

  for (NSString* mime_type in [mime_types allKeys]) {
    NSDictionary* mime_dict = [mime_types objectForKey:mime_type];
    NSNumber* type_enabled = [mime_dict objectForKey:@"WebPluginTypeEnabled"];
    NSString* mime_desc = [mime_dict objectForKey:@"WebPluginTypeDescription"];
    NSArray* mime_exts = [mime_dict objectForKey:@"WebPluginExtensions"];

    // Skip any disabled types.
    if (type_enabled && ![type_enabled boolValue])
      continue;

    WebPluginMimeType mime;
    mime.mime_type = base::SysNSStringToUTF8([mime_type lowercaseString]);
    // Remove PDF from the list of types handled by QuickTime, since it provides
    // a worse experience than just downloading the PDF.
    if (mime.mime_type == "application/pdf" &&
        StartsWithASCII(filename.BaseName().value(), "QuickTime", false)) {
      continue;
    }

    if (mime_desc)
      mime.description = base::SysNSStringToUTF16(mime_desc);
    for (NSString* ext in mime_exts)
      mime.file_extensions.push_back(
          base::SysNSStringToUTF8([ext lowercaseString]));

    info->mime_types.push_back(mime);
  }

  NSString* plugin_name =
      (NSString*)CFBundleGetValueForInfoDictionaryKey(bundle,
      CFSTR("WebPluginName"));
  NSString* plugin_vers =
      (NSString*)CFBundleGetValueForInfoDictionaryKey(bundle,
      CFSTR("CFBundleShortVersionString"));
  NSString* plugin_desc =
      (NSString*)CFBundleGetValueForInfoDictionaryKey(bundle,
      CFSTR("WebPluginDescription"));

  if (plugin_name)
    info->name = base::SysNSStringToUTF16(plugin_name);
  else
    info->name = base::UTF8ToUTF16(filename.BaseName().value());
  info->path = filename;
  if (plugin_vers)
    info->version = base::SysNSStringToUTF16(plugin_vers);
  if (plugin_desc)
    info->desc = base::SysNSStringToUTF16(plugin_desc);
  else
    info->desc = base::UTF8ToUTF16(filename.BaseName().value());

  return true;
}

}  // namespace

bool PluginList::ReadWebPluginInfo(const base::FilePath &filename,
                                   WebPluginInfo* info) {
  // There are three ways to get information about plugin capabilities:
  // 1) a set of Info.plist keys, documented at
  // http://developer.apple.com/documentation/InternetWeb/Conceptual/WebKit_PluginProgTopic/Concepts/AboutPlugins.html .
  // 2) a set of STR# resources, documented at
  // https://developer.mozilla.org/En/Gecko_Plugin_API_Reference/Plug-in_Development_Overview .
  // 3) a NP_GetMIMEDescription() entry point, documented at
  // https://developer.mozilla.org/en/NP_GetMIMEDescription
  //
  // Mozilla supported (3), but WebKit never has, so no plugins rely on it. Most
  // browsers supported (2) and then added support for (1); Chromium originally
  // supported (2) and (1), but now supports only (1) as (2) is deprecated.
  //
  // For the Info.plist version, the data is formatted as follows (in text plist
  // format):
  //  {
  //    ... the usual plist keys ...
  //    WebPluginDescription = <<plugindescription>>;
  //    WebPluginMIMETypes = {
  //      <<type0mimetype>> = {
  //        WebPluginExtensions = (
  //                               <<type0fileextension0>>,
  //                               ...
  //                               <<type0fileextensionk>>,
  //                               );
  //        WebPluginTypeDescription = <<type0description>>;
  //      };
  //      <<type1mimetype>> = { ... };
  //      ...
  //      <<typenmimetype>> = { ... };
  //    };
  //    WebPluginName = <<pluginname>>;
  //  }
  //
  // Alternatively (and this is undocumented), rather than a WebPluginMIMETypes
  // key, there may be a WebPluginMIMETypesFilename key. If it is present, then
  // it is the name of a file in the user's preferences folder in which to find
  // the WebPluginMIMETypes key. If the key is present but the file doesn't
  // exist, we must load the plugin and call a specific function to have the
  // plugin create the file.

  ScopedCFTypeRef<CFURLRef> bundle_url(CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, (const UInt8*)filename.value().c_str(),
      filename.value().length(), true));
  if (!bundle_url) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "PluginLib::ReadWebPluginInfo could not create bundle URL";
    return false;
  }
  ScopedCFTypeRef<CFBundleRef> bundle(CFBundleCreate(kCFAllocatorDefault,
                                                     bundle_url.get()));
  if (!bundle) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "PluginLib::ReadWebPluginInfo could not create CFBundleRef";
    return false;
  }

  // preflight

  OSType type = 0;
  CFBundleGetPackageInfo(bundle.get(), &type, NULL);
  if (type != FOUR_CHAR_CODE('BRPL')) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "PluginLib::ReadWebPluginInfo bundle is not BRPL, is " << type;
    return false;
  }

  CFErrorRef error;
  Boolean would_load = CFBundlePreflightExecutable(bundle.get(), &error);
  if (!would_load) {
    ScopedCFTypeRef<CFStringRef> error_string(CFErrorCopyDescription(error));
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "PluginLib::ReadWebPluginInfo bundle failed preflight: "
        << base::SysCFStringRefToUTF8(error_string);
    return false;
  }

  // get the info

  if (ReadPlistPluginInfo(filename, bundle.get(), info))
    return true;

  // ... or not

  return false;
}

void PluginList::GetPluginDirectories(
    std::vector<base::FilePath>* plugin_dirs) {
  if (PluginList::plugins_discovery_disabled_)
    return;

  // Load from the user's area
  GetPluginCommonDirectory(plugin_dirs, true);

  // Load from the machine-wide area
  GetPluginCommonDirectory(plugin_dirs, false);
}

void PluginList::GetPluginsInDir(
    const base::FilePath& path, std::vector<base::FilePath>* plugins) {
  base::FileEnumerator enumerator(path,
                                  false, // not recursive
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.value().empty();
       path = enumerator.Next()) {
    plugins->push_back(path);
  }
}

bool PluginList::ShouldLoadPluginUsingPluginList(
    const WebPluginInfo& info,
    std::vector<WebPluginInfo>* plugins) {
  return !IsBlacklistedPlugin(info);
}

}  // namespace content
