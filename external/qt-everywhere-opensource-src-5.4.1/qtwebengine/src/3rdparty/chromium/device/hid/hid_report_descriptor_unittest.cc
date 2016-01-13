// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "device/hid/hid_report_descriptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace testing;

namespace device {

namespace {

std::ostream& operator<<(std::ostream& os,
                         const HidUsageAndPage::Page& usage_page) {
  switch (usage_page) {
    case HidUsageAndPage::kPageUndefined:
      os << "Undefined";
      break;
    case HidUsageAndPage::kPageGenericDesktop:
      os << "Generic Desktop";
      break;
    case HidUsageAndPage::kPageSimulation:
      os << "Simulation";
      break;
    case HidUsageAndPage::kPageVirtualReality:
      os << "Virtual Reality";
      break;
    case HidUsageAndPage::kPageSport:
      os << "Sport";
      break;
    case HidUsageAndPage::kPageGame:
      os << "Game";
      break;
    case HidUsageAndPage::kPageKeyboard:
      os << "Keyboard";
      break;
    case HidUsageAndPage::kPageLed:
      os << "Led";
      break;
    case HidUsageAndPage::kPageButton:
      os << "Button";
      break;
    case HidUsageAndPage::kPageOrdinal:
      os << "Ordinal";
      break;
    case HidUsageAndPage::kPageTelephony:
      os << "Telephony";
      break;
    case HidUsageAndPage::kPageConsumer:
      os << "Consumer";
      break;
    case HidUsageAndPage::kPageDigitizer:
      os << "Digitizer";
      break;
    case HidUsageAndPage::kPagePidPage:
      os << "Pid Page";
      break;
    case HidUsageAndPage::kPageUnicode:
      os << "Unicode";
      break;
    case HidUsageAndPage::kPageAlphanumericDisplay:
      os << "Alphanumeric Display";
      break;
    case HidUsageAndPage::kPageMedicalInstruments:
      os << "Medical Instruments";
      break;
    case HidUsageAndPage::kPageMonitor0:
      os << "Monitor 0";
      break;
    case HidUsageAndPage::kPageMonitor1:
      os << "Monitor 1";
      break;
    case HidUsageAndPage::kPageMonitor2:
      os << "Monitor 2";
      break;
    case HidUsageAndPage::kPageMonitor3:
      os << "Monitor 3";
      break;
    case HidUsageAndPage::kPagePower0:
      os << "Power 0";
      break;
    case HidUsageAndPage::kPagePower1:
      os << "Power 1";
      break;
    case HidUsageAndPage::kPagePower2:
      os << "Power 2";
      break;
    case HidUsageAndPage::kPagePower3:
      os << "Power 3";
      break;
    case HidUsageAndPage::kPageBarCodeScanner:
      os << "Bar Code Scanner";
      break;
    case HidUsageAndPage::kPageScale:
      os << "Scale";
      break;
    case HidUsageAndPage::kPageMagneticStripeReader:
      os << "Magnetic Stripe Reader";
      break;
    case HidUsageAndPage::kPageReservedPointOfSale:
      os << "Reserved Point Of Sale";
      break;
    case HidUsageAndPage::kPageCameraControl:
      os << "Camera Control";
      break;
    case HidUsageAndPage::kPageArcade:
      os << "Arcade";
      break;
    case HidUsageAndPage::kPageVendor:
      os << "Vendor";
      break;
    case HidUsageAndPage::kPageMediaCenter:
      os << "Media Center";
      break;
    default:
      NOTREACHED();
      break;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const HidUsageAndPage& usage_and_page) {
  os << "Usage Page: " << usage_and_page.usage_page << ", Usage: "
     << "0x" << std::hex << std::uppercase << usage_and_page.usage;
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const HidReportDescriptorItem::Tag& tag) {
  switch (tag) {
    case HidReportDescriptorItem::kTagDefault:
      os << "Default";
      break;
    case HidReportDescriptorItem::kTagInput:
      os << "Input";
      break;
    case HidReportDescriptorItem::kTagOutput:
      os << "Output";
      break;
    case HidReportDescriptorItem::kTagFeature:
      os << "Feature";
      break;
    case HidReportDescriptorItem::kTagCollection:
      os << "Collection";
      break;
    case HidReportDescriptorItem::kTagEndCollection:
      os << "End Collection";
      break;
    case HidReportDescriptorItem::kTagUsagePage:
      os << "Usage Page";
      break;
    case HidReportDescriptorItem::kTagLogicalMinimum:
      os << "Logical Minimum";
      break;
    case HidReportDescriptorItem::kTagLogicalMaximum:
      os << "Logical Maximum";
      break;
    case HidReportDescriptorItem::kTagPhysicalMinimum:
      os << "Physical Minimum";
      break;
    case HidReportDescriptorItem::kTagPhysicalMaximum:
      os << "Physical Maximum";
      break;
    case HidReportDescriptorItem::kTagUnitExponent:
      os << "Unit Exponent";
      break;
    case HidReportDescriptorItem::kTagUnit:
      os << "Unit";
      break;
    case HidReportDescriptorItem::kTagReportSize:
      os << "Report Size";
      break;
    case HidReportDescriptorItem::kTagReportId:
      os << "Report ID";
      break;
    case HidReportDescriptorItem::kTagReportCount:
      os << "Report Count";
      break;
    case HidReportDescriptorItem::kTagPush:
      os << "Push";
      break;
    case HidReportDescriptorItem::kTagPop:
      os << "Pop";
      break;
    case HidReportDescriptorItem::kTagUsage:
      os << "Usage";
      break;
    case HidReportDescriptorItem::kTagUsageMinimum:
      os << "Usage Minimum";
      break;
    case HidReportDescriptorItem::kTagUsageMaximum:
      os << "Usage Maximum";
      break;
    case HidReportDescriptorItem::kTagDesignatorIndex:
      os << "Designator Index";
      break;
    case HidReportDescriptorItem::kTagDesignatorMinimum:
      os << "Designator Minimum";
      break;
    case HidReportDescriptorItem::kTagDesignatorMaximum:
      os << "Designator Maximum";
      break;
    case HidReportDescriptorItem::kTagStringIndex:
      os << "String Index";
      break;
    case HidReportDescriptorItem::kTagStringMinimum:
      os << "String Minimum";
      break;
    case HidReportDescriptorItem::kTagStringMaximum:
      os << "String Maximum";
      break;
    case HidReportDescriptorItem::kTagDelimiter:
      os << "Delimeter";
      break;
    case HidReportDescriptorItem::kTagLong:
      os << "Long";
      break;
    default:
      NOTREACHED();
      break;
  }

  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const HidReportDescriptorItem::ReportInfo& data) {
  if (data.data_or_constant)
    os << "Con";
  else
    os << "Dat";
  if (data.array_or_variable)
    os << "|Arr";
  else
    os << "|Var";
  if (data.absolute_or_relative)
    os << "|Abs";
  else
    os << "|Rel";
  if (data.wrap)
    os << "|Wrp";
  else
    os << "|NoWrp";
  if (data.linear)
    os << "|NoLin";
  else
    os << "|Lin";
  if (data.preferred)
    os << "|NoPrf";
  else
    os << "|Prf";
  if (data.null)
    os << "|Null";
  else
    os << "|NoNull";
  if (data.bit_field_or_buffer)
    os << "|Buff";
  else
    os << "|BitF";
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const HidReportDescriptorItem::CollectionType& type) {
  switch (type) {
    case HidReportDescriptorItem::kCollectionTypePhysical:
      os << "Physical";
      break;
    case HidReportDescriptorItem::kCollectionTypeApplication:
      os << "Application";
      break;
    case HidReportDescriptorItem::kCollectionTypeLogical:
      os << "Logical";
      break;
    case HidReportDescriptorItem::kCollectionTypeReport:
      os << "Report";
      break;
    case HidReportDescriptorItem::kCollectionTypeNamedArray:
      os << "Named Array";
      break;
    case HidReportDescriptorItem::kCollectionTypeUsageSwitch:
      os << "Usage Switch";
      break;
    case HidReportDescriptorItem::kCollectionTypeUsageModifier:
      os << "Usage Modifier";
      break;
    case HidReportDescriptorItem::kCollectionTypeReserved:
      os << "Reserved";
      break;
    case HidReportDescriptorItem::kCollectionTypeVendor:
      os << "Vendor";
      break;
    default:
      NOTREACHED();
      break;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const HidReportDescriptorItem& item) {
  HidReportDescriptorItem::Tag item_tag = item.tag();
  uint32_t data = item.GetShortData();

  std::ostringstream sstr;
  sstr << item_tag;
  sstr << " (";

  long pos = sstr.tellp();
  switch (item_tag) {
    case HidReportDescriptorItem::kTagDefault:
    case HidReportDescriptorItem::kTagEndCollection:
    case HidReportDescriptorItem::kTagPush:
    case HidReportDescriptorItem::kTagPop:
    case HidReportDescriptorItem::kTagLong:
      break;

    case HidReportDescriptorItem::kTagCollection:
      sstr << HidReportDescriptorItem::GetCollectionTypeFromValue(data);
      break;

    case HidReportDescriptorItem::kTagInput:
    case HidReportDescriptorItem::kTagOutput:
    case HidReportDescriptorItem::kTagFeature:
      sstr << (HidReportDescriptorItem::ReportInfo&)data;
      break;

    case HidReportDescriptorItem::kTagUsagePage:
      sstr << (HidUsageAndPage::Page)data;
      break;

    case HidReportDescriptorItem::kTagUsage:
    case HidReportDescriptorItem::kTagReportId:
      sstr << "0x" << std::hex << std::uppercase << data;
      break;

    default:
      sstr << data;
      break;
  }
  if (pos == sstr.tellp()) {
    std::string str = sstr.str();
    str.erase(str.end() - 2, str.end());
    os << str;
  } else {
    os << sstr.str() << ")";
  }

  return os;
}

const char kIndentStep[] = " ";

std::ostream& operator<<(std::ostream& os,
                         const HidReportDescriptor& descriptor) {
  for (std::vector<linked_ptr<HidReportDescriptorItem> >::const_iterator
           items_iter = descriptor.items().begin();
       items_iter != descriptor.items().end();
       ++items_iter) {
    linked_ptr<HidReportDescriptorItem> item = *items_iter;
    size_t indentLevel = item->GetDepth();
    for (size_t i = 0; i < indentLevel; i++)
      os << kIndentStep;
    os << *item.get() << std::endl;
  }
  return os;
}

// See 'E.6 Report Descriptor (Keyboard)'
// in HID specifications (v1.11)
const uint8_t kKeyboard[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29,
    0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05, 0x75, 0x01, 0x05,
    0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03,
    0x91, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05,
    0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0};

// See 'E.10 Report Descriptor (Mouse)'
// in HID specifications (v1.11)
const uint8_t kMouse[] = {0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1,
                          0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00,
                          0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95,
                          0x01, 0x75, 0x05, 0x81, 0x01, 0x05, 0x01, 0x09, 0x30,
                          0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95,
                          0x02, 0x81, 0x06, 0xC0, 0xC0};

const uint8_t kLogitechUnifyingReceiver[] = {
    0x06, 0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x10, 0x75, 0x08,
    0x95, 0x06, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x09, 0x01, 0x81, 0x00,
    0x09, 0x01, 0x91, 0x00, 0xC0, 0x06, 0x00, 0xFF, 0x09, 0x02, 0xA1,
    0x01, 0x85, 0x11, 0x75, 0x08, 0x95, 0x13, 0x15, 0x00, 0x26, 0xFF,
    0x00, 0x09, 0x02, 0x81, 0x00, 0x09, 0x02, 0x91, 0x00, 0xC0, 0x06,
    0x00, 0xFF, 0x09, 0x04, 0xA1, 0x01, 0x85, 0x20, 0x75, 0x08, 0x95,
    0x0E, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x09, 0x41, 0x81, 0x00, 0x09,
    0x41, 0x91, 0x00, 0x85, 0x21, 0x95, 0x1F, 0x15, 0x00, 0x26, 0xFF,
    0x00, 0x09, 0x42, 0x81, 0x00, 0x09, 0x42, 0x91, 0x00, 0xC0};

}  // namespace

class HidReportDescriptorTest : public testing::Test {

 protected:
  virtual void SetUp() OVERRIDE { descriptor_ = NULL; }

  virtual void TearDown() OVERRIDE {
    if (descriptor_) {
      delete descriptor_;
    }
  }

 public:
  void ParseDescriptor(const std::string& expected,
                       const uint8_t* bytes,
                       size_t size) {
    descriptor_ = new HidReportDescriptor(bytes, size);

    std::stringstream actual;
    actual << *descriptor_;

    std::cout << "HID report descriptor:" << std::endl;
    std::cout << actual.str();

    // TODO(jracle@logitech.com): refactor string comparison in favor of
    // testing individual fields.
    ASSERT_EQ(expected, actual.str());
  }

  void GetTopLevelCollections(const std::vector<HidUsageAndPage>& expected,
                              const uint8_t* bytes,
                              size_t size) {
    descriptor_ = new HidReportDescriptor(bytes, size);

    std::vector<HidUsageAndPage> actual;
    descriptor_->GetTopLevelCollections(&actual);

    std::cout << "HID top-level collections:" << std::endl;
    for (std::vector<HidUsageAndPage>::const_iterator iter = actual.begin();
         iter != actual.end();
         ++iter) {
      std::cout << *iter << std::endl;
    }

    ASSERT_THAT(actual, ContainerEq(expected));
  }

 private:
  HidReportDescriptor* descriptor_;
};

TEST_F(HidReportDescriptorTest, ParseDescriptor_Keyboard) {
  const char expected[] = {
      "Usage Page (Generic Desktop)\n"
      "Usage (0x6)\n"
      "Collection (Physical)\n"
      " Usage Page (Keyboard)\n"
      " Usage Minimum (224)\n"
      " Usage Maximum (231)\n"
      " Logical Minimum (0)\n"
      " Logical Maximum (1)\n"
      " Report Size (1)\n"
      " Report Count (8)\n"
      " Input (Dat|Arr|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      " Report Count (1)\n"
      " Report Size (8)\n"
      " Input (Con|Var|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      " Report Count (5)\n"
      " Report Size (1)\n"
      " Usage Page (Led)\n"
      " Usage Minimum (1)\n"
      " Usage Maximum (5)\n"
      " Output (Dat|Arr|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      " Report Count (1)\n"
      " Report Size (3)\n"
      " Output (Con|Var|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      " Report Count (6)\n"
      " Report Size (8)\n"
      " Logical Minimum (0)\n"
      " Logical Maximum (101)\n"
      " Usage Page (Keyboard)\n"
      " Usage Minimum (0)\n"
      " Usage Maximum (101)\n"
      " Input (Dat|Var|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      "End Collection\n"};

  ParseDescriptor(std::string(expected), kKeyboard, sizeof(kKeyboard));
}

TEST_F(HidReportDescriptorTest, TopLevelCollections_Keyboard) {
  HidUsageAndPage expected[] = {
      HidUsageAndPage(0x06, HidUsageAndPage::kPageGenericDesktop)};

  GetTopLevelCollections(std::vector<HidUsageAndPage>(
                             expected, expected + ARRAYSIZE_UNSAFE(expected)),
                         kKeyboard,
                         sizeof(kKeyboard));
}

TEST_F(HidReportDescriptorTest, ParseDescriptor_Mouse) {
  const char expected[] = {
      "Usage Page (Generic Desktop)\n"
      "Usage (0x2)\n"
      "Collection (Physical)\n"
      " Usage (0x1)\n"
      " Collection (Physical)\n"
      "  Usage Page (Button)\n"
      "  Usage Minimum (1)\n"
      "  Usage Maximum (3)\n"
      "  Logical Minimum (0)\n"
      "  Logical Maximum (1)\n"
      "  Report Count (3)\n"
      "  Report Size (1)\n"
      "  Input (Dat|Arr|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      "  Report Count (1)\n"
      "  Report Size (5)\n"
      "  Input (Con|Var|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      "  Usage Page (Generic Desktop)\n"
      "  Usage (0x30)\n"
      "  Usage (0x31)\n"
      "  Logical Minimum (129)\n"
      "  Logical Maximum (127)\n"
      "  Report Size (8)\n"
      "  Report Count (2)\n"
      "  Input (Dat|Arr|Abs|NoWrp|Lin|Prf|NoNull|BitF)\n"
      " End Collection\n"
      "End Collection\n"};

  ParseDescriptor(std::string(expected), kMouse, sizeof(kMouse));
}

TEST_F(HidReportDescriptorTest, TopLevelCollections_Mouse) {
  HidUsageAndPage expected[] = {
      HidUsageAndPage(0x02, HidUsageAndPage::kPageGenericDesktop)};

  GetTopLevelCollections(std::vector<HidUsageAndPage>(
                             expected, expected + ARRAYSIZE_UNSAFE(expected)),
                         kMouse,
                         sizeof(kMouse));
}

TEST_F(HidReportDescriptorTest, ParseDescriptor_LogitechUnifyingReceiver) {
  const char expected[] = {
      "Usage Page (Vendor)\n"
      "Usage (0x1)\n"
      "Collection (Physical)\n"
      " Report ID (0x10)\n"
      " Report Size (8)\n"
      " Report Count (6)\n"
      " Logical Minimum (0)\n"
      " Logical Maximum (255)\n"
      " Usage (0x1)\n"
      " Input (Dat|Var|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      " Usage (0x1)\n"
      " Output (Dat|Var|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      "End Collection\n"
      "Usage Page (Vendor)\n"
      "Usage (0x2)\n"
      "Collection (Physical)\n"
      " Report ID (0x11)\n"
      " Report Size (8)\n"
      " Report Count (19)\n"
      " Logical Minimum (0)\n"
      " Logical Maximum (255)\n"
      " Usage (0x2)\n"
      " Input (Dat|Var|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      " Usage (0x2)\n"
      " Output (Dat|Var|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      "End Collection\n"
      "Usage Page (Vendor)\n"
      "Usage (0x4)\n"
      "Collection (Physical)\n"
      " Report ID (0x20)\n"
      " Report Size (8)\n"
      " Report Count (14)\n"
      " Logical Minimum (0)\n"
      " Logical Maximum (255)\n"
      " Usage (0x41)\n"
      " Input (Dat|Var|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      " Usage (0x41)\n"
      " Output (Dat|Var|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      " Report ID (0x21)\n"
      " Report Count (31)\n"
      " Logical Minimum (0)\n"
      " Logical Maximum (255)\n"
      " Usage (0x42)\n"
      " Input (Dat|Var|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      " Usage (0x42)\n"
      " Output (Dat|Var|Rel|NoWrp|Lin|Prf|NoNull|BitF)\n"
      "End Collection\n"};

  ParseDescriptor(std::string(expected),
                  kLogitechUnifyingReceiver,
                  sizeof(kLogitechUnifyingReceiver));
}

TEST_F(HidReportDescriptorTest, TopLevelCollections_LogitechUnifyingReceiver) {
  HidUsageAndPage expected[] = {
      HidUsageAndPage(0x01, HidUsageAndPage::kPageVendor),
      HidUsageAndPage(0x02, HidUsageAndPage::kPageVendor),
      HidUsageAndPage(0x04, HidUsageAndPage::kPageVendor), };

  GetTopLevelCollections(std::vector<HidUsageAndPage>(
                             expected, expected + ARRAYSIZE_UNSAFE(expected)),
                         kLogitechUnifyingReceiver,
                         sizeof(kLogitechUnifyingReceiver));
}

}  // namespace device
