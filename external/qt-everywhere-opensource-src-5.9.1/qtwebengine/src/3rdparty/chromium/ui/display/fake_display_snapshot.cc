// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/fake_display_snapshot.h"

#include <inttypes.h>

#include <sstream>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "third_party/re2/src/re2/re2.h"

using base::StringPiece;

namespace display {

namespace {

static const float kInchInMm = 25.4f;

// Get pixel pitch in millimeters from DPI.
float PixelPitchMmFromDPI(float dpi) {
  return (1.0f / dpi) * kInchInMm;
}

std::string ModeListString(
    const std::vector<std::unique_ptr<const ui::DisplayMode>>& modes) {
  std::stringstream stream;
  bool first = true;
  for (auto& mode : modes) {
    if (!first)
      stream << ", ";
    stream << mode->ToString();
    first = false;
  }
  return stream.str();
}

std::string DisplayConnectionTypeString(ui::DisplayConnectionType type) {
  switch (type) {
    case ui::DISPLAY_CONNECTION_TYPE_NONE:
      return "none";
    case ui::DISPLAY_CONNECTION_TYPE_UNKNOWN:
      return "unknown";
    case ui::DISPLAY_CONNECTION_TYPE_INTERNAL:
      return "internal";
    case ui::DISPLAY_CONNECTION_TYPE_VGA:
      return "vga";
    case ui::DISPLAY_CONNECTION_TYPE_HDMI:
      return "hdmi";
    case ui::DISPLAY_CONNECTION_TYPE_DVI:
      return "dvi";
    case ui::DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
      return "dp";
    case ui::DISPLAY_CONNECTION_TYPE_NETWORK:
      return "network";
    case ui::DISPLAY_CONNECTION_TYPE_VIRTUAL:
      return "virtual";
  }
  NOTREACHED();
  return "";
}

// Extracts text after specified delimiter. If the delimiter doesn't appear
// exactly once the result will be empty and the input string will be
// unmodified. Otherwise, the input string will contain the text before the
// delimiter and the result will be the text after the delimiter.
StringPiece ExtractSuffix(StringPiece* str, StringPiece delimiter) {
  std::vector<StringPiece> parts = base::SplitStringPiece(
      *str, delimiter, base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (parts.size() == 2) {
    *str = parts[0];
    return parts[1];
  }

  return StringPiece();
}

// Parses a display mode from |str| in the format HxW[%R], returning null if
// |str| is invalid.
std::unique_ptr<ui::DisplayMode> ParseDisplayMode(const std::string& str) {
  int width = 0;
  int height = 0;
  std::string refresh_rate_str;

  // Check against regex and extract values.
  if (!RE2::FullMatch(str, "(\\d+)x(\\d+)(?:%(\\d+\\.?\\d*))?", &width, &height,
                      &refresh_rate_str)) {
    LOG(ERROR) << "Invalid display mode string \"" << str << "\"";
    return nullptr;
  }

  if (width <= 0 || height <= 0) {
    LOG(ERROR) << "Resolution " << width << "x" << height << " is invalid";
    return nullptr;
  }

  // Refresh rate is optional and will be be 60 if not specified.
  double refresh_rate = 60.0f;
  if (!refresh_rate_str.empty() &&
      !base::StringToDouble(refresh_rate_str, &refresh_rate)) {
    LOG(ERROR) << "Unable to parse display mode \"" << str << "\"";
    return nullptr;
  }

  return base::MakeUnique<ui::DisplayMode>(gfx::Size(width, height), false,
                                           static_cast<float>(refresh_rate));
}

// Parses a list of alternate display modes, adding each new display mode to
// |builder|. Returns false if any of the modes are invalid.
bool HandleModes(FakeDisplaySnapshot::Builder* builder,
                 StringPiece resolutions) {
  for (const std::string& mode_str :
       base::SplitString(resolutions, ":", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    std::unique_ptr<ui::DisplayMode> mode = ParseDisplayMode(mode_str);
    if (!mode)
      return false;

    builder->AddMode(std::move(mode));
  }

  return true;
}

// Parses device DPI and updates |builder|. Returns false if an invalid DPI
// string is provided.
bool HandleDPI(FakeDisplaySnapshot::Builder* builder, StringPiece dpi) {
  if (dpi.empty())
    return true;

  int dpi_value = 0;
  if (base::StringToInt(dpi, &dpi_value)) {
    builder->SetDPI(dpi_value);
    return true;
  }

  LOG(ERROR) << "Invalid DPI string \"" << dpi << "\"";
  return false;
}

// Parses a list of display options and set each option true on |builder|.
// Returns false if any invalid options are provided. If an option appears more
// than once it will have no effect the second time.
bool HandleOptions(FakeDisplaySnapshot::Builder* builder, StringPiece options) {
  for (size_t i = 0; i < options.size(); ++i) {
    switch (options[i]) {
      case 'o':
        builder->SetHasOverscan(true);
        break;
      case 'c':
        builder->SetHasColorCorrectionMatrix(true);
        break;
      case 'a':
        builder->SetIsAspectPerservingScaling(true);
        break;
      case 'i':
        builder->SetType(ui::DISPLAY_CONNECTION_TYPE_INTERNAL);
        break;
      default:
        LOG(ERROR) << "Invalid option specifier \"" << options[i] << "\"";
        return false;
    }
  }

  return true;
}

}  // namespace

using Builder = FakeDisplaySnapshot::Builder;

Builder::Builder() {}

Builder::~Builder() {}

std::unique_ptr<FakeDisplaySnapshot> Builder::Build() {
  if (modes_.empty() || id_ == Display::kInvalidDisplayID) {
    NOTREACHED() << "Display modes or display ID missing";
    return nullptr;
  }

  // If there is no native mode set, use the first display mode.
  if (!native_mode_)
    native_mode_ = modes_.back().get();

  // Calculate physical size to match set DPI.
  gfx::Size physical_size =
      gfx::ScaleToRoundedSize(native_mode_->size(), PixelPitchMmFromDPI(dpi_));

  return base::MakeUnique<FakeDisplaySnapshot>(
      id_, origin_, physical_size, type_, is_aspect_preserving_scaling_,
      has_overscan_, has_color_correction_matrix_, name_, product_id_,
      std::move(modes_), current_mode_, native_mode_);
}

Builder& Builder::SetId(int64_t id) {
  id_ = id;
  return *this;
}

Builder& Builder::SetNativeMode(const gfx::Size& size) {
  native_mode_ = AddOrFindDisplayMode(size);
  return *this;
}

Builder& Builder::SetNativeMode(std::unique_ptr<ui::DisplayMode> mode) {
  native_mode_ = AddOrFindDisplayMode(std::move(mode));
  return *this;
}

Builder& Builder::SetCurrentMode(const gfx::Size& size) {
  current_mode_ = AddOrFindDisplayMode(size);
  return *this;
}

Builder& Builder::SetCurrentMode(std::unique_ptr<ui::DisplayMode> mode) {
  current_mode_ = AddOrFindDisplayMode(std::move(mode));
  return *this;
}

Builder& Builder::AddMode(const gfx::Size& size) {
  AddOrFindDisplayMode(size);
  return *this;
}

Builder& Builder::AddMode(std::unique_ptr<ui::DisplayMode> mode) {
  AddOrFindDisplayMode(std::move(mode));
  return *this;
}

Builder& Builder::SetOrigin(const gfx::Point& origin) {
  origin_ = origin;
  return *this;
}

Builder& Builder::SetType(ui::DisplayConnectionType type) {
  type_ = type;
  return *this;
}

Builder& Builder::SetIsAspectPerservingScaling(bool val) {
  is_aspect_preserving_scaling_ = val;
  return *this;
}

Builder& Builder::SetHasOverscan(bool has_overscan) {
  has_overscan_ = has_overscan;
  return *this;
}

Builder& Builder::SetHasColorCorrectionMatrix(bool val) {
  has_color_correction_matrix_ = val;
  return *this;
}

Builder& Builder::SetName(const std::string& name) {
  name_ = name;
  return *this;
}

Builder& Builder::SetProductId(int64_t product_id) {
  product_id_ = product_id;
  return *this;
}

Builder& Builder::SetDPI(int dpi) {
  dpi_ = static_cast<float>(dpi);
  return *this;
}

Builder& Builder::SetLowDPI() {
  return SetDPI(96);
}

Builder& Builder::SetHighDPI() {
  return SetDPI(326);  // Retina-ish.
}

const ui::DisplayMode* Builder::AddOrFindDisplayMode(const gfx::Size& size) {
  for (auto& mode : modes_) {
    if (mode->size() == size)
      return mode.get();
  }

  // Not found, insert a mode with the size and return.
  modes_.push_back(base::MakeUnique<ui::DisplayMode>(size, false, 60.0f));
  return modes_.back().get();
}

const ui::DisplayMode* Builder::AddOrFindDisplayMode(
    std::unique_ptr<ui::DisplayMode> mode) {
  for (auto& existing : modes_) {
    if (mode->size() == existing->size() &&
        mode->is_interlaced() == existing->is_interlaced() &&
        mode->refresh_rate() == existing->refresh_rate())
      return existing.get();
  }

  // Not found, insert mode and return.
  modes_.push_back(std::move(mode));
  return modes_.back().get();
}

FakeDisplaySnapshot::FakeDisplaySnapshot(int64_t display_id,
                                         const gfx::Point& origin,
                                         const gfx::Size& physical_size,
                                         ui::DisplayConnectionType type,
                                         bool is_aspect_preserving_scaling,
                                         bool has_overscan,
                                         bool has_color_correction_matrix,
                                         std::string display_name,
                                         int64_t product_id,
                                         DisplayModeList modes,
                                         const ui::DisplayMode* current_mode,
                                         const ui::DisplayMode* native_mode)
    : DisplaySnapshot(display_id,
                      origin,
                      physical_size,
                      type,
                      is_aspect_preserving_scaling,
                      has_overscan,
                      has_color_correction_matrix,
                      display_name,
                      base::FilePath(),
                      std::move(modes),
                      std::vector<uint8_t>(),
                      current_mode,
                      native_mode) {
  product_id_ = product_id;
}

FakeDisplaySnapshot::~FakeDisplaySnapshot() {}

// static
std::unique_ptr<ui::DisplaySnapshot> FakeDisplaySnapshot::CreateFromSpec(
    int64_t id,
    const std::string& spec) {
  StringPiece leftover(spec);

  // Cut off end of string at each delimiter to split.
  StringPiece options = ExtractSuffix(&leftover, "/");
  StringPiece dpi = ExtractSuffix(&leftover, "^");
  StringPiece resolutions = ExtractSuffix(&leftover, "#");

  // Leftovers should be just the native mode at this point.
  std::unique_ptr<ui::DisplayMode> native_mode =
      ParseDisplayMode(leftover.as_string());

  // Fail without valid native mode.
  if (!native_mode)
    return nullptr;

  FakeDisplaySnapshot::Builder builder;
  builder.SetId(id).SetNativeMode(std::move(native_mode));
  builder.SetName(base::StringPrintf("Fake Display %" PRId64, id));

  if (!HandleModes(&builder, resolutions) || !HandleDPI(&builder, dpi) ||
      !HandleOptions(&builder, options))
    return nullptr;

  return builder.Build();
}

std::string FakeDisplaySnapshot::ToString() const {
  return base::StringPrintf(
      "id=%" PRId64
      " current_mode=%s native_mode=%s origin=%s"
      " physical_size=%s, type=%s name=\"%s\" modes=(%s)",
      display_id_,
      current_mode_ ? current_mode_->ToString().c_str() : "nullptr",
      native_mode_ ? native_mode_->ToString().c_str() : "nullptr",
      origin_.ToString().c_str(), physical_size_.ToString().c_str(),
      DisplayConnectionTypeString(type_).c_str(), display_name_.c_str(),
      ModeListString(modes_).c_str());
}

}  // namespace display
