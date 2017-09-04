// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_transform.h"

#include <vector>

#include "base/logging.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/transform.h"
#include "third_party/qcms/src/qcms.h"

#ifndef THIS_MUST_BE_INCLUDED_AFTER_QCMS_H
extern "C" {
#include "third_party/qcms/src/chain.h"
};
#endif

namespace gfx {

Transform Invert(const Transform& t) {
  Transform ret = t;
  if (!t.GetInverse(&ret)) {
    LOG(ERROR) << "Inverse should alsways be possible.";
  }
  return ret;
}

ColorTransform::TriStim Map(const Transform& t, ColorTransform::TriStim color) {
  t.TransformPoint(&color);
  return color;
}

ColorTransform::TriStim Xy2xyz(float x, float y) {
  return ColorTransform::TriStim(x, y, 1.0f - x - y);
}

void GetPrimaries(ColorSpace::PrimaryID id,
                  ColorTransform::TriStim primaries[4]) {
  switch (id) {
    case ColorSpace::PrimaryID::CUSTOM:
      NOTREACHED();

    case ColorSpace::PrimaryID::RESERVED0:
    case ColorSpace::PrimaryID::RESERVED:
    case ColorSpace::PrimaryID::UNSPECIFIED:
    case ColorSpace::PrimaryID::UNKNOWN:
    case ColorSpace::PrimaryID::BT709:
      // BT709 is our default case. Put it after the switch just
      // in case we somehow get an id which is not listed in the switch.
      // (We don't want to use "default", because we want the compiler
      //  to tell us if we forgot some enum values.)
      break;

    case ColorSpace::PrimaryID::BT470M:
      // Red
      primaries[0] = Xy2xyz(0.67f, 0.33f);
      // Green
      primaries[1] = Xy2xyz(0.21f, 0.71f);
      // Blue
      primaries[2] = Xy2xyz(0.14f, 0.08f);
      // Whitepoint
      primaries[3] = Xy2xyz(0.31f, 0.316f);
      return;

    case ColorSpace::PrimaryID::BT470BG:
      // Red
      primaries[0] = Xy2xyz(0.64f, 0.33f);
      // Green
      primaries[1] = Xy2xyz(0.29f, 0.60f);
      // Blue
      primaries[2] = Xy2xyz(0.15f, 0.06f);
      // Whitepoint (D65f)
      primaries[3] = Xy2xyz(0.3127f, 0.3290f);
      return;

    case ColorSpace::PrimaryID::SMPTE170M:
    case ColorSpace::PrimaryID::SMPTE240M:
      // Red
      primaries[0] = Xy2xyz(0.630f, 0.340f);
      // Green
      primaries[1] = Xy2xyz(0.310f, 0.595f);
      // Blue
      primaries[2] = Xy2xyz(0.155f, 0.070f);
      // Whitepoint (D65f)
      primaries[3] = Xy2xyz(0.3127f, 0.3290f);
      return;

    case ColorSpace::PrimaryID::FILM:
      // Red
      primaries[0] = Xy2xyz(0.681f, 0.319f);
      // Green
      primaries[1] = Xy2xyz(0.243f, 0.692f);
      // Blue
      primaries[2] = Xy2xyz(0.145f, 0.049f);
      // Whitepoint (Cf)
      primaries[3] = Xy2xyz(0.310f, 0.136f);
      return;

    case ColorSpace::PrimaryID::BT2020:
      // Red
      primaries[0] = Xy2xyz(0.708f, 0.292f);
      // Green
      primaries[1] = Xy2xyz(0.170f, 0.797f);
      // Blue
      primaries[2] = Xy2xyz(0.131f, 0.046f);
      // Whitepoint (D65f)
      primaries[3] = Xy2xyz(0.3127f, 0.3290f);
      return;

    case ColorSpace::PrimaryID::SMPTEST428_1:
      // X
      primaries[0] = Xy2xyz(1.0f, 0.0f);
      // Y
      primaries[1] = Xy2xyz(0.0f, 1.0f);
      // Z
      primaries[2] = Xy2xyz(0.0f, 0.0f);
      // Whitepoint (Ef)
      primaries[3] = Xy2xyz(1.0f / 3.0f, 1.0f / 3.0f);
      return;

    case ColorSpace::PrimaryID::SMPTEST431_2:
      // Red
      primaries[0] = Xy2xyz(0.680f, 0.320f);
      // Green
      primaries[1] = Xy2xyz(0.265f, 0.690f);
      // Blue
      primaries[2] = Xy2xyz(0.150f, 0.060f);
      // Whitepoint
      primaries[3] = Xy2xyz(0.314f, 0.351f);
      return;

    case ColorSpace::PrimaryID::SMPTEST432_1:
      // Red
      primaries[0] = Xy2xyz(0.680f, 0.320f);
      // Green
      primaries[1] = Xy2xyz(0.265f, 0.690f);
      // Blue
      primaries[2] = Xy2xyz(0.150f, 0.060f);
      // Whitepoint (D65f)
      primaries[3] = Xy2xyz(0.3127f, 0.3290f);
      return;

    case ColorSpace::PrimaryID::XYZ_D50:
      // X
      primaries[0] = Xy2xyz(1.0f, 0.0f);
      // Y
      primaries[1] = Xy2xyz(0.0f, 1.0f);
      // Z
      primaries[2] = Xy2xyz(0.0f, 0.0f);
      // D50
      primaries[3] = Xy2xyz(0.34567f, 0.35850f);
      return;
  }

  // Red
  primaries[0] = Xy2xyz(0.640f, 0.330f);
  // Green
  primaries[1] = Xy2xyz(0.300f, 0.600f);
  // Blue
  primaries[2] = Xy2xyz(0.150f, 0.060f);
  // Whitepoint (D65f)
  primaries[3] = Xy2xyz(0.3127f, 0.3290f);
}

GFX_EXPORT Transform GetPrimaryMatrix(ColorSpace::PrimaryID id) {
  ColorTransform::TriStim primaries[4];
  GetPrimaries(id, primaries);
  ColorTransform::TriStim WXYZ(primaries[3].x() / primaries[3].y(), 1.0f,
                               primaries[3].z() / primaries[3].y());

  Transform ret(
      primaries[0].x(), primaries[1].x(), primaries[2].x(), 0.0f,  // 1
      primaries[0].y(), primaries[1].y(), primaries[2].y(), 0.0f,  // 2
      primaries[0].z(), primaries[1].z(), primaries[2].z(), 0.0f,  // 3
      0.0f, 0.0f, 0.0f, 1.0f);                                     // 4

  ColorTransform::TriStim conv = Map(Invert(ret), WXYZ);
  ret.Scale3d(conv.x(), conv.y(), conv.z());

  // Chromatic adaptation.
  Transform bradford(0.8951000f, 0.2664000f, -0.1614000f, 0.0f,  // 1
                     -0.7502000f, 1.7135000f, 0.0367000f, 0.0f,  // 2
                     0.0389000f, -0.0685000f, 1.0296000f, 0.0f,  // 3
                     0.0f, 0.0f, 0.0f, 1.0f);                    // 4

  ColorTransform::TriStim D50(0.9642f, 1.0f, 0.8249f);
  ColorTransform::TriStim source_response = Map(bradford, WXYZ);
  ColorTransform::TriStim dest_response = Map(bradford, D50);

  Transform adapter;
  adapter.Scale3d(dest_response.x() / source_response.x(),
                  dest_response.y() / source_response.y(),
                  dest_response.z() / source_response.z());

  return Invert(bradford) * adapter * bradford * ret;
}

GFX_EXPORT float FromLinear(ColorSpace::TransferID id, float v) {
  switch (id) {
    case ColorSpace::TransferID::SMPTEST2084_NON_HDR:
      // Should already be handled.
      NOTREACHED();
    case ColorSpace::TransferID::CUSTOM:
    // TODO(hubbe): Actually implement custom transfer functions.
    case ColorSpace::TransferID::RESERVED0:
    case ColorSpace::TransferID::RESERVED:
    case ColorSpace::TransferID::UNSPECIFIED:
    case ColorSpace::TransferID::UNKNOWN:
    // All unknown values default to BT709

    case ColorSpace::TransferID::BT709:
    case ColorSpace::TransferID::SMPTE170M:
    case ColorSpace::TransferID::BT2020_10:
    case ColorSpace::TransferID::BT2020_12:
      // BT709 is our "default" cause, so put the code after the switch
      // to avoid "control reaches end of non-void function" errors.
      break;

    case ColorSpace::TransferID::GAMMA22:
      v = fmax(0.0f, v);
      return powf(v, 1.0f / 2.2f);

    case ColorSpace::TransferID::GAMMA28:
      v = fmax(0.0f, v);
      return powf(v, 1.0f / 2.8f);

    case ColorSpace::TransferID::SMPTE240M: {
      v = fmax(0.0f, v);
      float a = 1.11157219592173128753f;
      float b = 0.02282158552944503135f;
      if (v <= b) {
        return 4.0f * v;
      } else {
        return a * powf(v, 0.45f) - (a - 1.0f);
      }
    }

    case ColorSpace::TransferID::LINEAR:
      return v;

    case ColorSpace::TransferID::LOG:
      if (v < 0.01f)
        return 0.0f;
      return 1.0f + log(v) / log(10.0f) / 2.0f;

    case ColorSpace::TransferID::LOG_SQRT:
      if (v < sqrt(10.0f) / 1000.0f)
        return 0.0f;
      return 1.0f + log(v) / log(10.0f) / 2.5f;

    case ColorSpace::TransferID::IEC61966_2_4: {
      float a = 1.099296826809442f;
      float b = 0.018053968510807f;
      if (v < -b) {
        return -a * powf(-v, 0.45f) + (a - 1.0f);
      } else if (v <= b) {
        return 4.5f * v;
      } else {
        return a * powf(v, 0.45f) - (a - 1.0f);
      }
    }

    case ColorSpace::TransferID::BT1361_ECG: {
      float a = 1.099f;
      float b = 0.018f;
      float l = 0.0045f;
      if (v < -l) {
        return -(a * powf(-4.0f * v, 0.45f) + (a - 1.0f)) / 4.0f;
      } else if (v <= b) {
        return 4.5f * v;
      } else {
        return a * powf(v, 0.45f) - (a - 1.0f);
      }
    }

    case ColorSpace::TransferID::IEC61966_2_1: {  // SRGB
      v = fmax(0.0f, v);
      float a = 1.055f;
      float b = 0.0031308f;
      if (v < b) {
        return 12.92f * v;
      } else {
        return a * powf(v, 1.0f / 2.4f) - (a - 1.0f);
      }
    }
    case ColorSpace::TransferID::SMPTEST2084: {
      v = fmax(0.0f, v);
      float m1 = (2610.0f / 4096.0f) / 4.0f;
      float m2 = (2523.0f / 4096.0f) * 128.0f;
      float c1 = 3424.0f / 4096.0f;
      float c2 = (2413.0f / 4096.0f) * 32.0f;
      float c3 = (2392.0f / 4096.0f) * 32.0f;
      return powf((c1 + c2 * powf(v, m1)) / (1.0f + c3 * powf(v, m1)), m2);
    }

    case ColorSpace::TransferID::SMPTEST428_1:
      v = fmax(0.0f, v);
      return powf(48.0f * v + 52.37f, 1.0f / 2.6f);

    // Spec: http://www.arib.or.jp/english/html/overview/doc/2-STD-B67v1_0.pdf
    case ColorSpace::TransferID::ARIB_STD_B67: {
      const float a = 0.17883277f;
      const float b = 0.28466892f;
      const float c = 0.55991073f;
      const float Lmax = 12.0f;
      v = Lmax * fmax(0.0f, v);
      if (v <= 1)
        return 0.5f * sqrtf(v);
      else
        return a * log(v - b) + c;
    }

    // Chrome-specific values below
    case ColorSpace::TransferID::GAMMA24:
      v = fmax(0.0f, v);
      return powf(v, 1.0f / 2.4f);
  }

  v = fmax(0.0f, v);
  float a = 1.099296826809442f;
  float b = 0.018053968510807f;
  if (v <= b) {
    return 4.5f * v;
  } else {
    return a * powf(v, 0.45f) - (a - 1.0f);
  }
}

GFX_EXPORT float ToLinear(ColorSpace::TransferID id, float v) {
  switch (id) {
    case ColorSpace::TransferID::CUSTOM:
    // TODO(hubbe): Actually implement custom transfer functions.
    case ColorSpace::TransferID::RESERVED0:
    case ColorSpace::TransferID::RESERVED:
    case ColorSpace::TransferID::UNSPECIFIED:
    case ColorSpace::TransferID::UNKNOWN:
    // All unknown values default to BT709

    case ColorSpace::TransferID::BT709:
    case ColorSpace::TransferID::SMPTE170M:
    case ColorSpace::TransferID::BT2020_10:
    case ColorSpace::TransferID::BT2020_12:
      // BT709 is our "default" cause, so put the code after the switch
      // to avoid "control reaches end of non-void function" errors.
      break;

    case ColorSpace::TransferID::GAMMA22:
      v = fmax(0.0f, v);
      return powf(v, 2.2f);

    case ColorSpace::TransferID::GAMMA28:
      v = fmax(0.0f, v);
      return powf(v, 2.8f);

    case ColorSpace::TransferID::SMPTE240M: {
      v = fmax(0.0f, v);
      float a = 1.11157219592173128753f;
      float b = 0.02282158552944503135f;
      if (v <= FromLinear(ColorSpace::TransferID::SMPTE240M, b)) {
        return v / 4.0f;
      } else {
        return powf((v + a - 1.0f) / a, 1.0f / 0.45f);
      }
    }

    case ColorSpace::TransferID::LINEAR:
      return v;

    case ColorSpace::TransferID::LOG:
      if (v < 0.0f)
        return 0.0f;
      return powf(10.0f, (v - 1.0f) * 2.0f);

    case ColorSpace::TransferID::LOG_SQRT:
      if (v < 0.0f)
        return 0.0f;
      return powf(10.0f, (v - 1.0f) * 2.5f);

    case ColorSpace::TransferID::IEC61966_2_4: {
      float a = 1.099296826809442f;
      float b = 0.018053968510807f;
      if (v < FromLinear(ColorSpace::TransferID::IEC61966_2_4, -a)) {
        return -powf((a - 1.0f - v) / a, 1.0f / 0.45f);
      } else if (v <= FromLinear(ColorSpace::TransferID::IEC61966_2_4, b)) {
        return v / 4.5f;
      } else {
        return powf((v + a - 1.0f) / a, 1.0f / 0.45f);
      }
    }

    case ColorSpace::TransferID::BT1361_ECG: {
      float a = 1.099f;
      float b = 0.018f;
      float l = 0.0045f;
      if (v < FromLinear(ColorSpace::TransferID::BT1361_ECG, -l)) {
        return -powf((1.0f - a - v * 4.0f) / a, 1.0f / 0.45f) / 4.0f;
      } else if (v <= FromLinear(ColorSpace::TransferID::BT1361_ECG, b)) {
        return v / 4.5f;
      } else {
        return powf((v + a - 1.0f) / a, 1.0f / 0.45f);
      }
    }

    case ColorSpace::TransferID::IEC61966_2_1: {  // SRGB
      v = fmax(0.0f, v);
      float a = 1.055f;
      float b = 0.0031308f;
      if (v < FromLinear(ColorSpace::TransferID::IEC61966_2_1, b)) {
        return v / 12.92f;
      } else {
        return powf((v + a - 1.0f) / a, 2.4f);
      }
    }

    case ColorSpace::TransferID::SMPTEST2084: {
      v = fmax(0.0f, v);
      float m1 = (2610.0f / 4096.0f) / 4.0f;
      float m2 = (2523.0f / 4096.0f) * 128.0f;
      float c1 = 3424.0f / 4096.0f;
      float c2 = (2413.0f / 4096.0f) * 32.0f;
      float c3 = (2392.0f / 4096.0f) * 32.0f;
      return powf(
          fmax(powf(v, 1.0f / m2) - c1, 0) / (c2 - c3 * powf(v, 1.0f / m2)),
          1.0f / m1);
    }

    case ColorSpace::TransferID::SMPTEST428_1:
      return (powf(v, 2.6f) - 52.37f) / 48.0f;

    // Chrome-specific values below
    case ColorSpace::TransferID::GAMMA24:
      v = fmax(0.0f, v);
      return powf(v, 2.4f);

    case ColorSpace::TransferID::SMPTEST2084_NON_HDR:
      v = fmax(0.0f, v);
      return fmin(2.3f * pow(v, 2.8f), v / 5.0f + 0.8f);

    // Spec: http://www.arib.or.jp/english/html/overview/doc/2-STD-B67v1_0.pdf
    case ColorSpace::TransferID::ARIB_STD_B67: {
      v = fmax(0.0f, v);
      const float a = 0.17883277f;
      const float b = 0.28466892f;
      const float c = 0.55991073f;
      const float Lmax = 12.0f;
      float v_ = 0.0f;
      if (v <= 0.5f) {
        v_ = (v * 2.0f) * (v * 2.0f);
      } else {
        v_ = exp((v - c) / a) + b;
      }
      return v_ / Lmax;
    }
  }

  v = fmax(0.0f, v);
  float a = 1.099296826809442f;
  float b = 0.018053968510807f;
  if (v < FromLinear(ColorSpace::TransferID::BT709, b)) {
    return v / 4.5f;
  } else {
    return powf((v + a - 1.0f) / a, 1.0f / 0.45f);
  }
}

namespace {
// Assumes bt2020
float Luma(const ColorTransform::TriStim& c) {
  return c.x() * 0.2627f + c.y() * 0.6780f + c.z() * 0.0593f;
}
};

GFX_EXPORT ColorTransform::TriStim ToLinear(ColorSpace::TransferID id,
                                            ColorTransform::TriStim color) {
  ColorTransform::TriStim ret(ToLinear(id, color.x()), ToLinear(id, color.y()),
                              ToLinear(id, color.z()));

  if (id == ColorSpace::TransferID::SMPTEST2084_NON_HDR) {
    if (Luma(ret) > 0.0) {
      ColorTransform::TriStim smpte2084(
          ToLinear(ColorSpace::TransferID::SMPTEST2084, color.x()),
          ToLinear(ColorSpace::TransferID::SMPTEST2084, color.y()),
          ToLinear(ColorSpace::TransferID::SMPTEST2084, color.z()));
      smpte2084.Scale(Luma(ret) / Luma(smpte2084));
      ret = smpte2084;
    }
  }

  return ret;
}

GFX_EXPORT Transform GetTransferMatrix(ColorSpace::MatrixID id) {
  // Default values for BT709;
  float Kr = 0.2126f;
  float Kb = 0.0722f;
  switch (id) {
    case ColorSpace::MatrixID::RGB:
      return Transform();

    case ColorSpace::MatrixID::BT709:
    case ColorSpace::MatrixID::UNSPECIFIED:
    case ColorSpace::MatrixID::RESERVED:
    case ColorSpace::MatrixID::UNKNOWN:
      // Default values are already set.
      break;

    case ColorSpace::MatrixID::FCC:
      Kr = 0.30f;
      Kb = 0.11f;
      break;

    case ColorSpace::MatrixID::BT470BG:
    case ColorSpace::MatrixID::SMPTE170M:
      Kr = 0.299f;
      Kb = 0.144f;
      break;

    case ColorSpace::MatrixID::SMPTE240M:
      Kr = 0.212f;
      Kb = 0.087f;
      break;

    case ColorSpace::MatrixID::YCOCG:
      return Transform(0.25f, 0.5f, 0.25f, 0.5f,    // 1
                       -0.25f, 0.5f, -0.25f, 0.5f,  // 2
                       0.5f, 0.0f, -0.5f, 0.0f,     // 3
                       0.0f, 0.0f, 0.0f, 1.0f);     // 4

    // TODO(hubbe): Check if the CL equation is right.
    case ColorSpace::MatrixID::BT2020_NCL:
    case ColorSpace::MatrixID::BT2020_CL:
      Kr = 0.2627f;
      Kb = 0.0593f;
      break;

    case ColorSpace::MatrixID::YDZDX:
      return Transform(0.0f, 1.0f, 0.0f, 0.0f,               // 1
                       0.0f, -0.5f, 0.986566f / 2.0f, 0.5f,  // 2
                       0.5f, -0.991902f / 2.0f, 0.0f, 0.5f,  // 3
                       0.0f, 0.0f, 0.0f, 1.0f);              // 4
  }
  float u_m = 0.5f / (1.0f - Kb);
  float v_m = 0.5f / (1.0f - Kr);
  return Transform(
      Kr, 1.0f - Kr - Kb, Kb, 0.0f,                                 // 1
      u_m * -Kr, u_m * -(1.0f - Kr - Kb), u_m * (1.0f - Kb), 0.5f,  // 2
      v_m * (1.0f - Kr), v_m * -(1.0f - Kr - Kb), v_m * -Kb, 0.5f,  // 3
      0.0f, 0.0f, 0.0f, 1.0f);                                      // 4
}

Transform GetRangeAdjustMatrix(ColorSpace::RangeID range,
                               ColorSpace::MatrixID matrix) {
  switch (range) {
    case ColorSpace::RangeID::FULL:
    case ColorSpace::RangeID::UNSPECIFIED:
      return Transform();

    case ColorSpace::RangeID::DERIVED:
    case ColorSpace::RangeID::LIMITED:
      break;
  }
  switch (matrix) {
    case ColorSpace::MatrixID::RGB:
    case ColorSpace::MatrixID::YCOCG:
      return Transform(255.0f / 219.0f, 0.0f, 0.0f, -16.0f / 219.0f,  // 1
                       0.0f, 255.0f / 219.0f, 0.0f, -16.0f / 219.0f,  // 2
                       0.0f, 0.0f, 255.0f / 219.0f, -16.0f / 219.0f,  // 3
                       0.0f, 0.0f, 0.0f, 1.0f);                       // 4

    case ColorSpace::MatrixID::BT709:
    case ColorSpace::MatrixID::UNSPECIFIED:
    case ColorSpace::MatrixID::RESERVED:
    case ColorSpace::MatrixID::FCC:
    case ColorSpace::MatrixID::BT470BG:
    case ColorSpace::MatrixID::SMPTE170M:
    case ColorSpace::MatrixID::SMPTE240M:
    case ColorSpace::MatrixID::BT2020_NCL:
    case ColorSpace::MatrixID::BT2020_CL:
    case ColorSpace::MatrixID::YDZDX:
    case ColorSpace::MatrixID::UNKNOWN:
      return Transform(255.0f / 219.0f, 0.0f, 0.0f, -16.0f / 219.0f,  // 1
                       0.0f, 255.0f / 224.0f, 0.0f, -15.5f / 224.0f,  // 2
                       0.0f, 0.0f, 255.0f / 224.0f, -15.5f / 224.0f,  // 3
                       0.0f, 0.0f, 0.0f, 1.0f);                       // 4
  }
  NOTREACHED();
  return Transform();
}

class ColorSpaceToColorSpaceTransform : public ColorTransform {
 public:
  ColorSpaceToColorSpaceTransform(const ColorSpace& from,
                                  const ColorSpace& to,
                                  Intent intent)
      : from_(from), to_(to) {
    if (intent == Intent::INTENT_PERCEPTUAL) {
      switch (from_.transfer_) {
        case ColorSpace::TransferID::UNSPECIFIED:
        case ColorSpace::TransferID::BT709:
        case ColorSpace::TransferID::SMPTE170M:
          // See SMPTE 1886
          from_.transfer_ = ColorSpace::TransferID::GAMMA24;
          break;

        case ColorSpace::TransferID::SMPTEST2084:
          // We don't have an HDR display, so replace SMPTE 2084 with something
          // that returns ranges more or less suitable for a normal display.
          from_.transfer_ = ColorSpace::TransferID::SMPTEST2084_NON_HDR;
          break;

        case ColorSpace::TransferID::ARIB_STD_B67:
          // Interpreting HLG using a gamma 2.4 works reasonably well for SDR
          // displays. Once we have HDR output capabilies, we'll need to
          // change this.
          from_.transfer_ = ColorSpace::TransferID::GAMMA24;
          break;

        default:  // Do nothing
          break;
      }

      // TODO(hubbe): shrink gamuts here (never stretch gamuts)
    }

    Transform* from_transfer_matrix =
        from_.matrix_ == ColorSpace::MatrixID::BT2020_CL ? &b_ : &a_;
    Transform* to_transfer_matrix =
        to_.matrix_ == ColorSpace::MatrixID::BT2020_CL ? &b_ : &c_;

    c_ *= Invert(GetRangeAdjustMatrix(to_.range_, to_.matrix_));
    *to_transfer_matrix *= GetTransferMatrix(to_.matrix_);
    b_ *= Invert(GetPrimaryTransform(to_));
    b_ *= GetPrimaryTransform(from_);
    *from_transfer_matrix *= Invert(GetTransferMatrix(from_.matrix_));
    a_ *= GetRangeAdjustMatrix(from_.range_, from_.matrix_);
  }

  static Transform GetPrimaryTransform(const ColorSpace& c) {
    if (c.primaries_ == ColorSpace::PrimaryID::CUSTOM) {
      return Transform(c.custom_primary_matrix_[0], c.custom_primary_matrix_[1],
                       c.custom_primary_matrix_[2], c.custom_primary_matrix_[3],
                       c.custom_primary_matrix_[4], c.custom_primary_matrix_[5],
                       c.custom_primary_matrix_[6], c.custom_primary_matrix_[7],
                       c.custom_primary_matrix_[8], c.custom_primary_matrix_[9],
                       c.custom_primary_matrix_[10],
                       c.custom_primary_matrix_[11], 0.0f, 0.0f, 0.0f, 1.0f);
    } else {
      return GetPrimaryMatrix(c.primaries_);
    }
  }

  void transform(TriStim* colors, size_t num) override {
    for (size_t i = 0; i < num; i++) {
      TriStim c = colors[i];
      a_.TransformPoint(&c);
      c = ToLinear(from_.transfer_, c);
      b_.TransformPoint(&c);
      c.set_x(FromLinear(to_.transfer_, c.x()));
      c.set_y(FromLinear(to_.transfer_, c.y()));
      c.set_z(FromLinear(to_.transfer_, c.z()));
      c_.TransformPoint(&c);
      colors[i] = c;
    }
  }

 private:
  ColorSpace from_;
  ColorSpace to_;

  // a_ -> tolinear -> b_ -> fromlinear -> c_;
  Transform a_;
  Transform b_;
  Transform c_;
};

class QCMSColorTransform : public ColorTransform {
 public:
  // Takes ownership of the profiles
  QCMSColorTransform(qcms_profile* from, qcms_profile* to)
      : from_(from), to_(to) {}
  ~QCMSColorTransform() override {
    qcms_profile_release(from_);
    qcms_profile_release(to_);
  }
  void transform(TriStim* colors, size_t num) override {
    CHECK(sizeof(TriStim) == sizeof(float[3]));
    // QCMS doesn't like numbers outside 0..1
    for (size_t i = 0; i < num; i++) {
      colors[i].set_x(fmin(1.0f, fmax(0.0f, colors[i].x())));
      colors[i].set_y(fmin(1.0f, fmax(0.0f, colors[i].y())));
      colors[i].set_z(fmin(1.0f, fmax(0.0f, colors[i].z())));
    }
    qcms_chain_transform(from_, to_, reinterpret_cast<float*>(colors),
                         reinterpret_cast<float*>(colors), num * 3);
  }

 private:
  qcms_profile *from_, *to_;
};

class ChainColorTransform : public ColorTransform {
 public:
  ChainColorTransform(std::unique_ptr<ColorTransform> a,
                      std::unique_ptr<ColorTransform> b)
      : a_(std::move(a)), b_(std::move(b)) {}

 private:
  void transform(TriStim* colors, size_t num) override {
    a_->transform(colors, num);
    b_->transform(colors, num);
  }
  std::unique_ptr<ColorTransform> a_;
  std::unique_ptr<ColorTransform> b_;
};

qcms_profile* GetQCMSProfileIfAvailable(const ColorSpace& color_space) {
  ICCProfile icc_profile = ICCProfile::FromColorSpace(color_space);
  if (icc_profile.GetData().empty())
    return nullptr;
  return qcms_profile_from_memory(icc_profile.GetData().data(),
                                  icc_profile.GetData().size());
}

qcms_profile* GetXYZD50Profile() {
  // QCMS is trixy, it has a datatype called qcms_CIE_xyY, but what it expects
  // is in fact not xyY color coordinates, it just wants the x/y values of the
  // primaries with Y equal to 1.0.
  qcms_CIE_xyYTRIPLE xyz;
  qcms_CIE_xyY w;
  xyz.red.x = 1.0f;
  xyz.red.y = 0.0f;
  xyz.red.Y = 1.0f;
  xyz.green.x = 0.0f;
  xyz.green.y = 1.0f;
  xyz.green.Y = 1.0f;
  xyz.blue.x = 0.0f;
  xyz.blue.y = 0.0f;
  xyz.blue.Y = 1.0f;
  w.x = 0.34567f;
  w.y = 0.35850f;
  w.Y = 1.0f;
  return qcms_profile_create_rgb_with_gamma(w, xyz, 1.0f);
}

std::unique_ptr<ColorTransform> ColorTransform::NewColorTransform(
    const ColorSpace& from,
    const ColorSpace& to,
    Intent intent) {
  qcms_profile* from_profile = GetQCMSProfileIfAvailable(from);
  qcms_profile* to_profile = GetQCMSProfileIfAvailable(to);
  if (from_profile) {
    if (to_profile) {
      return std::unique_ptr<ColorTransform>(
          new QCMSColorTransform(from_profile, to_profile));
    } else {
      return std::unique_ptr<ColorTransform>(new ChainColorTransform(
          std::unique_ptr<ColorTransform>(
              new QCMSColorTransform(from_profile, GetXYZD50Profile())),
          std::unique_ptr<ColorTransform>(new ColorSpaceToColorSpaceTransform(
              ColorSpace::CreateXYZD50(), to, intent))));
    }
  } else {
    if (to_profile) {
      return std::unique_ptr<ColorTransform>(new ChainColorTransform(
          std::unique_ptr<ColorTransform>(new ColorSpaceToColorSpaceTransform(
              from, ColorSpace::CreateXYZD50(), intent)),
          std::unique_ptr<ColorTransform>(
              new QCMSColorTransform(GetXYZD50Profile(), to_profile))));
    } else {
      return std::unique_ptr<ColorTransform>(
          new ColorSpaceToColorSpaceTransform(from, to, intent));
    }
  }
}

}  // namespace gfx
