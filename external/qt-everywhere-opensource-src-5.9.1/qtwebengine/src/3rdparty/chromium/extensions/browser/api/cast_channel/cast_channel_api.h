// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_API_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_API_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/api/cast_channel/cast_socket.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/api/cast_channel.h"

class CastChannelAPITest;

namespace content {
class BrowserContext;
}

namespace net {
class IPEndPoint;
}

namespace extensions {

struct Event;

namespace api {
namespace cast_channel {
class Logger;
}  // namespace cast_channel
}  // namespace api

namespace cast_channel = api::cast_channel;

class CastChannelAPI : public BrowserContextKeyedAPI,
                       public base::SupportsWeakPtr<CastChannelAPI> {
 public:
  explicit CastChannelAPI(content::BrowserContext* context);

  static CastChannelAPI* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<CastChannelAPI>* GetFactoryInstance();

  // Returns a pointer to the Logger member variable.
  // TODO(imcheng): Consider whether it is possible for this class to own the
  // CastSockets and make this class the sole owner of Logger.
  // Alternatively,
  // consider making Logger not ref-counted by passing a weak
  // reference of Logger to the CastSockets instead.
  scoped_refptr<cast_channel::Logger> GetLogger();

  // Sets the CastSocket instance to be used for testing.
  void SetSocketForTest(
      std::unique_ptr<cast_channel::CastSocket> socket_for_test);

  // Returns a test CastSocket instance, if it is defined.
  // Otherwise returns a scoped_ptr with a nullptr value.
  std::unique_ptr<cast_channel::CastSocket> GetSocketForTest();

  // Returns the API browser context.
  content::BrowserContext* GetBrowserContext() const;

  // Sets injected ping timeout timer for testing.
  void SetPingTimeoutTimerForTest(std::unique_ptr<base::Timer> timer);

  // Gets the injected ping timeout timer, if set.
  // Returns a null scoped ptr if there is no injected timer.
  std::unique_ptr<base::Timer> GetInjectedTimeoutTimerForTest();

  // Sends an event to the extension's EventRouter, if it exists.
  void SendEvent(const std::string& extension_id, std::unique_ptr<Event> event);

 private:
  friend class BrowserContextKeyedAPIFactory<CastChannelAPI>;
  friend class ::CastChannelAPITest;
  friend class CastTransportDelegate;

  ~CastChannelAPI() override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "CastChannelAPI"; }

  content::BrowserContext* const browser_context_;
  scoped_refptr<cast_channel::Logger> logger_;
  std::unique_ptr<cast_channel::CastSocket> socket_for_test_;
  std::unique_ptr<base::Timer> injected_timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelAPI);
};

class CastChannelAsyncApiFunction : public AsyncApiFunction {
 public:
  CastChannelAsyncApiFunction();

 protected:
  typedef ApiResourceManager<cast_channel::CastSocket>::ApiResourceData
      SocketData;

  ~CastChannelAsyncApiFunction() override;

  // AsyncApiFunction:
  bool PrePrepare() override;
  bool Respond() override;

  // Returns the socket corresponding to |channel_id| if one exists.  Otherwise,
  // sets the function result with CHANNEL_ERROR_INVALID_CHANNEL_ID, completes
  // the function, and returns null.
  cast_channel::CastSocket* GetSocketOrCompleteWithError(int channel_id);

  // Adds |socket| to |manager_| and returns the new channel_id.  |manager_|
  // assumes ownership of |socket|.
  int AddSocket(cast_channel::CastSocket* socket);

  // Removes the CastSocket corresponding to |channel_id| from the resource
  // manager.
  void RemoveSocket(int channel_id);

  // Sets the function result to a ChannelInfo obtained from the state of
  // |socket|.
  void SetResultFromSocket(const cast_channel::CastSocket& socket);

  // Sets the function result to a ChannelInfo populated with |channel_id| and
  // |error|.
  void SetResultFromError(int channel_id, cast_channel::ChannelError error);

  // Returns the socket corresponding to |channel_id| if one exists, or null
  // otherwise.
  cast_channel::CastSocket* GetSocket(int channel_id) const;

 private:
  // Sets the function result from |channel_info|.
  void SetResultFromChannelInfo(const cast_channel::ChannelInfo& channel_info);

  // The collection of CastSocket API resources.
  scoped_refptr<SocketData> sockets_;
};

class CastChannelOpenFunction : public CastChannelAsyncApiFunction {
 public:
  CastChannelOpenFunction();

 protected:
  ~CastChannelOpenFunction() override;

  // AsyncApiFunction:
  bool PrePrepare() override;
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  DECLARE_EXTENSION_FUNCTION("cast.channel.open", CAST_CHANNEL_OPEN)

  // Defines a callback used to send events to the extension's
  // EventRouter.
  //     Parameter #0 is the extension's ID.
  //     Parameter #1 is a scoped pointer to the event payload.
  using EventDispatchCallback =
      base::Callback<void(const std::string&, std::unique_ptr<Event>)>;

  // Receives incoming messages and errors and provides additional API and
  // origin socket context.
  class CastMessageHandler : public cast_channel::CastTransport::Delegate {
   public:
    CastMessageHandler(const EventDispatchCallback& ui_dispatch_cb,
                       cast_channel::CastSocket* socket,
                       scoped_refptr<api::cast_channel::Logger> logger);
    ~CastMessageHandler() override;

    // CastTransport::Delegate implementation.
    void OnError(cast_channel::ChannelError error_state) override;
    void OnMessage(const cast_channel::CastMessage& message) override;
    void Start() override;

   private:
    // Callback for sending events to the extension.
    // Should be bound to a weak pointer, to prevent any use-after-free
    // conditions.
    EventDispatchCallback const ui_dispatch_cb_;
    cast_channel::CastSocket* const socket_;
    // Logger object for reporting error details.
    scoped_refptr<api::cast_channel::Logger> logger_;

    DISALLOW_COPY_AND_ASSIGN(CastMessageHandler);
  };

  // Validates that |connect_info| represents a valid IP end point and returns a
  // new IPEndPoint if so.  Otherwise returns nullptr.
  static net::IPEndPoint* ParseConnectInfo(
      const cast_channel::ConnectInfo& connect_info);

  void OnOpen(cast_channel::ChannelError result);

  std::unique_ptr<cast_channel::Open::Params> params_;
  // The id of the newly opened socket.
  int new_channel_id_;
  CastChannelAPI* api_;
  std::unique_ptr<net::IPEndPoint> ip_endpoint_;
  cast_channel::ChannelAuthType channel_auth_;
  base::TimeDelta liveness_timeout_;
  base::TimeDelta ping_interval_;

  FRIEND_TEST_ALL_PREFIXES(CastChannelOpenFunctionTest, TestParseConnectInfo);
  DISALLOW_COPY_AND_ASSIGN(CastChannelOpenFunction);
};

class CastChannelSendFunction : public CastChannelAsyncApiFunction {
 public:
  CastChannelSendFunction();

 protected:
  ~CastChannelSendFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  DECLARE_EXTENSION_FUNCTION("cast.channel.send", CAST_CHANNEL_SEND)

  void OnSend(int result);

  std::unique_ptr<cast_channel::Send::Params> params_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelSendFunction);
};

class CastChannelCloseFunction : public CastChannelAsyncApiFunction {
 public:
  CastChannelCloseFunction();

 protected:
  ~CastChannelCloseFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  DECLARE_EXTENSION_FUNCTION("cast.channel.close", CAST_CHANNEL_CLOSE)

  void OnClose(int result);

  std::unique_ptr<cast_channel::Close::Params> params_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelCloseFunction);
};

class CastChannelGetLogsFunction : public CastChannelAsyncApiFunction {
 public:
  CastChannelGetLogsFunction();

 protected:
  ~CastChannelGetLogsFunction() override;

  // AsyncApiFunction:
  bool PrePrepare() override;
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  DECLARE_EXTENSION_FUNCTION("cast.channel.getLogs", CAST_CHANNEL_GETLOGS)

  CastChannelAPI* api_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelGetLogsFunction);
};

// TODO(eroman): crbug.com/601171: Delete this entire extension API. It
// is currently deprecated and calling the function has no effect.
class CastChannelSetAuthorityKeysFunction : public CastChannelAsyncApiFunction {
 public:
  CastChannelSetAuthorityKeysFunction();

 protected:
  ~CastChannelSetAuthorityKeysFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  DECLARE_EXTENSION_FUNCTION("cast.channel.setAuthorityKeys",
                             CAST_CHANNEL_SETAUTHORITYKEYS)

  DISALLOW_COPY_AND_ASSIGN(CastChannelSetAuthorityKeysFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_API_H_
