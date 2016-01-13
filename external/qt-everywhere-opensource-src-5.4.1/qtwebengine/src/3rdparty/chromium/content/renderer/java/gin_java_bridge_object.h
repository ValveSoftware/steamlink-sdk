// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_OBJECT_H_
#define CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_OBJECT_H_

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/renderer/java/gin_java_bridge_dispatcher.h"
#include "gin/handle.h"
#include "gin/interceptor.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"

namespace blink {
class WebFrame;
}

namespace content {

class GinJavaBridgeValueConverter;

class GinJavaBridgeObject : public gin::Wrappable<GinJavaBridgeObject>,
                            public gin::NamedPropertyInterceptor {
 public:
  static gin::WrapperInfo kWrapperInfo;

  GinJavaBridgeDispatcher::ObjectID object_id() const { return object_id_; }

  // gin::Wrappable.
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;

  // gin::NamedPropertyInterceptor
  virtual v8::Local<v8::Value> GetNamedProperty(
      v8::Isolate* isolate, const std::string& property) OVERRIDE;
  virtual std::vector<std::string> EnumerateNamedProperties(
      v8::Isolate* isolate) OVERRIDE;

  static GinJavaBridgeObject* InjectNamed(
      blink::WebFrame* frame,
      const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
      const std::string& object_name,
      GinJavaBridgeDispatcher::ObjectID object_id);
  static GinJavaBridgeObject* InjectAnonymous(
      const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
      GinJavaBridgeDispatcher::ObjectID object_id);

 private:
  GinJavaBridgeObject(v8::Isolate* isolate,
                      const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
                      GinJavaBridgeDispatcher::ObjectID object_id);
  virtual ~GinJavaBridgeObject();

  v8::Handle<v8::Value> InvokeMethod(const std::string& name,
                                     gin::Arguments* args);

  base::WeakPtr<GinJavaBridgeDispatcher> dispatcher_;
  GinJavaBridgeDispatcher::ObjectID object_id_;
  scoped_ptr<GinJavaBridgeValueConverter> converter_;

  DISALLOW_COPY_AND_ASSIGN(GinJavaBridgeObject);
};

}  // namespace content

#endif  // CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_OBJECT_H_
