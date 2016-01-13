// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_service_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

const char kNPAPITestPluginMimeType[] = "application/vnd.npapi-test";

void OpenChannel(PluginProcessHost::Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Start opening the channel
  PluginServiceImpl::GetInstance()->OpenChannelToNpapiPlugin(
      0, 0, GURL(), GURL(), kNPAPITestPluginMimeType, client);
}

// Mock up of the Client and the Listener classes that would supply the
// communication channel with the plugin.
class MockPluginProcessHostClient : public PluginProcessHost::Client,
                                    public IPC::Listener {
 public:
  MockPluginProcessHostClient(ResourceContext* context, bool expect_fail)
      : context_(context),
        channel_(NULL),
        set_plugin_info_called_(false),
        expect_fail_(expect_fail) {
  }

  virtual ~MockPluginProcessHostClient() {
    if (channel_)
      BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, channel_);
  }

  // PluginProcessHost::Client implementation.
  virtual int ID() OVERRIDE { return 42; }
  virtual bool OffTheRecord() OVERRIDE { return false; }
  virtual ResourceContext* GetResourceContext() OVERRIDE {
    return context_;
  }
  virtual void OnFoundPluginProcessHost(PluginProcessHost* host) OVERRIDE {}
  virtual void OnSentPluginChannelRequest() OVERRIDE {}

  virtual void OnChannelOpened(const IPC::ChannelHandle& handle) OVERRIDE {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ASSERT_TRUE(set_plugin_info_called_);
    ASSERT_TRUE(!channel_);
    channel_ = IPC::Channel::CreateClient(handle, this).release();
    ASSERT_TRUE(channel_->Connect());
  }

  virtual void SetPluginInfo(const WebPluginInfo& info) OVERRIDE {
    ASSERT_TRUE(info.mime_types.size());
    ASSERT_EQ(kNPAPITestPluginMimeType, info.mime_types[0].mime_type);
    set_plugin_info_called_ = true;
  }

  virtual void OnError() OVERRIDE {
    Fail();
  }

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    Fail();
    return false;
  }
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE {
    if (expect_fail_)
      FAIL();
    QuitMessageLoop();
  }
  virtual void OnChannelError() OVERRIDE {
    Fail();
  }
#if defined(OS_POSIX)
  virtual void OnChannelDenied() OVERRIDE {
    Fail();
  }
  virtual void OnChannelListenError() OVERRIDE {
    Fail();
  }
#endif

 private:
  void Fail() {
    if (!expect_fail_)
      FAIL();
    QuitMessageLoop();
  }

  void QuitMessageLoop() {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::MessageLoop::QuitClosure());
  }

  ResourceContext* context_;
  IPC::Channel* channel_;
  bool set_plugin_info_called_;
  bool expect_fail_;
  DISALLOW_COPY_AND_ASSIGN(MockPluginProcessHostClient);
};

class MockPluginServiceFilter : public content::PluginServiceFilter {
 public:
  MockPluginServiceFilter() {}

  virtual bool IsPluginAvailable(
      int render_process_id,
      int render_view_id,
      const void* context,
      const GURL& url,
      const GURL& policy_url,
      WebPluginInfo* plugin) OVERRIDE { return true; }

  virtual bool CanLoadPlugin(
      int render_process_id,
      const base::FilePath& path) OVERRIDE { return false; }
};

class PluginServiceTest : public ContentBrowserTest {
 public:
  PluginServiceTest() {}

  ResourceContext* GetResourceContext() {
    return shell()->web_contents()->GetBrowserContext()->GetResourceContext();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
#if defined(OS_MACOSX)
    base::FilePath browser_directory;
    PathService::Get(base::DIR_MODULE, &browser_directory);
    command_line->AppendSwitchPath(switches::kExtraPluginDir,
                                   browser_directory.AppendASCII("plugins"));
#endif
    // TODO(jam): since these plugin tests are running under Chrome, we need to
    // tell it to disable its security features for old plugins. Once this is
    // running under content_browsertests, these flags won't be needed.
    // http://crbug.com/90448
    // switches::kAlwaysAuthorizePlugins
    command_line->AppendSwitch("always-authorize-plugins");
  }
};

// Try to open a channel to the test plugin. Minimal plugin process spawning
// test for the PluginService interface.
IN_PROC_BROWSER_TEST_F(PluginServiceTest, OpenChannelToPlugin) {
  if (!PluginServiceImpl::GetInstance()->NPAPIPluginsSupported())
    return;
  MockPluginProcessHostClient mock_client(GetResourceContext(), false);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&OpenChannel, &mock_client));
  RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(PluginServiceTest, OpenChannelToDeniedPlugin) {
  if (!PluginServiceImpl::GetInstance()->NPAPIPluginsSupported())
    return;
  MockPluginServiceFilter filter;
  PluginServiceImpl::GetInstance()->SetFilter(&filter);
  MockPluginProcessHostClient mock_client(GetResourceContext(), true);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&OpenChannel, &mock_client));
  RunMessageLoop();
}

// A strict mock that fails if any of the methods are called. They shouldn't be
// called since the request should get canceled before then.
class MockCanceledPluginServiceClient : public PluginProcessHost::Client {
 public:
  MockCanceledPluginServiceClient(ResourceContext* context)
      : context_(context),
        get_resource_context_called_(false) {
  }

  virtual ~MockCanceledPluginServiceClient() {}

  // Client implementation.
  MOCK_METHOD0(ID, int());
  virtual ResourceContext* GetResourceContext() OVERRIDE {
    get_resource_context_called_ = true;
    return context_;
  }
  MOCK_METHOD0(OffTheRecord, bool());
  MOCK_METHOD1(OnFoundPluginProcessHost, void(PluginProcessHost* host));
  MOCK_METHOD0(OnSentPluginChannelRequest, void());
  MOCK_METHOD1(OnChannelOpened, void(const IPC::ChannelHandle& handle));
  MOCK_METHOD1(SetPluginInfo, void(const WebPluginInfo& info));
  MOCK_METHOD0(OnError, void());

  bool get_resource_context_called() const {
    return get_resource_context_called_;
  }

 private:
  ResourceContext* context_;
  bool get_resource_context_called_;

  DISALLOW_COPY_AND_ASSIGN(MockCanceledPluginServiceClient);
};

void QuitUIMessageLoopFromIOThread() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::MessageLoop::QuitClosure());
}

void OpenChannelAndThenCancel(PluginProcessHost::Client* client) {
  OpenChannel(client);
  // Immediately cancel it. This is guaranteed to work since PluginService needs
  // to consult its filter on the FILE thread.
  PluginServiceImpl::GetInstance()->CancelOpenChannelToNpapiPlugin(client);
  // Before we terminate the test, add a roundtrip through the FILE thread to
  // make sure that it's had a chance to post back to the IO thread. Then signal
  // the UI thread to stop and exit the test.
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&base::DoNothing),
      base::Bind(&QuitUIMessageLoopFromIOThread));
}

// Should not attempt to open a channel, since it should be canceled early on.
IN_PROC_BROWSER_TEST_F(PluginServiceTest, CancelOpenChannelToPluginService) {
  ::testing::StrictMock<MockCanceledPluginServiceClient> mock_client(
      GetResourceContext());
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(OpenChannelAndThenCancel, &mock_client));
  RunMessageLoop();
  EXPECT_TRUE(mock_client.get_resource_context_called());
}

class MockCanceledBeforeSentPluginProcessHostClient
    : public MockCanceledPluginServiceClient {
 public:
  MockCanceledBeforeSentPluginProcessHostClient(
      ResourceContext* context)
      : MockCanceledPluginServiceClient(context),
        set_plugin_info_called_(false),
        on_found_plugin_process_host_called_(false),
        host_(NULL) {}

  virtual ~MockCanceledBeforeSentPluginProcessHostClient() {}

  // Client implementation.
  virtual void SetPluginInfo(const WebPluginInfo& info) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ASSERT_TRUE(info.mime_types.size());
    ASSERT_EQ(kNPAPITestPluginMimeType, info.mime_types[0].mime_type);
    set_plugin_info_called_ = true;
  }
  virtual void OnFoundPluginProcessHost(PluginProcessHost* host) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    set_on_found_plugin_process_host_called();
    set_host(host);
    // This gets called right before we request the plugin<=>renderer channel,
    // so we have to post a task to cancel it.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PluginProcessHost::CancelPendingRequest,
                   base::Unretained(host),
                   this));
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&QuitUIMessageLoopFromIOThread));
  }

  bool set_plugin_info_called() const {
    return set_plugin_info_called_;
  }

  bool on_found_plugin_process_host_called() const {
    return on_found_plugin_process_host_called_;
  }

 protected:
  void set_on_found_plugin_process_host_called() {
    on_found_plugin_process_host_called_ = true;
  }
  void set_host(PluginProcessHost* host) {
    host_ = host;
  }

  PluginProcessHost* host() const { return host_; }

 private:
  bool set_plugin_info_called_;
  bool on_found_plugin_process_host_called_;
  PluginProcessHost* host_;

  DISALLOW_COPY_AND_ASSIGN(MockCanceledBeforeSentPluginProcessHostClient);
};

IN_PROC_BROWSER_TEST_F(
    PluginServiceTest, CancelBeforeSentOpenChannelToPluginProcessHost) {
  if (!PluginServiceImpl::GetInstance()->NPAPIPluginsSupported())
    return;
  ::testing::StrictMock<MockCanceledBeforeSentPluginProcessHostClient>
      mock_client(GetResourceContext());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&OpenChannel, &mock_client));
  RunMessageLoop();
  EXPECT_TRUE(mock_client.get_resource_context_called());
  EXPECT_TRUE(mock_client.set_plugin_info_called());
  EXPECT_TRUE(mock_client.on_found_plugin_process_host_called());
}

class MockCanceledAfterSentPluginProcessHostClient
    : public MockCanceledBeforeSentPluginProcessHostClient {
 public:
  MockCanceledAfterSentPluginProcessHostClient(
      ResourceContext* context)
      : MockCanceledBeforeSentPluginProcessHostClient(context),
        on_sent_plugin_channel_request_called_(false) {}
  virtual ~MockCanceledAfterSentPluginProcessHostClient() {}

  // Client implementation.

  virtual int ID() OVERRIDE { return 42; }
  virtual bool OffTheRecord() OVERRIDE { return false; }

  // We override this guy again since we don't want to cancel yet.
  virtual void OnFoundPluginProcessHost(PluginProcessHost* host) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    set_on_found_plugin_process_host_called();
    set_host(host);
  }

  virtual void OnSentPluginChannelRequest() OVERRIDE {
    on_sent_plugin_channel_request_called_ = true;
    host()->CancelSentRequest(this);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::MessageLoop::QuitClosure());
  }

  bool on_sent_plugin_channel_request_called() const {
    return on_sent_plugin_channel_request_called_;
  }

 private:
  bool on_sent_plugin_channel_request_called_;

  DISALLOW_COPY_AND_ASSIGN(MockCanceledAfterSentPluginProcessHostClient);
};

// Should not attempt to open a channel, since it should be canceled early on.
IN_PROC_BROWSER_TEST_F(
    PluginServiceTest, CancelAfterSentOpenChannelToPluginProcessHost) {
  if (!PluginServiceImpl::GetInstance()->NPAPIPluginsSupported())
    return;
  ::testing::StrictMock<MockCanceledAfterSentPluginProcessHostClient>
      mock_client(GetResourceContext());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&OpenChannel, &mock_client));
  RunMessageLoop();
  EXPECT_TRUE(mock_client.get_resource_context_called());
  EXPECT_TRUE(mock_client.set_plugin_info_called());
  EXPECT_TRUE(mock_client.on_found_plugin_process_host_called());
  EXPECT_TRUE(mock_client.on_sent_plugin_channel_request_called());
}

}  // namespace content
