// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/test/data/web_ui_test_mojo_bindings.mojom.h"
#include "grit/content_resources.h"
#include "mojo/common/test/test_utils.h"
#include "mojo/public/js/bindings/constants.h"

namespace content {
namespace {

bool got_message = false;

// The bindings for the page are generated from a .mojom file. This code looks
// up the generated file from disk and returns it.
bool GetResource(const std::string& id,
                 const WebUIDataSource::GotDataCallback& callback) {
  // These are handled by the WebUIDataSource that AddMojoDataSource() creates.
  if (id == mojo::kCodecModuleName ||
      id == mojo::kConnectionModuleName ||
      id == mojo::kConnectorModuleName ||
      id == mojo::kUnicodeModuleName ||
      id == mojo::kRouterModuleName)
    return false;

  std::string contents;
  CHECK(base::ReadFileToString(mojo::test::GetFilePathForJSResource(id),
                               &contents,
                               std::string::npos)) << id;
  base::RefCountedString* ref_contents = new base::RefCountedString;
  ref_contents->data() = contents;
  callback.Run(ref_contents);
  return true;
}

class BrowserTargetImpl : public BrowserTarget {
 public:
  BrowserTargetImpl(mojo::ScopedMessagePipeHandle handle,
                    base::RunLoop* run_loop)
      : run_loop_(run_loop) {
    renderer_.Bind(handle.Pass());
    renderer_.set_client(this);
  }

  virtual ~BrowserTargetImpl() {}

  // BrowserTarget overrides:
  virtual void PingResponse() OVERRIDE {
    NOTREACHED();
  }

 protected:
  RendererTargetPtr renderer_;
  base::RunLoop* run_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserTargetImpl);
};

class PingBrowserTargetImpl : public BrowserTargetImpl {
 public:
  PingBrowserTargetImpl(mojo::ScopedMessagePipeHandle handle,
                        base::RunLoop* run_loop)
      : BrowserTargetImpl(handle.Pass(), run_loop) {
    renderer_->Ping();
  }

  virtual ~PingBrowserTargetImpl() {}

  // BrowserTarget overrides:
  // Quit the RunLoop when called.
  virtual void PingResponse() OVERRIDE {
    got_message = true;
    run_loop_->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PingBrowserTargetImpl);
};

// WebUIController that sets up mojo bindings.
class TestWebUIController : public WebUIController {
 public:
   TestWebUIController(WebUI* web_ui, base::RunLoop* run_loop)
      : WebUIController(web_ui),
        run_loop_(run_loop) {
    content::WebUIDataSource* data_source =
        WebUIDataSource::AddMojoDataSource(
            web_ui->GetWebContents()->GetBrowserContext());
    data_source->SetRequestFilter(base::Bind(&GetResource));
  }

 protected:
  base::RunLoop* run_loop_;
  scoped_ptr<BrowserTargetImpl> browser_target_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebUIController);
};

// TestWebUIController that additionally creates the ping test BrowserTarget
// implementation at the right time.
class PingTestWebUIController : public TestWebUIController {
 public:
   PingTestWebUIController(WebUI* web_ui, base::RunLoop* run_loop)
       : TestWebUIController(web_ui, run_loop) {
   }

  // WebUIController overrides:
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE {
    mojo::MessagePipe pipe;
    browser_target_.reset(
        new PingBrowserTargetImpl(pipe.handle0.Pass(), run_loop_));
    render_view_host->SetWebUIHandle(pipe.handle1.Pass());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PingTestWebUIController);
};

// WebUIControllerFactory that creates TestWebUIController.
class TestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  TestWebUIControllerFactory() : run_loop_(NULL) {}

  void set_run_loop(base::RunLoop* run_loop) { run_loop_ = run_loop; }

  virtual WebUIController* CreateWebUIControllerForURL(
      WebUI* web_ui, const GURL& url) const OVERRIDE {
    if (url.query() == "ping")
      return new PingTestWebUIController(web_ui, run_loop_);
    return NULL;
  }
  virtual WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
      const GURL& url) const OVERRIDE {
    return reinterpret_cast<WebUI::TypeID>(1);
  }
  virtual bool UseWebUIForURL(BrowserContext* browser_context,
                              const GURL& url) const OVERRIDE {
    return true;
  }
  virtual bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                                      const GURL& url) const OVERRIDE {
    return true;
  }

 private:
  base::RunLoop* run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestWebUIControllerFactory);
};

class WebUIMojoTest : public ContentBrowserTest {
 public:
  WebUIMojoTest() {
    WebUIControllerFactory::RegisterFactory(&factory_);
  }

  virtual ~WebUIMojoTest() {
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  TestWebUIControllerFactory* factory() { return &factory_; }

 private:
  TestWebUIControllerFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(WebUIMojoTest);
};

// Loads a webui page that contains mojo bindings and verifies a message makes
// it from the browser to the page and back.
IN_PROC_BROWSER_TEST_F(WebUIMojoTest, EndToEndPing) {
  // Currently there is no way to have a generated file included in the isolate
  // files. If the bindings file doesn't exist assume we're on such a bot and
  // pass.
  // TODO(sky): remove this conditional when isolates support copying from gen.
  const base::FilePath test_file_path(
      mojo::test::GetFilePathForJSResource(
          "content/test/data/web_ui_test_mojo_bindings.mojom"));
  if (!base::PathExists(test_file_path)) {
    LOG(WARNING) << " mojom binding file doesn't exist, assuming on isolate";
    return;
  }

  got_message = false;
  ASSERT_TRUE(test_server()->Start());
  base::RunLoop run_loop;
  factory()->set_run_loop(&run_loop);
  GURL test_url(test_server()->GetURL("files/web_ui_mojo.html?ping"));
  NavigateToURL(shell(), test_url);
  // RunLoop is quit when message received from page.
  run_loop.Run();
  EXPECT_TRUE(got_message);
}

}  // namespace
}  // namespace content
