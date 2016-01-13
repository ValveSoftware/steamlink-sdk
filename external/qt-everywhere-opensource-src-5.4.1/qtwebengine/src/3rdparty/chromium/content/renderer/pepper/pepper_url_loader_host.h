// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_URL_LOADER_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_URL_LOADER_HOST_H_

#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/shared_impl/url_request_info_data.h"
#include "ppapi/shared_impl/url_response_info_data.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"

namespace blink {
class WebLocalFrame;
class WebURLLoader;
}

namespace content {

class RendererPpapiHostImpl;

class PepperURLLoaderHost : public ppapi::host::ResourceHost,
                            public blink::WebURLLoaderClient {
 public:
  // If main_document_loader is true, PP_Resource must be 0 since it will be
  // pending until the plugin resource attaches to it.
  PepperURLLoaderHost(RendererPpapiHostImpl* host,
                      bool main_document_loader,
                      PP_Instance instance,
                      PP_Resource resource);
  virtual ~PepperURLLoaderHost();

  // ResourceHost implementation.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  // blink::WebURLLoaderClient implementation.
  virtual void willSendRequest(blink::WebURLLoader* loader,
                               blink::WebURLRequest& new_request,
                               const blink::WebURLResponse& redir_response);
  virtual void didSendData(blink::WebURLLoader* loader,
                           unsigned long long bytes_sent,
                           unsigned long long total_bytes_to_be_sent);
  virtual void didReceiveResponse(blink::WebURLLoader* loader,
                                  const blink::WebURLResponse& response);
  virtual void didDownloadData(blink::WebURLLoader* loader,
                               int data_length,
                               int encoded_data_length);
  virtual void didReceiveData(blink::WebURLLoader* loader,
                              const char* data,
                              int data_length,
                              int encoded_data_length);
  virtual void didFinishLoading(blink::WebURLLoader* loader,
                                double finish_time,
                                int64_t total_encoded_data_length);
  virtual void didFail(blink::WebURLLoader* loader,
                       const blink::WebURLError& error);

 private:
  // ResourceHost protected overrides.
  virtual void DidConnectPendingHostToResource() OVERRIDE;

  // IPC messages
  int32_t OnHostMsgOpen(ppapi::host::HostMessageContext* context,
                        const ppapi::URLRequestInfoData& request_data);
  int32_t InternalOnHostMsgOpen(ppapi::host::HostMessageContext* context,
                                const ppapi::URLRequestInfoData& request_data);
  int32_t OnHostMsgSetDeferLoading(ppapi::host::HostMessageContext* context,
                                   bool defers_loading);
  int32_t OnHostMsgClose(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgGrantUniversalAccess(
      ppapi::host::HostMessageContext* context);

  // Sends or queues an unsolicited message to the plugin resource. This
  // handles cases where messages must be reordered for the plugin and
  // the case where we have created a pending host resource and the
  // plugin has not connected to us yet.
  //
  // Takes ownership of the given pointer.
  void SendUpdateToPlugin(IPC::Message* msg);

  // Sends or queues an unsolicited message to the plugin resource. This is
  // used inside SendUpdateToPlugin for messages that are already ordered
  // properly.
  //
  // Takes ownership of the given pointer.
  void SendOrderedUpdateToPlugin(IPC::Message* msg);

  void Close();

  // Returns the frame for the current request.
  blink::WebLocalFrame* GetFrame();

  // Calls SetDefersLoading on the current load. This encapsulates the logic
  // differences between document loads and regular ones.
  void SetDefersLoading(bool defers_loading);

  // Converts a WebURLResponse to a URLResponseInfo and saves it.
  void SaveResponse(const blink::WebURLResponse& response);
  void DidDataFromWebURLResponse(const ppapi::URLResponseInfoData& data);

  // Sends the UpdateProgress message (if necessary) to the plugin.
  void UpdateProgress();

  // Non-owning pointer.
  RendererPpapiHostImpl* renderer_ppapi_host_;

  // If true, then the plugin instance is a full-frame plugin and we're just
  // wrapping the main document's loader (i.e. loader_ is null).
  bool main_document_loader_;

  // The data that generated the request.
  ppapi::URLRequestInfoData request_data_;

  // Set to true when this loader can ignore same originl policy.
  bool has_universal_access_;

  // The loader associated with this request. MAY BE NULL.
  //
  // This will be NULL if the load hasn't been opened yet, or if this is a main
  // document loader (when registered as a mime type). Therefore, you should
  // always NULL check this value before using it. In the case of a main
  // document load, you would call the functions on the document to cancel the
  // load, etc. since there is no loader.
  scoped_ptr<blink::WebURLLoader> loader_;

  int64_t bytes_sent_;
  int64_t total_bytes_to_be_sent_;
  int64_t bytes_received_;
  int64_t total_bytes_to_be_received_;

  // Messages sent while the resource host is pending. These will be forwarded
  // to the plugin when the plugin side connects. The pointers are owned by
  // this object and must be deleted.
  ScopedVector<IPC::Message> pending_replies_;
  ScopedVector<IPC::Message> out_of_order_replies_;

  // True when there's a pending DataFromURLResponse call which will send a
  // PpapiPluginMsg_URLLoader_ReceivedResponse to the plugin, which introduces
  // ordering constraints on following messages to the plugin.
  bool pending_response_;

  base::WeakPtrFactory<PepperURLLoaderHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperURLLoaderHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_URL_LOADER_HOST_H_
