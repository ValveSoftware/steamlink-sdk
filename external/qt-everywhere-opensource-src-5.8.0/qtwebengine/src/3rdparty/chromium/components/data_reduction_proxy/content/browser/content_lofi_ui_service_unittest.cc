// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/content_lofi_ui_service.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class ContentLoFiUIServiceTest : public content::RenderViewHostTestHarness {
 public:
  ContentLoFiUIServiceTest() : callback_called_(false), is_preview_(false) {
    // Cannot use IO_MAIN_LOOP with RenderViewHostTestHarness.
    SetThreadBundleOptions(content::TestBrowserThreadBundle::REAL_IO_THREAD);
  }

  void RunTestOnIOThread(base::RunLoop* ui_run_loop, bool is_preview) {
    ASSERT_TRUE(ui_run_loop);
    EXPECT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

    net::TestURLRequestContext context(true);
    net::MockClientSocketFactory mock_socket_factory;
    net::TestDelegate delegate;
    context.set_client_socket_factory(&mock_socket_factory);
    context.Init();

    content_lofi_ui_service_.reset(new ContentLoFiUIService(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::UI),
        base::Bind(&ContentLoFiUIServiceTest::OnLoFiResponseReceivedCallback,
                   base::Unretained(this))));

    std::unique_ptr<net::URLRequest> request =
        CreateRequest(context, &delegate);

    content_lofi_ui_service_->OnLoFiReponseReceived(*request, is_preview);

    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&base::RunLoop::Quit, base::Unretained(ui_run_loop)));
  }

  std::unique_ptr<net::URLRequest> CreateRequest(
      const net::TestURLRequestContext& context,
      net::TestDelegate* delegate) {
    EXPECT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

    std::unique_ptr<net::URLRequest> request = context.CreateRequest(
        GURL("http://www.google.com/"), net::IDLE, delegate);

    content::ResourceRequestInfo::AllocateForTesting(
        request.get(), content::RESOURCE_TYPE_SUB_FRAME, NULL,
        web_contents()->GetMainFrame()->GetProcess()->GetID(), -1,
        web_contents()->GetMainFrame()->GetRoutingID(),
        false,  // is_main_frame
        false,  // parent_is_main_frame
        false,  // allow_download
        false,  // is_async
        true);  // is_using_lofi

    return request;
  }

  void OnLoFiResponseReceivedCallback(content::WebContents* web_contents,
                                      bool is_preview) {
    EXPECT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    callback_called_ = true;
    is_preview_ = is_preview;
  }

  void VerifyOnLoFiResponseReceivedCallback(bool is_preview) {
    EXPECT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    EXPECT_TRUE(callback_called_);
    EXPECT_EQ(is_preview, is_preview_);
  }

 private:
  std::unique_ptr<ContentLoFiUIService> content_lofi_ui_service_;
  bool callback_called_;
  bool is_preview_;
};

TEST_F(ContentLoFiUIServiceTest, OnLoFiResponseReceived) {
  base::RunLoop ui_run_loop;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&ContentLoFiUIServiceTest::RunTestOnIOThread,
                 base::Unretained(this), &ui_run_loop, false));
  ui_run_loop.Run();
  base::RunLoop().RunUntilIdle();
  VerifyOnLoFiResponseReceivedCallback(false);
}

TEST_F(ContentLoFiUIServiceTest, OnLoFiPreviewResponseReceived) {
  base::RunLoop ui_run_loop;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&ContentLoFiUIServiceTest::RunTestOnIOThread,
                 base::Unretained(this), &ui_run_loop, true));
  ui_run_loop.Run();
  base::RunLoop().RunUntilIdle();
  VerifyOnLoFiResponseReceivedCallback(true);
}

}  // namespace data_reduction_proxy
