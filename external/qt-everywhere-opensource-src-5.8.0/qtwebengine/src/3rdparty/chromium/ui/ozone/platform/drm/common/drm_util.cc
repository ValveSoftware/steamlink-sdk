// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_util.h"

#include <drm_fourcc.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <utility>

#include "ui/display/util/edid_parser.h"

#if !defined(DRM_MODE_CONNECTOR_DSI)
#define DRM_MODE_CONNECTOR_DSI 16
#endif

#if !defined(DRM_CAP_CURSOR_WIDTH)
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#if !defined(DRM_CAP_CURSOR_HEIGHT)
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

#if !defined(DRM_FORMAT_R8)
// TODO(dshwang): after most linux and libdrm has this definition, remove it.
#define DRM_FORMAT_R8 fourcc_code('R', '8', ' ', ' ')
#endif
#if !defined(DRM_FORMAT_YV12)
// TODO(dcastagna): after libdrm has this definition, remove it.
#define DRM_FORMAT_YV12 fourcc_code('Y', 'V', '1', '2')
#endif

namespace ui {

namespace {

static const size_t kDefaultCursorWidth = 64;
static const size_t kDefaultCursorHeight = 64;

bool IsCrtcInUse(uint32_t crtc,
                 const ScopedVector<HardwareDisplayControllerInfo>& displays) {
  for (size_t i = 0; i < displays.size(); ++i) {
    if (crtc == displays[i]->crtc()->crtc_id)
      return true;
  }

  return false;
}

uint32_t GetCrtc(int fd,
                 drmModeConnector* connector,
                 drmModeRes* resources,
                 const ScopedVector<HardwareDisplayControllerInfo>& displays) {
  // If the connector already has an encoder try to re-use.
  if (connector->encoder_id) {
    ScopedDrmEncoderPtr encoder(drmModeGetEncoder(fd, connector->encoder_id));
    if (encoder && encoder->crtc_id && !IsCrtcInUse(encoder->crtc_id, displays))
      return encoder->crtc_id;
  }

  // Try to find an encoder for the connector.
  for (int i = 0; i < connector->count_encoders; ++i) {
    ScopedDrmEncoderPtr encoder(drmModeGetEncoder(fd, connector->encoders[i]));
    if (!encoder)
      continue;

    for (int j = 0; j < resources->count_crtcs; ++j) {
      // Check if the encoder is compatible with this CRTC
      if (!(encoder->possible_crtcs & (1 << j)) ||
          IsCrtcInUse(resources->crtcs[j], displays))
        continue;

      return resources->crtcs[j];
    }
  }

  return 0;
}

// Computes the refresh rate for the specific mode. If we have enough
// information use the mode timings to compute a more exact value otherwise
// fallback to using the mode's vertical refresh rate (the kernel computes this
// the same way, however there is a loss in precision since |vrefresh| is sent
// as an integer).
float GetRefreshRate(const drmModeModeInfo& mode) {
  if (!mode.htotal || !mode.vtotal)
    return mode.vrefresh;

  float clock = mode.clock;
  float htotal = mode.htotal;
  float vtotal = mode.vtotal;

  return (clock * 1000.0f) / (htotal * vtotal);
}

DisplayConnectionType GetDisplayType(drmModeConnector* connector) {
  switch (connector->connector_type) {
    case DRM_MODE_CONNECTOR_VGA:
      return DISPLAY_CONNECTION_TYPE_VGA;
    case DRM_MODE_CONNECTOR_DVII:
    case DRM_MODE_CONNECTOR_DVID:
    case DRM_MODE_CONNECTOR_DVIA:
      return DISPLAY_CONNECTION_TYPE_DVI;
    case DRM_MODE_CONNECTOR_LVDS:
    case DRM_MODE_CONNECTOR_eDP:
    case DRM_MODE_CONNECTOR_DSI:
      return DISPLAY_CONNECTION_TYPE_INTERNAL;
    case DRM_MODE_CONNECTOR_DisplayPort:
      return DISPLAY_CONNECTION_TYPE_DISPLAYPORT;
    case DRM_MODE_CONNECTOR_HDMIA:
    case DRM_MODE_CONNECTOR_HDMIB:
      return DISPLAY_CONNECTION_TYPE_HDMI;
    default:
      return DISPLAY_CONNECTION_TYPE_UNKNOWN;
  }
}

int GetDrmProperty(int fd,
                   drmModeConnector* connector,
                   const std::string& name,
                   ScopedDrmPropertyPtr* property) {
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr tmp(drmModeGetProperty(fd, connector->props[i]));
    if (!tmp)
      continue;

    if (name == tmp->name) {
      *property = std::move(tmp);
      return i;
    }
  }

  return -1;
}

std::string GetNameForEnumValue(drmModePropertyRes* property, uint32_t value) {
  for (int i = 0; i < property->count_enums; ++i)
    if (property->enums[i].value == value)
      return property->enums[i].name;

  return std::string();
}

ScopedDrmPropertyBlobPtr GetDrmPropertyBlob(int fd,
                                            drmModeConnector* connector,
                                            const std::string& name) {
  ScopedDrmPropertyPtr property;
  int index = GetDrmProperty(fd, connector, name, &property);
  if (index < 0)
    return nullptr;

  if (property->flags & DRM_MODE_PROP_BLOB) {
    return ScopedDrmPropertyBlobPtr(
        drmModeGetPropertyBlob(fd, connector->prop_values[index]));
  }

  return nullptr;
}

bool IsAspectPreserving(int fd, drmModeConnector* connector) {
  ScopedDrmPropertyPtr property;
  int index = GetDrmProperty(fd, connector, "scaling mode", &property);
  if (index < 0)
    return false;

  return (GetNameForEnumValue(property.get(), connector->prop_values[index]) ==
          "Full aspect");
}

int ConnectorIndex(int device_index, int display_index) {
  DCHECK_LT(device_index, 16);
  DCHECK_LT(display_index, 16);
  return ((device_index << 4) + display_index) & 0xFF;
}

bool HasColorCorrectionMatrix(int fd, drmModeCrtc* crtc) {
  ScopedDrmObjectPropertyPtr crtc_props(
      drmModeObjectGetProperties(fd, crtc->crtc_id, DRM_MODE_OBJECT_CRTC));

  for (uint32_t i = 0; i < crtc_props->count_props; ++i) {
    ScopedDrmPropertyPtr property(drmModeGetProperty(fd, crtc_props->props[i]));
    if (property && !strcmp(property->name, "CTM")) {
      return true;
    }
  }
  return false;
}

}  // namespace

gfx::Size GetMaximumCursorSize(int fd) {
  uint64_t width = 0, height = 0;
  if (drmGetCap(fd, DRM_CAP_CURSOR_WIDTH, &width)) {
    PLOG(WARNING) << "Unable to get cursor width capability";
    return gfx::Size(kDefaultCursorWidth, kDefaultCursorHeight);
  }
  if (drmGetCap(fd, DRM_CAP_CURSOR_HEIGHT, &height)) {
    PLOG(WARNING) << "Unable to get cursor height capability";
    return gfx::Size(kDefaultCursorWidth, kDefaultCursorHeight);
  }

  return gfx::Size(width, height);
}

HardwareDisplayControllerInfo::HardwareDisplayControllerInfo(
    ScopedDrmConnectorPtr connector,
    ScopedDrmCrtcPtr crtc,
    size_t index)
    : connector_(std::move(connector)), crtc_(std::move(crtc)), index_(index) {}

HardwareDisplayControllerInfo::~HardwareDisplayControllerInfo() {
}

ScopedVector<HardwareDisplayControllerInfo> GetAvailableDisplayControllerInfos(
    int fd) {
  ScopedDrmResourcesPtr resources(drmModeGetResources(fd));
  DCHECK(resources) << "Failed to get DRM resources";
  ScopedVector<HardwareDisplayControllerInfo> displays;

  for (int i = 0; i < resources->count_connectors; ++i) {
    ScopedDrmConnectorPtr connector(
        drmModeGetConnector(fd, resources->connectors[i]));

    if (!connector || connector->connection != DRM_MODE_CONNECTED ||
        connector->count_modes == 0)
      continue;

    uint32_t crtc_id = GetCrtc(fd, connector.get(), resources.get(), displays);
    if (!crtc_id)
      continue;

    ScopedDrmCrtcPtr crtc(drmModeGetCrtc(fd, crtc_id));
    displays.push_back(new HardwareDisplayControllerInfo(std::move(connector),
                                                         std::move(crtc), i));
  }

  return displays;
}

bool SameMode(const drmModeModeInfo& lhs, const drmModeModeInfo& rhs) {
  return lhs.clock == rhs.clock && lhs.hdisplay == rhs.hdisplay &&
         lhs.vdisplay == rhs.vdisplay && lhs.vrefresh == rhs.vrefresh &&
         lhs.hsync_start == rhs.hsync_start && lhs.hsync_end == rhs.hsync_end &&
         lhs.htotal == rhs.htotal && lhs.hskew == rhs.hskew &&
         lhs.vsync_start == rhs.vsync_start && lhs.vsync_end == rhs.vsync_end &&
         lhs.vtotal == rhs.vtotal && lhs.vscan == rhs.vscan &&
         lhs.flags == rhs.flags && strcmp(lhs.name, rhs.name) == 0;
}

DisplayMode_Params CreateDisplayModeParams(const drmModeModeInfo& mode) {
  DisplayMode_Params params;
  params.size = gfx::Size(mode.hdisplay, mode.vdisplay);
  params.is_interlaced = mode.flags & DRM_MODE_FLAG_INTERLACE;
  params.refresh_rate = GetRefreshRate(mode);

  return params;
}

DisplaySnapshot_Params CreateDisplaySnapshotParams(
    HardwareDisplayControllerInfo* info,
    int fd,
    const base::FilePath& sys_path,
    size_t device_index,
    const gfx::Point& origin) {
  DisplaySnapshot_Params params;
  int64_t connector_index = ConnectorIndex(device_index, info->index());
  params.display_id = connector_index;
  params.origin = origin;
  params.sys_path = sys_path;
  params.physical_size =
      gfx::Size(info->connector()->mmWidth, info->connector()->mmHeight);
  params.type = GetDisplayType(info->connector());
  params.is_aspect_preserving_scaling =
      IsAspectPreserving(fd, info->connector());
  params.has_color_correction_matrix =
      HasColorCorrectionMatrix(fd, info->crtc());
  params.maximum_cursor_size = GetMaximumCursorSize(fd);

  ScopedDrmPropertyBlobPtr edid_blob(
      GetDrmPropertyBlob(fd, info->connector(), "EDID"));

  if (edid_blob) {
    params.edid.assign(
        static_cast<uint8_t*>(edid_blob->data),
        static_cast<uint8_t*>(edid_blob->data) + edid_blob->length);

    GetDisplayIdFromEDID(params.edid, connector_index, &params.display_id,
                         &params.product_id);

    ParseOutputDeviceData(params.edid, nullptr, nullptr, &params.display_name,
                          nullptr, nullptr);
    ParseOutputOverscanFlag(params.edid, &params.has_overscan);
  } else {
    VLOG(1) << "Failed to get EDID blob for connector "
            << info->connector()->connector_id;
  }

  for (int i = 0; i < info->connector()->count_modes; ++i) {
    const drmModeModeInfo& mode = info->connector()->modes[i];
    params.modes.push_back(CreateDisplayModeParams(mode));

    if (info->crtc()->mode_valid && SameMode(info->crtc()->mode, mode)) {
      params.has_current_mode = true;
      params.current_mode = params.modes.back();
    }

    if (mode.type & DRM_MODE_TYPE_PREFERRED) {
      params.has_native_mode = true;
      params.native_mode = params.modes.back();
    }
  }

  // If no preferred mode is found then use the first one. Using the first one
  // since it should be the best mode.
  if (!params.has_native_mode && !params.modes.empty()) {
    params.has_native_mode = true;
    params.native_mode = params.modes.front();
  }

  return params;
}

int GetFourCCFormatFromBufferFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return DRM_FORMAT_R8;
    case gfx::BufferFormat::RGBA_8888:
      return DRM_FORMAT_ABGR8888;
    case gfx::BufferFormat::RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case gfx::BufferFormat::BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case gfx::BufferFormat::BGRX_8888:
      return DRM_FORMAT_XRGB8888;
    case gfx::BufferFormat::BGR_565:
      return DRM_FORMAT_RGB565;
    case gfx::BufferFormat::UYVY_422:
      return DRM_FORMAT_UYVY;
    case gfx::BufferFormat::YVU_420:
      return DRM_FORMAT_YV12;
    default:
      NOTREACHED();
      return 0;
  }
}

gfx::BufferFormat GetBufferFormatFromFourCCFormat(int format) {
  switch (format) {
    case DRM_FORMAT_R8:
      return gfx::BufferFormat::R_8;
    case DRM_FORMAT_ABGR8888:
      return gfx::BufferFormat::RGBA_8888;
    case DRM_FORMAT_XBGR8888:
      return gfx::BufferFormat::RGBX_8888;
    case DRM_FORMAT_ARGB8888:
      return gfx::BufferFormat::BGRA_8888;
    case DRM_FORMAT_XRGB8888:
      return gfx::BufferFormat::BGRX_8888;
    case DRM_FORMAT_RGB565:
      return gfx::BufferFormat::BGR_565;
    case DRM_FORMAT_UYVY:
      return gfx::BufferFormat::UYVY_422;
    case DRM_FORMAT_YV12:
      return gfx::BufferFormat::YVU_420;
    default:
      NOTREACHED();
      return gfx::BufferFormat::BGRA_8888;
  }
}

int GetFourCCFormatForFramebuffer(gfx::BufferFormat format) {
  // Currently, drm supports 24 bitcolordepth for hardware overlay.
  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
      return DRM_FORMAT_XRGB8888;
    case gfx::BufferFormat::BGR_565:
      return DRM_FORMAT_RGB565;
    case gfx::BufferFormat::UYVY_422:
      return DRM_FORMAT_UYVY;
    default:
      NOTREACHED();
      return 0;
  }
}
}  // namespace ui
