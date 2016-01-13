// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/devtools/renderer_overrides_handler.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

class RendererOverridesHandlerTest : public ContentBrowserTest {
 protected:
  scoped_refptr<DevToolsProtocol::Response> SendCommand(
      const std::string& method,
      base::DictionaryValue* params) {
    scoped_ptr<RendererOverridesHandler> handler(CreateHandler());
    scoped_refptr<DevToolsProtocol::Command> command(
        DevToolsProtocol::CreateCommand(1, method, params));
    return handler->HandleCommand(command);
  }

  void SendAsyncCommand(const std::string& method,
                        base::DictionaryValue* params) {
    scoped_ptr<RendererOverridesHandler> handler(CreateHandler());
    scoped_refptr<DevToolsProtocol::Command> command(
        DevToolsProtocol::CreateCommand(1, method, params));
    scoped_refptr<DevToolsProtocol::Response> response =
        handler->HandleCommand(command);
    EXPECT_TRUE(response->is_async_promise());
    base::MessageLoop::current()->Run();
  }

  bool HasValue(const std::string& path) {
    base::Value* value = 0;
    return result_->Get(path, &value);
  }

  bool HasListItem(const std::string& path_to_list,
                   const std::string& name,
                   const std::string& value) {
    base::ListValue* list;
    if (!result_->GetList(path_to_list, &list))
      return false;

    for (size_t i = 0; i != list->GetSize(); i++) {
      base::DictionaryValue* item;
      if (!list->GetDictionary(i, &item))
        return false;
      std::string id;
      if (!item->GetString(name, &id))
        return false;
      if (id == value)
        return true;
    }
    return false;
  }

  scoped_ptr<base::DictionaryValue> result_;

 private:
  RendererOverridesHandler* CreateHandler() {
    RenderViewHost* rvh = shell()->web_contents()->GetRenderViewHost();
    DevToolsAgentHost* agent = DevToolsAgentHost::GetOrCreateFor(rvh).get();
    scoped_ptr<RendererOverridesHandler> handler(
        new RendererOverridesHandler(agent));
    handler->SetNotifier(base::Bind(
        &RendererOverridesHandlerTest::OnMessageSent, base::Unretained(this)));
    return handler.release();
  }

  void OnMessageSent(const std::string& message) {
    scoped_ptr<base::DictionaryValue> root(
        static_cast<base::DictionaryValue*>(base::JSONReader::Read(message)));
    base::DictionaryValue* result;
    root->GetDictionary("result", &result);
    result_.reset(result->DeepCopy());
    base::MessageLoop::current()->QuitNow();
  }
};

IN_PROC_BROWSER_TEST_F(RendererOverridesHandlerTest, QueryUsageAndQuota) {
  base::DictionaryValue* params = new base::DictionaryValue();
  params->SetString("securityOrigin", "http://example.com");
  SendAsyncCommand("Page.queryUsageAndQuota", params);

  EXPECT_TRUE(HasValue("quota.persistent"));
  EXPECT_TRUE(HasValue("quota.temporary"));
  EXPECT_TRUE(HasListItem("usage.temporary", "id", "appcache"));
  EXPECT_TRUE(HasListItem("usage.temporary", "id", "database"));
  EXPECT_TRUE(HasListItem("usage.temporary", "id", "indexeddatabase"));
  EXPECT_TRUE(HasListItem("usage.temporary", "id", "filesystem"));
  EXPECT_TRUE(HasListItem("usage.persistent", "id", "filesystem"));
}

}  // namespace content
