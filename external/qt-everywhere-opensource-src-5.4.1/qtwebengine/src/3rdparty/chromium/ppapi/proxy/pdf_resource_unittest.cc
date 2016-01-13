// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "base/strings/utf_string_conversions.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/proxy/pdf_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/proxy/serialized_handle.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

typedef PluginProxyTest PDFResourceTest;

}  // namespace

TEST_F(PDFResourceTest, GetLocalizedString) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  std::string expected_string = "hello";
  PpapiPluginMsg_PDF_GetLocalizedStringReply reply_msg(expected_string);
  ResourceSyncCallHandler handler(
      &sink(),
      PpapiHostMsg_PDF_GetLocalizedString::ID,
      PP_OK,
      reply_msg);
  sink().AddFilter(&handler);

  PP_Var var = pdf_iface->GetLocalizedString(
      pp_instance(), PP_RESOURCESTRING_PDFGETPASSWORD);

  {
    ProxyAutoLock lock;
    ScopedPPVar release_var(ScopedPPVar::PassRef(), var);
    StringVar* string_var = StringVar::FromPPVar(var);
    ASSERT_TRUE(string_var != NULL);
    std::string actual_string = string_var->value();

    ASSERT_EQ(PpapiHostMsg_PDF_GetLocalizedString::ID,
              handler.last_handled_msg().type());
    ASSERT_EQ(expected_string, actual_string);
  }

  // Remove the filter or it will be destroyed before the sink() is destroyed.
  sink().RemoveFilter(&handler);
}

TEST_F(PDFResourceTest, SearchString) {
  ProxyAutoLock lock;
  // Instantiate a resource explicitly so we can specify the locale.
  scoped_refptr<PDFResource> pdf_resource(
      new PDFResource(Connection(&sink(), &sink()), pp_instance()));
  pdf_resource->SetLocaleForTest("en-US");

  base::string16 input;
  base::string16 term;
  base::UTF8ToUTF16("abcdefabcdef", 12, &input);
  base::UTF8ToUTF16("bc", 2, &term);

  PP_PrivateFindResult* results;
  int count = 0;
  pdf_resource->SearchString(
      reinterpret_cast<const unsigned short*>(input.c_str()),
      reinterpret_cast<const unsigned short*>(term.c_str()),
      true,
      &results,
      &count);

  ASSERT_EQ(2, count);
  ASSERT_EQ(1, results[0].start_index);
  ASSERT_EQ(2, results[0].length);
  ASSERT_EQ(7, results[1].start_index);
  ASSERT_EQ(2, results[1].length);

  const PPB_Memory_Dev* memory_iface = thunk::GetPPB_Memory_Dev_0_1_Thunk();
  memory_iface->MemFree(results);
}

TEST_F(PDFResourceTest, DidStartLoading) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  pdf_iface->DidStartLoading(pp_instance());

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_PDF_DidStartLoading::ID, &params, &msg));
}

TEST_F(PDFResourceTest, DidStopLoading) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  pdf_iface->DidStopLoading(pp_instance());

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_PDF_DidStopLoading::ID, &params, &msg));
}

TEST_F(PDFResourceTest, SetContentRestriction) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  int restrictions = 5;
  pdf_iface->SetContentRestriction(pp_instance(), restrictions);

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_PDF_SetContentRestriction::ID, &params, &msg));
}

TEST_F(PDFResourceTest, HasUnsupportedFeature) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  pdf_iface->HasUnsupportedFeature(pp_instance());

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_PDF_HasUnsupportedFeature::ID, &params, &msg));
}

TEST_F(PDFResourceTest, Print) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  pdf_iface->Print(pp_instance());

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_PDF_Print::ID, &params, &msg));
}

TEST_F(PDFResourceTest, SaveAs) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  pdf_iface->SaveAs(pp_instance());

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_PDF_SaveAs::ID, &params, &msg));
}

TEST_F(PDFResourceTest, GetResourceImageForScale) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  HostResource expected_resource;
  expected_resource.SetHostResource(pp_instance(), 5);
  PP_ImageDataDesc expected_desc = {
      PP_IMAGEDATAFORMAT_BGRA_PREMUL,
      { 5, 10 },
      20,
  };
  SerializedHandle serialized_handle(SerializedHandle::SHARED_MEMORY);
  PpapiPluginMsg_PDF_GetResourceImageReply reply_msg(expected_resource,
                                                     expected_desc);
  ResourceSyncCallHandler handler(
      &sink(),
      PpapiHostMsg_PDF_GetResourceImage::ID,
      PP_OK,
      reply_msg);
  handler.set_serialized_handle(&serialized_handle);
  sink().AddFilter(&handler);

  PP_Resource resource = pdf_iface->GetResourceImageForScale(pp_instance(),
      PP_RESOURCEIMAGE_PDF_BUTTON_FTP, 1.0f);
  {
    ProxyAutoLock lock;
    PluginResourceTracker* resource_tracker =
        static_cast<PluginResourceTracker*>(
            PluginGlobals::Get()->GetResourceTracker());
    Resource* resource_object = resource_tracker->GetResource(resource);
    ImageData* image_data_object = static_cast<ImageData*>(resource_object);
    PP_ImageDataDesc actual_desc = image_data_object->desc();
    ASSERT_EQ(expected_desc.format, actual_desc.format);
    ASSERT_EQ(expected_desc.size.width, actual_desc.size.width);
    ASSERT_EQ(expected_desc.size.height, actual_desc.size.height);
    ASSERT_EQ(expected_desc.stride, actual_desc.stride);

    ASSERT_EQ(resource_tracker->PluginResourceForHostResource(
        expected_resource), resource);

    resource_tracker->ReleaseResource(resource);
  }

  // Remove the filter or it will be destroyed before the sink() is destroyed.
  sink().RemoveFilter(&handler);
}

}  // namespace proxy
}  // namespace ppapi
