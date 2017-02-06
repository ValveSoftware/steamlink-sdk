// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/simple_feature.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/feature_util.h"
#include "extensions/common/switches.h"

using crx_file::id_util::HashedIdInHex;

namespace extensions {

namespace {

// A singleton copy of the --whitelisted-extension-id so that we don't need to
// copy it from the CommandLine each time.
std::string* g_whitelisted_extension_id = NULL;

Feature::Availability IsAvailableToManifestForBind(
    const std::string& extension_id,
    Manifest::Type type,
    Manifest::Location location,
    int manifest_version,
    Feature::Platform platform,
    const Feature* feature) {
  return feature->IsAvailableToManifest(
      extension_id, type, location, manifest_version, platform);
}

Feature::Availability IsAvailableToContextForBind(const Extension* extension,
                                                  Feature::Context context,
                                                  const GURL& url,
                                                  Feature::Platform platform,
                                                  const Feature* feature) {
  return feature->IsAvailableToContext(extension, context, url, platform);
}

// TODO(aa): Can we replace all this manual parsing with JSON schema stuff?

void ParseVector(const base::Value* value,
                 std::vector<std::string>* vector) {
  const base::ListValue* list_value = NULL;
  if (!value->GetAsList(&list_value))
    return;

  vector->clear();
  size_t list_size = list_value->GetSize();
  vector->reserve(list_size);
  for (size_t i = 0; i < list_size; ++i) {
    std::string str_val;
    CHECK(list_value->GetString(i, &str_val));
    vector->push_back(str_val);
  }
  std::sort(vector->begin(), vector->end());
}

template<typename T>
void ParseEnum(const std::string& string_value,
               T* enum_value,
               const std::map<std::string, T>& mapping) {
  const auto& iter = mapping.find(string_value);
  if (iter == mapping.end())
    CRASH_WITH_MINIDUMP("Enum value not found: " + string_value);
  *enum_value = iter->second;
}

template<typename T>
void ParseEnum(const base::DictionaryValue* value,
               const std::string& property,
               T* enum_value,
               const std::map<std::string, T>& mapping) {
  std::string string_value;
  if (!value->GetString(property, &string_value))
    return;

  ParseEnum(string_value, enum_value, mapping);
}

template<typename T>
void ParseEnumVector(const base::Value* value,
                     std::vector<T>* enum_vector,
                     const std::map<std::string, T>& mapping) {
  enum_vector->clear();
  std::string property_string;
  if (value->GetAsString(&property_string)) {
    if (property_string == "all") {
      enum_vector->reserve(mapping.size());
      for (const auto& it : mapping)
        enum_vector->push_back(it.second);
    }
    std::sort(enum_vector->begin(), enum_vector->end());
    return;
  }

  std::vector<std::string> string_vector;
  ParseVector(value, &string_vector);
  enum_vector->reserve(string_vector.size());
  for (const auto& str : string_vector) {
    T enum_value = static_cast<T>(0);
    ParseEnum(str, &enum_value, mapping);
    enum_vector->push_back(enum_value);
  }
  std::sort(enum_vector->begin(), enum_vector->end());
}

void ParseURLPatterns(const base::DictionaryValue* value,
                      const std::string& key,
                      URLPatternSet* set) {
  const base::ListValue* matches = NULL;
  if (value->GetList(key, &matches)) {
    set->ClearPatterns();
    for (size_t i = 0; i < matches->GetSize(); ++i) {
      std::string pattern;
      CHECK(matches->GetString(i, &pattern));
      set->AddPattern(URLPattern(URLPattern::SCHEME_ALL, pattern));
    }
  }
}

// Gets a human-readable name for the given extension type, suitable for giving
// to developers in an error message.
std::string GetDisplayName(Manifest::Type type) {
  switch (type) {
    case Manifest::TYPE_UNKNOWN:
      return "unknown";
    case Manifest::TYPE_EXTENSION:
      return "extension";
    case Manifest::TYPE_HOSTED_APP:
      return "hosted app";
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
      return "legacy packaged app";
    case Manifest::TYPE_PLATFORM_APP:
      return "packaged app";
    case Manifest::TYPE_THEME:
      return "theme";
    case Manifest::TYPE_USER_SCRIPT:
      return "user script";
    case Manifest::TYPE_SHARED_MODULE:
      return "shared module";
    case Manifest::NUM_LOAD_TYPES:
      NOTREACHED();
  }
  NOTREACHED();
  return "";
}

// Gets a human-readable name for the given context type, suitable for giving
// to developers in an error message.
std::string GetDisplayName(Feature::Context context) {
  switch (context) {
    case Feature::UNSPECIFIED_CONTEXT:
      return "unknown";
    case Feature::BLESSED_EXTENSION_CONTEXT:
      // "privileged" is vague but hopefully the developer will understand that
      // means background or app window.
      return "privileged page";
    case Feature::UNBLESSED_EXTENSION_CONTEXT:
      // "iframe" is a bit of a lie/oversimplification, but that's the most
      // common unblessed context.
      return "extension iframe";
    case Feature::CONTENT_SCRIPT_CONTEXT:
      return "content script";
    case Feature::WEB_PAGE_CONTEXT:
      return "web page";
    case Feature::BLESSED_WEB_PAGE_CONTEXT:
      return "hosted app";
    case Feature::WEBUI_CONTEXT:
      return "webui";
    case Feature::SERVICE_WORKER_CONTEXT:
      return "service worker";
  }
  NOTREACHED();
  return "";
}

// Gets a human-readable list of the display names (pluralized, comma separated
// with the "and" in the correct place) for each of |enum_types|.
template <typename EnumType>
std::string ListDisplayNames(const std::vector<EnumType>& enum_types) {
  std::string display_name_list;
  for (size_t i = 0; i < enum_types.size(); ++i) {
    // Pluralize type name.
    display_name_list += GetDisplayName(enum_types[i]) + "s";
    // Comma-separate entries, with an Oxford comma if there is more than 2
    // total entries.
    if (enum_types.size() > 2) {
      if (i < enum_types.size() - 2)
        display_name_list += ", ";
      else if (i == enum_types.size() - 2)
        display_name_list += ", and ";
    } else if (enum_types.size() == 2 && i == 0) {
      display_name_list += " and ";
    }
  }
  return display_name_list;
}

bool IsCommandLineSwitchEnabled(const std::string& switch_name) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switch_name + "=1"))
    return true;
  if (command_line->HasSwitch(std::string("enable-") + switch_name))
    return true;
  return false;
}

bool IsWhitelistedForTest(const std::string& extension_id) {
  // TODO(jackhou): Delete the commandline whitelisting mechanism.
  // Since it is only used it tests, ideally it should not be set via the
  // commandline. At the moment the commandline is used as a mechanism to pass
  // the id to the renderer process.
  if (!g_whitelisted_extension_id) {
    g_whitelisted_extension_id = new std::string(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kWhitelistedExtensionID));
  }
  return !g_whitelisted_extension_id->empty() &&
         *g_whitelisted_extension_id == extension_id;
}

}  // namespace

SimpleFeature::ScopedWhitelistForTest::ScopedWhitelistForTest(
    const std::string& id)
    : previous_id_(g_whitelisted_extension_id) {
  g_whitelisted_extension_id = new std::string(id);
}

SimpleFeature::ScopedWhitelistForTest::~ScopedWhitelistForTest() {
  delete g_whitelisted_extension_id;
  g_whitelisted_extension_id = previous_id_;
}

struct SimpleFeature::Mappings {
  Mappings() {
    extension_types["extension"] = Manifest::TYPE_EXTENSION;
    extension_types["theme"] = Manifest::TYPE_THEME;
    extension_types["legacy_packaged_app"] = Manifest::TYPE_LEGACY_PACKAGED_APP;
    extension_types["hosted_app"] = Manifest::TYPE_HOSTED_APP;
    extension_types["platform_app"] = Manifest::TYPE_PLATFORM_APP;
    extension_types["shared_module"] = Manifest::TYPE_SHARED_MODULE;

    contexts["blessed_extension"] = Feature::BLESSED_EXTENSION_CONTEXT;
    contexts["unblessed_extension"] = Feature::UNBLESSED_EXTENSION_CONTEXT;
    contexts["content_script"] = Feature::CONTENT_SCRIPT_CONTEXT;
    contexts["web_page"] = Feature::WEB_PAGE_CONTEXT;
    contexts["blessed_web_page"] = Feature::BLESSED_WEB_PAGE_CONTEXT;
    contexts["webui"] = Feature::WEBUI_CONTEXT;
    contexts["extension_service_worker"] = Feature::SERVICE_WORKER_CONTEXT;

    locations["component"] = SimpleFeature::COMPONENT_LOCATION;
    locations["external_component"] =
        SimpleFeature::EXTERNAL_COMPONENT_LOCATION;
    locations["policy"] = SimpleFeature::POLICY_LOCATION;

    platforms["chromeos"] = Feature::CHROMEOS_PLATFORM;
    platforms["linux"] = Feature::LINUX_PLATFORM;
    platforms["mac"] = Feature::MACOSX_PLATFORM;
    platforms["win"] = Feature::WIN_PLATFORM;
  }

  std::map<std::string, Manifest::Type> extension_types;
  std::map<std::string, Feature::Context> contexts;
  std::map<std::string, SimpleFeature::Location> locations;
  std::map<std::string, Feature::Platform> platforms;
};

SimpleFeature::SimpleFeature()
    : location_(UNSPECIFIED_LOCATION),
      min_manifest_version_(0),
      max_manifest_version_(0),
      component_extensions_auto_granted_(true) {}

SimpleFeature::~SimpleFeature() {}

bool SimpleFeature::HasDependencies() const {
  return !dependencies_.empty();
}

void SimpleFeature::AddFilter(std::unique_ptr<SimpleFeatureFilter> filter) {
  filters_.push_back(std::move(filter));
}

std::string SimpleFeature::Parse(const base::DictionaryValue* dictionary) {
  static base::LazyInstance<SimpleFeature::Mappings> mappings =
      LAZY_INSTANCE_INITIALIZER;

  no_parent_ = false;
  for (base::DictionaryValue::Iterator it(*dictionary);
      !it.IsAtEnd();
      it.Advance()) {
    std::string key = it.key();
    const base::Value* value = &it.value();
    if (key == "matches") {
      ParseURLPatterns(dictionary, "matches", &matches_);
    } else if (key == "blacklist") {
      ParseVector(value, &blacklist_);
    } else if (key == "whitelist") {
      ParseVector(value, &whitelist_);
    } else if (key == "dependencies") {
      ParseVector(value, &dependencies_);
    } else if (key == "extension_types") {
      ParseEnumVector<Manifest::Type>(value, &extension_types_,
                                      mappings.Get().extension_types);
    } else if (key == "contexts") {
      ParseEnumVector<Context>(value, &contexts_,
                               mappings.Get().contexts);
    } else if (key == "location") {
      ParseEnum<Location>(dictionary, "location", &location_,
                          mappings.Get().locations);
    } else if (key == "platforms") {
      ParseEnumVector<Platform>(value, &platforms_,
                                mappings.Get().platforms);
    } else if (key == "min_manifest_version") {
      dictionary->GetInteger("min_manifest_version", &min_manifest_version_);
    } else if (key == "max_manifest_version") {
      dictionary->GetInteger("max_manifest_version", &max_manifest_version_);
    } else if (key == "noparent") {
      dictionary->GetBoolean("noparent", &no_parent_);
    } else if (key == "component_extensions_auto_granted") {
      dictionary->GetBoolean("component_extensions_auto_granted",
                             &component_extensions_auto_granted_);
    } else if (key == "command_line_switch") {
      dictionary->GetString("command_line_switch", &command_line_switch_);
    }
  }

  // NOTE: ideally we'd sanity check that "matches" can be specified if and
  // only if there's a "web_page" or "webui" context, but without
  // (Simple)Features being aware of their own heirarchy this is impossible.
  //
  // For example, we might have feature "foo" available to "web_page" context
  // and "matches" google.com/*. Then a sub-feature "foo.bar" might override
  // "matches" to be chromium.org/*. That sub-feature doesn't need to specify
  // "web_page" context because it's inherited, but we don't know that here.

  std::string result;
  for (const auto& filter : filters_) {
    result = filter->Parse(dictionary);
    if (!result.empty())
      break;
  }

  return result;
}

Feature::Availability SimpleFeature::IsAvailableToManifest(
    const std::string& extension_id,
    Manifest::Type type,
    Manifest::Location location,
    int manifest_version,
    Platform platform) const {
  // Check extension type first to avoid granting platform app permissions
  // to component extensions.
  // HACK(kalman): user script -> extension. Solve this in a more generic way
  // when we compile feature files.
  Manifest::Type type_to_check = (type == Manifest::TYPE_USER_SCRIPT) ?
      Manifest::TYPE_EXTENSION : type;
  if (!extension_types_.empty() &&
      !ContainsValue(extension_types_, type_to_check)) {
    return CreateAvailability(INVALID_TYPE, type);
  }

  if (IsIdInBlacklist(extension_id))
    return CreateAvailability(FOUND_IN_BLACKLIST, type);

  // TODO(benwells): don't grant all component extensions.
  // See http://crbug.com/370375 for more details.
  // Component extensions can access any feature.
  // NOTE: Deliberately does not match EXTERNAL_COMPONENT.
  if (component_extensions_auto_granted_ && location == Manifest::COMPONENT)
    return CreateAvailability(IS_AVAILABLE, type);

  if (!whitelist_.empty() && !IsIdInWhitelist(extension_id) &&
      !IsWhitelistedForTest(extension_id)) {
    return CreateAvailability(NOT_FOUND_IN_WHITELIST, type);
  }

  if (!MatchesManifestLocation(location))
    return CreateAvailability(INVALID_LOCATION, type);

  if (!platforms_.empty() && !ContainsValue(platforms_, platform))
    return CreateAvailability(INVALID_PLATFORM, type);

  if (min_manifest_version_ != 0 && manifest_version < min_manifest_version_)
    return CreateAvailability(INVALID_MIN_MANIFEST_VERSION, type);

  if (max_manifest_version_ != 0 && manifest_version > max_manifest_version_)
    return CreateAvailability(INVALID_MAX_MANIFEST_VERSION, type);

  if (!command_line_switch_.empty() &&
      !IsCommandLineSwitchEnabled(command_line_switch_)) {
    return CreateAvailability(MISSING_COMMAND_LINE_SWITCH, type);
  }

  for (const auto& filter : filters_) {
    Availability availability = filter->IsAvailableToManifest(
        extension_id, type, location, manifest_version, platform);
    if (!availability.is_available())
      return availability;
  }

  return CheckDependencies(base::Bind(&IsAvailableToManifestForBind,
                                      extension_id,
                                      type,
                                      location,
                                      manifest_version,
                                      platform));
}

Feature::Availability SimpleFeature::IsAvailableToContext(
    const Extension* extension,
    SimpleFeature::Context context,
    const GURL& url,
    SimpleFeature::Platform platform) const {
  if (extension) {
    Availability result = IsAvailableToManifest(extension->id(),
                                                extension->GetType(),
                                                extension->location(),
                                                extension->manifest_version(),
                                                platform);
    if (!result.is_available())
      return result;
  }

  // TODO(lazyboy): This isn't quite right for Extension Service Worker
  // extension API calls, since there's no guarantee that the extension is
  // "active" in current renderer process when the API permission check is
  // done.
  if (!contexts_.empty() && !ContainsValue(contexts_, context))
    return CreateAvailability(INVALID_CONTEXT, context);

  // TODO(kalman): Consider checking |matches_| regardless of context type.
  // Fewer surprises, and if the feature configuration wants to isolate
  // "matches" from say "blessed_extension" then they can use complex features.
  if ((context == WEB_PAGE_CONTEXT || context == WEBUI_CONTEXT) &&
      !matches_.MatchesURL(url)) {
    return CreateAvailability(INVALID_URL, url);
  }

  for (const auto& filter : filters_) {
    Availability availability =
        filter->IsAvailableToContext(extension, context, url, platform);
    if (!availability.is_available())
      return availability;
  }

  // TODO(kalman): Assert that if the context was a webpage or WebUI context
  // then at some point a "matches" restriction was checked.
  return CheckDependencies(base::Bind(
      &IsAvailableToContextForBind, extension, context, url, platform));
}

std::string SimpleFeature::GetAvailabilityMessage(
    AvailabilityResult result,
    Manifest::Type type,
    const GURL& url,
    Context context) const {
  switch (result) {
    case IS_AVAILABLE:
      return std::string();
    case NOT_FOUND_IN_WHITELIST:
    case FOUND_IN_BLACKLIST:
      return base::StringPrintf(
          "'%s' is not allowed for specified extension ID.",
          name().c_str());
    case INVALID_URL:
      return base::StringPrintf("'%s' is not allowed on %s.",
                                name().c_str(), url.spec().c_str());
    case INVALID_TYPE:
      return base::StringPrintf(
          "'%s' is only allowed for %s, but this is a %s.",
          name().c_str(),
          ListDisplayNames(std::vector<Manifest::Type>(
              extension_types_.begin(), extension_types_.end())).c_str(),
          GetDisplayName(type).c_str());
    case INVALID_CONTEXT:
      return base::StringPrintf(
          "'%s' is only allowed to run in %s, but this is a %s",
          name().c_str(),
          ListDisplayNames(std::vector<Context>(
              contexts_.begin(), contexts_.end())).c_str(),
          GetDisplayName(context).c_str());
    case INVALID_LOCATION:
      return base::StringPrintf(
          "'%s' is not allowed for specified install location.",
          name().c_str());
    case INVALID_PLATFORM:
      return base::StringPrintf(
          "'%s' is not allowed for specified platform.",
          name().c_str());
    case INVALID_MIN_MANIFEST_VERSION:
      return base::StringPrintf(
          "'%s' requires manifest version of at least %d.",
          name().c_str(),
          min_manifest_version_);
    case INVALID_MAX_MANIFEST_VERSION:
      return base::StringPrintf(
          "'%s' requires manifest version of %d or lower.",
          name().c_str(),
          max_manifest_version_);
    case NOT_PRESENT:
      return base::StringPrintf(
          "'%s' requires a different Feature that is not present.",
          name().c_str());
    case UNSUPPORTED_CHANNEL:
      return base::StringPrintf(
          "'%s' is unsupported in this version of the platform.",
          name().c_str());
    case MISSING_COMMAND_LINE_SWITCH:
      return base::StringPrintf(
          "'%s' requires the '%s' command line switch to be enabled.",
          name().c_str(), command_line_switch_.c_str());
  }

  NOTREACHED();
  return std::string();
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, GURL(),
                                     UNSPECIFIED_CONTEXT));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result, Manifest::Type type) const {
  return Availability(result, GetAvailabilityMessage(result, type, GURL(),
                                                     UNSPECIFIED_CONTEXT));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    const GURL& url) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, url,
                                     UNSPECIFIED_CONTEXT));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    Context context) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, GURL(),
                                     context));
}

bool SimpleFeature::IsInternal() const {
  return false;
}

bool SimpleFeature::IsIdInBlacklist(const std::string& extension_id) const {
  return IsIdInList(extension_id, blacklist_);
}

bool SimpleFeature::IsIdInWhitelist(const std::string& extension_id) const {
  return IsIdInList(extension_id, whitelist_);
}

// static
bool SimpleFeature::IsIdInArray(const std::string& extension_id,
                                const char* const array[],
                                size_t array_length) {
  if (!IsValidExtensionId(extension_id))
    return false;

  const char* const* start = array;
  const char* const* end = array + array_length;

  return ((std::find(start, end, extension_id) != end) ||
          (std::find(start, end, HashedIdInHex(extension_id)) != end));
}

// static
bool SimpleFeature::IsIdInList(const std::string& extension_id,
                               const std::vector<std::string>& list) {
  if (!IsValidExtensionId(extension_id))
    return false;

  return (ContainsValue(list, extension_id) ||
          ContainsValue(list, HashedIdInHex(extension_id)));
}

bool SimpleFeature::MatchesManifestLocation(
    Manifest::Location manifest_location) const {
  switch (location_) {
    case SimpleFeature::UNSPECIFIED_LOCATION:
      return true;
    case SimpleFeature::COMPONENT_LOCATION:
      return manifest_location == Manifest::COMPONENT;
    case SimpleFeature::EXTERNAL_COMPONENT_LOCATION:
      return manifest_location == Manifest::EXTERNAL_COMPONENT;
    case SimpleFeature::POLICY_LOCATION:
      return manifest_location == Manifest::EXTERNAL_POLICY ||
             manifest_location == Manifest::EXTERNAL_POLICY_DOWNLOAD;
  }
  NOTREACHED();
  return false;
}

Feature::Availability SimpleFeature::CheckDependencies(
    const base::Callback<Availability(const Feature*)>& checker) const {
  for (const auto& dep_name : dependencies_) {
    Feature* dependency =
        ExtensionAPI::GetSharedInstance()->GetFeatureDependency(dep_name);
    if (!dependency)
      return CreateAvailability(NOT_PRESENT);
    Availability dependency_availability = checker.Run(dependency);
    if (!dependency_availability.is_available())
      return dependency_availability;
  }
  return CreateAvailability(IS_AVAILABLE);
}

// static
bool SimpleFeature::IsValidExtensionId(const std::string& extension_id) {
  // Belt-and-suspenders philosophy here. We should be pretty confident by this
  // point that we've validated the extension ID format, but in case something
  // slips through, we avoid a class of attack where creative ID manipulation
  // leads to hash collisions.
  // 128 bits / 4 = 32 mpdecimal characters
  return (extension_id.length() == 32);
}

}  // namespace extensions
