// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/image_util.h"

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/utils/SkParse.h"
#include "ui/gfx/color_utils.h"

namespace extensions {
namespace image_util {

bool ParseCssColorString(const std::string& color_string, SkColor* result) {
  if (color_string.empty())
    return false;
  if (color_string[0] == '#')
    return ParseHexColorString(color_string, result);
  if (base::StartsWith(color_string, "hsl", base::CompareCase::SENSITIVE))
    return ParseHslColorString(color_string, result);
  if (base::StartsWith(color_string, "rgb", base::CompareCase::SENSITIVE)) {
    NOTIMPLEMENTED();
    return false;
  }
  if (SkParse::FindNamedColor(color_string.c_str(), color_string.size(),
                              result) != nullptr) {
    return true;
  }

  return false;
}

bool ParseHexColorString(const std::string& color_string, SkColor* result) {
  std::string formatted_color;
  // Check the string for incorrect formatting.
  if (color_string.empty() || color_string[0] != '#')
    return false;

  // Convert the string from #FFF format to #FFFFFF format.
  if (color_string.length() == 4) {
    for (size_t i = 1; i < 4; ++i) {
      formatted_color += color_string[i];
      formatted_color += color_string[i];
    }
  } else if (color_string.length() == 7) {
    formatted_color = color_string.substr(1, 6);
  } else {
    return false;
  }

  // Convert the string to an integer and make sure it is in the correct value
  // range.
  std::vector<uint8_t> color_bytes;
  if (!base::HexStringToBytes(formatted_color, &color_bytes))
    return false;

  *result = SkColorSetARGB(255, color_bytes[0], color_bytes[1], color_bytes[2]);
  return true;
}

std::string GenerateHexColorString(SkColor color) {
  return base::StringPrintf("#%02X%02X%02X", SkColorGetR(color),
                            SkColorGetG(color), SkColorGetB(color));
}

bool ParseHslColorString(const std::string& color_string, SkColor* result) {
  // http://www.w3.org/wiki/CSS/Properties/color/HSL#The_format_of_the_HSL_Value
  // The CSS3 specification defines the format of a HSL color as
  // hsl(<number>, <percent>, <percent>)
  // and with alpha, the format is
  // hsla(<number>, <percent>, <percent>, <number>)
  // e.g.: hsl(120, 100%, 50%), hsla(120, 100%, 50%, 0.5);
  double hue = 0.0;
  double saturation = 0.0;
  double lightness = 0.0;
  // 'hsl()' has '1' alpha value implicitly.
  double alpha = 1.0;
  if (!re2::RE2::FullMatch(color_string,
                           "hsl\\((-?[\\d.]+),\\s*([\\d.]+)%,\\s*([\\d.]+)%\\)",
                           &hue, &saturation, &lightness) &&
      !re2::RE2::FullMatch(
          color_string,
          "hsla\\((-?[\\d.]+),\\s*([\\d.]+)%,\\s*([\\d.]+)%,\\s*([\\d.]+)\\)",
          &hue, &saturation, &lightness, &alpha)) {
    return false;
  }

  color_utils::HSL hsl;
  // Normalize the value between 0.0 and 1.0.
  hsl.h = (((static_cast<int>(hue) % 360) + 360) % 360) / 360.0;
  hsl.s = std::max(0.0, std::min(100.0, saturation)) / 100.0;
  hsl.l = std::max(0.0, std::min(100.0, lightness)) / 100.0;

  SkAlpha sk_alpha = std::max(0.0, std::min(1.0, alpha)) * 255;

  *result = color_utils::HSLToSkColor(hsl, sk_alpha);
  return true;
}

}  // namespace image_util
}  // namespace extensions
