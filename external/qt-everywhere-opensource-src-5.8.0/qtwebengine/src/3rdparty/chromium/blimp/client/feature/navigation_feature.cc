// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/navigation_feature.h"

#include <map>
#include <string>

#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/navigation.pb.h"
#include "components/url_formatter/url_fixer.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace blimp {
namespace client {

NavigationFeature::NavigationFeature() {}

NavigationFeature::~NavigationFeature() {}

void NavigationFeature::set_outgoing_message_processor(
    std::unique_ptr<BlimpMessageProcessor> processor) {
  outgoing_message_processor_ = std::move(processor);
}

void NavigationFeature::SetDelegate(int tab_id,
                                    NavigationFeatureDelegate* delegate) {
  DCHECK(!FindDelegate(tab_id));
  delegates_[tab_id] = delegate;
}

void NavigationFeature::RemoveDelegate(int tab_id) {
  DelegateMap::iterator it = delegates_.find(tab_id);
  if (it != delegates_.end())
    delegates_.erase(it);
}

void NavigationFeature::NavigateToUrlText(int tab_id,
                                          const std::string& url_text) {
  // Fixes up url, e.g., convert "google.com" to "http://google.com".
  // It also converts "example" to "http://example/" (a valid GURL but no
  // website). In order to use search instead in this case, we check
  // the host part of the URL to see if it contains '.', and use search
  // instead if it does not. Note that this heuristic is not correct, since
  // valid GURL such as "chrome://version/" do not have '.'. The heuristic is
  // used only for v0.5 and useful for dev&test.
  // TODO(haibinlu): Remove once omnibox is used.
  GURL url = url_formatter::FixupURL(url_text, std::string());
  if (!url.is_valid() || url.host_piece().find('.') == std::string::npos) {
    url::RawCanonOutputT<char> buffer;
    url::EncodeURIComponent(url_text.data(), url_text.size(), &buffer);
    std::string encoded_query(buffer.data(), buffer.length());
    url = GURL("https://www.google.com/#q=" + encoded_query);
    DCHECK(url.is_valid());
  }

  NavigationMessage* navigation_message;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&navigation_message, tab_id);
  navigation_message->set_type(NavigationMessage::LOAD_URL);
  navigation_message->mutable_load_url()->set_url(url.spec());
  outgoing_message_processor_->ProcessMessage(std::move(blimp_message),
                                              net::CompletionCallback());
}

void NavigationFeature::Reload(int tab_id) {
  NavigationMessage* navigation_message;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&navigation_message, tab_id);
  navigation_message->set_type(NavigationMessage::RELOAD);

  outgoing_message_processor_->ProcessMessage(std::move(blimp_message),
                                              net::CompletionCallback());
}

void NavigationFeature::GoForward(int tab_id) {
  NavigationMessage* navigation_message;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&navigation_message, tab_id);
  navigation_message->set_type(NavigationMessage::GO_FORWARD);

  outgoing_message_processor_->ProcessMessage(std::move(blimp_message),
                                              net::CompletionCallback());
}

void NavigationFeature::GoBack(int tab_id) {
  NavigationMessage* navigation_message;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&navigation_message, tab_id);
  navigation_message->set_type(NavigationMessage::GO_BACK);

  outgoing_message_processor_->ProcessMessage(std::move(blimp_message),
                                              net::CompletionCallback());
}

void NavigationFeature::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK_EQ(BlimpMessage::kNavigation, message->feature_case());

  int tab_id = message->target_tab_id();
  const NavigationMessage& navigation_message = message->navigation();

  NavigationFeatureDelegate* delegate = FindDelegate(tab_id);
  DCHECK(delegate) << "NavigationFeatureDelegate not found for tab " << tab_id;
  switch (navigation_message.type()) {
    case NavigationMessage::NAVIGATION_STATE_CHANGED: {
      const NavigationStateChangeMessage& details =
          navigation_message.navigation_state_changed();
      if (details.has_url())
        delegate->OnUrlChanged(tab_id, GURL(details.url()));

      if (details.has_title())
        delegate->OnTitleChanged(tab_id, details.title());

      if (details.has_loading())
        delegate->OnLoadingChanged(tab_id, details.loading());

      if (details.has_favicon()) {
        NOTIMPLEMENTED();
      }

      if (details.has_page_load_completed()) {
        delegate->OnPageLoadStatusUpdate(tab_id,
                                         details.page_load_completed());
      }
    } break;
    case NavigationMessage::LOAD_URL:
    case NavigationMessage::GO_BACK:
    case NavigationMessage::GO_FORWARD:
    case NavigationMessage::RELOAD:
      NOTREACHED() << "Client received unexpected navigation type.";
      break;
    case NavigationMessage::UNKNOWN:
      NOTREACHED();
  }

  callback.Run(net::OK);
}

NavigationFeature::NavigationFeatureDelegate* NavigationFeature::FindDelegate(
    const int tab_id) {
  DelegateMap::const_iterator it = delegates_.find(tab_id);
  if (it != delegates_.end())
    return it->second;
  return nullptr;
}

}  // namespace client
}  // namespace blimp
