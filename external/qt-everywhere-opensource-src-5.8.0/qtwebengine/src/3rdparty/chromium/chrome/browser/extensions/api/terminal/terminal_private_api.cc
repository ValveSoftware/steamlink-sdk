// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/terminal_private_api.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/extensions/api/terminal_private.h"
#include "chromeos/process_proxy/process_proxy_registry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/event_router.h"

namespace terminal_private = extensions::api::terminal_private;
namespace OnTerminalResize =
    extensions::api::terminal_private::OnTerminalResize;
namespace OpenTerminalProcess =
    extensions::api::terminal_private::OpenTerminalProcess;
namespace CloseTerminalProcess =
    extensions::api::terminal_private::CloseTerminalProcess;
namespace SendInput = extensions::api::terminal_private::SendInput;
namespace AckOutput = extensions::api::terminal_private::AckOutput;

namespace {

const char kCroshName[] = "crosh";
const char kCroshCommand[] = "/usr/bin/crosh";
// We make stubbed crosh just echo back input.
const char kStubbedCroshCommand[] = "cat";

const char* GetCroshPath() {
  if (base::SysInfo::IsRunningOnChromeOS())
    return kCroshCommand;
  else
    return kStubbedCroshCommand;
}

const char* GetProcessCommandForName(const std::string& name) {
  if (name == kCroshName)
    return GetCroshPath();
  else
    return NULL;
}

void NotifyProcessOutput(content::BrowserContext* browser_context,
                         const std::string& extension_id,
                         int tab_id,
                         int terminal_id,
                         const std::string& output_type,
                         const std::string& output) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&NotifyProcessOutput, browser_context, extension_id, tab_id,
                   terminal_id, output_type, output));
    return;
  }

  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(new base::FundamentalValue(tab_id));
  args->Append(new base::FundamentalValue(terminal_id));
  args->Append(new base::StringValue(output_type));
  args->Append(new base::StringValue(output));

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(browser_context);
  if (event_router) {
    std::unique_ptr<extensions::Event> event(new extensions::Event(
        extensions::events::TERMINAL_PRIVATE_ON_PROCESS_OUTPUT,
        terminal_private::OnProcessOutput::kEventName, std::move(args)));
    event_router->DispatchEventToExtension(extension_id, std::move(event));
  }
}

// Returns tab ID, or window session ID (for platform apps) for |web_contents|.
int GetTabOrWindowSessionId(content::BrowserContext* browser_context,
                            content::WebContents* web_contents) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  if (tab_id >= 0)
    return tab_id;
  extensions::AppWindow* window =
      extensions::AppWindowRegistry::Get(browser_context)
          ->GetAppWindowForWebContents(web_contents);
  return window ? window->session_id().id() : -1;
}

}  // namespace

namespace extensions {

TerminalPrivateOpenTerminalProcessFunction::
    TerminalPrivateOpenTerminalProcessFunction() : command_(NULL) {}

TerminalPrivateOpenTerminalProcessFunction::
    ~TerminalPrivateOpenTerminalProcessFunction() {}

ExtensionFunction::ResponseAction
TerminalPrivateOpenTerminalProcessFunction::Run() {
  std::unique_ptr<OpenTerminalProcess::Params> params(
      OpenTerminalProcess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  command_ = GetProcessCommandForName(params->process_name);
  if (!command_)
    return RespondNow(Error("Invalid process name."));

  content::WebContents* caller_contents = GetSenderWebContents();
  if (!caller_contents)
    return RespondNow(Error("No web contents."));

  // Passed to terminalPrivate.ackOutput, which is called from the API's custom
  // bindings after terminalPrivate.onProcessOutput is dispatched. It is used to
  // determine whether ackOutput call should be handled or not. ackOutput will
  // be called from every web contents in which a onProcessOutput listener
  // exists (because the API custom bindings hooks are run in every web contents
  // with a listener). Only ackOutput called from the web contents that has the
  // target terminal instance should be handled.
  // TODO(tbarzic): Instead of passing tab/app window session id around, keep
  //     mapping from web_contents to terminal ID running in it. This will be
  //     needed to fix crbug.com/210295.
  int tab_id = GetTabOrWindowSessionId(browser_context(), caller_contents);
  if (tab_id < 0)
    return RespondNow(Error("Not called from a tab or app window"));

  // Registry lives on FILE thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &TerminalPrivateOpenTerminalProcessFunction::OpenOnFileThread, this,
          base::Bind(&NotifyProcessOutput, browser_context(), extension_id(),
                     tab_id),
          base::Bind(
              &TerminalPrivateOpenTerminalProcessFunction::RespondOnUIThread,
              this)));
  return RespondLater();
}

void TerminalPrivateOpenTerminalProcessFunction::OpenOnFileThread(
    const ProcessOutputCallback& output_callback,
    const OpenProcessCallback& callback) {
  DCHECK(command_);

  chromeos::ProcessProxyRegistry* registry =
      chromeos::ProcessProxyRegistry::Get();

  int terminal_id = registry->OpenProcess(command_, output_callback);

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(callback, terminal_id));
}

TerminalPrivateSendInputFunction::~TerminalPrivateSendInputFunction() {}

void TerminalPrivateOpenTerminalProcessFunction::RespondOnUIThread(
    int terminal_id) {
  if (terminal_id < 0) {
    SetError("Failed to open process.");
    SendResponse(false);
    return;
  }
  SetResult(base::MakeUnique<base::FundamentalValue>(terminal_id));
  SendResponse(true);
}

ExtensionFunction::ResponseAction TerminalPrivateSendInputFunction::Run() {
  std::unique_ptr<SendInput::Params> params(SendInput::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Registry lives on the FILE thread.
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&TerminalPrivateSendInputFunction::SendInputOnFileThread,
                 this, params->pid, params->input));
  return RespondLater();
}

void TerminalPrivateSendInputFunction::SendInputOnFileThread(
    int terminal_id,
    const std::string& text) {
  bool success =
      chromeos::ProcessProxyRegistry::Get()->SendInput(terminal_id, text);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&TerminalPrivateSendInputFunction::RespondOnUIThread, this,
                 success));
}

void TerminalPrivateSendInputFunction::RespondOnUIThread(bool success) {
  SetResult(base::MakeUnique<base::FundamentalValue>(success));
  SendResponse(true);
}

TerminalPrivateCloseTerminalProcessFunction::
    ~TerminalPrivateCloseTerminalProcessFunction() {}

ExtensionFunction::ResponseAction
TerminalPrivateCloseTerminalProcessFunction::Run() {
  std::unique_ptr<CloseTerminalProcess::Params> params(
      CloseTerminalProcess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Registry lives on the FILE thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &TerminalPrivateCloseTerminalProcessFunction::CloseOnFileThread, this,
          params->pid));

  return RespondLater();
}

void TerminalPrivateCloseTerminalProcessFunction::CloseOnFileThread(
    int terminal_id) {
  bool success =
      chromeos::ProcessProxyRegistry::Get()->CloseProcess(terminal_id);

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&TerminalPrivateCloseTerminalProcessFunction::
                 RespondOnUIThread, this, success));
}

void TerminalPrivateCloseTerminalProcessFunction::RespondOnUIThread(
    bool success) {
  SetResult(base::MakeUnique<base::FundamentalValue>(success));
  SendResponse(true);
}

TerminalPrivateOnTerminalResizeFunction::
    ~TerminalPrivateOnTerminalResizeFunction() {}

ExtensionFunction::ResponseAction
TerminalPrivateOnTerminalResizeFunction::Run() {
  std::unique_ptr<OnTerminalResize::Params> params(
      OnTerminalResize::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Registry lives on the FILE thread.
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&TerminalPrivateOnTerminalResizeFunction::OnResizeOnFileThread,
                 this, params->pid, params->width, params->height));

  return RespondLater();
}

void TerminalPrivateOnTerminalResizeFunction::OnResizeOnFileThread(
    int terminal_id,
    int width,
    int height) {
  bool success = chromeos::ProcessProxyRegistry::Get()->OnTerminalResize(
      terminal_id, width, height);

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&TerminalPrivateOnTerminalResizeFunction::RespondOnUIThread,
                 this, success));
}

void TerminalPrivateOnTerminalResizeFunction::RespondOnUIThread(bool success) {
  SetResult(base::MakeUnique<base::FundamentalValue>(success));
  SendResponse(true);
}

TerminalPrivateAckOutputFunction::~TerminalPrivateAckOutputFunction() {}

ExtensionFunction::ResponseAction TerminalPrivateAckOutputFunction::Run() {
  std::unique_ptr<AckOutput::Params> params(AckOutput::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::WebContents* caller_contents = GetSenderWebContents();
  if (!caller_contents)
    return RespondNow(Error("No web contents."));

  int tab_id = GetTabOrWindowSessionId(browser_context(), caller_contents);
  if (tab_id < 0)
    return RespondNow(Error("Not called from a tab or app window"));

  if (tab_id != params->tab_id)
    return RespondNow(NoArguments());

  // Registry lives on the FILE thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&TerminalPrivateAckOutputFunction::AckOutputOnFileThread, this,
                 params->pid));

  return RespondNow(NoArguments());
}

void TerminalPrivateAckOutputFunction::AckOutputOnFileThread(int terminal_id) {
  chromeos::ProcessProxyRegistry::Get()->AckOutput(terminal_id);
}

}  // namespace extensions
