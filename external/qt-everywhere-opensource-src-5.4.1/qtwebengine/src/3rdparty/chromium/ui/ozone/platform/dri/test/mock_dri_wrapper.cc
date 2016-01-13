// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/test/mock_dri_wrapper.h"

#include <xf86drmMode.h>

#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"

namespace ui {

MockDriWrapper::MockDriWrapper(int fd)
    : DriWrapper(""),
      get_crtc_call_count_(0),
      free_crtc_call_count_(0),
      restore_crtc_call_count_(0),
      add_framebuffer_call_count_(0),
      remove_framebuffer_call_count_(0),
      page_flip_call_count_(0),
      set_crtc_expectation_(true),
      add_framebuffer_expectation_(true),
      page_flip_expectation_(true) {
  fd_ = fd;
}

MockDriWrapper::~MockDriWrapper() {
  fd_ = -1;
}

drmModeCrtc* MockDriWrapper::GetCrtc(uint32_t crtc_id) {
  get_crtc_call_count_++;
  return new drmModeCrtc;
}

void MockDriWrapper::FreeCrtc(drmModeCrtc* crtc) {
  free_crtc_call_count_++;
  delete crtc;
}

bool MockDriWrapper::SetCrtc(uint32_t crtc_id,
                             uint32_t framebuffer,
                             uint32_t* connectors,
                             drmModeModeInfo* mode) {
  return set_crtc_expectation_;
}

bool MockDriWrapper::SetCrtc(drmModeCrtc* crtc, uint32_t* connectors) {
  restore_crtc_call_count_++;
  return true;
}

bool MockDriWrapper::AddFramebuffer(uint32_t width,
                                    uint32_t height,
                                    uint8_t depth,
                                    uint8_t bpp,
                                    uint32_t stride,
                                    uint32_t handle,
                                    uint32_t* framebuffer) {
  add_framebuffer_call_count_++;
  *framebuffer = add_framebuffer_call_count_;
  return add_framebuffer_expectation_;
}

bool MockDriWrapper::RemoveFramebuffer(uint32_t framebuffer) {
  remove_framebuffer_call_count_++;
  return true;
}

bool MockDriWrapper::PageFlip(uint32_t crtc_id,
                              uint32_t framebuffer,
                              void* data) {
  page_flip_call_count_++;
  static_cast<ui::HardwareDisplayController*>(data)->surface()->SwapBuffers();
  return page_flip_expectation_;
}

bool MockDriWrapper::SetProperty(uint32_t connector_id,
                                 uint32_t property_id,
                                 uint64_t value) {
  return true;
}

void MockDriWrapper::FreeProperty(drmModePropertyRes* prop) {
  delete prop;
}

drmModePropertyBlobRes* MockDriWrapper::GetPropertyBlob(
    drmModeConnector* connector,
    const char* name) {
  return new drmModePropertyBlobRes;
}

void MockDriWrapper::FreePropertyBlob(drmModePropertyBlobRes* blob) {
  delete blob;
}

bool MockDriWrapper::SetCursor(uint32_t crtc_id,
                               uint32_t handle,
                               uint32_t width,
                               uint32_t height) {
  return true;
}

bool MockDriWrapper::MoveCursor(uint32_t crtc_id, int x, int y) {
  return true;
}

void MockDriWrapper::HandleEvent(drmEventContext& event) {
}

}  // namespace ui
