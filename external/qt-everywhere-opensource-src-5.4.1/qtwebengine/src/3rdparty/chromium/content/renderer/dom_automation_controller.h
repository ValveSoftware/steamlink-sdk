// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_AUTOMATION_CONTROLLER_H_
#define CONTENT_RENDERER_DOM_AUTOMATION_CONTROLLER_H_

#include "base/basictypes.h"
#include "content/public/renderer/render_frame_observer.h"
#include "gin/wrappable.h"

/* DomAutomationController class:
   Bound to Javascript window.domAutomationController object.
   At the very basic, this object makes any native value (string, numbers,
   boolean) from javascript available to the automation host in Cpp.
   Any renderer implementation that is built with this binding will allow the
   above facility.
   The intended use of this object is to expose the DOM Objects and their
   attributes to the automation host.

   A typical usage would be like following (JS code):

   var object = document.getElementById('some_id');
   window.domAutomationController.send(object.nodeName); // get the tag name

   For the exact mode of usage,
   refer AutomationProxyTest.*DomAutomationController tests.

   The class provides a single send method that can send variety of native
   javascript values. (NPString, Number(double), Boolean)

   The actual communication occurs in the following manner:

    TEST            MASTER          RENDERER
              (1)             (3)
   |AProxy| ----->|AProvider|----->|RenderView|------|
      /\                |               |            |
      |                 |               |            |
      |(6)              |(2)            |(0)         |(4)
      |                 |               \/           |
      |                 |-------->|DAController|<----|
      |                                 |
      |                                 |(5)
      |-------|WebContentsImpl|<--------|


   Legends:
   - AProxy = AutomationProxy
   - AProvider = AutomationProvider
   - DAController = DomAutomationController

   (0) Initialization step where DAController is bound to the renderer
       and the view_id of the renderer is supplied to the DAController for
       routing message in (5).
   (1) A 'javascript:' url is sent from the test process to master as an IPC
       message. A unique routing id is generated at this stage (automation_id_)
   (2) The automation_id_ of step (1) is supplied to DAController by calling
       the bound method setAutomationId(). This is required for routing message
       in (6).
   (3) The 'javascript:' url is sent for execution by calling into
       Browser::LoadURL()
   (4) A callback is generated as a result of domAutomationController.send()
       into Cpp. The supplied value is received as a result of this callback.
   (5) The value received in (4) is sent to the master along with the
       stored automation_id_ as an IPC message. The frame_'s RenderFrameImpl is
       used to route the message. (IPC messages, ViewHostMsg_*DomAutomation* )
   (6) The value and the automation_id_ is extracted out of the message received
       in (5). This value is relayed to AProxy using another IPC message.
       automation_id_ is used to route the message.
       (IPC messages, AutomationMsg_Dom*Response)

*/

namespace blink {
class WebFrame;
}

namespace gin {
class Arguments;
}

namespace content {

class RenderFrame;

class DomAutomationController : public gin::Wrappable<DomAutomationController>,
                                public RenderFrameObserver {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(RenderFrame* render_frame, blink::WebFrame* frame);

  // Makes the renderer send a javascript value to the app.
  // The value to be sent can be either of type String,
  // Number (double casted to int32) or Boolean. Any other type or no
  // argument at all is ignored.
  bool SendMsg(const gin::Arguments& args);

  // Makes the renderer send a javascript value to the app.
  // The value should be properly formed JSON.
  bool SendJSON(const std::string& json);

  // Sends a string with a provided Automation Id.
  bool SendWithId(int automation_id, const std::string& str);

  bool SetAutomationId(int automation_id);

 private:
  explicit DomAutomationController(RenderFrame* render_view);
  virtual ~DomAutomationController();

  // gin::WrappableBase
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;

  // RenderViewObserver
  virtual void OnDestruct() OVERRIDE;

  int automation_id_;  // routing id to be used by the next channel.

  DISALLOW_COPY_AND_ASSIGN(DomAutomationController);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_AUTOMATION_CONTROLLER_H_
