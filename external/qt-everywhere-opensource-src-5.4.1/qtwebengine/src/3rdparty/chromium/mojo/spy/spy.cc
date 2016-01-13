// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/spy/spy.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"

#include "mojo/public/cpp/system/core.h"
#include "mojo/service_manager/service_manager.h"
#include "mojo/spy/websocket_server.h"

namespace {

const size_t kMessageBufSize = 2 * 1024;
const size_t kHandleBufSize = 64;
const int kDefaultWebSocketPort = 42424;

void CloseHandles(MojoHandle* handles, size_t count) {
  for (size_t ix = 0; ix != count; ++count)
    MojoClose(handles[ix]);
}

// In charge of processing messages that flow over a
// single message pipe.
class MessageProcessor :
    public base::RefCountedThreadSafe<MessageProcessor> {
 public:

  MessageProcessor()
      : last_result_(MOJO_RESULT_OK),
        bytes_transfered_(0) {

    message_count_[0] = 0;
    message_count_[1] = 0;
    handle_count_[0] = 0;
    handle_count_[1] = 0;
  }

  void Start(mojo::ScopedMessagePipeHandle client,
             mojo::ScopedMessagePipeHandle interceptor) {
    std::vector<mojo::MessagePipeHandle> pipes;
    pipes.push_back(client.get());
    pipes.push_back(interceptor.get());
    std::vector<MojoHandleSignals> handle_signals;
    handle_signals.push_back(MOJO_HANDLE_SIGNAL_READABLE);
    handle_signals.push_back(MOJO_HANDLE_SIGNAL_READABLE);

    scoped_ptr<char[]> mbuf(new char[kMessageBufSize]);
    scoped_ptr<MojoHandle[]> hbuf(new MojoHandle[kHandleBufSize]);

    // Main processing loop:
    // 1- Wait for an endpoint to have a message.
    // 2- Read the message
    // 3- Log data
    // 4- Wait until the opposite port is ready for writting
    // 4- Write the message to opposite port.

    for (;;) {
      int r = WaitMany(pipes, handle_signals, MOJO_DEADLINE_INDEFINITE);
      if ((r < 0) || (r > 1)) {
        last_result_ = r;
        break;
      }

      uint32_t bytes_read = kMessageBufSize;
      uint32_t handles_read = kHandleBufSize;

      if (!CheckResult(ReadMessageRaw(pipes[r],
                                      mbuf.get(), &bytes_read,
                                      hbuf.get(), &handles_read,
                                      MOJO_READ_MESSAGE_FLAG_NONE)))
        break;

      if (!bytes_read && !handles_read)
        continue;

      if (handles_read)
        handle_count_[r] += handles_read;

      ++message_count_[r];
      bytes_transfered_ += bytes_read;

      mojo::MessagePipeHandle write_handle = (r == 0) ? pipes[1] : pipes[0];
      if (!CheckResult(Wait(write_handle,
                            MOJO_HANDLE_SIGNAL_WRITABLE,
                            MOJO_DEADLINE_INDEFINITE)))
        break;

      if (!CheckResult(WriteMessageRaw(write_handle,
                                       mbuf.get(), bytes_read,
                                       hbuf.get(), handles_read,
                                       MOJO_WRITE_MESSAGE_FLAG_NONE))) {
        // On failure we own the handles. For now just close them.
        if (handles_read)
          CloseHandles(hbuf.get(), handles_read);
        break;
      }
    }
  }

 private:
   friend class base::RefCountedThreadSafe<MessageProcessor>;
   virtual ~MessageProcessor() {}

   bool CheckResult(MojoResult mr) {
     if (mr == MOJO_RESULT_OK)
       return true;
     last_result_ = mr;
     return false;
   }

   MojoResult last_result_;
   uint32_t bytes_transfered_;
   uint32_t message_count_[2];
   uint32_t handle_count_[2];
};

// In charge of intercepting access to the service manager.
class SpyInterceptor : public mojo::ServiceManager::Interceptor {
 private:
  virtual mojo::ScopedMessagePipeHandle OnConnectToClient(
    const GURL& url, mojo::ScopedMessagePipeHandle real_client) OVERRIDE {
      if (!MustIntercept(url))
        return real_client.Pass();

      // You can get an invalid handle if the app (or service) is
      // created by unconventional means, for example the command line.
      if (!real_client.is_valid())
        return real_client.Pass();

      mojo::ScopedMessagePipeHandle faux_client;
      mojo::ScopedMessagePipeHandle interceptor;
      CreateMessagePipe(NULL, &faux_client, &interceptor);

      scoped_refptr<MessageProcessor> processor = new MessageProcessor();
      base::WorkerPool::PostTask(
          FROM_HERE,
          base::Bind(&MessageProcessor::Start,
                     processor,
                     base::Passed(&real_client), base::Passed(&interceptor)),
          true);

      return faux_client.Pass();
  }

  bool MustIntercept(const GURL& url) {
    // TODO(cpu): manage who and when to intercept.
    return true;
  }
};

spy::WebSocketServer* ws_server = NULL;

void StartServer(int port) {
  // TODO(cpu) figure out lifetime of the server. See Spy() dtor.
  ws_server = new spy::WebSocketServer(port);
  ws_server->Start();
}

struct SpyOptions {
  int websocket_port;

  SpyOptions()
      : websocket_port(kDefaultWebSocketPort) {
  }
};

SpyOptions ProcessOptions(const std::string& options) {
  SpyOptions spy_options;
  if (options.empty())
    return spy_options;
  base::StringPairs kv_pairs;
  base::SplitStringIntoKeyValuePairs(options, ':', ',', &kv_pairs);
  base::StringPairs::iterator it = kv_pairs.begin();
  for (; it != kv_pairs.end(); ++it) {
    if (it->first == "port") {
      int port;
      if (base::StringToInt(it->second, &port))
        spy_options.websocket_port = port;
    }
  }
  return spy_options;
}

}  // namespace

namespace mojo {

Spy::Spy(mojo::ServiceManager* service_manager, const std::string& options) {
  SpyOptions spy_options = ProcessOptions(options);
  // Start the tread what will accept commands from the frontend.
  control_thread_.reset(new base::Thread("mojo_spy_control_thread"));
  base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
  control_thread_->StartWithOptions(thread_options);
  control_thread_->message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(&StartServer, spy_options.websocket_port));

  // Start intercepting mojo services.
  service_manager->SetInterceptor(new SpyInterceptor());
}

Spy::~Spy(){
  // TODO(cpu): Do not leak the interceptor. Lifetime between the
  // service_manager and the spy is still unclear hence the leak.
}

}  // namespace mojo
