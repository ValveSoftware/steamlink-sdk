// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/ping_manager.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/request_sender.h"
#include "components/update_client/updater_state.h"
#include "components/update_client/utils.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

namespace update_client {

namespace {

// Returns a string literal corresponding to the value of the downloader |d|.
const char* DownloaderToString(CrxDownloader::DownloadMetrics::Downloader d) {
  switch (d) {
    case CrxDownloader::DownloadMetrics::kUrlFetcher:
      return "direct";
    case CrxDownloader::DownloadMetrics::kBits:
      return "bits";
    default:
      return "unknown";
  }
}

// Returns a string representing a sequence of download complete events
// corresponding to each download metrics in |item|.
std::string BuildDownloadCompleteEventElements(const CrxUpdateItem* item) {
  using base::StringAppendF;
  std::string download_events;
  for (size_t i = 0; i != item->download_metrics.size(); ++i) {
    const CrxDownloader::DownloadMetrics& metrics = item->download_metrics[i];
    std::string event("<event eventtype=\"14\"");
    StringAppendF(&event, " eventresult=\"%d\"", metrics.error == 0);
    StringAppendF(&event, " downloader=\"%s\"",
                  DownloaderToString(metrics.downloader));
    if (metrics.error) {
      StringAppendF(&event, " errorcode=\"%d\"", metrics.error);
    }
    StringAppendF(&event, " url=\"%s\"", metrics.url.spec().c_str());

    // -1 means that the  byte counts are not known.
    if (metrics.downloaded_bytes != -1) {
      StringAppendF(&event, " downloaded=\"%s\"",
                    base::Int64ToString(metrics.downloaded_bytes).c_str());
    }
    if (metrics.total_bytes != -1) {
      StringAppendF(&event, " total=\"%s\"",
                    base::Int64ToString(metrics.total_bytes).c_str());
    }

    if (metrics.download_time_ms) {
      StringAppendF(&event, " download_time_ms=\"%s\"",
                    base::Uint64ToString(metrics.download_time_ms).c_str());
    }
    StringAppendF(&event, "/>");

    download_events += event;
  }
  return download_events;
}

// Returns a string representing one ping event for the update of an item.
// The event type for this ping event is 3.
std::string BuildUpdateCompleteEventElement(const CrxUpdateItem* item) {
  DCHECK(item->state == CrxUpdateItem::State::kNoUpdate ||
         item->state == CrxUpdateItem::State::kUpdated);

  using base::StringAppendF;

  std::string ping_event("<event eventtype=\"3\"");
  const int event_result = item->state == CrxUpdateItem::State::kUpdated;
  StringAppendF(&ping_event, " eventresult=\"%d\"", event_result);
  if (item->error_category)
    StringAppendF(&ping_event, " errorcat=\"%d\"", item->error_category);
  if (item->error_code)
    StringAppendF(&ping_event, " errorcode=\"%d\"", item->error_code);
  if (item->extra_code1)
    StringAppendF(&ping_event, " extracode1=\"%d\"", item->extra_code1);
  if (HasDiffUpdate(item))
    StringAppendF(&ping_event, " diffresult=\"%d\"", !item->diff_update_failed);
  if (item->diff_error_category) {
    StringAppendF(&ping_event, " differrorcat=\"%d\"",
                  item->diff_error_category);
  }
  if (item->diff_error_code)
    StringAppendF(&ping_event, " differrorcode=\"%d\"", item->diff_error_code);
  if (item->diff_extra_code1) {
    StringAppendF(&ping_event, " diffextracode1=\"%d\"",
                  item->diff_extra_code1);
  }
  if (!item->previous_fp.empty())
    StringAppendF(&ping_event, " previousfp=\"%s\"", item->previous_fp.c_str());
  if (!item->next_fp.empty())
    StringAppendF(&ping_event, " nextfp=\"%s\"", item->next_fp.c_str());
  StringAppendF(&ping_event, "/>");
  return ping_event;
}

// Returns a string representing one ping event for the uninstall of an item.
// The event type for this ping event is 4.
std::string BuildUninstalledEventElement(const CrxUpdateItem* item) {
  DCHECK(item->state == CrxUpdateItem::State::kUninstalled);

  using base::StringAppendF;

  std::string ping_event("<event eventtype=\"4\" eventresult=\"1\"");
  if (item->extra_code1)
    StringAppendF(&ping_event, " extracode1=\"%d\"", item->extra_code1);
  StringAppendF(&ping_event, "/>");
  return ping_event;
}

// Builds a ping message for the specified update item.
std::string BuildPing(const Configurator& config, const CrxUpdateItem* item) {
  const char app_element_format[] =
      "<app appid=\"%s\" version=\"%s\" nextversion=\"%s\">"
      "%s"
      "%s"
      "</app>";

  std::string ping_event;
  switch (item->state) {
    case CrxUpdateItem::State::kNoUpdate:  // Fall through.
    case CrxUpdateItem::State::kUpdated:
      ping_event = BuildUpdateCompleteEventElement(item);
      break;
    case CrxUpdateItem::State::kUninstalled:
      ping_event = BuildUninstalledEventElement(item);
      break;
    default:
      NOTREACHED();
      break;
  }

  const std::string app_element(base::StringPrintf(
      app_element_format,
      item->id.c_str(),                                    // "appid"
      item->previous_version.GetString().c_str(),          // "version"
      item->next_version.GetString().c_str(),              // "nextversion"
      ping_event.c_str(),                                  // ping event
      BuildDownloadCompleteEventElements(item).c_str()));  // download events

  // The ping request does not include any updater state.
  return BuildProtocolRequest(
      config.GetProdId(), config.GetBrowserVersion().GetString(),
      config.GetChannel(), config.GetLang(), config.GetOSLongName(),
      config.GetDownloadPreference(), app_element, "", nullptr);
}

// Sends a fire and forget ping. The instances of this class have no
// ownership and they self-delete upon completion. One instance of this class
// can send only one ping.
class PingSender {
 public:
  explicit PingSender(const scoped_refptr<Configurator>& config);
  ~PingSender();

  bool SendPing(const CrxUpdateItem* item);

 private:
  void OnRequestSenderComplete(int error,
                               const std::string& response,
                               int retry_after_sec);

  const scoped_refptr<Configurator> config_;
  std::unique_ptr<RequestSender> request_sender_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(PingSender);
};

PingSender::PingSender(const scoped_refptr<Configurator>& config)
    : config_(config) {}

PingSender::~PingSender() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void PingSender::OnRequestSenderComplete(int error,
                                         const std::string& response,
                                         int retry_after_sec) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delete this;
}

bool PingSender::SendPing(const CrxUpdateItem* item) {
  DCHECK(item);
  DCHECK(thread_checker_.CalledOnValidThread());

  auto urls(config_->PingUrl());
  if (item->component.requires_network_encryption)
    RemoveUnsecureUrls(&urls);

  if (urls.empty())
    return false;

  request_sender_.reset(new RequestSender(config_));
  request_sender_->Send(
      false, BuildPing(*config_, item), urls,
      base::Bind(&PingSender::OnRequestSenderComplete, base::Unretained(this)));
  return true;
}

}  // namespace

PingManager::PingManager(const scoped_refptr<Configurator>& config)
    : config_(config) {}

PingManager::~PingManager() {
}

bool PingManager::SendPing(const CrxUpdateItem* item) {
  std::unique_ptr<PingSender> ping_sender(new PingSender(config_));
  if (!ping_sender->SendPing(item))
    return false;

  // The ping sender object self-deletes after sending the ping asynchrously.
  ping_sender.release();
  return true;
}

}  // namespace update_client
