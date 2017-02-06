// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_PROXY_MESSAGE_FILTER_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_PROXY_MESSAGE_FILTER_WIN_H_

#include <dwrite.h>
#include <dwrite_2.h>
#include <wrl.h>
#include <set>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_platform_file.h"

struct DWriteFontStyle;
struct MapCharactersResult;

namespace content {

// Implements a message filter that handles the dwrite font proxy messages.
// If DWrite is enabled, calls into the system font collection to obtain
// results. Otherwise, acts as if the system collection contains no fonts.
class CONTENT_EXPORT DWriteFontProxyMessageFilter
    : public BrowserMessageFilter {
 public:
  DWriteFontProxyMessageFilter();

  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;

  void SetWindowsFontsPathForTesting(base::string16 path);

 protected:
  ~DWriteFontProxyMessageFilter() override;

  void OnFindFamily(const base::string16& family_name, UINT32* family_index);
  void OnGetFamilyCount(UINT32* count);
  void OnGetFamilyNames(
      UINT32 family_index,
      std::vector<std::pair<base::string16, base::string16>>* family_names);
  void OnGetFontFiles(UINT32 family_index,
                      std::vector<base::string16>* file_paths,
                      std::vector<IPC::PlatformFileForTransit>* file_handles);
  void OnMapCharacters(const base::string16& text,
                       const DWriteFontStyle& font_style,
                       const base::string16& locale_name,
                       uint32_t reading_direction,
                       const base::string16& base_family_name,
                       MapCharactersResult* result);

  void InitializeDirectWrite();

 private:
  bool AddFilesForFont(std::set<base::string16>* path_set,
                       std::set<base::string16>* custom_font_path_set,
                       IDWriteFont* font);
  bool AddLocalFile(std::set<base::string16>* path_set,
                    std::set<base::string16>* custom_font_path_set,
                    IDWriteLocalFontFileLoader* local_loader,
                    IDWriteFontFile* font_file);

 private:
  enum CustomFontFileLoadingMode { ENABLE, DISABLE, FORCE };

  bool direct_write_initialized_ = false;
  Microsoft::WRL::ComPtr<IDWriteFontCollection> collection_;
  Microsoft::WRL::ComPtr<IDWriteFactory2> factory2_;
  Microsoft::WRL::ComPtr<IDWriteFontFallback> font_fallback_;
  base::string16 windows_fonts_path_;
  CustomFontFileLoadingMode custom_font_file_loading_mode_;

  DISALLOW_COPY_AND_ASSIGN(DWriteFontProxyMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_PROXY_MESSAGE_FILTER_WIN_H_
