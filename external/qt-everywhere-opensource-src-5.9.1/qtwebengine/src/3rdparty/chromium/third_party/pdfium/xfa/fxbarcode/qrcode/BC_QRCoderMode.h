// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXBARCODE_QRCODE_BC_QRCODERMODE_H_
#define XFA_FXBARCODE_QRCODE_BC_QRCODERMODE_H_

#include <stdint.h>

#include <vector>

#include "core/fxcrt/fx_string.h"

class CBC_QRCoderVersion;

class CBC_QRCoderMode {
 public:
  virtual ~CBC_QRCoderMode();

  static void Initialize();
  static void Finalize();
  static CBC_QRCoderMode* ForBits(int32_t bits, int32_t& e);
  static void Destroy();

  int32_t GetCharacterCountBits(CBC_QRCoderVersion* version, int32_t& e) const;
  int32_t GetBits() const;
  CFX_ByteString GetName() const;

  static CBC_QRCoderMode* sBYTE;
  static CBC_QRCoderMode* sNUMERIC;
  static CBC_QRCoderMode* sALPHANUMERIC;
  static CBC_QRCoderMode* sKANJI;
  static CBC_QRCoderMode* sECI;
  static CBC_QRCoderMode* sGBK;
  static CBC_QRCoderMode* sTERMINATOR;
  static CBC_QRCoderMode* sFNC1_FIRST_POSITION;
  static CBC_QRCoderMode* sFNC1_SECOND_POSITION;
  static CBC_QRCoderMode* sSTRUCTURED_APPEND;

 private:
  CBC_QRCoderMode();
  CBC_QRCoderMode(std::vector<int32_t> charCountBits,
                  int32_t bits,
                  CFX_ByteString name);

  std::vector<int32_t> m_characterCountBitsForVersions;
  const int32_t m_bits;
  const CFX_ByteString m_name;
};

#endif  // XFA_FXBARCODE_QRCODE_BC_QRCODERMODE_H_
