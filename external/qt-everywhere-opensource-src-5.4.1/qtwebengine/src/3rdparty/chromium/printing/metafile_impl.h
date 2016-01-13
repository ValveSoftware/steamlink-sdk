// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_METAFILE_IMPL_H_
#define PRINTING_METAFILE_IMPL_H_

#include "printing/pdf_metafile_skia.h"

#if defined(OS_WIN)
#include "printing/emf_win.h"
#endif

namespace printing {

#if defined(OS_WIN) && !defined(WIN_PDF_METAFILE_FOR_PRINTING)
typedef Emf NativeMetafile;
typedef PdfMetafileSkia PreviewMetafile;
#else
typedef PdfMetafileSkia NativeMetafile;
typedef PdfMetafileSkia PreviewMetafile;
#endif

}  // namespace printing

#endif  // PRINTING_METAFILE_IMPL_H_
