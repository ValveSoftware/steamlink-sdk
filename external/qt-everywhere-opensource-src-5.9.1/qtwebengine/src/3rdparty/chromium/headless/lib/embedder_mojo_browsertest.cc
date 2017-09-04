// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "base/optional.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/test/browser_test.h"
#include "headless/grit/headless_browsertest_resources.h"
#include "headless/lib/embedder_test.mojom.h"
#include "headless/public/devtools/domains/network.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace headless {

#define DEVTOOLS_CLIENT_TEST_F(TEST_FIXTURE_NAME)                        \
  IN_PROC_BROWSER_TEST_F(TEST_FIXTURE_NAME, RunAsyncTest) { RunTest(); } \
  class AsyncHeadlessBrowserTestNeedsSemicolon##TEST_FIXTURE_NAME {}

// A test fixture which attaches a devtools client and registers a mojo
// interface before starting the test.
class EmbedderMojoTest : public HeadlessBrowserTest,
                         public HeadlessWebContents::Observer,
                         public embedder_test::TestEmbedderService {
 public:
  enum HttpPolicy { DEFAULT, ENABLE_HTTP };

  EmbedderMojoTest() : EmbedderMojoTest(HttpPolicy::DEFAULT) {}

  explicit EmbedderMojoTest(HttpPolicy http_policy)
      : browser_context_(nullptr),
        web_contents_(nullptr),
        devtools_client_(HeadlessDevToolsClient::Create()),
        http_policy_(http_policy) {}

  ~EmbedderMojoTest() override {}

  void SetUp() override {
    // Set service names before they are used during main thread initialization.
    options()->mojo_service_names.insert("embedder_test::TestEmbedderService");

    HeadlessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    base::ThreadRestrictions::SetIOAllowed(true);
    base::FilePath pak_path;
    ASSERT_TRUE(PathService::Get(base::DIR_MODULE, &pak_path));
    pak_path = pak_path.AppendASCII("headless_browser_tests.pak");
    ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path, ui::SCALE_FACTOR_NONE);
  }

  // HeadlessWebContentsObserver implementation:
  void DevToolsTargetReady() override {
    EXPECT_TRUE(web_contents_->GetDevToolsTarget());
    web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());

    RunMojoTest();
  }

  virtual void RunMojoTest() = 0;

  virtual GURL GetInitialUrl() const { return GURL("about:blank"); }

  void OnEvalResult(std::unique_ptr<runtime::EvaluateResult> result) {
    EXPECT_FALSE(result->HasExceptionDetails())
        << "JS exception: " << result->GetExceptionDetails()->GetText();
    if (result->HasExceptionDetails()) {
      FinishAsynchronousTest();
    }
  }

 protected:
  void RunTest() {
    // Using a pak file is idiomatic chromium style, but most embedders probably
    // wouln't load the javascript bindings file this way.
    std::string embedder_test_mojom_js =
        ResourceBundle::GetSharedInstance()
            .GetRawDataResource(IDR_HEADLESS_EMBEDDER_TEST_MOJOM_JS)
            .as_string();

    HeadlessBrowserContext::Builder builder =
        browser()->CreateBrowserContextBuilder();
    builder.AddJsMojoBindings("headless/lib/embedder_test.mojom",
                              embedder_test_mojom_js);
    if (http_policy_ == HttpPolicy::ENABLE_HTTP) {
      builder.EnableUnsafeNetworkAccessWithMojoBindings(true);
    }
    if (host_resolver_rules_) {
      builder.SetHostResolverRules(*host_resolver_rules_);
    }

    browser_context_ = builder.Build();

    web_contents_ =
        browser_context_->CreateWebContentsBuilder()
            .SetInitialURL(GURL(GetInitialUrl()))
            .AddMojoService(base::Bind(&EmbedderMojoTest::CreateTestMojoService,
                                       base::Unretained(this)))
            .Build();

    web_contents_->AddObserver(this);
    RunAsynchronousTest();

    web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
    web_contents_->RemoveObserver(this);
    web_contents_->Close();
    web_contents_ = nullptr;

    browser_context_->Close();
    browser_context_ = nullptr;
  }

  void CreateTestMojoService(
      mojo::InterfaceRequest<embedder_test::TestEmbedderService> request) {
    test_embedder_mojo_bindings_.AddBinding(this, std::move(request));
  }

  HeadlessBrowserContext* browser_context_;
  HeadlessWebContents* web_contents_;
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;

  mojo::BindingSet<embedder_test::TestEmbedderService>
      test_embedder_mojo_bindings_;

  HttpPolicy http_policy_;
  base::Optional<std::string> host_resolver_rules_;
};

class MojoBindingsTest : public EmbedderMojoTest {
 public:
  void RunMojoTest() override {
    devtools_client_->GetRuntime()->Evaluate(
        "// Note define() defines a module in the mojo module dependency     \n"
        "// system. While we don't expose our module, the callback below only\n"
        "// fires after the requested modules have been loaded.              \n"
        "define([                                                            \n"
        "    'headless/lib/embedder_test.mojom',                             \n"
        "    'mojo/public/js/core',                                          \n"
        "    'mojo/public/js/router',                                        \n"
        "    'content/public/renderer/frame_interfaces',                     \n"
        "    ], function(embedderMojom, mojoCore, routerModule,              \n"
        "                frameInterfaces) {                                  \n"
        "  var testEmbedderService =                                         \n"
        "      new embedderMojom.TestEmbedderService.proxyClass(             \n"
        "          new routerModule.Router(                                  \n"
        "              frameInterfaces.getInterface(                         \n"
        "                  embedderMojom.TestEmbedderService.name)));        \n"
        "                                                                    \n"
        "  // Send a message to the embedder!                                \n"
        "  testEmbedderService.returnTestResult('hello world');              \n"
        "});",
        base::Bind(&EmbedderMojoTest::OnEvalResult, base::Unretained(this)));
  }

  // embedder_test::TestEmbedderService:
  void ReturnTestResult(const std::string& result) override {
    EXPECT_EQ("hello world", result);
    FinishAsynchronousTest();
  }
};

DEVTOOLS_CLIENT_TEST_F(MojoBindingsTest);

class MojoBindingsReinstalledAfterNavigation : public EmbedderMojoTest {
 public:
  MojoBindingsReinstalledAfterNavigation()
      : EmbedderMojoTest(HttpPolicy::ENABLE_HTTP), seen_page_one_(false) {
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  void SetUpOnMainThread() override {
    // We want to make sure bindings work across browser initiated cross-origin
    // navigation, which is why we're setting up this fake tld.
    host_resolver_rules_ =
        base::StringPrintf("MAP not-an-actual-domain.tld 127.0.0.1:%d",
                           embedded_test_server()->host_port_pair().port());

    EmbedderMojoTest::SetUpOnMainThread();
  }

  void RunMojoTest() override {}

  GURL GetInitialUrl() const override {
    return embedded_test_server()->GetURL("/page_one.html");
  }

  // embedder_test::TestEmbedderService:
  void ReturnTestResult(const std::string& result) override {
    if (result == "page one") {
      seen_page_one_ = true;
      devtools_client_->GetPage()->Navigate(
          "http://not-an-actual-domain.tld/page_two.html");
    } else {
      EXPECT_TRUE(seen_page_one_);
      EXPECT_EQ("page two", result);
      FinishAsynchronousTest();
    }
  }

 private:
  bool seen_page_one_;
};

DEVTOOLS_CLIENT_TEST_F(MojoBindingsReinstalledAfterNavigation);

class HttpDisabledByDefaultWhenMojoBindingsUsed : public EmbedderMojoTest,
                                                  network::Observer,
                                                  page::Observer {
 public:
  HttpDisabledByDefaultWhenMojoBindingsUsed() {
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  void RunMojoTest() override {
    devtools_client_->GetNetwork()->AddObserver(this);
    devtools_client_->GetNetwork()->Enable();
  }

  GURL GetInitialUrl() const override {
    return embedded_test_server()->GetURL("/page_one.html");
  }

  void ReturnTestResult(const std::string& result) override {
    FinishAsynchronousTest();
    FAIL() << "The HTTP page should not have been served and we should not have"
              " recieved a mojo callback!";
  }

  void OnLoadingFailed(const network::LoadingFailedParams& params) override {
    // The navigation should fail since HTTP requests are blackholed.
    EXPECT_EQ(params.GetErrorText(), "net::ERR_FILE_NOT_FOUND");
    FinishAsynchronousTest();
  }
};

DEVTOOLS_CLIENT_TEST_F(HttpDisabledByDefaultWhenMojoBindingsUsed);

}  // namespace headless
