// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_http_handler_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_browser_target.h"
#include "content/browser/devtools/devtools_protocol.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/browser/devtools/devtools_system_info_handler.h"
#include "content/browser/devtools/devtools_tracing_handler.h"
#include "content/browser/devtools/tethering_handler.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "content/public/common/user_agent.h"
#include "grit/devtools_resources_map.h"
#include "net/base/escape.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace content {

namespace {

const base::FilePath::CharType kDevToolsActivePortFileName[] =
    FILE_PATH_LITERAL("DevToolsActivePort");

const char kDevToolsHandlerThreadName[] = "Chrome_DevToolsHandlerThread";

const char kThumbUrlPrefix[] = "/thumb/";
const char kPageUrlPrefix[] = "/devtools/page/";

const char kTargetIdField[] = "id";
const char kTargetParentIdField[] = "parentId";
const char kTargetTypeField[] = "type";
const char kTargetTitleField[] = "title";
const char kTargetDescriptionField[] = "description";
const char kTargetUrlField[] = "url";
const char kTargetThumbnailUrlField[] = "thumbnailUrl";
const char kTargetFaviconUrlField[] = "faviconUrl";
const char kTargetWebSocketDebuggerUrlField[] = "webSocketDebuggerUrl";
const char kTargetDevtoolsFrontendUrlField[] = "devtoolsFrontendUrl";

// An internal implementation of DevToolsClientHost that delegates
// messages sent for DevToolsClient to a DebuggerShell instance.
class DevToolsClientHostImpl : public DevToolsClientHost {
 public:
  DevToolsClientHostImpl(base::MessageLoop* message_loop,
                         net::HttpServer* server,
                         int connection_id)
      : message_loop_(message_loop),
        server_(server),
        connection_id_(connection_id),
        is_closed_(false),
        detach_reason_("target_closed") {}

  virtual ~DevToolsClientHostImpl() {}

  // DevToolsClientHost interface
  virtual void InspectedContentsClosing() OVERRIDE {
    if (is_closed_)
      return;
    is_closed_ = true;

    base::DictionaryValue notification;
    notification.SetString(
        devtools::Inspector::detached::kParamReason, detach_reason_);
    std::string response = DevToolsProtocol::CreateNotification(
        devtools::Inspector::detached::kName,
        notification.DeepCopy())->Serialize();
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&net::HttpServer::SendOverWebSocket,
                   server_,
                   connection_id_,
                   response));

    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&net::HttpServer::Close, server_, connection_id_));
  }

  virtual void DispatchOnInspectorFrontend(const std::string& data) OVERRIDE {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&net::HttpServer::SendOverWebSocket,
                   server_,
                   connection_id_,
                   data));
  }

  virtual void ReplacedWithAnotherClient() OVERRIDE {
    detach_reason_ = "replaced_with_devtools";
  }

 private:
  base::MessageLoop* message_loop_;
  net::HttpServer* server_;
  int connection_id_;
  bool is_closed_;
  std::string detach_reason_;
};

static bool TimeComparator(const DevToolsTarget* target1,
                           const DevToolsTarget* target2) {
  return target1->GetLastActivityTime() > target2->GetLastActivityTime();
}

}  // namespace

// static
bool DevToolsHttpHandler::IsSupportedProtocolVersion(
    const std::string& version) {
  return devtools::IsSupportedProtocolVersion(version);
}

// static
int DevToolsHttpHandler::GetFrontendResourceId(const std::string& name) {
  for (size_t i = 0; i < kDevtoolsResourcesSize; ++i) {
    if (name == kDevtoolsResources[i].name)
      return kDevtoolsResources[i].value;
  }
  return -1;
}

// static
DevToolsHttpHandler* DevToolsHttpHandler::Start(
    const net::StreamListenSocketFactory* socket_factory,
    const std::string& frontend_url,
    DevToolsHttpHandlerDelegate* delegate,
    const base::FilePath& active_port_output_directory) {
  DevToolsHttpHandlerImpl* http_handler =
      new DevToolsHttpHandlerImpl(socket_factory,
                                  frontend_url,
                                  delegate,
                                  active_port_output_directory);
  http_handler->Start();
  return http_handler;
}

DevToolsHttpHandlerImpl::~DevToolsHttpHandlerImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Stop() must be called prior to destruction.
  DCHECK(server_.get() == NULL);
  DCHECK(thread_.get() == NULL);
  STLDeleteValues(&target_map_);
}

void DevToolsHttpHandlerImpl::Start() {
  if (thread_)
    return;
  thread_.reset(new base::Thread(kDevToolsHandlerThreadName));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::StartHandlerThread, this));
}

// Runs on FILE thread.
void DevToolsHttpHandlerImpl::StartHandlerThread() {
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  if (!thread_->StartWithOptions(options)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::ResetHandlerThread, this));
    return;
  }

  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::Init, this));
}

void DevToolsHttpHandlerImpl::ResetHandlerThread() {
  thread_.reset();
}

void DevToolsHttpHandlerImpl::ResetHandlerThreadAndRelease() {
  ResetHandlerThread();
  Release();
}

void DevToolsHttpHandlerImpl::Stop() {
  if (!thread_)
    return;
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::StopHandlerThread, this),
      base::Bind(&DevToolsHttpHandlerImpl::ResetHandlerThreadAndRelease, this));
}

GURL DevToolsHttpHandlerImpl::GetFrontendURL() {
  net::IPEndPoint ip_address;
  if (server_->GetLocalAddress(&ip_address))
    return GURL();
  return GURL(std::string("http://") + ip_address.ToString() + frontend_url_);
}

static std::string PathWithoutParams(const std::string& path) {
  size_t query_position = path.find("?");
  if (query_position != std::string::npos)
    return path.substr(0, query_position);
  return path;
}

static std::string GetMimeType(const std::string& filename) {
  if (EndsWith(filename, ".html", false)) {
    return "text/html";
  } else if (EndsWith(filename, ".css", false)) {
    return "text/css";
  } else if (EndsWith(filename, ".js", false)) {
    return "application/javascript";
  } else if (EndsWith(filename, ".png", false)) {
    return "image/png";
  } else if (EndsWith(filename, ".gif", false)) {
    return "image/gif";
  } else if (EndsWith(filename, ".json", false)) {
    return "application/json";
  }
  LOG(ERROR) << "GetMimeType doesn't know mime type for: "
             << filename
             << " text/plain will be returned";
  NOTREACHED();
  return "text/plain";
}

void DevToolsHttpHandlerImpl::OnHttpRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  if (info.path.find("/json") == 0) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnJsonRequestUI,
                   this,
                   connection_id,
                   info));
    return;
  }

  if (info.path.find(kThumbUrlPrefix) == 0) {
    // Thumbnail request.
    const std::string target_id = info.path.substr(strlen(kThumbUrlPrefix));
    DevToolsTarget* target = GetTarget(target_id);
    GURL page_url;
    if (target)
      page_url = target->GetURL();
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnThumbnailRequestUI,
                   this,
                   connection_id,
                   page_url));
    return;
  }

  if (info.path == "" || info.path == "/") {
    // Discovery page request.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DevToolsHttpHandlerImpl::OnDiscoveryPageRequestUI,
                   this,
                   connection_id));
    return;
  }

  if (info.path.find("/devtools/") != 0) {
    server_->Send404(connection_id);
    return;
  }

  std::string filename = PathWithoutParams(info.path.substr(10));
  std::string mime_type = GetMimeType(filename);

  base::FilePath frontend_dir = delegate_->GetDebugFrontendDir();
  if (!frontend_dir.empty()) {
    base::FilePath path = frontend_dir.AppendASCII(filename);
    std::string data;
    base::ReadFileToString(path, &data);
    server_->Send200(connection_id, data, mime_type);
    return;
  }
  if (delegate_->BundlesFrontendResources()) {
    int resource_id = DevToolsHttpHandler::GetFrontendResourceId(filename);
    if (resource_id != -1) {
      base::StringPiece data = GetContentClient()->GetDataResource(
              resource_id, ui::SCALE_FACTOR_NONE);
      server_->Send200(connection_id, data.as_string(), mime_type);
      return;
    }
  }
  server_->Send404(connection_id);
}

void DevToolsHttpHandlerImpl::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  std::string browser_prefix = "/devtools/browser";
  size_t browser_pos = request.path.find(browser_prefix);
  if (browser_pos == 0) {
    scoped_refptr<DevToolsBrowserTarget> browser_target =
        new DevToolsBrowserTarget(server_.get(), connection_id);
    browser_target->RegisterDomainHandler(
        devtools::Tracing::kName,
        new DevToolsTracingHandler(DevToolsTracingHandler::Browser),
        true /* handle on UI thread */);
    browser_target->RegisterDomainHandler(
        TetheringHandler::kDomain,
        new TetheringHandler(delegate_.get()),
        false /* handle on this thread */);
    browser_target->RegisterDomainHandler(
        devtools::SystemInfo::kName,
        new DevToolsSystemInfoHandler(),
        true /* handle on UI thread */);
    browser_targets_[connection_id] = browser_target;

    server_->AcceptWebSocket(connection_id, request);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &DevToolsHttpHandlerImpl::OnWebSocketRequestUI,
          this,
          connection_id,
          request));
}

void DevToolsHttpHandlerImpl::OnWebSocketMessage(
    int connection_id,
    const std::string& data) {
  BrowserTargets::iterator it = browser_targets_.find(connection_id);
  if (it != browser_targets_.end()) {
    it->second->HandleMessage(data);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &DevToolsHttpHandlerImpl::OnWebSocketMessageUI,
          this,
          connection_id,
          data));
}

void DevToolsHttpHandlerImpl::OnClose(int connection_id) {
  BrowserTargets::iterator it = browser_targets_.find(connection_id);
  if (it != browser_targets_.end()) {
    it->second->Detach();
    browser_targets_.erase(it);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &DevToolsHttpHandlerImpl::OnCloseUI,
          this,
          connection_id));
}

std::string DevToolsHttpHandlerImpl::GetFrontendURLInternal(
    const std::string id,
    const std::string& host) {
  return base::StringPrintf(
      "%s%sws=%s%s%s",
      frontend_url_.c_str(),
      frontend_url_.find("?") == std::string::npos ? "?" : "&",
      host.c_str(),
      kPageUrlPrefix,
      id.c_str());
}

static bool ParseJsonPath(
    const std::string& path,
    std::string* command,
    std::string* target_id) {

  // Fall back to list in case of empty query.
  if (path.empty()) {
    *command = "list";
    return true;
  }

  if (path.find("/") != 0) {
    // Malformed command.
    return false;
  }
  *command = path.substr(1);

  size_t separator_pos = command->find("/");
  if (separator_pos != std::string::npos) {
    *target_id = command->substr(separator_pos + 1);
    *command = command->substr(0, separator_pos);
  }
  return true;
}

void DevToolsHttpHandlerImpl::OnJsonRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  // Trim /json
  std::string path = info.path.substr(5);

  // Trim fragment and query
  std::string query;
  size_t query_pos = path.find("?");
  if (query_pos != std::string::npos) {
    query = path.substr(query_pos + 1);
    path = path.substr(0, query_pos);
  }

  size_t fragment_pos = path.find("#");
  if (fragment_pos != std::string::npos)
    path = path.substr(0, fragment_pos);

  std::string command;
  std::string target_id;
  if (!ParseJsonPath(path, &command, &target_id)) {
    SendJson(connection_id,
             net::HTTP_NOT_FOUND,
             NULL,
             "Malformed query: " + info.path);
    return;
  }

  if (command == "version") {
    base::DictionaryValue version;
    version.SetString("Protocol-Version", devtools::kProtocolVersion);
    version.SetString("WebKit-Version", GetWebKitVersion());
    version.SetString("Browser", GetContentClient()->GetProduct());
    version.SetString("User-Agent", GetContentClient()->GetUserAgent());
#if defined(OS_ANDROID)
    version.SetString("Android-Package",
        base::android::BuildInfo::GetInstance()->package_name());
#endif
    SendJson(connection_id, net::HTTP_OK, &version, std::string());
    return;
  }

  if (command == "list") {
    std::string host = info.headers["host"];
    AddRef();  // Balanced in OnTargetListReceived.
    delegate_->EnumerateTargets(
        base::Bind(&DevToolsHttpHandlerImpl::OnTargetListReceived,
                   this, connection_id, host));
    return;
  }

  if (command == "new") {
    GURL url(net::UnescapeURLComponent(
        query, net::UnescapeRule::URL_SPECIAL_CHARS));
    if (!url.is_valid())
      url = GURL(url::kAboutBlankURL);
    scoped_ptr<DevToolsTarget> target(delegate_->CreateNewTarget(url));
    if (!target) {
      SendJson(connection_id,
               net::HTTP_INTERNAL_SERVER_ERROR,
               NULL,
               "Could not create new page");
      return;
    }
    std::string host = info.headers["host"];
    scoped_ptr<base::DictionaryValue> dictionary(
        SerializeTarget(*target.get(), host));
    SendJson(connection_id, net::HTTP_OK, dictionary.get(), std::string());
    const std::string target_id = target->GetId();
    target_map_[target_id] = target.release();
    return;
  }

  if (command == "activate" || command == "close") {
    DevToolsTarget* target = GetTarget(target_id);
    if (!target) {
      SendJson(connection_id,
               net::HTTP_NOT_FOUND,
               NULL,
               "No such target id: " + target_id);
      return;
    }

    if (command == "activate") {
      if (target->Activate()) {
        SendJson(connection_id, net::HTTP_OK, NULL, "Target activated");
      } else {
        SendJson(connection_id,
                 net::HTTP_INTERNAL_SERVER_ERROR,
                 NULL,
                 "Could not activate target id: " + target_id);
      }
      return;
    }

    if (command == "close") {
      if (target->Close()) {
        SendJson(connection_id, net::HTTP_OK, NULL, "Target is closing");
      } else {
        SendJson(connection_id,
                 net::HTTP_INTERNAL_SERVER_ERROR,
                 NULL,
                 "Could not close target id: " + target_id);
      }
      return;
    }
  }
  SendJson(connection_id,
           net::HTTP_NOT_FOUND,
           NULL,
           "Unknown command: " + command);
  return;
}

void DevToolsHttpHandlerImpl::OnTargetListReceived(
    int connection_id,
    const std::string& host,
    const DevToolsHttpHandlerDelegate::TargetList& targets) {
  DevToolsHttpHandlerDelegate::TargetList sorted_targets = targets;
  std::sort(sorted_targets.begin(), sorted_targets.end(), TimeComparator);

  STLDeleteValues(&target_map_);
  base::ListValue list_value;
  for (DevToolsHttpHandlerDelegate::TargetList::const_iterator it =
      sorted_targets.begin(); it != sorted_targets.end(); ++it) {
    DevToolsTarget* target = *it;
    target_map_[target->GetId()] = target;
    list_value.Append(SerializeTarget(*target, host));
  }
  SendJson(connection_id, net::HTTP_OK, &list_value, std::string());
  Release();  // Balanced in OnJsonRequestUI.
}

DevToolsTarget* DevToolsHttpHandlerImpl::GetTarget(const std::string& id) {
  TargetMap::const_iterator it = target_map_.find(id);
  if (it == target_map_.end())
    return NULL;
  return it->second;
}

void DevToolsHttpHandlerImpl::OnThumbnailRequestUI(
    int connection_id, const GURL& page_url) {
  std::string data = delegate_->GetPageThumbnailData(page_url);
  if (!data.empty())
    Send200(connection_id, data, "image/png");
  else
    Send404(connection_id);
}

void DevToolsHttpHandlerImpl::OnDiscoveryPageRequestUI(int connection_id) {
  std::string response = delegate_->GetDiscoveryPageHTML();
  Send200(connection_id, response, "text/html; charset=UTF-8");
}

void DevToolsHttpHandlerImpl::OnWebSocketRequestUI(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  if (!thread_)
    return;

  size_t pos = request.path.find(kPageUrlPrefix);
  if (pos != 0) {
    Send404(connection_id);
    return;
  }

  std::string page_id = request.path.substr(strlen(kPageUrlPrefix));
  DevToolsTarget* target = GetTarget(page_id);
  scoped_refptr<DevToolsAgentHost> agent =
      target ? target->GetAgentHost() : NULL;
  if (!agent) {
    Send500(connection_id, "No such target id: " + page_id);
    return;
  }

  if (agent->IsAttached()) {
    Send500(connection_id,
            "Target with given id is being inspected: " + page_id);
    return;
  }

  DevToolsClientHostImpl* client_host = new DevToolsClientHostImpl(
      thread_->message_loop(), server_.get(), connection_id);
  connection_to_client_host_ui_[connection_id] = client_host;

  DevToolsManager::GetInstance()->
      RegisterDevToolsClientHostFor(agent, client_host);

  AcceptWebSocket(connection_id, request);
}

void DevToolsHttpHandlerImpl::OnWebSocketMessageUI(
    int connection_id,
    const std::string& data) {
  ConnectionToClientHostMap::iterator it =
      connection_to_client_host_ui_.find(connection_id);
  if (it == connection_to_client_host_ui_.end())
    return;

  DevToolsManager* manager = DevToolsManager::GetInstance();
  manager->DispatchOnInspectorBackend(it->second, data);
}

void DevToolsHttpHandlerImpl::OnCloseUI(int connection_id) {
  ConnectionToClientHostMap::iterator it =
      connection_to_client_host_ui_.find(connection_id);
  if (it != connection_to_client_host_ui_.end()) {
    DevToolsClientHostImpl* client_host =
        static_cast<DevToolsClientHostImpl*>(it->second);
    DevToolsManager::GetInstance()->ClientHostClosing(client_host);
    delete client_host;
    connection_to_client_host_ui_.erase(connection_id);
  }
}

DevToolsHttpHandlerImpl::DevToolsHttpHandlerImpl(
    const net::StreamListenSocketFactory* socket_factory,
    const std::string& frontend_url,
    DevToolsHttpHandlerDelegate* delegate,
    const base::FilePath& active_port_output_directory)
    : frontend_url_(frontend_url),
      socket_factory_(socket_factory),
      delegate_(delegate),
      active_port_output_directory_(active_port_output_directory) {
  if (frontend_url_.empty())
      frontend_url_ = "/devtools/devtools.html";

  // Balanced in ResetHandlerThreadAndRelease().
  AddRef();
}

// Runs on the handler thread
void DevToolsHttpHandlerImpl::Init() {
  server_ = new net::HttpServer(*socket_factory_.get(), this);
  if (!active_port_output_directory_.empty())
    WriteActivePortToUserProfile();
}

// Runs on the handler thread
void DevToolsHttpHandlerImpl::Teardown() {
  server_ = NULL;
}

// Runs on FILE thread to make sure that it is serialized against
// {Start|Stop}HandlerThread and to allow calling pthread_join.
void DevToolsHttpHandlerImpl::StopHandlerThread() {
  if (!thread_->message_loop())
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&DevToolsHttpHandlerImpl::Teardown, this));
  // Thread::Stop joins the thread.
  thread_->Stop();
}

void DevToolsHttpHandlerImpl::WriteActivePortToUserProfile() {
  DCHECK(!active_port_output_directory_.empty());
  net::IPEndPoint endpoint;
  int err;
  if ((err = server_->GetLocalAddress(&endpoint)) != net::OK) {
    LOG(ERROR) << "Error " << err << " getting local address";
    return;
  }

  // Write this port to a well-known file in the profile directory
  // so Telemetry can pick it up.
  base::FilePath path = active_port_output_directory_.Append(
      kDevToolsActivePortFileName);
  std::string port_string = base::IntToString(endpoint.port());
  if (base::WriteFile(path, port_string.c_str(), port_string.length()) < 0) {
    LOG(ERROR) << "Error writing DevTools active port to file";
  }
}

void DevToolsHttpHandlerImpl::SendJson(int connection_id,
                                       net::HttpStatusCode status_code,
                                       base::Value* value,
                                       const std::string& message) {
  if (!thread_)
    return;

  // Serialize value and message.
  std::string json_value;
  if (value) {
    base::JSONWriter::WriteWithOptions(value,
                                       base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                       &json_value);
  }
  std::string json_message;
  scoped_ptr<base::Value> message_object(new base::StringValue(message));
  base::JSONWriter::Write(message_object.get(), &json_message);

  net::HttpServerResponseInfo response(status_code);
  response.SetBody(json_value + message, "application/json; charset=UTF-8");

  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::SendResponse,
                 server_.get(),
                 connection_id,
                 response));
}

void DevToolsHttpHandlerImpl::Send200(int connection_id,
                                      const std::string& data,
                                      const std::string& mime_type) {
  if (!thread_)
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::Send200,
                 server_.get(),
                 connection_id,
                 data,
                 mime_type));
}

void DevToolsHttpHandlerImpl::Send404(int connection_id) {
  if (!thread_)
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::Send404, server_.get(), connection_id));
}

void DevToolsHttpHandlerImpl::Send500(int connection_id,
                                      const std::string& message) {
  if (!thread_)
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::Send500, server_.get(), connection_id,
                 message));
}

void DevToolsHttpHandlerImpl::AcceptWebSocket(
    int connection_id,
    const net::HttpServerRequestInfo& request) {
  if (!thread_)
    return;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::AcceptWebSocket, server_.get(),
                 connection_id, request));
}

base::DictionaryValue* DevToolsHttpHandlerImpl::SerializeTarget(
    const DevToolsTarget& target,
    const std::string& host) {
  base::DictionaryValue* dictionary = new base::DictionaryValue;

  std::string id = target.GetId();
  dictionary->SetString(kTargetIdField, id);
  std::string parent_id = target.GetParentId();
  if (!parent_id.empty())
    dictionary->SetString(kTargetParentIdField, parent_id);
  dictionary->SetString(kTargetTypeField, target.GetType());
  dictionary->SetString(kTargetTitleField,
                        net::EscapeForHTML(target.GetTitle()));
  dictionary->SetString(kTargetDescriptionField, target.GetDescription());

  GURL url = target.GetURL();
  dictionary->SetString(kTargetUrlField, url.spec());

  GURL favicon_url = target.GetFaviconURL();
  if (favicon_url.is_valid())
    dictionary->SetString(kTargetFaviconUrlField, favicon_url.spec());

  if (!delegate_->GetPageThumbnailData(url).empty()) {
    dictionary->SetString(kTargetThumbnailUrlField,
                          std::string(kThumbUrlPrefix) + id);
  }

  if (!target.IsAttached()) {
    dictionary->SetString(kTargetWebSocketDebuggerUrlField,
                          base::StringPrintf("ws://%s%s%s",
                                             host.c_str(),
                                             kPageUrlPrefix,
                                             id.c_str()));
    std::string devtools_frontend_url = GetFrontendURLInternal(
        id.c_str(),
        host);
    dictionary->SetString(
        kTargetDevtoolsFrontendUrlField, devtools_frontend_url);
  }

  return dictionary;
}

}  // namespace content
