// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_wrapper.h"

#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "base/logging.h"

namespace ui {

DriWrapper::DriWrapper(const char* device_path) {
  fd_ = open(device_path, O_RDWR | O_CLOEXEC);
}

DriWrapper::~DriWrapper() {
  if (fd_ >= 0)
    close(fd_);
}

drmModeCrtc* DriWrapper::GetCrtc(uint32_t crtc_id) {
  CHECK(fd_ >= 0);
  return drmModeGetCrtc(fd_, crtc_id);
}

void DriWrapper::FreeCrtc(drmModeCrtc* crtc) {
  drmModeFreeCrtc(crtc);
}

bool DriWrapper::SetCrtc(uint32_t crtc_id,
                         uint32_t framebuffer,
                         uint32_t* connectors,
                         drmModeModeInfo* mode) {
  CHECK(fd_ >= 0);
  return !drmModeSetCrtc(fd_, crtc_id, framebuffer, 0, 0, connectors, 1, mode);
}

bool DriWrapper::SetCrtc(drmModeCrtc* crtc, uint32_t* connectors) {
  CHECK(fd_ >= 0);
  return !drmModeSetCrtc(fd_,
                         crtc->crtc_id,
                         crtc->buffer_id,
                         crtc->x,
                         crtc->y,
                         connectors,
                         1,
                         &crtc->mode);
}

bool DriWrapper::DisableCrtc(uint32_t crtc_id) {
  CHECK(fd_ >= 0);
  return !drmModeSetCrtc(fd_, crtc_id, 0, 0, 0, NULL, 0, NULL);
}

bool DriWrapper::AddFramebuffer(uint32_t width,
                                uint32_t height,
                                uint8_t depth,
                                uint8_t bpp,
                                uint32_t stride,
                                uint32_t handle,
                                uint32_t* framebuffer) {
  CHECK(fd_ >= 0);
  return !drmModeAddFB(fd_,
                       width,
                       height,
                       depth,
                       bpp,
                       stride,
                       handle,
                       framebuffer);
}

bool DriWrapper::RemoveFramebuffer(uint32_t framebuffer) {
  CHECK(fd_ >= 0);
  return !drmModeRmFB(fd_, framebuffer);
}

bool DriWrapper::PageFlip(uint32_t crtc_id,
                          uint32_t framebuffer,
                          void* data) {
  CHECK(fd_ >= 0);
  return !drmModePageFlip(fd_,
                          crtc_id,
                          framebuffer,
                          DRM_MODE_PAGE_FLIP_EVENT,
                          data);
}

drmModePropertyRes* DriWrapper::GetProperty(drmModeConnector* connector,
                                            const char* name) {
  for (int i = 0; i < connector->count_props; ++i) {
    drmModePropertyRes* property = drmModeGetProperty(fd_, connector->props[i]);
    if (!property)
      continue;

    if (strcmp(property->name, name) == 0)
      return property;

    drmModeFreeProperty(property);
  }

  return NULL;
}

bool DriWrapper::SetProperty(uint32_t connector_id,
                             uint32_t property_id,
                             uint64_t value) {
  CHECK(fd_ >= 0);
  return !drmModeConnectorSetProperty(fd_, connector_id, property_id, value);
}

void DriWrapper::FreeProperty(drmModePropertyRes* prop) {
  drmModeFreeProperty(prop);
}

drmModePropertyBlobRes* DriWrapper::GetPropertyBlob(drmModeConnector* connector,
                                                    const char* name) {
  CHECK(fd_ >= 0);
  for (int i = 0; i < connector->count_props; ++i) {
    drmModePropertyRes* property = drmModeGetProperty(fd_, connector->props[i]);
    if (!property)
      continue;

    if (strcmp(property->name, name) == 0 &&
        property->flags & DRM_MODE_PROP_BLOB) {
      drmModePropertyBlobRes* blob =
          drmModeGetPropertyBlob(fd_, connector->prop_values[i]);
      drmModeFreeProperty(property);
      return blob;
    }

    drmModeFreeProperty(property);
  }

  return NULL;
}

void DriWrapper::FreePropertyBlob(drmModePropertyBlobRes* blob) {
  drmModeFreePropertyBlob(blob);
}

bool DriWrapper::SetCursor(uint32_t crtc_id,
                           uint32_t handle,
                           uint32_t width,
                           uint32_t height) {
  CHECK(fd_ >= 0);
  return !drmModeSetCursor(fd_, crtc_id, handle, width, height);
}

bool DriWrapper::MoveCursor(uint32_t crtc_id, int x, int y) {
  CHECK(fd_ >= 0);
  return !drmModeMoveCursor(fd_, crtc_id, x, y);
}

void DriWrapper::HandleEvent(drmEventContext& event) {
  CHECK(fd_ >= 0);
  drmHandleEvent(fd_, &event);
}

}  // namespace ui
