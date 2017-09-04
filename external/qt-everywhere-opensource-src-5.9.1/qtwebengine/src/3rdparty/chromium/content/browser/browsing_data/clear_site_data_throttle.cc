// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/clear_site_data_throttle.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

static const char* kClearSiteDataHeader = "Clear-Site-Data";

static const char* kTypesKey = "types";

// Pretty-printed log output.
static const char* kConsoleMessagePrefix = "Clear-Site-Data header on '%s': %s";
static const char* kClearingOneType = "Clearing %s.";
static const char* kClearingTwoTypes = "Clearing %s and %s.";
static const char* kClearingThreeTypes = "Clearing %s, %s, and %s.";

// Console logging. Adds a |text| message with |level| to |messages|.
void ConsoleLog(std::vector<ClearSiteDataThrottle::ConsoleMessage>* messages,
                const GURL& url,
                const std::string& text,
                ConsoleMessageLevel level) {
  messages->push_back({url, text, level});
}

bool AreExperimentalFeaturesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
}

// Represents the parameters as a single number to be recorded in a histogram.
int ParametersMask(bool clear_cookies, bool clear_storage, bool clear_cache) {
  return static_cast<int>(clear_cookies) * (1 << 0) +
         static_cast<int>(clear_storage) * (1 << 1) +
         static_cast<int>(clear_cache) * (1 << 2);
}

}  // namespace

// static
std::unique_ptr<NavigationThrottle>
ClearSiteDataThrottle::CreateThrottleForNavigation(NavigationHandle* handle) {
  if (AreExperimentalFeaturesEnabled())
    return base::WrapUnique(new ClearSiteDataThrottle(handle));

  return std::unique_ptr<NavigationThrottle>();
}

ClearSiteDataThrottle::ClearSiteDataThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle),
      clearing_in_progress_(false),
      weak_ptr_factory_(this) {}

ClearSiteDataThrottle::~ClearSiteDataThrottle() {
  // At the end of the navigation we finally have access to the correct
  // RenderFrameHost. Output the cached console messages. Prefix each sequence
  // of messages belonging to the same URL with |kConsoleMessagePrefix|.
  GURL last_seen_url;
  for (const ConsoleMessage& message : messages_) {
    if (message.url == last_seen_url) {
      navigation_handle()->GetRenderFrameHost()->AddMessageToConsole(
          message.level, message.text);
    } else {
      navigation_handle()->GetRenderFrameHost()->AddMessageToConsole(
          message.level,
          base::StringPrintf(kConsoleMessagePrefix, message.url.spec().c_str(),
                             message.text.c_str()));
    }

    last_seen_url = message.url;
  }
}

ClearSiteDataThrottle::ThrottleCheckResult
ClearSiteDataThrottle::WillStartRequest() {
  current_url_ = navigation_handle()->GetURL();
  return PROCEED;
}

ClearSiteDataThrottle::ThrottleCheckResult
ClearSiteDataThrottle::WillRedirectRequest() {
  // We are processing a redirect from url1 to url2. GetResponseHeaders()
  // contains headers from url1, but GetURL() is already equal to url2. Handle
  // the headers before updating the URL, so that |current_url_| corresponds
  // to the URL that sent the headers.
  HandleHeader();
  current_url_ = navigation_handle()->GetURL();

  return clearing_in_progress_ ? DEFER : PROCEED;
}

ClearSiteDataThrottle::ThrottleCheckResult
ClearSiteDataThrottle::WillProcessResponse() {
  HandleHeader();
  return clearing_in_progress_ ? DEFER : PROCEED;
}

void ClearSiteDataThrottle::HandleHeader() {
  NavigationHandleImpl* handle =
      static_cast<NavigationHandleImpl*>(navigation_handle());
  const net::HttpResponseHeaders* headers = handle->GetResponseHeaders();

  if (!headers || !headers->HasHeader(kClearSiteDataHeader))
    return;

  // Only accept the header on secure origins.
  if (!IsOriginSecure(current_url_)) {
    ConsoleLog(&messages_, current_url_, "Not supported for insecure origins.",
               CONSOLE_MESSAGE_LEVEL_ERROR);
    return;
  }

  std::string header_value;
  headers->GetNormalizedHeader(kClearSiteDataHeader, &header_value);

  bool clear_cookies;
  bool clear_storage;
  bool clear_cache;

  if (!ParseHeader(header_value, &clear_cookies, &clear_storage, &clear_cache,
                   &messages_)) {
    return;
  }

  // Record the call parameters.
  UMA_HISTOGRAM_ENUMERATION(
      "Navigation.ClearSiteData.Parameters",
      ParametersMask(clear_cookies, clear_storage, clear_cache), (1 << 3));

  // If the header is valid, clear the data for this browser context and origin.
  BrowserContext* browser_context =
      navigation_handle()->GetWebContents()->GetBrowserContext();
  url::Origin origin(current_url_);

  if (origin.unique()) {
    ConsoleLog(&messages_, current_url_, "Not supported for unique origins.",
               CONSOLE_MESSAGE_LEVEL_ERROR);
    return;
  }

  clearing_in_progress_ = true;
  clearing_started_ = base::TimeTicks::Now();
  GetContentClient()->browser()->ClearSiteData(
      browser_context, origin, clear_cookies, clear_storage, clear_cache,
      base::Bind(&ClearSiteDataThrottle::TaskFinished,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool ClearSiteDataThrottle::ParseHeader(const std::string& header,
                                        bool* clear_cookies,
                                        bool* clear_storage,
                                        bool* clear_cache,
                                        std::vector<ConsoleMessage>* messages) {
  if (!base::IsStringASCII(header)) {
    ConsoleLog(messages, current_url_, "Must only contain ASCII characters.",
               CONSOLE_MESSAGE_LEVEL_ERROR);
    return false;
  }

  std::unique_ptr<base::Value> parsed_header = base::JSONReader::Read(header);

  if (!parsed_header) {
    ConsoleLog(messages, current_url_, "Not a valid JSON.",
               CONSOLE_MESSAGE_LEVEL_ERROR);
    return false;
  }

  const base::DictionaryValue* dictionary = nullptr;
  const base::ListValue* types = nullptr;
  if (!parsed_header->GetAsDictionary(&dictionary) ||
      !dictionary->GetListWithoutPathExpansion(kTypesKey, &types)) {
    ConsoleLog(messages, current_url_,
               "Expecting a JSON dictionary with a 'types' field.",
               CONSOLE_MESSAGE_LEVEL_ERROR);
    return false;
  }

  DCHECK(types);

  *clear_cookies = false;
  *clear_storage = false;
  *clear_cache = false;

  std::vector<std::string> type_names;
  for (const std::unique_ptr<base::Value>& value : *types) {
    std::string type;
    value->GetAsString(&type);

    bool* datatype = nullptr;

    if (type == "cookies") {
      datatype = clear_cookies;
    } else if (type == "storage") {
      datatype = clear_storage;
    } else if (type == "cache") {
      datatype = clear_cache;
    } else {
      std::string serialized_type;
      JSONStringValueSerializer serializer(&serialized_type);
      serializer.Serialize(*value);
      ConsoleLog(
          messages, current_url_,
          base::StringPrintf("Invalid type: %s.", serialized_type.c_str()),
          CONSOLE_MESSAGE_LEVEL_ERROR);
      continue;
    }

    // Each data type should only be processed once.
    DCHECK(datatype);
    if (*datatype)
      continue;

    *datatype = true;
    type_names.push_back(type);
  }

  if (!*clear_cookies && !*clear_storage && !*clear_cache) {
    ConsoleLog(messages, current_url_,
               "No valid types specified in the 'types' field.",
               CONSOLE_MESSAGE_LEVEL_ERROR);
    return false;
  }

  // Pretty-print which types are to be cleared.
  std::string output;
  switch (type_names.size()) {
    case 1:
      output = base::StringPrintf(kClearingOneType, type_names[0].c_str());
      break;
    case 2:
      output = base::StringPrintf(kClearingTwoTypes, type_names[0].c_str(),
                                  type_names[1].c_str());
      break;
    case 3:
      output = base::StringPrintf(kClearingThreeTypes, type_names[0].c_str(),
                                  type_names[1].c_str(), type_names[2].c_str());
      break;
    default:
      NOTREACHED();
  }
  ConsoleLog(messages, current_url_, output, CONSOLE_MESSAGE_LEVEL_LOG);

  return true;
}

void ClearSiteDataThrottle::TaskFinished() {
  DCHECK(clearing_in_progress_);
  clearing_in_progress_ = false;

  UMA_HISTOGRAM_CUSTOM_TIMES("Navigation.ClearSiteData.Duration",
                             base::TimeTicks::Now() - clearing_started_,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromSeconds(1), 50);

  navigation_handle()->Resume();
}

}  // namespace content
