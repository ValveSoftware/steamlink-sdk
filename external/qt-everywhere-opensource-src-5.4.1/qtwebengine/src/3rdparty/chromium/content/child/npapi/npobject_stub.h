// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that receives IPC messages from an NPObjectProxy and calls the real
// NPObject.

#ifndef CONTENT_CHILD_NPAPI_NPOBJECT_STUB_H_
#define CONTENT_CHILD_NPAPI_NPOBJECT_STUB_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/child/npapi/npobject_base.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "url/gurl.h"

struct NPObject;

namespace content {
class NPChannelBase;
struct NPIdentifier_Param;
struct NPVariant_Param;

// This wraps an NPObject and converts IPC messages from NPObjectProxy to calls
// to the object.  The results are marshalled back.  See npobject_proxy.h for
// more information.
class NPObjectStub : public IPC::Listener,
                     public IPC::Sender,
                     public base::SupportsWeakPtr<NPObjectStub>,
                     public NPObjectBase {
 public:
  NPObjectStub(NPObject* npobject,
               NPChannelBase* channel,
               int route_id,
               int render_view_id,
               const GURL& page_url);
  virtual ~NPObjectStub();

  // Schedules tear-down of this stub.  The underlying NPObject reference is
  // released, and further invokations from the IPC channel will fail once this
  // call has returned.  Deletion of the stub is deferred to the main loop, in
  // case it is touched as the stack unwinds.  DeleteSoon() is safe to call
  // more than once, until control returns to the main loop.
  void DeleteSoon();

  // IPC::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // NPObjectBase implementation.
  virtual NPObject* GetUnderlyingNPObject() OVERRIDE;
  virtual IPC::Listener* GetChannelListener() OVERRIDE;

 private:
  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // message handlers
  void OnRelease(IPC::Message* reply_msg);
  void OnHasMethod(const NPIdentifier_Param& name,
                   bool* result);
  void OnInvoke(bool is_default,
                const NPIdentifier_Param& method,
                const std::vector<NPVariant_Param>& args,
                IPC::Message* reply_msg);
  void OnHasProperty(const NPIdentifier_Param& name,
                     bool* result);
  void OnGetProperty(const NPIdentifier_Param& name,
                     NPVariant_Param* property,
                     bool* result);
  void OnSetProperty(const NPIdentifier_Param& name,
                     const NPVariant_Param& property,
                     IPC::Message* reply_msg);
  void OnRemoveProperty(const NPIdentifier_Param& name,
                        bool* result);
  void OnInvalidate();
  void OnEnumeration(std::vector<NPIdentifier_Param>* value,
                     bool* result);
  void OnConstruct(const std::vector<NPVariant_Param>& args,
                   IPC::Message* reply_msg);
  void OnEvaluate(const std::string& script, bool popups_allowed,
                  IPC::Message* reply_msg);

 private:
  NPObject* npobject_;
  scoped_refptr<NPChannelBase> channel_;
  int route_id_;
  int render_view_id_;

  // The url of the main frame hosting the plugin.
  GURL page_url_;
};

}  // namespace content

#endif  // CONTENT_CHILD_NPAPI_NPOBJECT_STUB_H_
