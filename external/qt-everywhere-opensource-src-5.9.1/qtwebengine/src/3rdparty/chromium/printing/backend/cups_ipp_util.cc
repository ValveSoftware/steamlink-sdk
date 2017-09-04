// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_ipp_util.h"

#include <cups/cups.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "printing/backend/cups_printer.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/units.h"

namespace printing {

// property names
const char kIppCollate[] = "sheet-collate";  // RFC 3381
const char kIppCopies[] = CUPS_COPIES;
const char kIppColor[] = CUPS_PRINT_COLOR_MODE;
const char kIppMedia[] = CUPS_MEDIA;
const char kIppDuplex[] = CUPS_SIDES;

// collation values
const char kCollated[] = "collated";
const char kUncollated[] = "uncollated";

namespace {

const int kMicronsPerMM = 1000;
const double kMMPerInch = 25.4;
const double kMicronsPerInch = kMMPerInch * kMicronsPerMM;

enum Unit {
  INCHES,
  MILLIMETERS,
};

struct ColorMap {
  const char* color;
  ColorModel model;
};

const ColorMap kColorList[]{
    {CUPS_PRINT_COLOR_MODE_COLOR, COLORMODE_COLOR},
    {CUPS_PRINT_COLOR_MODE_MONOCHROME, COLORMODE_MONOCHROME},
};

ColorModel ColorModelFromIppColor(base::StringPiece ippColor) {
  for (const ColorMap& color : kColorList) {
    if (ippColor.compare(color.color) == 0) {
      return color.model;
    }
  }

  return UNKNOWN_COLOR_MODEL;
}

bool PrinterSupportsValue(const CupsOptionProvider& printer,
                          base::StringPiece name,
                          base::StringPiece value) {
  std::vector<base::StringPiece> values =
      printer.GetSupportedOptionValueStrings(name);
  return base::ContainsValue(values, value);
}

DuplexMode PrinterDefaultDuplex(const CupsOptionProvider& printer) {
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppDuplex);
  if (!attr)
    return UNKNOWN_DUPLEX_MODE;

  const char* value = ippGetString(attr, 0, nullptr);
  if (base::EqualsCaseInsensitiveASCII(value, CUPS_SIDES_ONE_SIDED))
    return SIMPLEX;

  if (base::EqualsCaseInsensitiveASCII(value, CUPS_SIDES_TWO_SIDED_PORTRAIT))
    return LONG_EDGE;

  if (base::EqualsCaseInsensitiveASCII(value, CUPS_SIDES_TWO_SIDED_LANDSCAPE))
    return SHORT_EDGE;

  return UNKNOWN_DUPLEX_MODE;
}

gfx::Size DimensionsToMicrons(base::StringPiece value) {
  Unit unit;
  base::StringPiece dims;
  size_t unit_position;
  if ((unit_position = value.find("mm")) != base::StringPiece::npos) {
    unit = MILLIMETERS;
    dims = value.substr(0, unit_position);
  } else if ((unit_position = value.find("in")) != base::StringPiece::npos) {
    unit = INCHES;
    dims = value.substr(0, unit_position);
  } else {
    LOG(WARNING) << "Could not parse paper dimensions";
    return {0, 0};
  }

  std::vector<std::string> pieces = base::SplitString(
      dims, "x", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  double width;
  double height;

  if (pieces.size() != 2 || !base::StringToDouble(pieces[0], &width) ||
      !base::StringToDouble(pieces[1], &height)) {
    return {0, 0};
  }

  int width_microns;
  int height_microns;

  switch (unit) {
    case MILLIMETERS:
      width_microns = width * kMicronsPerMM;
      height_microns = height * kMicronsPerMM;
      break;
    case INCHES:
      width_microns = width * kMicronsPerInch;
      height_microns = height * kMicronsPerInch;
      break;
    default:
      NOTREACHED();
      break;
  }

  return gfx::Size{width_microns, height_microns};
}

PrinterSemanticCapsAndDefaults::Paper ParsePaper(base::StringPiece value) {
  // <name>_<width>x<height>{in,mm}
  // e.g. na_letter_8.5x11in, iso_a4_210x297mm

  std::vector<base::StringPiece> pieces = base::SplitStringPiece(
      value, "_", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // we expect at least a display string and a dimension string
  if (pieces.size() < 2)
    return PrinterSemanticCapsAndDefaults::Paper();

  base::StringPiece dimensions = pieces.back();

  std::string display = pieces[0].as_string();
  for (size_t i = 1; i <= pieces.size() - 2; ++i) {
    display.append(" ");
    pieces[i].AppendToString(&display);
  }

  PrinterSemanticCapsAndDefaults::Paper paper;
  paper.display_name = display;
  paper.vendor_id = value.as_string();
  paper.size_um = DimensionsToMicrons(dimensions);

  return paper;
}

void ExtractColor(const CupsOptionProvider& printer,
                  PrinterSemanticCapsAndDefaults* printer_info) {
  printer_info->bw_model = UNKNOWN_COLOR_MODEL;
  printer_info->color_model = UNKNOWN_COLOR_MODEL;

  // color and b&w
  std::vector<ColorModel> color_models = SupportedColorModels(printer);
  for (ColorModel color : color_models) {
    switch (color) {
      case COLORMODE_COLOR:
        printer_info->color_model = COLORMODE_COLOR;
        break;
      case COLORMODE_MONOCHROME:
        printer_info->bw_model = COLORMODE_MONOCHROME;
        break;
      default:
        // value not needed
        break;
    }
  }

  // changeable
  printer_info->color_changeable =
      (printer_info->color_model != UNKNOWN_COLOR_MODEL &&
       printer_info->bw_model != UNKNOWN_COLOR_MODEL);

  // default color
  printer_info->color_default = DefaultColorModel(printer) == COLORMODE_COLOR;
}

void ExtractCopies(const CupsOptionProvider& printer,
                   PrinterSemanticCapsAndDefaults* printer_info) {
  // copies
  int upper_bound;
  int lower_bound;
  CopiesRange(printer, &lower_bound, &upper_bound);
  printer_info->copies_capable = (lower_bound != -1) && (upper_bound >= 2);
}

}  // namespace

ColorModel DefaultColorModel(const CupsOptionProvider& printer) {
  // default color
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppColor);
  if (!attr)
    return UNKNOWN_COLOR_MODEL;

  return ColorModelFromIppColor(ippGetString(attr, 0, nullptr));
}

std::vector<ColorModel> SupportedColorModels(
    const CupsOptionProvider& printer) {
  std::vector<ColorModel> colors;

  std::vector<base::StringPiece> color_modes =
      printer.GetSupportedOptionValueStrings(kIppColor);

  for (base::StringPiece color : color_modes) {
    ColorModel color_model = ColorModelFromIppColor(color);
    if (color_model != UNKNOWN_COLOR_MODEL) {
      colors.push_back(color_model);
    }
  }

  return colors;
}

PrinterSemanticCapsAndDefaults::Paper DefaultPaper(
    const CupsOptionProvider& printer) {
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppMedia);
  if (!attr)
    return PrinterSemanticCapsAndDefaults::Paper();

  return ParsePaper(ippGetString(attr, 0, nullptr));
}

std::vector<PrinterSemanticCapsAndDefaults::Paper> SupportedPapers(
    const CupsOptionProvider& printer) {
  std::vector<base::StringPiece> papers =
      printer.GetSupportedOptionValueStrings(kIppMedia);
  std::vector<PrinterSemanticCapsAndDefaults::Paper> parsed_papers;
  for (base::StringPiece paper : papers) {
    parsed_papers.push_back(ParsePaper(paper));
  }

  return parsed_papers;
}

void CopiesRange(const CupsOptionProvider& printer,
                 int* lower_bound,
                 int* upper_bound) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(kIppCopies);
  if (!attr) {
    *lower_bound = -1;
    *upper_bound = -1;
  }

  *lower_bound = ippGetRange(attr, 0, upper_bound);
}

bool CollateCapable(const CupsOptionProvider& printer) {
  std::vector<base::StringPiece> values =
      printer.GetSupportedOptionValueStrings(kIppCollate);
  auto iter = std::find(values.begin(), values.end(), kCollated);
  return iter != values.end();
}

bool CollateDefault(const CupsOptionProvider& printer) {
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppCollate);
  if (!attr)
    return false;

  base::StringPiece name = ippGetString(attr, 0, nullptr);
  return name.compare(kCollated) == 0;
}

void CapsAndDefaultsFromPrinter(const CupsOptionProvider& printer,
                                PrinterSemanticCapsAndDefaults* printer_info) {
  // duplex
  printer_info->duplex_default = PrinterDefaultDuplex(printer);
  printer_info->duplex_capable =
      PrinterSupportsValue(printer, kIppDuplex, CUPS_SIDES_TWO_SIDED_PORTRAIT);

  // collate
  printer_info->collate_default = CollateDefault(printer);
  printer_info->collate_capable = CollateCapable(printer);

  // paper
  printer_info->default_paper = DefaultPaper(printer);
  printer_info->papers = SupportedPapers(printer);

  ExtractCopies(printer, printer_info);
  ExtractColor(printer, printer_info);

  // TODO(skau): Add dpi and default_dpi
}

}  //  namespace printing
