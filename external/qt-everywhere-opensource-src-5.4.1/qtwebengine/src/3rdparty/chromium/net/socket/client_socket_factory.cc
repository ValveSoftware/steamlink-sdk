// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_factory.h"

#include "base/lazy_instance.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "build/build_config.h"
#include "net/cert/cert_database.h"
#include "net/socket/client_socket_handle.h"
#if defined(USE_OPENSSL)
#include "net/socket/ssl_client_socket_openssl.h"
#elif defined(USE_NSS) || defined(OS_MACOSX) || defined(OS_WIN)
#include "net/socket/ssl_client_socket_nss.h"
#endif
#include "net/socket/tcp_client_socket.h"
#include "net/udp/udp_client_socket.h"

namespace net {

class X509Certificate;

namespace {

// ChromeOS and Linux may require interaction with smart cards or TPMs, which
// may cause NSS functions to block for upwards of several seconds. To avoid
// blocking all activity on the current task runner, such as network or IPC
// traffic, run NSS SSL functions on a dedicated thread.
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
bool g_use_dedicated_nss_thread = true;
#else
bool g_use_dedicated_nss_thread = false;
#endif

class DefaultClientSocketFactory : public ClientSocketFactory,
                                   public CertDatabase::Observer {
 public:
  DefaultClientSocketFactory() {
    if (g_use_dedicated_nss_thread) {
      // Use a single thread for the worker pool.
      worker_pool_ = new base::SequencedWorkerPool(1, "NSS SSL Thread");
      nss_thread_task_runner_ =
          worker_pool_->GetSequencedTaskRunnerWithShutdownBehavior(
              worker_pool_->GetSequenceToken(),
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
    }

    CertDatabase::GetInstance()->AddObserver(this);
  }

  virtual ~DefaultClientSocketFactory() {
    // Note: This code never runs, as the factory is defined as a Leaky
    // singleton.
    CertDatabase::GetInstance()->RemoveObserver(this);
  }

  virtual void OnCertAdded(const X509Certificate* cert) OVERRIDE {
    ClearSSLSessionCache();
  }

  virtual void OnCACertChanged(const X509Certificate* cert) OVERRIDE {
    // Per wtc, we actually only need to flush when trust is reduced.
    // Always flush now because OnCACertChanged does not tell us this.
    // See comments in ClientSocketPoolManager::OnCACertChanged.
    ClearSSLSessionCache();
  }

  virtual scoped_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      const RandIntCallback& rand_int_cb,
      NetLog* net_log,
      const NetLog::Source& source) OVERRIDE {
    return scoped_ptr<DatagramClientSocket>(
        new UDPClientSocket(bind_type, rand_int_cb, net_log, source));
  }

  virtual scoped_ptr<StreamSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      NetLog* net_log,
      const NetLog::Source& source) OVERRIDE {
    return scoped_ptr<StreamSocket>(
        new TCPClientSocket(addresses, net_log, source));
  }

  virtual scoped_ptr<SSLClientSocket> CreateSSLClientSocket(
      scoped_ptr<ClientSocketHandle> transport_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config,
      const SSLClientSocketContext& context) OVERRIDE {
    // nss_thread_task_runner_ may be NULL if g_use_dedicated_nss_thread is
    // false or if the dedicated NSS thread failed to start. If so, cause NSS
    // functions to execute on the current task runner.
    //
    // Note: The current task runner is obtained on each call due to unit
    // tests, which may create and tear down the current thread's TaskRunner
    // between each test. Because the DefaultClientSocketFactory is leaky, it
    // may span multiple tests, and thus the current task runner may change
    // from call to call.
    scoped_refptr<base::SequencedTaskRunner> nss_task_runner(
        nss_thread_task_runner_);
    if (!nss_task_runner.get())
      nss_task_runner = base::ThreadTaskRunnerHandle::Get();

#if defined(USE_OPENSSL)
    return scoped_ptr<SSLClientSocket>(
        new SSLClientSocketOpenSSL(transport_socket.Pass(), host_and_port,
                                   ssl_config, context));
#elif defined(USE_NSS) || defined(OS_MACOSX) || defined(OS_WIN)
    return scoped_ptr<SSLClientSocket>(
        new SSLClientSocketNSS(nss_task_runner.get(),
                               transport_socket.Pass(),
                               host_and_port,
                               ssl_config,
                               context));
#else
    NOTIMPLEMENTED();
    return scoped_ptr<SSLClientSocket>();
#endif
  }

  virtual void ClearSSLSessionCache() OVERRIDE {
    SSLClientSocket::ClearSessionCache();
  }

 private:
  scoped_refptr<base::SequencedWorkerPool> worker_pool_;
  scoped_refptr<base::SequencedTaskRunner> nss_thread_task_runner_;
};

static base::LazyInstance<DefaultClientSocketFactory>::Leaky
    g_default_client_socket_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ClientSocketFactory* ClientSocketFactory::GetDefaultFactory() {
  return g_default_client_socket_factory.Pointer();
}

}  // namespace net
