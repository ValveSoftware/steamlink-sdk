// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/navigation/public/interfaces/view.mojom.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_test.h"

namespace navigation {

class NavigationTest : public service_manager::test::ServiceTest,
                       public mojom::ViewClient {
 public:
  NavigationTest()
      : service_manager::test::ServiceTest("navigation_unittests"),
        binding_(this) {}
  ~NavigationTest() override {}

 protected:
   void SetUp() override {
     service_manager::test::ServiceTest::SetUp();
     window_manager_connection_ = connector()->Connect("test_wm");
   }

  mojom::ViewClientPtr GetViewClient() {
    return binding_.CreateInterfacePtrAndBind();
  }

  void QuitOnLoadingStateChange(base::RunLoop* loop) {
    loop_ = loop;
  }

 private:
  // mojom::ViewClient:
  void OpenURL(mojom::OpenURLParamsPtr params) override {}
  void LoadingStateChanged(bool is_loading) override {
    // Should see loading start, then stop.
    if (++load_count_ == 2 && loop_)
      loop_->Quit();
  }
  void NavigationStateChanged(const GURL& url,
                              const std::string& title,
                              bool can_go_back,
                              bool can_go_forward) override {}
  void LoadProgressChanged(double progress) override {}
  void UpdateHoverURL(const GURL& url) override {}
  void ViewCreated(mojom::ViewPtr,
                   mojom::ViewClientRequest,
                   bool,
                   const gfx::Rect&,
                   bool) override {}
  void Close() override {}
  void NavigationPending(mojom::NavigationEntryPtr entry) override {}
  void NavigationCommitted(
      mojom::NavigationCommittedDetailsPtr details,
      int current_index) override {}
  void NavigationEntryChanged(mojom::NavigationEntryPtr entry,
                              int entry_index) override {}
  void NavigationListPruned(bool from_front, int count) override {}

  int load_count_ = 0;
  mojo::Binding<mojom::ViewClient> binding_;
  base::RunLoop* loop_ = nullptr;
  std::unique_ptr<service_manager::Connection> window_manager_connection_;

  DISALLOW_COPY_AND_ASSIGN(NavigationTest);
};

// See crbug.com/619523
TEST_F(NavigationTest, DISABLED_Navigate) {
  mojom::ViewFactoryPtr view_factory;
  connector()->ConnectToInterface("navigation", &view_factory);

  mojom::ViewPtr view;
  view_factory->CreateView(GetViewClient(), GetProxy(&view));
  view->NavigateTo(GURL("about:blank"));

  base::RunLoop loop;
  QuitOnLoadingStateChange(&loop);
  loop.Run();
}

}  // namespace navigation
