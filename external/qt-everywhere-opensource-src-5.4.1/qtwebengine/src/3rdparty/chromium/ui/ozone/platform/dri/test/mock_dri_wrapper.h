// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_TEST_MOCK_DRI_WRAPPER_H_
#define UI_OZONE_PLATFORM_DRI_TEST_MOCK_DRI_WRAPPER_H_

#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

// The real DriWrapper makes actual DRM calls which we can't use in unit tests.
class MockDriWrapper : public ui::DriWrapper {
 public:
  MockDriWrapper(int fd);
  virtual ~MockDriWrapper();

  int get_get_crtc_call_count() const { return get_crtc_call_count_; }
  int get_free_crtc_call_count() const { return free_crtc_call_count_; }
  int get_restore_crtc_call_count() const { return restore_crtc_call_count_; }
  int get_add_framebuffer_call_count() const {
    return add_framebuffer_call_count_;
  }
  int get_remove_framebuffer_call_count() const {
    return remove_framebuffer_call_count_;
  }
  int get_page_flip_call_count() const { return page_flip_call_count_; }
  void fail_init() { fd_ = -1; }
  void set_set_crtc_expectation(bool state) { set_crtc_expectation_ = state; }
  void set_page_flip_expectation(bool state) { page_flip_expectation_ = state; }
  void set_add_framebuffer_expectation(bool state) {
    add_framebuffer_expectation_ = state;
  }

  // DriWrapper:
  virtual drmModeCrtc* GetCrtc(uint32_t crtc_id) OVERRIDE;
  virtual void FreeCrtc(drmModeCrtc* crtc) OVERRIDE;
  virtual bool SetCrtc(uint32_t crtc_id,
                       uint32_t framebuffer,
                       uint32_t* connectors,
                       drmModeModeInfo* mode) OVERRIDE;
  virtual bool SetCrtc(drmModeCrtc* crtc, uint32_t* connectors) OVERRIDE;
  virtual bool AddFramebuffer(uint32_t width,
                              uint32_t height,
                              uint8_t depth,
                              uint8_t bpp,
                              uint32_t stride,
                              uint32_t handle,
                              uint32_t* framebuffer) OVERRIDE;
  virtual bool RemoveFramebuffer(uint32_t framebuffer) OVERRIDE;
  virtual bool PageFlip(uint32_t crtc_id,
                        uint32_t framebuffer,
                        void* data) OVERRIDE;
  virtual bool SetProperty(uint32_t connector_id,
                           uint32_t property_id,
                           uint64_t value) OVERRIDE;
  virtual void FreeProperty(drmModePropertyRes* prop) OVERRIDE;
  virtual drmModePropertyBlobRes* GetPropertyBlob(drmModeConnector* connector,
                                                  const char* name) OVERRIDE;
  virtual void FreePropertyBlob(drmModePropertyBlobRes* blob) OVERRIDE;
  virtual bool SetCursor(uint32_t crtc_id,
                         uint32_t handle,
                         uint32_t width,
                         uint32_t height) OVERRIDE;
  virtual bool MoveCursor(uint32_t crtc_id, int x, int y) OVERRIDE;
  virtual void HandleEvent(drmEventContext& event) OVERRIDE;

 private:
  int get_crtc_call_count_;
  int free_crtc_call_count_;
  int restore_crtc_call_count_;
  int add_framebuffer_call_count_;
  int remove_framebuffer_call_count_;
  int page_flip_call_count_;

  bool set_crtc_expectation_;
  bool add_framebuffer_expectation_;
  bool page_flip_expectation_;

  DISALLOW_COPY_AND_ASSIGN(MockDriWrapper);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_TEST_MOCK_DRI_WRAPPER_H_
