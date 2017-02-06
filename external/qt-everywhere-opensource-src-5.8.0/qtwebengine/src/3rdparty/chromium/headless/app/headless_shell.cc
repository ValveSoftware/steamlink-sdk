// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/content_switches.h"
#include "headless/app/headless_shell_switches.h"
#include "headless/public/domains/page.h"
#include "headless/public/domains/runtime.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "ui/gfx/geometry/size.h"

using headless::HeadlessBrowser;
using headless::HeadlessDevToolsClient;
using headless::HeadlessWebContents;
namespace page = headless::page;
namespace runtime = headless::runtime;

namespace {
// Address where to listen to incoming DevTools connections.
const char kDevToolsHttpServerAddress[] = "127.0.0.1";
// Default file name for screenshot. Can be overriden by "--screenshot" switch.
const char kDefaultScreenshotFileName[] = "screenshot.png";
}

// A sample application which demonstrates the use of the headless API.
class HeadlessShell : public HeadlessWebContents::Observer, page::Observer {
 public:
  HeadlessShell()
      : browser_(nullptr),
        devtools_client_(HeadlessDevToolsClient::Create()),
        web_contents_(nullptr),
        processed_page_ready_(false) {}
  ~HeadlessShell() override {}

  void OnStart(HeadlessBrowser* browser) {
    browser_ = browser;

    HeadlessWebContents::Builder builder(browser_->CreateWebContentsBuilder());
    base::CommandLine::StringVector args =
        base::CommandLine::ForCurrentProcess()->GetArgs();

    if (!args.empty() && !args[0].empty())
      builder.SetInitialURL(GURL(args[0]));

    web_contents_ = builder.Build();
    if (!web_contents_) {
      LOG(ERROR) << "Navigation failed";
      browser_->Shutdown();
      return;
    }
    web_contents_->AddObserver(this);
  }

  void Shutdown() {
    if (!web_contents_)
      return;
    if (!RemoteDebuggingEnabled()) {
      devtools_client_->GetPage()->RemoveObserver(this);
      web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
    }
    web_contents_->RemoveObserver(this);
    web_contents_ = nullptr;
    browser_->Shutdown();
  }

  // HeadlessWebContents::Observer implementation:
  void DevToolsTargetReady() override {
    if (RemoteDebuggingEnabled())
      return;
    web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
    devtools_client_->GetPage()->AddObserver(this);
    devtools_client_->GetPage()->Enable();
    // Check if the document had already finished loading by the time we
    // attached.
    PollReadyState();
    // TODO(skyostil): Implement more features to demonstrate the devtools API.
  }

  void PollReadyState() {
    // We need to check the current location in addition to the ready state to
    // be sure the expected page is ready.
    devtools_client_->GetRuntime()->Evaluate(
        "document.readyState + ' ' + document.location.href",
        base::Bind(&HeadlessShell::OnReadyState, base::Unretained(this)));
  }

  void OnReadyState(std::unique_ptr<runtime::EvaluateResult> result) {
    std::string ready_state_and_url;
    if (result->GetResult()->GetValue()->GetAsString(&ready_state_and_url)) {
      std::stringstream stream(ready_state_and_url);
      std::string ready_state;
      std::string url;
      stream >> ready_state;
      stream >> url;

      if (ready_state == "complete" &&
          (url_.spec() == url || url != "about:blank")) {
        OnPageReady();
        return;
      }
    }
  }

  // page::Observer implementation:
  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    OnPageReady();
  }

  void OnPageReady() {
    if (processed_page_ready_)
      return;
    processed_page_ready_ = true;

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            headless::switches::kDumpDom)) {
      FetchDom();
    } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                   headless::switches::kRepl)) {
      std::cout
          << "Type a Javascript expression to evaluate or \"quit\" to exit."
          << std::endl;
      InputExpression();
    } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                   headless::switches::kScreenshot)) {
      CaptureScreenshot();
    } else {
      Shutdown();
    }
  }

  void FetchDom() {
    devtools_client_->GetRuntime()->Evaluate(
        "document.body.innerHTML",
        base::Bind(&HeadlessShell::OnDomFetched, base::Unretained(this)));
  }

  void OnDomFetched(std::unique_ptr<runtime::EvaluateResult> result) {
    if (result->GetWasThrown()) {
      LOG(ERROR) << "Failed to evaluate document.body.innerHTML";
    } else {
      std::string dom;
      if (result->GetResult()->GetValue()->GetAsString(&dom)) {
        std::cout << dom << std::endl;
      }
    }
    Shutdown();
  }

  void InputExpression() {
    // Note that a real system should read user input asynchronously, because
    // otherwise all other browser activity is suspended (e.g., page loading).
    std::string expression;
    std::cout << ">>> ";
    std::getline(std::cin, expression);
    if (std::cin.bad() || std::cin.eof() || expression == "quit") {
      Shutdown();
      return;
    }
    devtools_client_->GetRuntime()->Evaluate(
        expression,
        base::Bind(&HeadlessShell::OnExpressionResult, base::Unretained(this)));
  }

  void OnExpressionResult(std::unique_ptr<runtime::EvaluateResult> result) {
    std::unique_ptr<base::Value> value = result->Serialize();
    std::string result_json;
    base::JSONWriter::Write(*value, &result_json);
    std::cout << result_json << std::endl;
    InputExpression();
  }

  void CaptureScreenshot() {
    devtools_client_->GetPage()->GetExperimental()->CaptureScreenshot(
        page::CaptureScreenshotParams::Builder().Build(),
        base::Bind(&HeadlessShell::OnScreenshotCaptured,
                   base::Unretained(this)));
  }

  void OnScreenshotCaptured(
      std::unique_ptr<page::CaptureScreenshotResult> result) {
    base::FilePath file_name =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            headless::switches::kScreenshot);
    if (file_name.empty()) {
      file_name = base::FilePath().AppendASCII(kDefaultScreenshotFileName);
    }

    screenshot_file_stream_.reset(
        new net::FileStream(browser_->BrowserFileThread()));
    const int open_result = screenshot_file_stream_->Open(
        file_name, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                       base::File::FLAG_ASYNC,
        base::Bind(&HeadlessShell::OnScreenshotFileOpened,
                   base::Unretained(this), base::Passed(std::move(result)),
                   file_name));
    if (open_result != net::ERR_IO_PENDING) {
      // Operation could not be started.
      OnScreenshotFileOpened(nullptr, file_name, open_result);
    }
  }

  void OnScreenshotFileOpened(
      std::unique_ptr<page::CaptureScreenshotResult> result,
      const base::FilePath file_name,
      const int open_result) {
    if (open_result != net::OK) {
      LOG(ERROR) << "Writing screenshot to file " << file_name.value()
                 << " was unsuccessful, could not open file: "
                 << net::ErrorToString(open_result);
      return;
    }

    std::string decoded_png;
    base::Base64Decode(result->GetData(), &decoded_png);
    scoped_refptr<net::IOBufferWithSize> buf =
        new net::IOBufferWithSize(decoded_png.size());
    memcpy(buf->data(), decoded_png.data(), decoded_png.size());
    const int write_result = screenshot_file_stream_->Write(
        buf.get(), buf->size(),
        base::Bind(&HeadlessShell::OnScreenshotFileWritten,
                   base::Unretained(this), file_name, buf->size()));
    if (write_result != net::ERR_IO_PENDING) {
      // Operation may have completed successfully or failed.
      OnScreenshotFileWritten(file_name, buf->size(), write_result);
    }
  }

  void OnScreenshotFileWritten(const base::FilePath file_name,
                               const int length,
                               const int write_result) {
    if (write_result < length) {
      // TODO(eseckler): Support recovering from partial writes.
      LOG(ERROR) << "Writing screenshot to file " << file_name.value()
                 << " was unsuccessful: " << net::ErrorToString(write_result);
    } else {
      std::cout << "Screenshot written to file " << file_name.value() << "."
                << std::endl;
    }
    int close_result = screenshot_file_stream_->Close(base::Bind(
        &HeadlessShell::OnScreenshotFileClosed, base::Unretained(this)));
    if (close_result != net::ERR_IO_PENDING) {
      // Operation could not be started.
      OnScreenshotFileClosed(close_result);
    }
  }

  void OnScreenshotFileClosed(const int close_result) { Shutdown(); }

  bool RemoteDebuggingEnabled() const {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    return command_line.HasSwitch(switches::kRemoteDebuggingPort);
  }

 private:
  GURL url_;
  HeadlessBrowser* browser_;  // Not owned.
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;
  HeadlessWebContents* web_contents_;
  bool processed_page_ready_;
  std::unique_ptr<net::FileStream> screenshot_file_stream_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessShell);
};

int main(int argc, const char** argv) {
  headless::RunChildProcessIfNeeded(argc, argv);
  HeadlessShell shell;
  HeadlessBrowser::Options::Builder builder(argc, argv);

  // Enable devtools if requested.
  base::CommandLine command_line(argc, argv);
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    std::string address = kDevToolsHttpServerAddress;
    if (command_line.HasSwitch(headless::switches::kRemoteDebuggingAddress)) {
      address = command_line.GetSwitchValueASCII(
          headless::switches::kRemoteDebuggingAddress);
      net::IPAddress parsed_address;
      if (!net::ParseURLHostnameToAddress(address, &parsed_address)) {
        LOG(ERROR) << "Invalid devtools server address";
        return EXIT_FAILURE;
      }
    }
    int parsed_port;
    std::string port_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    if (!base::StringToInt(port_str, &parsed_port) ||
        !base::IsValueInRangeForNumericType<uint16_t>(parsed_port)) {
      LOG(ERROR) << "Invalid devtools server port";
      return EXIT_FAILURE;
    }
    net::IPAddress devtools_address;
    bool result = devtools_address.AssignFromIPLiteral(address);
    DCHECK(result);
    builder.EnableDevToolsServer(net::IPEndPoint(
        devtools_address, base::checked_cast<uint16_t>(parsed_port)));
  }

  if (command_line.HasSwitch(headless::switches::kProxyServer)) {
    std::string proxy_server =
        command_line.GetSwitchValueASCII(headless::switches::kProxyServer);
    net::HostPortPair parsed_proxy_server =
        net::HostPortPair::FromString(proxy_server);
    if (parsed_proxy_server.host().empty() || !parsed_proxy_server.port()) {
      LOG(ERROR) << "Malformed proxy server url";
      return EXIT_FAILURE;
    }
    builder.SetProxyServer(parsed_proxy_server);
  }

  if (command_line.HasSwitch(switches::kHostResolverRules)) {
    builder.SetHostResolverRules(
        command_line.GetSwitchValueASCII(switches::kHostResolverRules));
  }

  return HeadlessBrowserMain(
      builder.Build(),
      base::Bind(&HeadlessShell::OnStart, base::Unretained(&shell)));
}
