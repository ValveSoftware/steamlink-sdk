// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_CLASS_PROXY_H_
#define PPAPI_PROXY_PPP_CLASS_PROXY_H_

#include <vector>

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_Var_Deprecated;

namespace ppapi {
namespace proxy {

class SerializedVar;
class SerializedVarReceiveInput;
class SerializedVarVectorReceiveInput;
class SerializedVarOutParam;
class SerializedVarReturnValue;

class PPP_Class_Proxy : public InterfaceProxy {
 public:
  // PPP_Class isn't a normal interface that you can query for, so this
  // constructor doesn't take an interface pointer.
  explicit PPP_Class_Proxy(Dispatcher* dispatcher);
  virtual ~PPP_Class_Proxy();

  // Factory function used for registration (normal code can just use the
  // constructor).
  static InterfaceProxy* Create(Dispatcher* dispatcher);

  // Creates a proxied object in the browser process. This takes the browser's
  // PPB_Var_Deprecated interface to use to create the object. The class and
  static PP_Var CreateProxiedObject(const PPB_Var_Deprecated* var,
                                    Dispatcher* dispatcher,
                                    PP_Instance instance_id,
                                    int64 ppp_class,
                                    int64 class_data);

  static PP_Bool IsInstanceOf(const PPB_Var_Deprecated* ppb_var_impl,
                              const PP_Var& var,
                              int64 ppp_class,
                              int64* ppp_class_data);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // IPC message handlers.
  void OnMsgHasProperty(int64 ppp_class, int64 object,
                        SerializedVarReceiveInput property,
                        SerializedVarOutParam exception,
                        bool* result);
  void OnMsgHasMethod(int64 ppp_class, int64 object,
                      SerializedVarReceiveInput property,
                      SerializedVarOutParam exception,
                      bool* result);
  void OnMsgGetProperty(int64 ppp_class, int64 object,
                        SerializedVarReceiveInput property,
                        SerializedVarOutParam exception,
                        SerializedVarReturnValue result);
  void OnMsgEnumerateProperties(
      int64 ppp_class, int64 object,
      std::vector<SerializedVar>* props,
      SerializedVarOutParam exception);
  void OnMsgSetProperty(int64 ppp_class, int64 object,
                        SerializedVarReceiveInput property,
                        SerializedVarReceiveInput value,
                        SerializedVarOutParam exception);
  void OnMsgRemoveProperty(int64 ppp_class, int64 object,
                           SerializedVarReceiveInput property,
                           SerializedVarOutParam exception);
  void OnMsgCall(int64 ppp_class, int64 object,
                 SerializedVarReceiveInput method_name,
                 SerializedVarVectorReceiveInput arg_vector,
                 SerializedVarOutParam exception,
                 SerializedVarReturnValue result);
  void OnMsgConstruct(int64 ppp_class, int64 object,
                      SerializedVarVectorReceiveInput arg_vector,
                      SerializedVarOutParam exception,
                      SerializedVarReturnValue result);
  void OnMsgDeallocate(int64 ppp_class, int64 object);

  // Returns true if the given class/data points to a plugin-implemented
  // object. On failure, the exception, if non-NULL, will also be set.
  bool ValidateUserData(int64 ppp_class, int64 class_data,
                        SerializedVarOutParam* exception);

  DISALLOW_COPY_AND_ASSIGN(PPP_Class_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_CLASS_PROXY_H_
