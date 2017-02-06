// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_USAGE_AND_PAGE_H_
#define DEVICE_HID_HID_USAGE_AND_PAGE_H_

#include <stdint.h>

namespace device {

struct HidUsageAndPage {
  enum Page {
    kPageUndefined = 0x00,
    kPageGenericDesktop = 0x01,
    kPageSimulation = 0x02,
    kPageVirtualReality = 0x03,
    kPageSport = 0x04,
    kPageGame = 0x05,
    kPageKeyboard = 0x07,
    kPageLed = 0x08,
    kPageButton = 0x09,
    kPageOrdinal = 0x0A,
    kPageTelephony = 0x0B,
    kPageConsumer = 0x0C,
    kPageDigitizer = 0x0D,
    kPagePidPage = 0x0F,
    kPageUnicode = 0x10,
    kPageAlphanumericDisplay = 0x14,
    kPageMedicalInstruments = 0x40,
    kPageMonitor0 = 0x80,
    kPageMonitor1 = 0x81,
    kPageMonitor2 = 0x82,
    kPageMonitor3 = 0x83,
    kPagePower0 = 0x84,
    kPagePower1 = 0x85,
    kPagePower2 = 0x86,
    kPagePower3 = 0x87,
    kPageBarCodeScanner = 0x8C,
    kPageScale = 0x8D,
    kPageMagneticStripeReader = 0x8E,
    kPageReservedPointOfSale = 0x8F,
    kPageCameraControl = 0x90,
    kPageArcade = 0x91,
    kPageVendor = 0xFF00,
    kPageMediaCenter = 0xFFBC
  };

  // These usage enumerations are derived from the HID Usage Tables v1.11 spec.
  enum GenericDesktopUsage {
    kGenericDesktopUndefined = 0,
    kGenericDesktopPointer = 1,
    kGenericDesktopMouse = 2,
    kGenericDesktopJoystick = 4,
    kGenericDesktopGamePad = 5,
    kGenericDesktopKeyboard = 6,
    kGenericDesktopKeypad = 7,
    kGenericDesktopMultiAxisController = 8,
    kGenericDesktopX = 0x30,
    kGenericDesktopY = 0x31,
    kGenericDesktopZ = 0x32,
    kGenericDesktopRx = 0x33,
    kGenericDesktopRy = 0x34,
    kGenericDesktopRz = 0x35,
    kGenericDesktopSlider = 0x36,
    kGenericDesktopDial = 0x37,
    kGenericDesktopWheel = 0x38,
    kGenericDesktopHatSwitch = 0x39,
    kGenericDesktopCountedBuffer = 0x3a,
    kGenericDesktopByteCount = 0x3b,
    kGenericDesktopMotionWakeup = 0x3c,
    kGenericDesktopStart = 0x3d,
    kGenericDesktopSelect = 0x3e,
    kGenericDesktopVx = 0x40,
    kGenericDesktopVy = 0x41,
    kGenericDesktopVz = 0x42,
    kGenericDesktopVbrx = 0x43,
    kGenericDesktopVbry = 0x44,
    kGenericDesktopVbrz = 0x45,
    kGenericDesktopVno = 0x46,

    kGenericDesktopSystemControl = 0x80,
    kGenericDesktopSystemPowerDown = 0x81,
    kGenericDesktopSystemSleep = 0x82,
    kGenericDesktopSystemWakeUp = 0x83,
    kGenericDesktopSystemContextMenu = 0x84,
    kGenericDesktopSystemMainMenu = 0x85,
    kGenericDesktopSystemAppMenu = 0x86,
    kGenericDesktopSystemMenuHelp = 0x87,
    kGenericDesktopSystemMenuExit = 0x88,
    kGenericDesktopSystemMenuSelect = 0x89,
    kGenericDesktopSystemMenuRight = 0x8a,
    kGenericDesktopSystemMenuLeft = 0x8b,
    kGenericDesktopSystemMenuUp = 0x8c,
    kGenericDesktopSystemMenuDown = 0x8d,
    kGenericDesktopSystemColdRestart = 0x8e,
    kGenericDesktopSystemWarmRestart = 0x8f,

    kGenericDesktopDPadUp = 0x90,
    kGenericDesktopDPadDown = 0x91,
    kGenericDesktopDPadLeft = 0x92,
    kGenericDesktopDPadRight = 0x93,

    kGenericDesktopSystemDock = 0xa0,
    kGenericDesktopSystemUndock = 0xa1,
    kGenericDesktopSystemSetup = 0xa2,
    kGenericDesktopSystemBreak = 0xa3,
    kGenericDesktopSystemDebuggerBreak = 0xa4,
    kGenericDesktopApplicationBreak = 0xa5,
    kGenericDesktopApplicationDebuggerBreak = 0xa6,
    kGenericDesktopSystemSpeakerMute = 0xa7,
    kGenericDesktopSystemHibernate = 0xa8,
    kGenericDesktopSystemDisplayInvert = 0xb0,
    kGenericDesktopSystemDisplayInternal = 0xb1,
    kGenericDesktopSystemDisplayExternal = 0xb2,
    kGenericDesktopSystemDisplayBoth = 0xb3,
    kGenericDesktopSystemDisplayDual = 0xb4,
    kGenericDesktopSystemDisplayToggle = 0xb5,
    kGenericDesktopSystemDisplaySwap = 0xb6,
  };

  HidUsageAndPage(uint16_t usage, Page usage_page)
      : usage(usage), usage_page(usage_page) {}
  ~HidUsageAndPage() {}

  uint16_t usage;
  Page usage_page;

  // Indicates whether this usage is protected by Chrome.
  bool IsProtected() const;
};

}  // namespace device

#endif  // DEVICE_HID_HID_USAGE_AND_PAGE_H_
