// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXBARCODE_QRCODE_BC_QRCODERBLOCKPAIR_H_
#define XFA_FXBARCODE_QRCODE_BC_QRCODERBLOCKPAIR_H_

#include <memory>

class CBC_CommonByteArray;

class CBC_QRCoderBlockPair {
 public:
  CBC_QRCoderBlockPair(std::unique_ptr<CBC_CommonByteArray> data,
                       std::unique_ptr<CBC_CommonByteArray> errorCorrection);
  virtual ~CBC_QRCoderBlockPair();

  const CBC_CommonByteArray* GetDataBytes() const;
  const CBC_CommonByteArray* GetErrorCorrectionBytes() const;

 private:
  std::unique_ptr<CBC_CommonByteArray> m_dataBytes;
  std::unique_ptr<CBC_CommonByteArray> m_errorCorrectionBytes;
};

#endif  // XFA_FXBARCODE_QRCODE_BC_QRCODERBLOCKPAIR_H_
