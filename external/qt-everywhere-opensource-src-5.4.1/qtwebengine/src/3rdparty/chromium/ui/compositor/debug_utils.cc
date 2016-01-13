// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES // For VC++ to get M_PI. This has to be first.

#include "ui/compositor/debug_utils.h"

#include <cmath>
#include <iomanip>
#include <ostream>
#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/point.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/transform.h"

using base::UTF8ToWide;

namespace ui {

namespace {

void PrintLayerHierarchyImp(const Layer* layer,
                            int indent,
                            gfx::Point mouse_location,
                            std::wostringstream* out) {
  std::string indent_str(indent, ' ');

  layer->transform().TransformPointReverse(&mouse_location);
  bool mouse_inside_layer_bounds = layer->bounds().Contains(mouse_location);
  mouse_location.Offset(-layer->bounds().x(), -layer->bounds().y());

  *out << UTF8ToWide(indent_str);
  if (mouse_inside_layer_bounds)
    *out << L'*';
  else
    *out << L' ';

  *out << UTF8ToWide(layer->name()) << L' ' << layer;

  switch (layer->type()) {
    case ui::LAYER_NOT_DRAWN:
      *out << L" not_drawn";
      break;
    case ui::LAYER_TEXTURED:
      *out << L" textured";
      if (layer->fills_bounds_opaquely())
        *out << L" opaque";
      break;
    case ui::LAYER_SOLID_COLOR:
      *out << L" solid";
      break;
  }

  if (!layer->visible())
    *out << L" !visible";

  std::string property_indent_str(indent+3, ' ');
  *out << L'\n' << UTF8ToWide(property_indent_str);
  *out << L"bounds: " << layer->bounds().x() << L',' << layer->bounds().y();
  *out << L' ' << layer->bounds().width() << L'x' << layer->bounds().height();

  if (layer->opacity() != 1.0f) {
    *out << L'\n' << UTF8ToWide(property_indent_str);
    *out << L"opacity: " << std::setprecision(2) << layer->opacity();
  }

  gfx::DecomposedTransform decomp;
  if (!layer->transform().IsIdentity() &&
      gfx::DecomposeTransform(&decomp, layer->transform())) {
    *out << L'\n' << UTF8ToWide(property_indent_str);
    *out << L"translation: " << std::fixed << decomp.translate[0];
    *out << L", " << decomp.translate[1];

    *out << L'\n' << UTF8ToWide(property_indent_str);
    *out << L"rotation: ";
    *out << std::acos(decomp.quaternion[3]) * 360.0 / M_PI;

    *out << L'\n' << UTF8ToWide(property_indent_str);
    *out << L"scale: " << decomp.scale[0];
    *out << L", " << decomp.scale[1];
  }

  *out << L'\n';

  for (size_t i = 0, count = layer->children().size(); i < count; ++i) {
    PrintLayerHierarchyImp(
        layer->children()[i], indent + 3, mouse_location, out);
  }
}

}  // namespace

void PrintLayerHierarchy(const Layer* layer, gfx::Point mouse_location) {
  std::wostringstream out;
  out << L"Layer hierarchy:\n";
  PrintLayerHierarchyImp(layer, 0, mouse_location, &out);
  // Error so logs can be collected from end-users.
  LOG(ERROR) << out.str();
}

}  // namespace ui
