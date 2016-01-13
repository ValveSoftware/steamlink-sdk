// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/service_manager/service_loader.h"
#include "mojo/service_manager/service_manager.h"
#include "mojo/service_manager/test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

const char kTestURLString[] = "test:testService";
const char kTestAURLString[] = "test:TestA";
const char kTestBURLString[] = "test:TestB";

struct TestContext {
  TestContext() : num_impls(0), num_loader_deletes(0) {}
  std::string last_test_string;
  int num_impls;
  int num_loader_deletes;
};

class QuitMessageLoopErrorHandler : public ErrorHandler {
 public:
  QuitMessageLoopErrorHandler() {}
  virtual ~QuitMessageLoopErrorHandler() {}

  // |ErrorHandler| implementation:
  virtual void OnConnectionError() OVERRIDE {
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuitMessageLoopErrorHandler);
};

class TestServiceImpl : public InterfaceImpl<TestService> {
 public:
  explicit TestServiceImpl(TestContext* context) : context_(context) {
    ++context_->num_impls;
  }

  virtual ~TestServiceImpl() {
    --context_->num_impls;
  }

  virtual void OnConnectionError() OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

  // TestService implementation:
  virtual void Test(const String& test_string) OVERRIDE {
    context_->last_test_string = test_string;
    client()->AckTest();
  }

 private:
  TestContext* context_;
};

class TestClientImpl : public TestClient {
 public:
  explicit TestClientImpl(TestServicePtr service)
      : service_(service.Pass()),
        quit_after_ack_(false) {
    service_.set_client(this);
  }

  virtual ~TestClientImpl() {}

  virtual void AckTest() OVERRIDE {
    if (quit_after_ack_)
      base::MessageLoop::current()->Quit();
  }

  void Test(std::string test_string) {
    quit_after_ack_ = true;
    service_->Test(test_string);
  }

 private:
  TestServicePtr service_;
  bool quit_after_ack_;
  DISALLOW_COPY_AND_ASSIGN(TestClientImpl);
};

class TestServiceLoader : public ServiceLoader {
 public:
  TestServiceLoader()
      : context_(NULL),
        num_loads_(0) {
  }

  virtual ~TestServiceLoader() {
    if (context_)
      ++context_->num_loader_deletes;
    test_app_.reset(NULL);
  }

  void set_context(TestContext* context) { context_ = context; }
  int num_loads() const { return num_loads_; }

 private:
  virtual void LoadService(
      ServiceManager* manager,
      const GURL& url,
      ScopedMessagePipeHandle service_provider_handle) OVERRIDE {
    ++num_loads_;
    test_app_.reset(new Application(service_provider_handle.Pass()));
    test_app_->AddService<TestServiceImpl>(context_);
  }

  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE {
  }

  scoped_ptr<Application> test_app_;
  TestContext* context_;
  int num_loads_;
  DISALLOW_COPY_AND_ASSIGN(TestServiceLoader);
};

// Used to test that the requestor url will be correctly passed.
class TestAImpl : public InterfaceImpl<TestA> {
 public:
  explicit TestAImpl(Application* app) : app_(app) {}

  virtual void LoadB() OVERRIDE {
    TestBPtr b;
    app_->ConnectTo(kTestBURLString, &b);
    b->Test();
  }

 private:
  Application* app_;
};

class TestBImpl : public InterfaceImpl<TestB> {
 public:
  virtual void Test() OVERRIDE {
    base::MessageLoop::current()->Quit();
  }
};

class TestApp : public Application, public ServiceLoader {
 public:
  explicit TestApp(std::string requestor_url)
      : requestor_url_(requestor_url),
        num_connects_(0) {
  }

  int num_connects() const { return num_connects_; }

 private:
  virtual void LoadService(
      ServiceManager* manager,
      const GURL& url,
      ScopedMessagePipeHandle service_provider_handle) OVERRIDE {
    BindServiceProvider(service_provider_handle.Pass());
  }

  virtual bool AllowIncomingConnection(const mojo::String& service_name,
                                       const mojo::String& requestor_url)
      MOJO_OVERRIDE {
    if (requestor_url_.empty() || requestor_url_ == requestor_url) {
      ++num_connects_;
      return true;
    } else {
      base::MessageLoop::current()->Quit();
      return false;
    }
  }

  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE {}
  std::string requestor_url_;
  int num_connects_;
};

class TestServiceInterceptor : public ServiceManager::Interceptor {
 public:
  TestServiceInterceptor() : call_count_(0) {}

  virtual ScopedMessagePipeHandle OnConnectToClient(
      const GURL& url, ScopedMessagePipeHandle handle) OVERRIDE {
    ++call_count_;
    url_ = url;
    return handle.Pass();
  }

  std::string url_spec() const {
    if (!url_.is_valid())
      return "invalid url";
    return url_.spec();
  }

  int call_count() const {
    return call_count_;
  }

 private:
  int call_count_;
  GURL url_;
  DISALLOW_COPY_AND_ASSIGN(TestServiceInterceptor);
};

}  // namespace

class ServiceManagerTest : public testing::Test {
 public:
  ServiceManagerTest() {}

  virtual ~ServiceManagerTest() {}

  virtual void SetUp() OVERRIDE {
    GURL test_url(kTestURLString);
    service_manager_.reset(new ServiceManager);

    MessagePipe pipe;
    TestServicePtr service_proxy = MakeProxy<TestService>(pipe.handle0.Pass());
    test_client_.reset(new TestClientImpl(service_proxy.Pass()));

    TestServiceLoader* default_loader = new TestServiceLoader;
    default_loader->set_context(&context_);
    service_manager_->set_default_loader(
        scoped_ptr<ServiceLoader>(default_loader));

    service_manager_->ConnectToService(
        test_url, TestService::Name_, pipe.handle1.Pass(), GURL());
  }

  virtual void TearDown() OVERRIDE {
    test_client_.reset(NULL);
    service_manager_.reset(NULL);
  }

  bool HasFactoryForTestURL() {
    ServiceManager::TestAPI manager_test_api(service_manager_.get());
    return manager_test_api.HasFactoryForURL(GURL(kTestURLString));
  }

 protected:
  base::ShadowingAtExitManager at_exit_;
  base::MessageLoop loop_;
  TestContext context_;
  scoped_ptr<TestClientImpl> test_client_;
  scoped_ptr<ServiceManager> service_manager_;
  DISALLOW_COPY_AND_ASSIGN(ServiceManagerTest);
};

TEST_F(ServiceManagerTest, Basic) {
  test_client_->Test("test");
  loop_.Run();
  EXPECT_EQ(std::string("test"), context_.last_test_string);
}

TEST_F(ServiceManagerTest, ClientError) {
  test_client_->Test("test");
  EXPECT_TRUE(HasFactoryForTestURL());
  loop_.Run();
  EXPECT_EQ(1, context_.num_impls);
  test_client_.reset(NULL);
  loop_.Run();
  EXPECT_EQ(0, context_.num_impls);
  EXPECT_TRUE(HasFactoryForTestURL());
}

TEST_F(ServiceManagerTest, Deletes) {
  {
    ServiceManager sm;
    TestServiceLoader* default_loader = new TestServiceLoader;
    default_loader->set_context(&context_);
    TestServiceLoader* url_loader1 = new TestServiceLoader;
    TestServiceLoader* url_loader2 = new TestServiceLoader;
    url_loader1->set_context(&context_);
    url_loader2->set_context(&context_);
    TestServiceLoader* scheme_loader1 = new TestServiceLoader;
    TestServiceLoader* scheme_loader2 = new TestServiceLoader;
    scheme_loader1->set_context(&context_);
    scheme_loader2->set_context(&context_);
    sm.set_default_loader(scoped_ptr<ServiceLoader>(default_loader));
    sm.SetLoaderForURL(scoped_ptr<ServiceLoader>(url_loader1),
                       GURL("test:test1"));
    sm.SetLoaderForURL(scoped_ptr<ServiceLoader>(url_loader2),
                       GURL("test:test1"));
    sm.SetLoaderForScheme(scoped_ptr<ServiceLoader>(scheme_loader1), "test");
    sm.SetLoaderForScheme(scoped_ptr<ServiceLoader>(scheme_loader2), "test");
  }
  EXPECT_EQ(5, context_.num_loader_deletes);
}

// Confirm that both urls and schemes can have their loaders explicitly set.
TEST_F(ServiceManagerTest, SetLoaders) {
  ServiceManager sm;
  TestServiceLoader* default_loader = new TestServiceLoader;
  TestServiceLoader* url_loader = new TestServiceLoader;
  TestServiceLoader* scheme_loader = new TestServiceLoader;
  sm.set_default_loader(scoped_ptr<ServiceLoader>(default_loader));
  sm.SetLoaderForURL(scoped_ptr<ServiceLoader>(url_loader), GURL("test:test1"));
  sm.SetLoaderForScheme(scoped_ptr<ServiceLoader>(scheme_loader), "test");

  // test::test1 should go to url_loader.
  TestServicePtr test_service;
  sm.ConnectTo(GURL("test:test1"), &test_service, GURL());
  EXPECT_EQ(1, url_loader->num_loads());
  EXPECT_EQ(0, scheme_loader->num_loads());
  EXPECT_EQ(0, default_loader->num_loads());

  // test::test2 should go to scheme loader.
  sm.ConnectTo(GURL("test:test2"), &test_service, GURL());
  EXPECT_EQ(1, url_loader->num_loads());
  EXPECT_EQ(1, scheme_loader->num_loads());
  EXPECT_EQ(0, default_loader->num_loads());

  // http::test1 should go to default loader.
  sm.ConnectTo(GURL("http:test1"), &test_service, GURL());
  EXPECT_EQ(1, url_loader->num_loads());
  EXPECT_EQ(1, scheme_loader->num_loads());
  EXPECT_EQ(1, default_loader->num_loads());
}

// Confirm that the url of a service is correctly passed to another service that
// it loads.
TEST_F(ServiceManagerTest, ALoadB) {
  ServiceManager sm;

  // Any url can load a.
  TestApp* a_app = new TestApp(std::string());
  a_app->AddService<TestAImpl>(a_app);
  sm.SetLoaderForURL(scoped_ptr<ServiceLoader>(a_app), GURL(kTestAURLString));

  // Only a can load b.
  TestApp* b_app = new TestApp(kTestAURLString);
  b_app->AddService<TestBImpl>();
  sm.SetLoaderForURL(scoped_ptr<ServiceLoader>(b_app), GURL(kTestBURLString));

  TestAPtr a;
  sm.ConnectTo(GURL(kTestAURLString), &a, GURL());
  a->LoadB();
  loop_.Run();
  EXPECT_EQ(1, b_app->num_connects());
}

// Confirm that the url of a service is correctly passed to another service that
// it loads, and that it can be rejected.
TEST_F(ServiceManagerTest, ANoLoadB) {
  ServiceManager sm;

  // Any url can load a.
  TestApp* a_app = new TestApp(std::string());
  a_app->AddService<TestAImpl>(a_app);
  sm.SetLoaderForURL(scoped_ptr<ServiceLoader>(a_app), GURL(kTestAURLString));

  // Only c can load b, so this will fail.
  TestApp* b_app = new TestApp("test:TestC");
  b_app->AddService<TestBImpl>();
  sm.SetLoaderForURL(scoped_ptr<ServiceLoader>(b_app), GURL(kTestBURLString));

  TestAPtr a;
  sm.ConnectTo(GURL(kTestAURLString), &a, GURL());
  a->LoadB();
  loop_.Run();
  EXPECT_EQ(0, b_app->num_connects());
}

TEST_F(ServiceManagerTest, NoServiceNoLoad) {
  ServiceManager sm;

  TestApp* b_app = new TestApp(std::string());
  b_app->AddService<TestBImpl>();
  sm.SetLoaderForURL(scoped_ptr<ServiceLoader>(b_app), GURL(kTestBURLString));

  // There is no TestA service implementation registered with ServiceManager,
  // so this cannot succeed (but also shouldn't crash).
  TestAPtr a;
  sm.ConnectTo(GURL(kTestBURLString), &a, GURL());
  QuitMessageLoopErrorHandler quitter;
  a.set_error_handler(&quitter);
  a->LoadB();

  loop_.Run();
  EXPECT_TRUE(a.encountered_error());
  EXPECT_EQ(0, b_app->num_connects());
}

TEST_F(ServiceManagerTest, Interceptor) {
  ServiceManager sm;
  TestServiceInterceptor interceptor;
  TestServiceLoader* default_loader = new TestServiceLoader;
  sm.set_default_loader(scoped_ptr<ServiceLoader>(default_loader));
  sm.SetInterceptor(&interceptor);

  std::string url("test:test3");
  TestServicePtr test_service;
  sm.ConnectTo(GURL(url), &test_service, GURL());
  EXPECT_EQ(1, interceptor.call_count());
  EXPECT_EQ(url, interceptor.url_spec());
  EXPECT_EQ(1, default_loader->num_loads());
}

}  // namespace mojo
