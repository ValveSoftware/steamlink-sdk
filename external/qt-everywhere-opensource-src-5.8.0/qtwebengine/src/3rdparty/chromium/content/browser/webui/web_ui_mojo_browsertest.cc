// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/data/web_ui_test_mojo_bindings.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/shell/public/cpp/interface_registry.h"

namespace content {
namespace {

base::FilePath GetFilePathForJSResource(const std::string& path) {
  std::string binding_path = "gen/" + path + ".js";
#if defined(OS_WIN)
  base::ReplaceChars(binding_path, "//", "\\", &binding_path);
#endif
  base::FilePath exe_dir;
  PathService::Get(base::DIR_EXE, &exe_dir);
  return exe_dir.AppendASCII(binding_path);
}

bool got_message = false;

// The bindings for the page are generated from a .mojom file. This code looks
// up the generated file from disk and returns it.
bool GetResource(const std::string& id,
                 const WebUIDataSource::GotDataCallback& callback) {
  if (id.find(".mojom") != std::string::npos) {
    std::string contents;
    CHECK(base::ReadFileToString(GetFilePathForJSResource(id), &contents))
        << id;
    base::RefCountedString* ref_contents = new base::RefCountedString;
    ref_contents->data() = contents;
    callback.Run(ref_contents);
    return true;
  }

  base::FilePath path;
  CHECK(base::PathService::Get(content::DIR_TEST_DATA, &path));
  path = path.AppendASCII(id.substr(0, id.find("?")));
  std::string contents;
  CHECK(base::ReadFileToString(path, &contents)) << path.value();
  base::RefCountedString* ref_contents = new base::RefCountedString;
  ref_contents->data() = contents;
  callback.Run(ref_contents);
  return true;
}

class BrowserTargetImpl : public mojom::BrowserTarget {
 public:
  BrowserTargetImpl(base::RunLoop* run_loop,
                    mojo::InterfaceRequest<mojom::BrowserTarget> request)
      : run_loop_(run_loop), binding_(this, std::move(request)) {}

  ~BrowserTargetImpl() override {}

  // mojom::BrowserTarget overrides:
  void Start(const StartCallback& closure) override {
    closure.Run();
  }
  void Stop() override {
    got_message = true;
    run_loop_->Quit();
  }

 protected:
  base::RunLoop* run_loop_;

 private:
  mojo::Binding<mojom::BrowserTarget> binding_;
  DISALLOW_COPY_AND_ASSIGN(BrowserTargetImpl);
};

// WebUIController that sets up mojo bindings.
class TestWebUIController : public WebUIController {
 public:
  TestWebUIController(WebUI* web_ui, base::RunLoop* run_loop)
      : WebUIController(web_ui), run_loop_(run_loop) {
    content::WebUIDataSource* data_source =
        WebUIDataSource::Create("mojo-web-ui");
    data_source->SetRequestFilter(base::Bind(&GetResource));
    content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                  data_source);
  }

 protected:
  base::RunLoop* run_loop_;
  std::unique_ptr<BrowserTargetImpl> browser_target_;

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
   ~PingTestWebUIController() override {}

  // WebUIController overrides:
  void RenderViewCreated(RenderViewHost* render_view_host) override {
    render_view_host->GetMainFrame()->GetInterfaceRegistry()->AddInterface(
        base::Bind(&PingTestWebUIController::CreateHandler,
                   base::Unretained(this)));
  }

  void CreateHandler(mojo::InterfaceRequest<mojom::BrowserTarget> request) {
    browser_target_.reset(new BrowserTargetImpl(run_loop_, std::move(request)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PingTestWebUIController);
};

// WebUIControllerFactory that creates TestWebUIController.
class TestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  TestWebUIControllerFactory() : run_loop_(NULL) {}

  void set_run_loop(base::RunLoop* run_loop) { run_loop_ = run_loop; }

  WebUIController* CreateWebUIControllerForURL(WebUI* web_ui,
                                               const GURL& url) const override {
    if (url.query() == "ping")
      return new PingTestWebUIController(web_ui, run_loop_);
    return new TestWebUIController(web_ui, run_loop_);
  }
  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) const override {
    return reinterpret_cast<WebUI::TypeID>(1);
  }
  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) const override {
    return true;
  }
  bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                              const GURL& url) const override {
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

  ~WebUIMojoTest() override {
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  TestWebUIControllerFactory* factory() { return &factory_; }

 private:
  TestWebUIControllerFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(WebUIMojoTest);
};

bool IsGeneratedResourceAvailable(const std::string& resource_path) {
  // Currently there is no way to have a generated file included in the isolate
  // files. If the bindings file doesn't exist assume we're on such a bot and
  // pass.
  // TODO(sky): remove this conditional when isolates support copying from gen.
  const base::FilePath test_file_path(GetFilePathForJSResource(resource_path));
  if (base::PathExists(test_file_path))
    return true;
  LOG(WARNING) << " mojom binding file doesn't exist, assuming on isolate";
  return false;
}

// Loads a webui page that contains mojo bindings and verifies a message makes
// it from the browser to the page and back.
IN_PROC_BROWSER_TEST_F(WebUIMojoTest, EndToEndPing) {
  if (!IsGeneratedResourceAvailable(
          "content/test/data/web_ui_test_mojo_bindings.mojom"))
    return;

  got_message = false;
  ASSERT_TRUE(embedded_test_server()->Start());
  base::RunLoop run_loop;
  factory()->set_run_loop(&run_loop);
  GURL test_url("chrome://mojo-web-ui/web_ui_mojo.html?ping");
  NavigateToURL(shell(), test_url);
  // RunLoop is quit when message received from page.
  run_loop.Run();
  EXPECT_TRUE(got_message);

  // Check that a second render frame in the same renderer process works
  // correctly.
  Shell* other_shell = CreateBrowser();
  got_message = false;
  base::RunLoop other_run_loop;
  factory()->set_run_loop(&other_run_loop);
  NavigateToURL(other_shell, test_url);
  // RunLoop is quit when message received from page.
  other_run_loop.Run();
  EXPECT_TRUE(got_message);
  EXPECT_EQ(shell()->web_contents()->GetRenderProcessHost(),
            other_shell->web_contents()->GetRenderProcessHost());
}

}  // namespace
}  // namespace content
