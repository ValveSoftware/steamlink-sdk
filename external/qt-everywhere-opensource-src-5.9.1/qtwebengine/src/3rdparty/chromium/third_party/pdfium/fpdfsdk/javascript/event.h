// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_JAVASCRIPT_EVENT_H_
#define FPDFSDK_JAVASCRIPT_EVENT_H_

#include "fpdfsdk/javascript/JS_Define.h"

class event : public CJS_EmbedObj {
 public:
  event(CJS_Object* pJSObject);
  ~event() override;

 public:
  bool change(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool changeEx(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool commitKey(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool fieldFull(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool keyDown(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool modifier(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool name(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool rc(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool richChange(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool richChangeEx(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool richValue(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool selEnd(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool selStart(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool shift(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool source(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool target(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool targetName(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool type(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool value(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool willCommit(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
};

class CJS_Event : public CJS_Object {
 public:
  CJS_Event(v8::Local<v8::Object> pObject) : CJS_Object(pObject) {}
  ~CJS_Event() override {}

  DECLARE_JS_CLASS();
  JS_STATIC_PROP(change, event);
  JS_STATIC_PROP(changeEx, event);
  JS_STATIC_PROP(commitKey, event);
  JS_STATIC_PROP(fieldFull, event);
  JS_STATIC_PROP(keyDown, event);
  JS_STATIC_PROP(modifier, event);
  JS_STATIC_PROP(name, event);
  JS_STATIC_PROP(rc, event);
  JS_STATIC_PROP(richChange, event);
  JS_STATIC_PROP(richChangeEx, event);
  JS_STATIC_PROP(richValue, event);
  JS_STATIC_PROP(selEnd, event);
  JS_STATIC_PROP(selStart, event);
  JS_STATIC_PROP(shift, event);
  JS_STATIC_PROP(source, event);
  JS_STATIC_PROP(target, event);
  JS_STATIC_PROP(targetName, event);
  JS_STATIC_PROP(type, event);
  JS_STATIC_PROP(value, event);
  JS_STATIC_PROP(willCommit, event);
};

#endif  // FPDFSDK_JAVASCRIPT_EVENT_H_
