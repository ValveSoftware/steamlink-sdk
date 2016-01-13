// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace content {
namespace {

const char kPrivilegedScheme[] = "privileged";

class SiteInstanceTestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  virtual WebUIController* CreateWebUIControllerForURL(
      WebUI* web_ui, const GURL& url) const OVERRIDE {
    return NULL;
  }
  virtual WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
      const GURL& url) const OVERRIDE {
    return WebUI::kNoWebUI;
  }
  virtual bool UseWebUIForURL(BrowserContext* browser_context,
                              const GURL& url) const OVERRIDE {
    return HasWebUIScheme(url);
  }
  virtual bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                                      const GURL& url) const OVERRIDE {
    return HasWebUIScheme(url);
  }
};

class SiteInstanceTestBrowserClient : public TestContentBrowserClient {
 public:
  SiteInstanceTestBrowserClient()
      : privileged_process_id_(-1) {
    WebUIControllerFactory::RegisterFactory(&factory_);
  }

  virtual ~SiteInstanceTestBrowserClient() {
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  virtual bool IsSuitableHost(RenderProcessHost* process_host,
                              const GURL& site_url) OVERRIDE {
    return (privileged_process_id_ == process_host->GetID()) ==
        site_url.SchemeIs(kPrivilegedScheme);
  }

  void set_privileged_process_id(int process_id) {
    privileged_process_id_ = process_id;
  }

 private:
  SiteInstanceTestWebUIControllerFactory factory_;
  int privileged_process_id_;
};

class SiteInstanceTest : public testing::Test {
 public:
  SiteInstanceTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_user_blocking_thread_(BrowserThread::FILE_USER_BLOCKING,
                                   &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_),
        old_browser_client_(NULL) {
  }

  virtual void SetUp() {
    old_browser_client_ = SetBrowserClientForTesting(&browser_client_);
    url::AddStandardScheme(kPrivilegedScheme);
    url::AddStandardScheme(kChromeUIScheme);

    SiteInstanceImpl::set_render_process_host_factory(&rph_factory_);
  }

  virtual void TearDown() {
    // Ensure that no RenderProcessHosts are left over after the tests.
    EXPECT_TRUE(RenderProcessHost::AllHostsIterator().IsAtEnd());

    SetBrowserClientForTesting(old_browser_client_);
    SiteInstanceImpl::set_render_process_host_factory(NULL);

    // http://crbug.com/143565 found SiteInstanceTest leaking an
    // AppCacheDatabase. This happens because some part of the test indirectly
    // calls StoragePartitionImplMap::PostCreateInitialization(), which posts
    // a task to the IO thread to create the AppCacheDatabase. Since the
    // message loop is not running, the AppCacheDatabase ends up getting
    // created when DrainMessageLoops() gets called at the end of a test case.
    // Immediately after, the test case ends and the AppCacheDatabase gets
    // scheduled for deletion. Here, call DrainMessageLoops() again so the
    // AppCacheDatabase actually gets deleted.
    DrainMessageLoops();
  }

  void set_privileged_process_id(int process_id) {
    browser_client_.set_privileged_process_id(process_id);
  }

  void DrainMessageLoops() {
    // We don't just do this in TearDown() because we create TestBrowserContext
    // objects in each test, which will be destructed before
    // TearDown() is called.
    base::MessageLoop::current()->RunUntilIdle();
    message_loop_.RunUntilIdle();
  }

 private:
  base::MessageLoopForUI message_loop_;
  TestBrowserThread ui_thread_;
  TestBrowserThread file_user_blocking_thread_;
  TestBrowserThread io_thread_;

  SiteInstanceTestBrowserClient browser_client_;
  ContentBrowserClient* old_browser_client_;
  MockRenderProcessHostFactory rph_factory_;
};

// Subclass of BrowsingInstance that updates a counter when deleted and
// returns TestSiteInstances from GetSiteInstanceForURL.
class TestBrowsingInstance : public BrowsingInstance {
 public:
  TestBrowsingInstance(BrowserContext* browser_context, int* delete_counter)
      : BrowsingInstance(browser_context),
        delete_counter_(delete_counter) {
  }

  // Make a few methods public for tests.
  using BrowsingInstance::browser_context;
  using BrowsingInstance::HasSiteInstance;
  using BrowsingInstance::GetSiteInstanceForURL;
  using BrowsingInstance::RegisterSiteInstance;
  using BrowsingInstance::UnregisterSiteInstance;

 private:
  virtual ~TestBrowsingInstance() {
    (*delete_counter_)++;
  }

  int* delete_counter_;
};

// Subclass of SiteInstanceImpl that updates a counter when deleted.
class TestSiteInstance : public SiteInstanceImpl {
 public:
  static TestSiteInstance* CreateTestSiteInstance(
      BrowserContext* browser_context,
      int* site_delete_counter,
      int* browsing_delete_counter) {
    TestBrowsingInstance* browsing_instance =
        new TestBrowsingInstance(browser_context, browsing_delete_counter);
    return new TestSiteInstance(browsing_instance, site_delete_counter);
  }

 private:
  TestSiteInstance(BrowsingInstance* browsing_instance, int* delete_counter)
    : SiteInstanceImpl(browsing_instance), delete_counter_(delete_counter) {}
  virtual ~TestSiteInstance() {
    (*delete_counter_)++;
  }

  int* delete_counter_;
};

}  // namespace

// Test to ensure no memory leaks for SiteInstance objects.
TEST_F(SiteInstanceTest, SiteInstanceDestructor) {
  // The existence of this object will cause WebContentsImpl to create our
  // test one instead of the real one.
  RenderViewHostTestEnabler rvh_test_enabler;
  int site_delete_counter = 0;
  int browsing_delete_counter = 0;
  const GURL url("test:foo");

  // Ensure that instances are deleted when their NavigationEntries are gone.
  TestSiteInstance* instance =
      TestSiteInstance::CreateTestSiteInstance(NULL, &site_delete_counter,
                                               &browsing_delete_counter);
  EXPECT_EQ(0, site_delete_counter);

  NavigationEntryImpl* e1 = new NavigationEntryImpl(
      instance, 0, url, Referrer(), base::string16(), PAGE_TRANSITION_LINK,
      false);

  // Redundantly setting e1's SiteInstance shouldn't affect the ref count.
  e1->set_site_instance(instance);
  EXPECT_EQ(0, site_delete_counter);

  // Add a second reference
  NavigationEntryImpl* e2 = new NavigationEntryImpl(
      instance, 0, url, Referrer(), base::string16(), PAGE_TRANSITION_LINK,
      false);

  // Now delete both entries and be sure the SiteInstance goes away.
  delete e1;
  EXPECT_EQ(0, site_delete_counter);
  EXPECT_EQ(0, browsing_delete_counter);
  delete e2;
  EXPECT_EQ(1, site_delete_counter);
  // instance is now deleted
  EXPECT_EQ(1, browsing_delete_counter);
  // browsing_instance is now deleted

  // Ensure that instances are deleted when their RenderViewHosts are gone.
  scoped_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  instance =
      TestSiteInstance::CreateTestSiteInstance(browser_context.get(),
                                               &site_delete_counter,
                                               &browsing_delete_counter);
  {
    scoped_ptr<WebContentsImpl> web_contents(static_cast<WebContentsImpl*>(
        WebContents::Create(WebContents::CreateParams(
            browser_context.get(), instance))));
    EXPECT_EQ(1, site_delete_counter);
    EXPECT_EQ(1, browsing_delete_counter);
  }

  // Make sure that we flush any messages related to the above WebContentsImpl
  // destruction.
  DrainMessageLoops();

  EXPECT_EQ(2, site_delete_counter);
  EXPECT_EQ(2, browsing_delete_counter);
  // contents is now deleted, along with instance and browsing_instance
}

// Test that NavigationEntries with SiteInstances can be cloned, but that their
// SiteInstances can be changed afterwards.  Also tests that the ref counts are
// updated properly after the change.
TEST_F(SiteInstanceTest, CloneNavigationEntry) {
  int site_delete_counter1 = 0;
  int site_delete_counter2 = 0;
  int browsing_delete_counter = 0;
  const GURL url("test:foo");

  SiteInstanceImpl* instance1 =
      TestSiteInstance::CreateTestSiteInstance(NULL, &site_delete_counter1,
                                               &browsing_delete_counter);
  SiteInstanceImpl* instance2 =
      TestSiteInstance::CreateTestSiteInstance(NULL, &site_delete_counter2,
                                               &browsing_delete_counter);

  NavigationEntryImpl* e1 = new NavigationEntryImpl(
      instance1, 0, url, Referrer(), base::string16(), PAGE_TRANSITION_LINK,
      false);
  // Clone the entry
  NavigationEntryImpl* e2 = new NavigationEntryImpl(*e1);

  // Should be able to change the SiteInstance of the cloned entry.
  e2->set_site_instance(instance2);

  // The first SiteInstance should go away after deleting e1, since e2 should
  // no longer be referencing it.
  delete e1;
  EXPECT_EQ(1, site_delete_counter1);
  EXPECT_EQ(0, site_delete_counter2);

  // The second SiteInstance should go away after deleting e2.
  delete e2;
  EXPECT_EQ(1, site_delete_counter1);
  EXPECT_EQ(1, site_delete_counter2);

  // Both BrowsingInstances are also now deleted
  EXPECT_EQ(2, browsing_delete_counter);

  DrainMessageLoops();
}

// Test to ensure GetProcess returns and creates processes correctly.
TEST_F(SiteInstanceTest, GetProcess) {
  // Ensure that GetProcess returns a process.
  scoped_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  scoped_ptr<RenderProcessHost> host1;
  scoped_refptr<SiteInstanceImpl> instance(static_cast<SiteInstanceImpl*>(
      SiteInstance::Create(browser_context.get())));
  host1.reset(instance->GetProcess());
  EXPECT_TRUE(host1.get() != NULL);

  // Ensure that GetProcess creates a new process.
  scoped_refptr<SiteInstanceImpl> instance2(static_cast<SiteInstanceImpl*>(
      SiteInstance::Create(browser_context.get())));
  scoped_ptr<RenderProcessHost> host2(instance2->GetProcess());
  EXPECT_TRUE(host2.get() != NULL);
  EXPECT_NE(host1.get(), host2.get());

  DrainMessageLoops();
}

// Test to ensure SetSite and site() work properly.
TEST_F(SiteInstanceTest, SetSite) {
  scoped_refptr<SiteInstanceImpl> instance(static_cast<SiteInstanceImpl*>(
      SiteInstance::Create(NULL)));
  EXPECT_FALSE(instance->HasSite());
  EXPECT_TRUE(instance->GetSiteURL().is_empty());

  instance->SetSite(GURL("http://www.google.com/index.html"));
  EXPECT_EQ(GURL("http://google.com"), instance->GetSiteURL());

  EXPECT_TRUE(instance->HasSite());

  DrainMessageLoops();
}

// Test to ensure GetSiteForURL properly returns sites for URLs.
TEST_F(SiteInstanceTest, GetSiteForURL) {
  // Pages are irrelevant.
  GURL test_url = GURL("http://www.google.com/index.html");
  EXPECT_EQ(GURL("http://google.com"),
            SiteInstanceImpl::GetSiteForURL(NULL, test_url));

  // Ports are irrlevant.
  test_url = GURL("https://www.google.com:8080");
  EXPECT_EQ(GURL("https://google.com"),
            SiteInstanceImpl::GetSiteForURL(NULL, test_url));

  // Javascript URLs have no site.
  test_url = GURL("javascript:foo();");
  EXPECT_EQ(GURL(), SiteInstanceImpl::GetSiteForURL(NULL, test_url));

  test_url = GURL("http://foo/a.html");
  EXPECT_EQ(GURL("http://foo"), SiteInstanceImpl::GetSiteForURL(
      NULL, test_url));

  test_url = GURL("file:///C:/Downloads/");
  EXPECT_EQ(GURL(), SiteInstanceImpl::GetSiteForURL(NULL, test_url));

  std::string guest_url(kGuestScheme);
  guest_url.append("://abc123");
  test_url = GURL(guest_url);
  EXPECT_EQ(test_url, SiteInstanceImpl::GetSiteForURL(NULL, test_url));

  // TODO(creis): Do we want to special case file URLs to ensure they have
  // either no site or a special "file://" site?  We currently return
  // "file://home/" as the site, which seems broken.
  // test_url = GURL("file://home/");
  // EXPECT_EQ(GURL(), SiteInstanceImpl::GetSiteForURL(NULL, test_url));

  DrainMessageLoops();
}

// Test of distinguishing URLs from different sites.  Most of this logic is
// tested in RegistryControlledDomainTest.  This test focuses on URLs with
// different schemes or ports.
TEST_F(SiteInstanceTest, IsSameWebSite) {
  GURL url_foo = GURL("http://foo/a.html");
  GURL url_foo2 = GURL("http://foo/b.html");
  GURL url_foo_https = GURL("https://foo/a.html");
  GURL url_foo_port = GURL("http://foo:8080/a.html");
  GURL url_javascript = GURL("javascript:alert(1);");

  // Same scheme and port -> same site.
  EXPECT_TRUE(SiteInstance::IsSameWebSite(NULL, url_foo, url_foo2));

  // Different scheme -> different site.
  EXPECT_FALSE(SiteInstance::IsSameWebSite(NULL, url_foo, url_foo_https));

  // Different port -> same site.
  // (Changes to document.domain make renderer ignore the port.)
  EXPECT_TRUE(SiteInstance::IsSameWebSite(NULL, url_foo, url_foo_port));

  // JavaScript links should be considered same site for anything.
  EXPECT_TRUE(SiteInstance::IsSameWebSite(NULL, url_javascript, url_foo));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(NULL, url_javascript, url_foo_https));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(NULL, url_javascript, url_foo_port));

  DrainMessageLoops();
}

// Test to ensure that there is only one SiteInstance per site in a given
// BrowsingInstance, when process-per-site is not in use.
TEST_F(SiteInstanceTest, OneSiteInstancePerSite) {
  ASSERT_FALSE(CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kProcessPerSite));
  int delete_counter = 0;
  scoped_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  TestBrowsingInstance* browsing_instance =
      new TestBrowsingInstance(browser_context.get(), &delete_counter);

  const GURL url_a1("http://www.google.com/1.html");
  scoped_refptr<SiteInstanceImpl> site_instance_a1(
      static_cast<SiteInstanceImpl*>(
          browsing_instance->GetSiteInstanceForURL(url_a1)));
  EXPECT_TRUE(site_instance_a1.get() != NULL);

  // A separate site should create a separate SiteInstance.
  const GURL url_b1("http://www.yahoo.com/");
  scoped_refptr<SiteInstanceImpl> site_instance_b1(
      static_cast<SiteInstanceImpl*>(
          browsing_instance->GetSiteInstanceForURL(url_b1)));
  EXPECT_NE(site_instance_a1.get(), site_instance_b1.get());
  EXPECT_TRUE(site_instance_a1->IsRelatedSiteInstance(site_instance_b1.get()));

  // Getting the new SiteInstance from the BrowsingInstance and from another
  // SiteInstance in the BrowsingInstance should give the same result.
  EXPECT_EQ(site_instance_b1.get(),
            site_instance_a1->GetRelatedSiteInstance(url_b1));

  // A second visit to the original site should return the same SiteInstance.
  const GURL url_a2("http://www.google.com/2.html");
  EXPECT_EQ(site_instance_a1.get(),
            browsing_instance->GetSiteInstanceForURL(url_a2));
  EXPECT_EQ(site_instance_a1.get(),
            site_instance_a1->GetRelatedSiteInstance(url_a2));

  // A visit to the original site in a new BrowsingInstance (same or different
  // browser context) should return a different SiteInstance.
  TestBrowsingInstance* browsing_instance2 =
      new TestBrowsingInstance(browser_context.get(), &delete_counter);
  // Ensure the new SiteInstance is ref counted so that it gets deleted.
  scoped_refptr<SiteInstanceImpl> site_instance_a2_2(
      static_cast<SiteInstanceImpl*>(
          browsing_instance2->GetSiteInstanceForURL(url_a2)));
  EXPECT_NE(site_instance_a1.get(), site_instance_a2_2.get());
  EXPECT_FALSE(
      site_instance_a1->IsRelatedSiteInstance(site_instance_a2_2.get()));

  // The two SiteInstances for http://google.com should not use the same process
  // if process-per-site is not enabled.
  scoped_ptr<RenderProcessHost> process_a1(site_instance_a1->GetProcess());
  scoped_ptr<RenderProcessHost> process_a2_2(site_instance_a2_2->GetProcess());
  EXPECT_NE(process_a1.get(), process_a2_2.get());

  // Should be able to see that we do have SiteInstances.
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GURL("http://mail.google.com")));
  EXPECT_TRUE(browsing_instance2->HasSiteInstance(
      GURL("http://mail.google.com")));
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GURL("http://mail.yahoo.com")));

  // Should be able to see that we don't have SiteInstances.
  EXPECT_FALSE(browsing_instance->HasSiteInstance(
      GURL("https://www.google.com")));
  EXPECT_FALSE(browsing_instance2->HasSiteInstance(
      GURL("http://www.yahoo.com")));

  // browsing_instances will be deleted when their SiteInstances are deleted.
  // The processes will be unregistered when the RPH scoped_ptrs go away.

  DrainMessageLoops();
}

// Test to ensure that there is only one RenderProcessHost per site for an
// entire BrowserContext, if process-per-site is in use.
TEST_F(SiteInstanceTest, OneSiteInstancePerSiteInBrowserContext) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerSite);
  int delete_counter = 0;
  scoped_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  TestBrowsingInstance* browsing_instance =
      new TestBrowsingInstance(browser_context.get(), &delete_counter);

  const GURL url_a1("http://www.google.com/1.html");
  scoped_refptr<SiteInstanceImpl> site_instance_a1(
      static_cast<SiteInstanceImpl*>(
          browsing_instance->GetSiteInstanceForURL(url_a1)));
  EXPECT_TRUE(site_instance_a1.get() != NULL);
  scoped_ptr<RenderProcessHost> process_a1(site_instance_a1->GetProcess());

  // A separate site should create a separate SiteInstance.
  const GURL url_b1("http://www.yahoo.com/");
  scoped_refptr<SiteInstanceImpl> site_instance_b1(
      static_cast<SiteInstanceImpl*>(
          browsing_instance->GetSiteInstanceForURL(url_b1)));
  EXPECT_NE(site_instance_a1.get(), site_instance_b1.get());
  EXPECT_TRUE(site_instance_a1->IsRelatedSiteInstance(site_instance_b1.get()));

  // Getting the new SiteInstance from the BrowsingInstance and from another
  // SiteInstance in the BrowsingInstance should give the same result.
  EXPECT_EQ(site_instance_b1.get(),
            site_instance_a1->GetRelatedSiteInstance(url_b1));

  // A second visit to the original site should return the same SiteInstance.
  const GURL url_a2("http://www.google.com/2.html");
  EXPECT_EQ(site_instance_a1.get(),
            browsing_instance->GetSiteInstanceForURL(url_a2));
  EXPECT_EQ(site_instance_a1.get(),
            site_instance_a1->GetRelatedSiteInstance(url_a2));

  // A visit to the original site in a new BrowsingInstance (same browser
  // context) should return a different SiteInstance with the same process.
  TestBrowsingInstance* browsing_instance2 =
      new TestBrowsingInstance(browser_context.get(), &delete_counter);
  scoped_refptr<SiteInstanceImpl> site_instance_a1_2(
      static_cast<SiteInstanceImpl*>(
          browsing_instance2->GetSiteInstanceForURL(url_a1)));
  EXPECT_TRUE(site_instance_a1.get() != NULL);
  EXPECT_NE(site_instance_a1.get(), site_instance_a1_2.get());
  EXPECT_EQ(process_a1.get(), site_instance_a1_2->GetProcess());

  // A visit to the original site in a new BrowsingInstance (different browser
  // context) should return a different SiteInstance with a different process.
  scoped_ptr<TestBrowserContext> browser_context2(new TestBrowserContext());
  TestBrowsingInstance* browsing_instance3 =
      new TestBrowsingInstance(browser_context2.get(), &delete_counter);
  scoped_refptr<SiteInstanceImpl> site_instance_a2_3(
      static_cast<SiteInstanceImpl*>(
          browsing_instance3->GetSiteInstanceForURL(url_a2)));
  EXPECT_TRUE(site_instance_a2_3.get() != NULL);
  scoped_ptr<RenderProcessHost> process_a2_3(site_instance_a2_3->GetProcess());
  EXPECT_NE(site_instance_a1.get(), site_instance_a2_3.get());
  EXPECT_NE(process_a1.get(), process_a2_3.get());

  // Should be able to see that we do have SiteInstances.
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GURL("http://mail.google.com")));  // visited before
  EXPECT_TRUE(browsing_instance2->HasSiteInstance(
      GURL("http://mail.google.com")));  // visited before
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GURL("http://mail.yahoo.com")));  // visited before

  // Should be able to see that we don't have SiteInstances.
  EXPECT_FALSE(browsing_instance2->HasSiteInstance(
      GURL("http://www.yahoo.com")));  // different BI, same browser context
  EXPECT_FALSE(browsing_instance->HasSiteInstance(
      GURL("https://www.google.com")));  // not visited before
  EXPECT_FALSE(browsing_instance3->HasSiteInstance(
      GURL("http://www.yahoo.com")));  // different BI, different context

  // browsing_instances will be deleted when their SiteInstances are deleted.
  // The processes will be unregistered when the RPH scoped_ptrs go away.

  DrainMessageLoops();
}

static SiteInstanceImpl* CreateSiteInstance(BrowserContext* browser_context,
                                            const GURL& url) {
  return static_cast<SiteInstanceImpl*>(
      SiteInstance::CreateForURL(browser_context, url));
}

// Test to ensure that pages that require certain privileges are grouped
// in processes with similar pages.
TEST_F(SiteInstanceTest, ProcessSharingByType) {
  // This test shouldn't run with --site-per-process mode, since it doesn't
  // allow render process reuse, which this test explicitly exercises.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSitePerProcess))
    return;

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Make a bunch of mock renderers so that we hit the limit.
  scoped_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  ScopedVector<MockRenderProcessHost> hosts;
  for (size_t i = 0; i < kMaxRendererProcessCount; ++i)
    hosts.push_back(new MockRenderProcessHost(browser_context.get()));

  // Create some extension instances and make sure they share a process.
  scoped_refptr<SiteInstanceImpl> extension1_instance(
      CreateSiteInstance(browser_context.get(),
          GURL(kPrivilegedScheme + std::string("://foo/bar"))));
  set_privileged_process_id(extension1_instance->GetProcess()->GetID());

  scoped_refptr<SiteInstanceImpl> extension2_instance(
      CreateSiteInstance(browser_context.get(),
          GURL(kPrivilegedScheme + std::string("://baz/bar"))));

  scoped_ptr<RenderProcessHost> extension_host(
      extension1_instance->GetProcess());
  EXPECT_EQ(extension1_instance->GetProcess(),
            extension2_instance->GetProcess());

  // Create some WebUI instances and make sure they share a process.
  scoped_refptr<SiteInstanceImpl> webui1_instance(CreateSiteInstance(
      browser_context.get(), GURL(kChromeUIScheme + std::string("://newtab"))));
  policy->GrantWebUIBindings(webui1_instance->GetProcess()->GetID());

  scoped_refptr<SiteInstanceImpl> webui2_instance(
      CreateSiteInstance(browser_context.get(),
                         GURL(kChromeUIScheme + std::string("://history"))));

  scoped_ptr<RenderProcessHost> dom_host(webui1_instance->GetProcess());
  EXPECT_EQ(webui1_instance->GetProcess(), webui2_instance->GetProcess());

  // Make sure none of differing privilege processes are mixed.
  EXPECT_NE(extension1_instance->GetProcess(), webui1_instance->GetProcess());

  for (size_t i = 0; i < kMaxRendererProcessCount; ++i) {
    EXPECT_NE(extension1_instance->GetProcess(), hosts[i]);
    EXPECT_NE(webui1_instance->GetProcess(), hosts[i]);
  }

  DrainMessageLoops();
}

// Test to ensure that HasWrongProcessForURL behaves properly for different
// types of URLs.
TEST_F(SiteInstanceTest, HasWrongProcessForURL) {
  scoped_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  scoped_ptr<RenderProcessHost> host;
  scoped_refptr<SiteInstanceImpl> instance(static_cast<SiteInstanceImpl*>(
      SiteInstance::Create(browser_context.get())));

  EXPECT_FALSE(instance->HasSite());
  EXPECT_TRUE(instance->GetSiteURL().is_empty());

  instance->SetSite(GURL("http://evernote.com/"));
  EXPECT_TRUE(instance->HasSite());

  // Check prior to "assigning" a process to the instance, which is expected
  // to return false due to not being attached to any process yet.
  EXPECT_FALSE(instance->HasWrongProcessForURL(GURL("http://google.com")));

  // The call to GetProcess actually creates a new real process, which works
  // fine, but might be a cause for problems in different contexts.
  host.reset(instance->GetProcess());
  EXPECT_TRUE(host.get() != NULL);
  EXPECT_TRUE(instance->HasProcess());

  EXPECT_FALSE(instance->HasWrongProcessForURL(GURL("http://evernote.com")));
  EXPECT_FALSE(instance->HasWrongProcessForURL(
      GURL("javascript:alert(document.location.href);")));

  EXPECT_TRUE(instance->HasWrongProcessForURL(GURL("chrome://settings")));

  // Test that WebUI SiteInstances reject normal web URLs.
  const GURL webui_url("chrome://settings");
  scoped_refptr<SiteInstanceImpl> webui_instance(static_cast<SiteInstanceImpl*>(
      SiteInstance::Create(browser_context.get())));
  webui_instance->SetSite(webui_url);
  scoped_ptr<RenderProcessHost> webui_host(webui_instance->GetProcess());

  // Simulate granting WebUI bindings for the process.
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantWebUIBindings(
      webui_host->GetID());

  EXPECT_TRUE(webui_instance->HasProcess());
  EXPECT_FALSE(webui_instance->HasWrongProcessForURL(webui_url));
  EXPECT_TRUE(webui_instance->HasWrongProcessForURL(GURL("http://google.com")));

  // WebUI uses process-per-site, so another instance will use the same process
  // even if we haven't called GetProcess yet.  Make sure HasWrongProcessForURL
  // doesn't crash (http://crbug.com/137070).
  scoped_refptr<SiteInstanceImpl> webui_instance2(
      static_cast<SiteInstanceImpl*>(
          SiteInstance::Create(browser_context.get())));
  webui_instance2->SetSite(webui_url);
  EXPECT_FALSE(webui_instance2->HasWrongProcessForURL(webui_url));
  EXPECT_TRUE(
      webui_instance2->HasWrongProcessForURL(GURL("http://google.com")));

  DrainMessageLoops();
}

// Test to ensure that HasWrongProcessForURL behaves properly even when
// --site-per-process is used (http://crbug.com/160671).
TEST_F(SiteInstanceTest, HasWrongProcessForURLInSitePerProcess) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSitePerProcess);

  scoped_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  scoped_ptr<RenderProcessHost> host;
  scoped_refptr<SiteInstanceImpl> instance(static_cast<SiteInstanceImpl*>(
      SiteInstance::Create(browser_context.get())));

  instance->SetSite(GURL("http://evernote.com/"));
  EXPECT_TRUE(instance->HasSite());

  // Check prior to "assigning" a process to the instance, which is expected
  // to return false due to not being attached to any process yet.
  EXPECT_FALSE(instance->HasWrongProcessForURL(GURL("http://google.com")));

  // The call to GetProcess actually creates a new real process, which works
  // fine, but might be a cause for problems in different contexts.
  host.reset(instance->GetProcess());
  EXPECT_TRUE(host.get() != NULL);
  EXPECT_TRUE(instance->HasProcess());

  EXPECT_FALSE(instance->HasWrongProcessForURL(GURL("http://evernote.com")));
  EXPECT_FALSE(instance->HasWrongProcessForURL(
      GURL("javascript:alert(document.location.href);")));

  EXPECT_TRUE(instance->HasWrongProcessForURL(GURL("chrome://settings")));

  DrainMessageLoops();
}

// Test that we do not reuse a process in process-per-site mode if it has the
// wrong bindings for its URL.  http://crbug.com/174059.
TEST_F(SiteInstanceTest, ProcessPerSiteWithWrongBindings) {
  scoped_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  scoped_ptr<RenderProcessHost> host;
  scoped_ptr<RenderProcessHost> host2;
  scoped_refptr<SiteInstanceImpl> instance(static_cast<SiteInstanceImpl*>(
      SiteInstance::Create(browser_context.get())));

  EXPECT_FALSE(instance->HasSite());
  EXPECT_TRUE(instance->GetSiteURL().is_empty());

  // Simulate navigating to a WebUI URL in a process that does not have WebUI
  // bindings.  This already requires bypassing security checks.
  const GURL webui_url("chrome://settings");
  instance->SetSite(webui_url);
  EXPECT_TRUE(instance->HasSite());

  // The call to GetProcess actually creates a new real process.
  host.reset(instance->GetProcess());
  EXPECT_TRUE(host.get() != NULL);
  EXPECT_TRUE(instance->HasProcess());

  // Without bindings, this should look like the wrong process.
  EXPECT_TRUE(instance->HasWrongProcessForURL(webui_url));

  // WebUI uses process-per-site, so another instance would normally use the
  // same process.  Make sure it doesn't use the same process if the bindings
  // are missing.
  scoped_refptr<SiteInstanceImpl> instance2(
      static_cast<SiteInstanceImpl*>(
          SiteInstance::Create(browser_context.get())));
  instance2->SetSite(webui_url);
  host2.reset(instance2->GetProcess());
  EXPECT_TRUE(host2.get() != NULL);
  EXPECT_TRUE(instance2->HasProcess());
  EXPECT_NE(host.get(), host2.get());

  DrainMessageLoops();
}

// Test that we do not register processes with empty sites for process-per-site
// mode.
TEST_F(SiteInstanceTest, NoProcessPerSiteForEmptySite) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerSite);
  scoped_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  scoped_ptr<RenderProcessHost> host;
  scoped_refptr<SiteInstanceImpl> instance(static_cast<SiteInstanceImpl*>(
      SiteInstance::Create(browser_context.get())));

  instance->SetSite(GURL());
  EXPECT_TRUE(instance->HasSite());
  EXPECT_TRUE(instance->GetSiteURL().is_empty());
  host.reset(instance->GetProcess());

  EXPECT_FALSE(RenderProcessHostImpl::GetProcessHostForSite(
      browser_context.get(), GURL()));

  DrainMessageLoops();
}

}  // namespace content
