// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/http_server.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/server/http_connection.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "net/server/web_socket.h"
#include "net/socket/tcp_listen_socket.h"

namespace net {

HttpServer::HttpServer(const StreamListenSocketFactory& factory,
                       HttpServer::Delegate* delegate)
    : delegate_(delegate),
      server_(factory.CreateAndListen(this)) {
}

void HttpServer::AcceptWebSocket(
    int connection_id,
    const HttpServerRequestInfo& request) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  DCHECK(connection->web_socket_.get());
  connection->web_socket_->Accept(request);
}

void HttpServer::SendOverWebSocket(int connection_id,
                                   const std::string& data) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;
  DCHECK(connection->web_socket_.get());
  connection->web_socket_->Send(data);
}

void HttpServer::SendRaw(int connection_id, const std::string& data) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;
  connection->Send(data);
}

void HttpServer::SendResponse(int connection_id,
                              const HttpServerResponseInfo& response) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;
  connection->Send(response);
}

void HttpServer::Send(int connection_id,
                      HttpStatusCode status_code,
                      const std::string& data,
                      const std::string& content_type) {
  HttpServerResponseInfo response(status_code);
  response.SetBody(data, content_type);
  SendResponse(connection_id, response);
}

void HttpServer::Send200(int connection_id,
                         const std::string& data,
                         const std::string& content_type) {
  Send(connection_id, HTTP_OK, data, content_type);
}

void HttpServer::Send404(int connection_id) {
  SendResponse(connection_id, HttpServerResponseInfo::CreateFor404());
}

void HttpServer::Send500(int connection_id, const std::string& message) {
  SendResponse(connection_id, HttpServerResponseInfo::CreateFor500(message));
}

void HttpServer::Close(int connection_id) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  // Initiating close from server-side does not lead to the DidClose call.
  // Do it manually here.
  DidClose(connection->socket_.get());
}

int HttpServer::GetLocalAddress(IPEndPoint* address) {
  if (!server_)
    return ERR_SOCKET_NOT_CONNECTED;
  return server_->GetLocalAddress(address);
}

void HttpServer::DidAccept(StreamListenSocket* server,
                           scoped_ptr<StreamListenSocket> socket) {
  HttpConnection* connection = new HttpConnection(this, socket.Pass());
  id_to_connection_[connection->id()] = connection;
  // TODO(szym): Fix socket access. Make HttpConnection the Delegate.
  socket_to_connection_[connection->socket_.get()] = connection;
}

void HttpServer::DidRead(StreamListenSocket* socket,
                         const char* data,
                         int len) {
  HttpConnection* connection = FindConnection(socket);
  DCHECK(connection != NULL);
  if (connection == NULL)
    return;

  connection->recv_data_.append(data, len);
  while (connection->recv_data_.length()) {
    if (connection->web_socket_.get()) {
      std::string message;
      WebSocket::ParseResult result = connection->web_socket_->Read(&message);
      if (result == WebSocket::FRAME_INCOMPLETE)
        break;

      if (result == WebSocket::FRAME_CLOSE ||
          result == WebSocket::FRAME_ERROR) {
        Close(connection->id());
        break;
      }
      delegate_->OnWebSocketMessage(connection->id(), message);
      continue;
    }

    HttpServerRequestInfo request;
    size_t pos = 0;
    if (!ParseHeaders(connection, &request, &pos))
      break;

    // Sets peer address if exists.
    socket->GetPeerAddress(&request.peer);

    if (request.HasHeaderValue("connection", "upgrade")) {
      connection->web_socket_.reset(WebSocket::CreateWebSocket(connection,
                                                               request,
                                                               &pos));

      if (!connection->web_socket_.get())  // Not enough data was received.
        break;
      delegate_->OnWebSocketRequest(connection->id(), request);
      connection->Shift(pos);
      continue;
    }

    const char kContentLength[] = "content-length";
    if (request.headers.count(kContentLength)) {
      size_t content_length = 0;
      const size_t kMaxBodySize = 100 << 20;
      if (!base::StringToSizeT(request.GetHeaderValue(kContentLength),
                               &content_length) ||
          content_length > kMaxBodySize) {
        connection->Send(HttpServerResponseInfo::CreateFor500(
            "request content-length too big or unknown: " +
            request.GetHeaderValue(kContentLength)));
        DidClose(socket);
        break;
      }

      if (connection->recv_data_.length() - pos < content_length)
        break;  // Not enough data was received yet.
      request.data = connection->recv_data_.substr(pos, content_length);
      pos += content_length;
    }

    delegate_->OnHttpRequest(connection->id(), request);
    connection->Shift(pos);
  }
}

void HttpServer::DidClose(StreamListenSocket* socket) {
  HttpConnection* connection = FindConnection(socket);
  DCHECK(connection != NULL);
  id_to_connection_.erase(connection->id());
  socket_to_connection_.erase(connection->socket_.get());
  delete connection;
}

HttpServer::~HttpServer() {
  STLDeleteContainerPairSecondPointers(
      id_to_connection_.begin(), id_to_connection_.end());
}

//
// HTTP Request Parser
// This HTTP request parser uses a simple state machine to quickly parse
// through the headers.  The parser is not 100% complete, as it is designed
// for use in this simple test driver.
//
// Known issues:
//   - does not handle whitespace on first HTTP line correctly.  Expects
//     a single space between the method/url and url/protocol.

// Input character types.
enum header_parse_inputs {
  INPUT_LWS,
  INPUT_CR,
  INPUT_LF,
  INPUT_COLON,
  INPUT_DEFAULT,
  MAX_INPUTS,
};

// Parser states.
enum header_parse_states {
  ST_METHOD,     // Receiving the method
  ST_URL,        // Receiving the URL
  ST_PROTO,      // Receiving the protocol
  ST_HEADER,     // Starting a Request Header
  ST_NAME,       // Receiving a request header name
  ST_SEPARATOR,  // Receiving the separator between header name and value
  ST_VALUE,      // Receiving a request header value
  ST_DONE,       // Parsing is complete and successful
  ST_ERR,        // Parsing encountered invalid syntax.
  MAX_STATES
};

// State transition table
int parser_state[MAX_STATES][MAX_INPUTS] = {
/* METHOD    */ { ST_URL,       ST_ERR,     ST_ERR,   ST_ERR,       ST_METHOD },
/* URL       */ { ST_PROTO,     ST_ERR,     ST_ERR,   ST_URL,       ST_URL },
/* PROTOCOL  */ { ST_ERR,       ST_HEADER,  ST_NAME,  ST_ERR,       ST_PROTO },
/* HEADER    */ { ST_ERR,       ST_ERR,     ST_NAME,  ST_ERR,       ST_ERR },
/* NAME      */ { ST_SEPARATOR, ST_DONE,    ST_ERR,   ST_VALUE,     ST_NAME },
/* SEPARATOR */ { ST_SEPARATOR, ST_ERR,     ST_ERR,   ST_VALUE,     ST_ERR },
/* VALUE     */ { ST_VALUE,     ST_HEADER,  ST_NAME,  ST_VALUE,     ST_VALUE },
/* DONE      */ { ST_DONE,      ST_DONE,    ST_DONE,  ST_DONE,      ST_DONE },
/* ERR       */ { ST_ERR,       ST_ERR,     ST_ERR,   ST_ERR,       ST_ERR }
};

// Convert an input character to the parser's input token.
int charToInput(char ch) {
  switch(ch) {
    case ' ':
    case '\t':
      return INPUT_LWS;
    case '\r':
      return INPUT_CR;
    case '\n':
      return INPUT_LF;
    case ':':
      return INPUT_COLON;
  }
  return INPUT_DEFAULT;
}

bool HttpServer::ParseHeaders(HttpConnection* connection,
                              HttpServerRequestInfo* info,
                              size_t* ppos) {
  size_t& pos = *ppos;
  size_t data_len = connection->recv_data_.length();
  int state = ST_METHOD;
  std::string buffer;
  std::string header_name;
  std::string header_value;
  while (pos < data_len) {
    char ch = connection->recv_data_[pos++];
    int input = charToInput(ch);
    int next_state = parser_state[state][input];

    bool transition = (next_state != state);
    HttpServerRequestInfo::HeadersMap::iterator it;
    if (transition) {
      // Do any actions based on state transitions.
      switch (state) {
        case ST_METHOD:
          info->method = buffer;
          buffer.clear();
          break;
        case ST_URL:
          info->path = buffer;
          buffer.clear();
          break;
        case ST_PROTO:
          // TODO(mbelshe): Deal better with parsing protocol.
          DCHECK(buffer == "HTTP/1.1");
          buffer.clear();
          break;
        case ST_NAME:
          header_name = StringToLowerASCII(buffer);
          buffer.clear();
          break;
        case ST_VALUE:
          base::TrimWhitespaceASCII(buffer, base::TRIM_LEADING, &header_value);
          it = info->headers.find(header_name);
          // See last paragraph ("Multiple message-header fields...")
          // of www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
          if (it == info->headers.end()) {
            info->headers[header_name] = header_value;
          } else {
            it->second.append(",");
            it->second.append(header_value);
          }
          buffer.clear();
          break;
        case ST_SEPARATOR:
          break;
      }
      state = next_state;
    } else {
      // Do any actions based on current state
      switch (state) {
        case ST_METHOD:
        case ST_URL:
        case ST_PROTO:
        case ST_VALUE:
        case ST_NAME:
          buffer.append(&ch, 1);
          break;
        case ST_DONE:
          DCHECK(input == INPUT_LF);
          return true;
        case ST_ERR:
          return false;
      }
    }
  }
  // No more characters, but we haven't finished parsing yet.
  return false;
}

HttpConnection* HttpServer::FindConnection(int connection_id) {
  IdToConnectionMap::iterator it = id_to_connection_.find(connection_id);
  if (it == id_to_connection_.end())
    return NULL;
  return it->second;
}

HttpConnection* HttpServer::FindConnection(StreamListenSocket* socket) {
  SocketToConnectionMap::iterator it = socket_to_connection_.find(socket);
  if (it == socket_to_connection_.end())
    return NULL;
  return it->second;
}

}  // namespace net
