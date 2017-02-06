// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/navigation/view_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace navigation {
namespace {

class InterstitialPageDelegate : public content::InterstitialPageDelegate {
 public:
  explicit InterstitialPageDelegate(const std::string& html) : html_(html) {}
  ~InterstitialPageDelegate() override {}
  InterstitialPageDelegate(const InterstitialPageDelegate&) = delete;
  void operator=(const InterstitialPageDelegate&) = delete;

 private:

  // content::InterstitialPageDelegate:
  std::string GetHTMLContents() override {
    return html_;
  }

  const std::string html_;
};

// TODO(beng): Explicitly not writing a TypeConverter for this, and not doing a
//             typemap just yet since I'm still figuring out what these
//             interfaces should take as parameters.
mojom::NavigationEntryPtr EntryPtrFromNavEntry(
    const content::NavigationEntry& entry) {
  mojom::NavigationEntryPtr entry_ptr(mojom::NavigationEntry::New());
  entry_ptr->id = entry.GetUniqueID();
  entry_ptr->url = entry.GetURL();
  entry_ptr->title = base::UTF16ToUTF8(entry.GetTitle());
  entry_ptr->redirect_chain = entry.GetRedirectChain();
  return entry_ptr;
}

}  // namespace

ViewImpl::ViewImpl(shell::Connector* connector,
                   const std::string& client_user_id,
                   mojom::ViewClientPtr client,
                   mojom::ViewRequest request,
                   std::unique_ptr<shell::ShellConnectionRef> ref)
    : connector_(connector),
      binding_(this, std::move(request)),
      client_(std::move(client)),
      ref_(std::move(ref)),
      web_view_(new views::WebView(
          content::BrowserContext::GetBrowserContextForShellUserId(
              client_user_id))) {
  web_view_->GetWebContents()->SetDelegate(this);
  const content::NavigationController* controller =
      &web_view_->GetWebContents()->GetController();
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                 content::Source<content::NavigationController>(controller));
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::Source<content::NavigationController>(controller));
  registrar_.Add(this, content::NOTIFICATION_NAV_LIST_PRUNED,
                 content::Source<content::NavigationController>(controller));
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_CHANGED,
                 content::Source<content::NavigationController>(controller));
}
ViewImpl::~ViewImpl() {}

void ViewImpl::NavigateTo(const GURL& url) {
  web_view_->GetWebContents()->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
}

void ViewImpl::GoBack() {
  web_view_->GetWebContents()->GetController().GoBack();
}

void ViewImpl::GoForward() {
  web_view_->GetWebContents()->GetController().GoForward();
}

void ViewImpl::NavigateToOffset(int offset) {
  web_view_->GetWebContents()->GetController().GoToOffset(offset);
}

void ViewImpl::Reload(bool skip_cache) {
  if (skip_cache)
    web_view_->GetWebContents()->GetController().Reload(true);
  else
    web_view_->GetWebContents()->GetController().ReloadBypassingCache(true);
}

void ViewImpl::Stop() {
  web_view_->GetWebContents()->Stop();
}

void ViewImpl::GetWindowTreeClient(
    mus::mojom::WindowTreeClientRequest request) {
  new mus::WindowTreeClient(this, nullptr, std::move(request));
}

void ViewImpl::ShowInterstitial(const mojo::String& html) {
  content::InterstitialPage* interstitial =
      content::InterstitialPage::Create(web_view_->GetWebContents(),
                                        false,
                                        GURL(),
                                        new InterstitialPageDelegate(html));
  interstitial->Show();
}

void ViewImpl::HideInterstitial() {
  // TODO(beng): this is not quite right.
  if (web_view_->GetWebContents()->ShowingInterstitialPage())
    web_view_->GetWebContents()->GetInterstitialPage()->Proceed();
}

void ViewImpl::SetResizerSize(const gfx::Size& size) {
  resizer_size_ = size;
}

void ViewImpl::AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_rect,
                              bool user_gesture,
                              bool* was_blocked) {
  mojom::ViewClientPtr client;
  mojom::ViewPtr view;
  mojom::ViewRequest view_request = GetProxy(&view);
  client_->ViewCreated(std::move(view), GetProxy(&client),
                       disposition == NEW_POPUP, initial_rect, user_gesture);

  const std::string new_user_id =
      content::BrowserContext::GetShellUserIdFor(
          new_contents->GetBrowserContext());
  ViewImpl* impl = new ViewImpl(connector_, new_user_id, std::move(client),
                                std::move(view_request), ref_->Clone());
  // TODO(beng): This is a bit crappy. should be able to create the ViewImpl
  //             with |new_contents| instead.
  impl->web_view_->SetWebContents(new_contents);
  impl->web_view_->GetWebContents()->SetDelegate(impl);

  // TODO(beng): this reply is currently synchronous, figure out a fix.
  if (was_blocked)
    *was_blocked = false;
}

void ViewImpl::CloseContents(content::WebContents* source) {
  client_->Close();
}

content::WebContents* ViewImpl::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  mojom::OpenURLParamsPtr params_ptr = mojom::OpenURLParams::New();
  params_ptr->url = params.url;
  params_ptr->disposition =
      static_cast<mojom::WindowOpenDisposition>(params.disposition);
  client_->OpenURL(std::move(params_ptr));
  // TODO(beng): Obviously this is the wrong thing to return for dispositions
  // that would lead to the creation of a new View, i.e. NEW_TAB, NEW_POPUP etc.
  // However it seems the callers of this function that we've seen so far
  // disregard the return value. Rather than returning |source| I'm returning
  // nullptr to locate (via crash) any sites that depend on a valid result.
  // If we actually had to do this then we'd have to create the new WebContents
  // here, store it with a cookie, and pass the cookie through to the client to
  // pass back with their call to CreateView(), so it would get bound to the
  // WebContents.
  return nullptr;
}

void ViewImpl::LoadingStateChanged(content::WebContents* source,
                                   bool to_different_document) {
  client_->LoadingStateChanged(source->IsLoading());
}

void ViewImpl::NavigationStateChanged(content::WebContents* source,
                                      content::InvalidateTypes changed_flags) {
  client_->NavigationStateChanged(source->GetVisibleURL(),
                                  base::UTF16ToUTF8(source->GetTitle()),
                                  source->GetController().CanGoBack(),
                                  source->GetController().CanGoForward());
}

void ViewImpl::LoadProgressChanged(content::WebContents* source,
                                   double progress) {
  client_->LoadProgressChanged(progress);
}

void ViewImpl::UpdateTargetURL(content::WebContents* source, const GURL& url) {
  client_->UpdateHoverURL(url);
}

gfx::Rect ViewImpl::GetRootWindowResizerRect() const {
  gfx::Rect bounds = web_view_->GetLocalBounds();
  return gfx::Rect(bounds.right() - resizer_size_.width(),
                   bounds.bottom() - resizer_size_.height(),
                   resizer_size_.width(), resizer_size_.height());
}

void ViewImpl::Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
  if (content::Source<content::NavigationController>(source).ptr() !=
      &web_view_->GetWebContents()->GetController()) {
    return;
  }

  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_PENDING: {
      const content::NavigationEntry* entry =
          content::Details<content::NavigationEntry>(details).ptr();
      client_->NavigationPending(EntryPtrFromNavEntry(*entry));
      break;
    }
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      const content::LoadCommittedDetails* lcd =
          content::Details<content::LoadCommittedDetails>(details).ptr();
      mojom::NavigationCommittedDetailsPtr details_ptr(
          mojom::NavigationCommittedDetails::New());
      details_ptr->entry = lcd->entry->GetUniqueID();
      details_ptr->type = static_cast<mojom::NavigationType>(lcd->type);
      details_ptr->previous_entry_index = lcd->previous_entry_index;
      details_ptr->previous_url = lcd->previous_url;
      details_ptr->is_in_page = lcd->is_in_page;
      details_ptr->is_main_frame = lcd->is_main_frame;
      details_ptr->http_status_code = lcd->http_status_code;
      client_->NavigationCommitted(
          std::move(details_ptr),
          web_view_->GetWebContents()->GetController().GetCurrentEntryIndex());
      break;
    }
    case content::NOTIFICATION_NAV_ENTRY_CHANGED: {
      const content::EntryChangedDetails* ecd =
          content::Details<content::EntryChangedDetails>(details).ptr();
      client_->NavigationEntryChanged(EntryPtrFromNavEntry(*ecd->changed_entry),
                                      ecd->index);
      break;
    }
    case content::NOTIFICATION_NAV_LIST_PRUNED: {
      const content::PrunedDetails* pd =
          content::Details<content::PrunedDetails>(details).ptr();
      client_->NavigationListPruned(pd->from_front, pd->count);
      break;
    }
  default:
    NOTREACHED();
    break;
  }
}

void ViewImpl::OnEmbed(mus::Window* root) {
  DCHECK(!widget_.get());
  widget_.reset(new views::Widget);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = this;
  params.native_widget = new views::NativeWidgetMus(
      widget_.get(), connector_, root, mus::mojom::SurfaceType::DEFAULT);
  widget_->Init(params);
  widget_->Show();
}

void ViewImpl::OnWindowTreeClientDestroyed(mus::WindowTreeClient* client) {}
void ViewImpl::OnEventObserved(const ui::Event& event, mus::Window* target) {}

views::View* ViewImpl::GetContentsView() {
  return web_view_;
}

views::Widget* ViewImpl::GetWidget() {
  return web_view_->GetWidget();
}

const views::Widget* ViewImpl::GetWidget() const {
  return web_view_->GetWidget();
}

}  // navigation
