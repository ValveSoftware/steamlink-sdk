// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/host_var_tracker.h"
#include "content/renderer/pepper/mock_resource.h"
#include "content/renderer/pepper/npapi_glue.h"
#include "content/renderer/pepper/npobject_var.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/test/ppapi_unittest.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_instance.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "third_party/WebKit/public/web/WebBindings.h"

using ppapi::NPObjectVar;

namespace content {

namespace {

// Tracked NPObjects -----------------------------------------------------------

int g_npobjects_alive = 0;

void TrackedClassDeallocate(NPObject* npobject) {
  g_npobjects_alive--;
  delete npobject;
}

NPClass g_tracked_npclass = {
    NP_CLASS_STRUCT_VERSION, NULL, &TrackedClassDeallocate, NULL, NULL, NULL,
    NULL,                    NULL, NULL,                    NULL, NULL, NULL, };

// Returns a new tracked NPObject with a refcount of 1. You'll want to put this
// in a NPObjectReleaser to free this ref when the test completes.
NPObject* NewTrackedNPObject() {
  NPObject* object = new NPObject;
  object->_class = &g_tracked_npclass;
  object->referenceCount = 1;

  g_npobjects_alive++;
  return object;
}

struct ReleaseNPObject {
  void operator()(NPObject* o) const { blink::WebBindings::releaseObject(o); }
};

// Handles automatically releasing a reference to the NPObject on destruction.
// It's assumed the input has a ref already taken.
typedef scoped_ptr<NPObject, ReleaseNPObject> NPObjectReleaser;

}  // namespace

class HostVarTrackerTest : public PpapiUnittest {
 public:
  HostVarTrackerTest() {}

  HostVarTracker& tracker() { return *HostGlobals::Get()->host_var_tracker(); }
};

TEST_F(HostVarTrackerTest, DeleteObjectVarWithInstance) {
  // Make a second instance (the test harness already creates & manages one).
  scoped_refptr<PepperPluginInstanceImpl> instance2(
      PepperPluginInstanceImpl::Create(NULL, module(), NULL, GURL()));
  PP_Instance pp_instance2 = instance2->pp_instance();

  // Make an object var.
  NPObjectReleaser npobject(NewTrackedNPObject());
  NPObjectToPPVarForTest(instance2.get(), npobject.get());

  EXPECT_EQ(1, g_npobjects_alive);
  EXPECT_EQ(1, tracker().GetLiveNPObjectVarsForInstance(pp_instance2));

  // Free the instance, this should release the ObjectVar.
  instance2 = NULL;
  EXPECT_EQ(0, tracker().GetLiveNPObjectVarsForInstance(pp_instance2));
}

// Make sure that using the same NPObject should give the same PP_Var
// each time.
TEST_F(HostVarTrackerTest, ReuseVar) {
  NPObjectReleaser npobject(NewTrackedNPObject());

  PP_Var pp_object1 = NPObjectToPPVarForTest(instance(), npobject.get());
  PP_Var pp_object2 = NPObjectToPPVarForTest(instance(), npobject.get());

  // The two results should be the same.
  EXPECT_EQ(pp_object1.value.as_id, pp_object2.value.as_id);

  // The objects should be able to get us back to the associated NPObject.
  // This ObjectVar must be released before we do NPObjectToPPVarForTest again
  // below so it gets freed and we get a new identifier.
  {
    scoped_refptr<NPObjectVar> check_object(NPObjectVar::FromPPVar(pp_object1));
    ASSERT_TRUE(check_object.get());
    EXPECT_EQ(instance()->pp_instance(), check_object->pp_instance());
    EXPECT_EQ(npobject.get(), check_object->np_object());
  }

  // Remove both of the refs we made above.
  ppapi::VarTracker* var_tracker = ppapi::PpapiGlobals::Get()->GetVarTracker();
  var_tracker->ReleaseVar(static_cast<int32_t>(pp_object2.value.as_id));
  var_tracker->ReleaseVar(static_cast<int32_t>(pp_object1.value.as_id));

  // Releasing the resource should free the internal ref, and so making a new
  // one now should generate a new ID.
  PP_Var pp_object3 = NPObjectToPPVarForTest(instance(), npobject.get());
  EXPECT_NE(pp_object1.value.as_id, pp_object3.value.as_id);
  var_tracker->ReleaseVar(static_cast<int32_t>(pp_object3.value.as_id));
}

}  // namespace content
