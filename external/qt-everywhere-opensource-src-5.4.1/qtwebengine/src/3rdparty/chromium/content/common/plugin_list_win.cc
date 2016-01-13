// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/plugin_list.h"

#include <set>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/file_version_info_win.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/pe_image.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "content/common/plugin_constants_win.h"

namespace content {
namespace {

const base::char16 kRegistryApps[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths";
const base::char16 kRegistryFirefox[] = L"firefox.exe";
const base::char16 kRegistryAcrobat[] = L"Acrobat.exe";
const base::char16 kRegistryAcrobatReader[] = L"AcroRd32.exe";
const base::char16 kRegistryWindowsMedia[] = L"wmplayer.exe";
const base::char16 kRegistryQuickTime[] = L"QuickTimePlayer.exe";
const base::char16 kRegistryPath[] = L"Path";
const base::char16 kRegistryFirefoxInstalled[] =
    L"SOFTWARE\\Mozilla\\Mozilla Firefox";
const base::char16 kRegistryJava[] =
    L"Software\\JavaSoft\\Java Runtime Environment";
const base::char16 kRegistryBrowserJavaVersion[] = L"BrowserJavaVersion";
const base::char16 kRegistryCurrentJavaVersion[] = L"CurrentVersion";
const base::char16 kRegistryJavaHome[] = L"JavaHome";
const base::char16 kJavaDeploy1[] = L"npdeploytk.dll";
const base::char16 kJavaDeploy2[] = L"npdeployjava1.dll";

base::FilePath AppendPluginsDir(const base::FilePath& path) {
  return path.AppendASCII("plugins");
}

// Gets the directory where the application data and libraries exist.  This
// may be a versioned subdirectory, or it may be the same directory as the
// GetExeDirectory(), depending on the embedder's implementation.
// Path is an output parameter to receive the path.
void GetAppDirectory(std::set<base::FilePath>* plugin_dirs) {
  base::FilePath app_path;
  if (!PathService::Get(base::DIR_MODULE, &app_path))
    return;
  plugin_dirs->insert(AppendPluginsDir(app_path));
}

// Gets the directory where the launching executable resides on disk.
// Path is an output parameter to receive the path.
void GetExeDirectory(std::set<base::FilePath>* plugin_dirs) {
  base::FilePath exe_path;
  if (!PathService::Get(base::DIR_EXE, &exe_path))
    return;
  plugin_dirs->insert(AppendPluginsDir(exe_path));
}

// Gets the installed path for a registered app.
bool GetInstalledPath(const base::char16* app, base::FilePath* out) {
  base::string16 reg_path(kRegistryApps);
  reg_path.append(L"\\");
  reg_path.append(app);

  base::win::RegKey hkcu_key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  base::string16 path;
  // As of Win7 AppPaths can also be registered in HKCU: http://goo.gl/UgFOf.
  if (base::win::GetVersion() >= base::win::VERSION_WIN7 &&
      hkcu_key.ReadValue(kRegistryPath, &path) == ERROR_SUCCESS) {
    *out = base::FilePath(path);
    return true;
  } else {
    base::win::RegKey hklm_key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ);
    if (hklm_key.ReadValue(kRegistryPath, &path) == ERROR_SUCCESS) {
      *out = base::FilePath(path);
      return true;
    }
  }

  return false;
}

// Search the registry at the given path and detect plugin directories.
void GetPluginsInRegistryDirectory(
    HKEY root_key,
    const base::string16& registry_folder,
    std::set<base::FilePath>* plugin_dirs) {
  for (base::win::RegistryKeyIterator iter(root_key, registry_folder.c_str());
       iter.Valid(); ++iter) {
    // Use the registry to gather plugin across the file system.
    base::string16 reg_path = registry_folder;
    reg_path.append(L"\\");
    reg_path.append(iter.Name());
    base::win::RegKey key(root_key, reg_path.c_str(), KEY_READ);

    base::string16 path;
    if (key.ReadValue(kRegistryPath, &path) == ERROR_SUCCESS)
      plugin_dirs->insert(base::FilePath(path));
  }
}

// Enumerate through the registry key to find all installed FireFox paths.
// FireFox 3 beta and version 2 can coexist. See bug: 1025003
void GetFirefoxInstalledPaths(std::vector<base::FilePath>* out) {
  base::win::RegistryKeyIterator it(HKEY_LOCAL_MACHINE,
                                    kRegistryFirefoxInstalled);
  for (; it.Valid(); ++it) {
    base::string16 full_path = base::string16(kRegistryFirefoxInstalled) +
        L"\\" + it.Name() + L"\\Main";
    base::win::RegKey key(HKEY_LOCAL_MACHINE, full_path.c_str(), KEY_READ);
    base::string16 install_dir;
    if (key.ReadValue(L"Install Directory", &install_dir) != ERROR_SUCCESS)
      continue;
    out->push_back(base::FilePath(install_dir));
  }
}

// Get plugin directory locations from the Firefox install path.  This is kind
// of a kludge, but it helps us locate the flash player for users that
// already have it for firefox.  Not having to download yet-another-plugin
// is a good thing.
void GetFirefoxDirectory(std::set<base::FilePath>* plugin_dirs) {
  std::vector<base::FilePath> paths;
  GetFirefoxInstalledPaths(&paths);
  for (unsigned int i = 0; i < paths.size(); ++i) {
    plugin_dirs->insert(AppendPluginsDir(paths[i]));
  }

  base::FilePath firefox_app_data_plugin_path;
  if (PathService::Get(base::DIR_APP_DATA, &firefox_app_data_plugin_path)) {
    firefox_app_data_plugin_path =
        firefox_app_data_plugin_path.AppendASCII("Mozilla");
    plugin_dirs->insert(AppendPluginsDir(firefox_app_data_plugin_path));
  }
}

// Hardcoded logic to detect Acrobat plugins locations.
void GetAcrobatDirectory(std::set<base::FilePath>* plugin_dirs) {
  base::FilePath path;
  if (!GetInstalledPath(kRegistryAcrobatReader, &path) &&
      !GetInstalledPath(kRegistryAcrobat, &path)) {
    return;
  }

  plugin_dirs->insert(path.Append(L"Browser"));
}

// Hardcoded logic to detect QuickTime plugin location.
void GetQuicktimeDirectory(std::set<base::FilePath>* plugin_dirs) {
  base::FilePath path;
  if (GetInstalledPath(kRegistryQuickTime, &path))
    plugin_dirs->insert(AppendPluginsDir(path));
}

// Hardcoded logic to detect Windows Media Player plugin location.
void GetWindowsMediaDirectory(std::set<base::FilePath>* plugin_dirs) {
  base::FilePath path;
  if (GetInstalledPath(kRegistryWindowsMedia, &path))
    plugin_dirs->insert(path);
}

// Hardcoded logic to detect Java plugin location.
void GetJavaDirectory(std::set<base::FilePath>* plugin_dirs) {
  // Load the new NPAPI Java plugin
  // 1. Open the main JRE key under HKLM
  base::win::RegKey java_key(HKEY_LOCAL_MACHINE, kRegistryJava,
                             KEY_QUERY_VALUE);

  // 2. Read the current Java version
  base::string16 java_version;
  if (java_key.ReadValue(kRegistryBrowserJavaVersion, &java_version) !=
      ERROR_SUCCESS) {
    java_key.ReadValue(kRegistryCurrentJavaVersion, &java_version);
  }

  if (!java_version.empty()) {
    java_key.OpenKey(java_version.c_str(), KEY_QUERY_VALUE);

    // 3. Install path of the JRE binaries is specified in "JavaHome"
    //    value under the Java version key.
    base::string16 java_plugin_directory;
    if (java_key.ReadValue(kRegistryJavaHome, &java_plugin_directory) ==
        ERROR_SUCCESS) {
      // 4. The new plugin resides under the 'bin/new_plugin'
      //    subdirectory.
      DCHECK(!java_plugin_directory.empty());
      java_plugin_directory.append(L"\\bin\\new_plugin");

      // 5. We don't know the exact name of the DLL but it's in the form
      //    NP*.dll so just invoke LoadPlugins on this path.
      plugin_dirs->insert(base::FilePath(java_plugin_directory));
    }
  }
}

bool IsValid32BitImage(const base::FilePath& path) {
  base::MemoryMappedFile plugin_image;

  if (!plugin_image.InitializeAsImageSection(path))
    return false;

  base::win::PEImage image(plugin_image.data());

  PIMAGE_NT_HEADERS nt_headers = image.GetNTHeaders();
  return (nt_headers->FileHeader.Machine == IMAGE_FILE_MACHINE_I386);
}

// Returns true if the given plugins share at least one mime type.  This is used
// to differentiate newer versions of a plugin vs two plugins which happen to
// have the same filename.
bool HaveSharedMimeType(const WebPluginInfo& plugin1,
                        const WebPluginInfo& plugin2) {
  for (size_t i = 0; i < plugin1.mime_types.size(); ++i) {
    for (size_t j = 0; j < plugin2.mime_types.size(); ++j) {
      if (plugin1.mime_types[i].mime_type == plugin2.mime_types[j].mime_type)
        return true;
    }
  }

  return false;
}

// Compares Windows style version strings (i.e. 1,2,3,4).  Returns true if b's
// version is newer than a's, or false if it's equal or older.
bool IsNewerVersion(const base::string16& a, const base::string16& b) {
  std::vector<base::string16> a_ver, b_ver;
  base::SplitString(a, ',', &a_ver);
  base::SplitString(b, ',', &b_ver);
  if (a_ver.size() == 1 && b_ver.size() == 1) {
    base::SplitString(a, '.', &a_ver);
    base::SplitString(b, '.', &b_ver);
  }
  if (a_ver.size() != b_ver.size())
    return false;
  for (size_t i = 0; i < a_ver.size(); i++) {
    int cur_a, cur_b;
    base::StringToInt(a_ver[i], &cur_a);
    base::StringToInt(b_ver[i], &cur_b);

    if (cur_a > cur_b)
      return false;
    if (cur_a < cur_b)
      return true;
  }
  return false;
}

}  // namespace

bool PluginList::ReadWebPluginInfo(const base::FilePath& filename,
                                   WebPluginInfo* info) {
  // On windows, the way we get the mime types for the library is
  // to check the version information in the DLL itself.  This
  // will be a string of the format:  <type1>|<type2>|<type3>|...
  // For example:
  //     video/quicktime|audio/aiff|image/jpeg
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(filename));
  if (!version_info) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Could not get version info for plugin "
        << filename.value();
    return false;
  }

  FileVersionInfoWin* version_info_win =
      static_cast<FileVersionInfoWin*>(version_info.get());

  info->name = version_info->product_name();
  info->desc = version_info->file_description();
  info->version = version_info->file_version();
  info->path = filename;

  // TODO(evan): Move the ParseMimeTypes code inline once Pepper is updated.
  if (!PluginList::ParseMimeTypes(
          base::UTF16ToASCII(version_info_win->GetStringValue(L"MIMEType")),
          base::UTF16ToASCII(version_info_win->GetStringValue(L"FileExtents")),
          version_info_win->GetStringValue(L"FileOpenName"),
          &info->mime_types)) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Plugin " << info->name << " has bad MIME types, skipping";
    return false;
  }

  return true;
}

void PluginList::GetPluginDirectories(
    std::vector<base::FilePath>* plugin_dirs) {
  if (PluginList::plugins_discovery_disabled_)
    return;

  // We use a set for uniqueness, which we require, over order, which we do not.
  std::set<base::FilePath> dirs;

  // Load from the application-specific area
  GetAppDirectory(&dirs);

  // Load from the executable area
  GetExeDirectory(&dirs);

  // Load Java
  GetJavaDirectory(&dirs);

  // Load firefox plugins too.  This is mainly to try to locate
  // a pre-installed Flash player.
  GetFirefoxDirectory(&dirs);

  // Firefox hard-codes the paths of some popular plugins to ensure that
  // the plugins are found.  We are going to copy this as well.
  GetAcrobatDirectory(&dirs);
  GetQuicktimeDirectory(&dirs);
  GetWindowsMediaDirectory(&dirs);

  for (std::set<base::FilePath>::iterator i = dirs.begin(); i != dirs.end(); ++i)
    plugin_dirs->push_back(*i);
}

void PluginList::GetPluginsInDir(
    const base::FilePath& path, std::vector<base::FilePath>* plugins) {
  WIN32_FIND_DATA find_file_data;
  HANDLE find_handle;

  base::string16 dir = path.value();
  // FindFirstFile requires that you specify a wildcard for directories.
  dir.append(L"\\NP*.DLL");

  find_handle = FindFirstFile(dir.c_str(), &find_file_data);
  if (find_handle == INVALID_HANDLE_VALUE)
    return;

  do {
    if (!(find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      base::FilePath filename = path.Append(find_file_data.cFileName);
      plugins->push_back(filename);
    }
  } while (FindNextFile(find_handle, &find_file_data) != 0);

  DCHECK(GetLastError() == ERROR_NO_MORE_FILES);
  FindClose(find_handle);
}

void PluginList::GetPluginPathsFromRegistry(
    std::vector<base::FilePath>* plugins) {
  if (PluginList::plugins_discovery_disabled_)
    return;

  std::set<base::FilePath> plugin_dirs;

  GetPluginsInRegistryDirectory(
      HKEY_CURRENT_USER, kRegistryMozillaPlugins, &plugin_dirs);
  GetPluginsInRegistryDirectory(
      HKEY_LOCAL_MACHINE, kRegistryMozillaPlugins, &plugin_dirs);

  for (std::set<base::FilePath>::iterator i = plugin_dirs.begin();
       i != plugin_dirs.end(); ++i) {
    plugins->push_back(*i);
  }
}

bool PluginList::ShouldLoadPluginUsingPluginList(
    const WebPluginInfo& info,
    std::vector<WebPluginInfo>* plugins) {
  bool should_check_version = true;
  {
    base::AutoLock lock(lock_);
    should_check_version =
        std::find(extra_plugin_paths_.begin(), extra_plugin_paths_.end(),
                  info.path) == extra_plugin_paths_.end();
  }
  // Version check for plugins that are not coming from |extra_plugin_paths_|.
  if (should_check_version) {
    for (size_t j = 0; j < plugins->size(); ++j) {
      base::FilePath::StringType plugin1 =
          StringToLowerASCII((*plugins)[j].path.BaseName().value());
      base::FilePath::StringType plugin2 =
          StringToLowerASCII(info.path.BaseName().value());
      if ((plugin1 == plugin2 && HaveSharedMimeType((*plugins)[j], info)) ||
          (plugin1 == kJavaDeploy1 && plugin2 == kJavaDeploy2) ||
          (plugin1 == kJavaDeploy2 && plugin2 == kJavaDeploy1)) {
        if (IsNewerVersion(info.version, (*plugins)[j].version))
          return false;  // We have loaded a plugin whose version is newer.
        plugins->erase(plugins->begin() + j);
        break;
      }
    }
  }

  // The checks below only apply to NPAPI plugins.
  if (info.type != WebPluginInfo::PLUGIN_TYPE_NPAPI)
    return true;

  {
    base::AutoLock lock(lock_);
    // If the plugin is in our internal list we should load it.
    for (size_t i = 0; i < internal_plugins_.size(); ++i) {
      if (info.path == internal_plugins_[i].path)
        return true;
    }
  }

  // Troublemakers.
  base::FilePath::StringType filename =
      StringToLowerASCII(info.path.BaseName().value());
  // Depends on XPCOM.
  if (filename == kMozillaActiveXPlugin)
    return false;

  // Disable the Yahoo Application State plugin as it crashes the plugin
  // process on return from NPObjectStub::OnInvoke. Please refer to
  // http://b/issue?id=1372124 for more information.
  if (filename == kYahooApplicationStatePlugin)
    return false;

  // Disable the WangWang protocol handler plugin (npww.dll) as it crashes
  // chrome during shutdown. Firefox also disables this plugin.
  // Please refer to http://code.google.com/p/chromium/issues/detail?id=3953
  // for more information.
  if (filename == kWanWangProtocolHandlerPlugin)
    return false;

  // We only work with newer versions of the Java plugin which use NPAPI only
  // and don't depend on XPCOM.
  if (filename == kJavaPlugin1 || filename == kJavaPlugin2) {
    std::vector<base::FilePath::StringType> ver;
    base::SplitString(info.version, '.', &ver);
    int major, minor, update;
    if (ver.size() == 4 &&
        base::StringToInt(ver[0], &major) &&
        base::StringToInt(ver[1], &minor) &&
        base::StringToInt(ver[2], &update)) {
      if (major == 6 && minor == 0 && update < 120)
        return false;  // Java SE6 Update 11 or older.
    }
  }

  // Special WMP handling:
  // If both the new and old WMP plugins exist, only load the new one.
  if (filename == kNewWMPPlugin) {
    for (size_t j = 0; j < plugins->size(); ++j) {
      if ((*plugins)[j].path.BaseName().value() == kOldWMPPlugin) {
        plugins->erase(plugins->begin() + j);
        break;
      }
    }

  } else if (filename == kOldWMPPlugin) {
    for (size_t j = 0; j < plugins->size(); ++j) {
      if ((*plugins)[j].path.BaseName().value() == kNewWMPPlugin)
        return false;
    }
  }

  base::FilePath plugin_path(info.path);
#if defined(ARCH_CPU_X86_64)
  // The plugin in question could be a 32 bit plugin which we cannot load.
  if (IsValid32BitImage(base::MakeAbsoluteFilePath(plugin_path)))
    return false;
#else
  // The plugin in question could be a 64 bit plugin which we cannot load.
  if (!IsValid32BitImage(base::MakeAbsoluteFilePath(plugin_path)))
    return false;
#endif
  return true;
}

}  // namespace content
