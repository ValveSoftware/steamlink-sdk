// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_NAVIGATION_FEATURE_H_
#define BLIMP_CLIENT_FEATURE_NAVIGATION_FEATURE_H_

#include <map>
#include <string>

#include "base/containers/small_map.h"
#include "base/macros.h"
#include "blimp/net/blimp_message_processor.h"

class GURL;
class SkBitmap;

namespace blimp {
namespace client {

// Handles all incoming and outgoing protobuf messages of type
// RenderWidget::NAVIGATION.  Delegates can be added to be notified of incoming
// messages.
class NavigationFeature : public BlimpMessageProcessor {
 public:
  // A delegate to be notified of specific navigation events related to a
  // a particular tab.
  class NavigationFeatureDelegate {
   public:
    virtual ~NavigationFeatureDelegate() {}
    virtual void OnUrlChanged(int tab_id, const GURL& url) = 0;
    virtual void OnFaviconChanged(int tab_id, const SkBitmap& favicon) = 0;
    virtual void OnTitleChanged(int tab_id, const std::string& title) = 0;
    virtual void OnLoadingChanged(int tab_id, bool loading) = 0;
    virtual void OnPageLoadStatusUpdate(int tab_id, bool completed) = 0;
  };

  NavigationFeature();
  ~NavigationFeature() override;

  // Set the BlimpMessageProcessor that will be used to send
  // BlimpMessage::NAVIGATION messages to the engine.
  void set_outgoing_message_processor(
      std::unique_ptr<BlimpMessageProcessor> processor);

  // Sets a NavigationMessageDelegate to be notified of all navigation messages
  // for |tab_id| from the engine.
  void SetDelegate(int tab_id, NavigationFeatureDelegate* delegate);
  void RemoveDelegate(int tab_id);

  void NavigateToUrlText(int tab_id, const std::string& url_text);
  void Reload(int tab_id);
  void GoForward(int tab_id);
  void GoBack(int tab_id);

  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  NavigationFeatureDelegate* FindDelegate(const int tab_id);

  typedef base::SmallMap<std::map<int, NavigationFeatureDelegate*>> DelegateMap;

  DelegateMap delegates_;

  // Used to send BlimpMessage::NAVIGATION messages to the engine.
  std::unique_ptr<BlimpMessageProcessor> outgoing_message_processor_;

  DISALLOW_COPY_AND_ASSIGN(NavigationFeature);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_NAVIGATION_FEATURE_H_
