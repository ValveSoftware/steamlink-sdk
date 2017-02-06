// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/dom_distiller_viewer_source.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/dom_distiller/content/browser/distiller_javascript_service_impl.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/content/browser/distiller_ui_handle.h"
#include "components/dom_distiller/content/common/distiller_page_notifier_service.mojom.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_request_view_base.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/dom_distiller/core/feedback_reporter.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/dom_distiller/core/viewer.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "grit/components_strings.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "ui/base/l10n/l10n_util.h"

namespace dom_distiller {

// Handles receiving data asynchronously for a specific entry, and passing
// it along to the data callback for the data source. Lifetime matches that of
// the current main frame's page in the Viewer instance.
class DomDistillerViewerSource::RequestViewerHandle
    : public DomDistillerRequestViewBase,
      public content::WebContentsObserver {
 public:
  RequestViewerHandle(content::WebContents* web_contents,
                      const std::string& expected_scheme,
                      const std::string& expected_request_path,
                      DistilledPagePrefs* distilled_page_prefs);
  ~RequestViewerHandle() override;

  // content::WebContentsObserver implementation:
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void WebContentsDestroyed() override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 private:
  // Sends JavaScript to the attached Viewer, buffering data if the viewer isn't
  // ready.
  void SendJavaScript(const std::string& buffer) override;

  // Cancels the current view request. Once called, no updates will be
  // propagated to the view, and the request to DomDistillerService will be
  // cancelled.
  void Cancel();

  // The scheme hosting the current view request;
  std::string expected_scheme_;

  // The query path for the current view request.
  std::string expected_request_path_;

  // Whether the page is sufficiently initialized to handle updates from the
  // distiller.
  bool waiting_for_page_ready_;

  // Temporary store of pending JavaScript if the page isn't ready to receive
  // data from distillation.
  std::string buffer_;
};

DomDistillerViewerSource::RequestViewerHandle::RequestViewerHandle(
    content::WebContents* web_contents,
    const std::string& expected_scheme,
    const std::string& expected_request_path,
    DistilledPagePrefs* distilled_page_prefs)
    : DomDistillerRequestViewBase(distilled_page_prefs),
      expected_scheme_(expected_scheme),
      expected_request_path_(expected_request_path),
      waiting_for_page_ready_(true) {
  content::WebContentsObserver::Observe(web_contents);
  distilled_page_prefs_->AddObserver(this);
}

DomDistillerViewerSource::RequestViewerHandle::~RequestViewerHandle() {
  distilled_page_prefs_->RemoveObserver(this);
}

void DomDistillerViewerSource::RequestViewerHandle::SendJavaScript(
    const std::string& buffer) {
  if (waiting_for_page_ready_) {
    buffer_ += buffer;
  } else {
    DCHECK(buffer_.empty());
    if (web_contents()) {
      RunIsolatedJavaScript(web_contents()->GetMainFrame(), buffer);
    }
  }
}

void DomDistillerViewerSource::RequestViewerHandle::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  const GURL& navigation = details.entry->GetURL();
  if (details.is_in_page || (navigation.SchemeIs(expected_scheme_.c_str()) &&
                             expected_request_path_ == navigation.query())) {
    // In-page navigations, as well as the main view request can be ignored.
    return;
  }

  Cancel();
}

void DomDistillerViewerSource::RequestViewerHandle::RenderProcessGone(
    base::TerminationStatus status) {
  Cancel();
}

void DomDistillerViewerSource::RequestViewerHandle::WebContentsDestroyed() {
  Cancel();
}

void DomDistillerViewerSource::RequestViewerHandle::Cancel() {
  // No need to listen for notifications.
  content::WebContentsObserver::Observe(NULL);

  // Schedule the Viewer for deletion. Ensures distillation is cancelled, and
  // any pending data stored in |buffer_| is released.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void DomDistillerViewerSource::RequestViewerHandle::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (render_frame_host->GetParent()) {
    return;
  }

  int64_t start_time_ms = url_utils::GetTimeFromDistillerUrl(validated_url);
  if (start_time_ms > 0) {
    base::TimeTicks start_time =
        base::TimeDelta::FromMilliseconds(start_time_ms) + base::TimeTicks();
    base::TimeDelta latency = base::TimeTicks::Now() - start_time;

    UMA_HISTOGRAM_TIMES("DomDistiller.Time.ViewerLoading", latency);
  }

  // No SendJavaScript() calls allowed before |buffer_| is run and cleared.
  waiting_for_page_ready_ = false;
  if (!buffer_.empty()) {
    RunIsolatedJavaScript(web_contents()->GetMainFrame(), buffer_);
    buffer_.clear();
  }
  // No need to Cancel() here.
}

DomDistillerViewerSource::DomDistillerViewerSource(
    DomDistillerServiceInterface* dom_distiller_service,
    const std::string& scheme,
    std::unique_ptr<DistillerUIHandle> ui_handle)
    : scheme_(scheme),
      dom_distiller_service_(dom_distiller_service),
      distiller_ui_handle_(std::move(ui_handle)) {}

DomDistillerViewerSource::~DomDistillerViewerSource() {
}

std::string DomDistillerViewerSource::GetSource() const {
  return scheme_ + "://";
}

void DomDistillerViewerSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return;
  content::RenderViewHost* render_view_host =
      render_frame_host->GetRenderViewHost();
  DCHECK(render_view_host);
  CHECK_EQ(0, render_view_host->GetEnabledBindings());

  if (kViewerCssPath == path) {
    std::string css = viewer::GetCss();
    callback.Run(base::RefCountedString::TakeString(&css));
    return;
  } else if (kViewerLoadingImagePath == path) {
    std::string image = viewer::GetLoadingImage();
    callback.Run(base::RefCountedString::TakeString(&image));
    return;
  } else if (base::StartsWith(path, kViewerSaveFontScalingPath,
                              base::CompareCase::SENSITIVE)) {
    double scale = 1.0;
    if (base::StringToDouble(
        path.substr(strlen(kViewerSaveFontScalingPath)), &scale)) {
      dom_distiller_service_->GetDistilledPagePrefs()->SetFontScaling(scale);
    }
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);
  // An empty |path| is invalid, but guard against it. If not empty, assume
  // |path| starts with '?', which is stripped away.
  const std::string path_after_query_separator =
      path.size() > 0 ? path.substr(1) : "";
  RequestViewerHandle* request_viewer_handle =
      new RequestViewerHandle(web_contents, scheme_, path_after_query_separator,
                              dom_distiller_service_->GetDistilledPagePrefs());
  std::unique_ptr<ViewerHandle> viewer_handle = viewer::CreateViewRequest(
      dom_distiller_service_, path, request_viewer_handle,
      web_contents->GetContainerBounds().size());

  GURL current_url(url_utils::GetValueForKeyInUrlPathQuery(path, kUrlKey));
  std::string unsafe_page_html = viewer::GetUnsafeArticleTemplateHtml(
      url_utils::GetOriginalUrlFromDistillerUrl(current_url).spec(),
      dom_distiller_service_->GetDistilledPagePrefs()->GetTheme(),
      dom_distiller_service_->GetDistilledPagePrefs()->GetFontFamily());

  // Add mojo service for JavaScript functionality. This is the receiving end
  // of this particular service.
  render_frame_host->GetInterfaceRegistry()->AddInterface(
      base::Bind(&CreateDistillerJavaScriptService,
          render_frame_host,
          distiller_ui_handle_.get()));

  // Tell the renderer that this is currently a distilled page.
  mojom::DistillerPageNotifierServicePtr page_notifier_service;
  render_frame_host->GetRemoteInterfaces()->GetInterface(
      &page_notifier_service);
  DCHECK(page_notifier_service);
  page_notifier_service->NotifyIsDistillerPage();

  if (viewer_handle) {
    // The service returned a |ViewerHandle| and guarantees it will call
    // the |RequestViewerHandle|, so passing ownership to it, to ensure the
    // request is not cancelled. The |RequestViewerHandle| will delete itself
    // after receiving the callback.
    request_viewer_handle->TakeViewerHandle(std::move(viewer_handle));
  } else {
    request_viewer_handle->FlagAsErrorPage();
  }

  // Place template on the page.
  callback.Run(base::RefCountedString::TakeString(&unsafe_page_html));
};

std::string DomDistillerViewerSource::GetMimeType(
    const std::string& path) const {
  if (kViewerCssPath == path) {
    return "text/css";
  } else if (kViewerLoadingImagePath == path) {
    return "image/svg+xml";
  }
  return "text/html";
}

bool DomDistillerViewerSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  return request->url().SchemeIs(scheme_.c_str());
}

// TODO(nyquist): Start tracking requests using this method.
void DomDistillerViewerSource::WillServiceRequest(
    const net::URLRequest* request,
    std::string* path) const {
}

std::string DomDistillerViewerSource::GetContentSecurityPolicyStyleSrc()
    const {
  return "style-src 'self' https://fonts.googleapis.com;";
}

std::string DomDistillerViewerSource::GetContentSecurityPolicyChildSrc() const {
  return "child-src *;";
}

}  // namespace dom_distiller
