// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/imagecapture/PhotoCapabilities.h"

namespace blink {

namespace {

String meteringModeToString(media::mojom::blink::MeteringMode mode) {
  switch (mode) {
    case media::mojom::blink::MeteringMode::NONE:
      return "none";
    case media::mojom::blink::MeteringMode::MANUAL:
      return "manual";
    case media::mojom::blink::MeteringMode::SINGLE_SHOT:
      return "single-shot";
    case media::mojom::blink::MeteringMode::CONTINUOUS:
      return "continuous";
    default:
      NOTREACHED();
  }
  return emptyString();
}

}  // anonymous namespace

// static
PhotoCapabilities* PhotoCapabilities::create() {
  return new PhotoCapabilities();
}

String PhotoCapabilities::focusMode() const {
  return meteringModeToString(m_focusMode);
}

String PhotoCapabilities::exposureMode() const {
  return meteringModeToString(m_exposureMode);
}

String PhotoCapabilities::whiteBalanceMode() const {
  return meteringModeToString(m_whiteBalanceMode);
}

String PhotoCapabilities::fillLightMode() const {
  switch (m_fillLightMode) {
    case media::mojom::blink::FillLightMode::NONE:
      return "none";
    case media::mojom::blink::FillLightMode::OFF:
      return "off";
    case media::mojom::blink::FillLightMode::AUTO:
      return "auto";
    case media::mojom::blink::FillLightMode::FLASH:
      return "flash";
    case media::mojom::blink::FillLightMode::TORCH:
      return "torch";
    default:
      NOTREACHED();
  }
  return emptyString();
}

DEFINE_TRACE(PhotoCapabilities) {
  visitor->trace(m_iso);
  visitor->trace(m_imageHeight);
  visitor->trace(m_imageWidth);
  visitor->trace(m_zoom);
  visitor->trace(m_exposureCompensation);
  visitor->trace(m_colorTemperature);
  visitor->trace(m_brightness);
  visitor->trace(m_contrast);
  visitor->trace(m_saturation);
  visitor->trace(m_sharpness);
}

}  // namespace blink
