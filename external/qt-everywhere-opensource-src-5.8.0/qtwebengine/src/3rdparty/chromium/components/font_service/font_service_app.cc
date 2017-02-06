// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/font_service/font_service_app.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/shell/public/cpp/connection.h"

static_assert(
    static_cast<uint32_t>(SkFontStyle::kUpright_Slant) ==
        static_cast<uint32_t>(font_service::mojom::TypefaceSlant::ROMAN),
    "Skia and font service flags must match");
static_assert(
    static_cast<uint32_t>(SkFontStyle::kItalic_Slant) ==
        static_cast<uint32_t>(font_service::mojom::TypefaceSlant::ITALIC),
    "Skia and font service flags must match");
static_assert(
    static_cast<uint32_t>(SkFontStyle::kOblique_Slant) ==
        static_cast<uint32_t>(font_service::mojom::TypefaceSlant::OBLIQUE),
    "Skia and font service flags must match");

namespace {

mojo::ScopedHandle GetHandleForPath(const base::FilePath& path) {
  if (path.empty())
    return mojo::ScopedHandle();

  mojo::ScopedHandle to_pass;
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(WARNING) << "file not valid, path=" << path.value();
    return mojo::ScopedHandle();
  }

  return mojo::WrapPlatformFile(file.TakePlatformFile());
}

}  // namespace

namespace font_service {

FontServiceApp::FontServiceApp() {}

FontServiceApp::~FontServiceApp() {}

void FontServiceApp::Initialize(shell::Connector* connector,
                                const shell::Identity& identity,
                                uint32_t id) {
  tracing_.Initialize(connector, identity.name());
}

bool FontServiceApp::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface(this);
  return true;
}

void FontServiceApp::Create(
    shell::Connection* connection,
    mojo::InterfaceRequest<mojom::FontService> request) {
  bindings_.AddBinding(this, std::move(request));
}

void FontServiceApp::MatchFamilyName(const mojo::String& family_name,
                                     mojom::TypefaceStylePtr requested_style,
                                     const MatchFamilyNameCallback& callback) {
  SkFontConfigInterface::FontIdentity result_identity;
  SkString result_family;
  SkFontStyle result_style;
  SkFontConfigInterface* fc =
      SkFontConfigInterface::GetSingletonDirectInterface();
  const bool r = fc->matchFamilyName(
      family_name.data(),
      SkFontStyle(requested_style->weight,
                  requested_style->width,
                  static_cast<SkFontStyle::Slant>(requested_style->slant)),
      &result_identity, &result_family, &result_style);

  if (!r) {
    mojom::TypefaceStylePtr style(mojom::TypefaceStyle::New());
    style->weight = SkFontStyle().weight();
    style->width = SkFontStyle().width();
    style->slant = static_cast<mojom::TypefaceSlant>(SkFontStyle().slant());
    callback.Run(nullptr, "", std::move(style));
    return;
  }

  // Stash away the returned path, so we can give it an ID (index)
  // which will later be given to us in a request to open the file.
  int index = FindOrAddPath(result_identity.fString);

  mojom::FontIdentityPtr identity(mojom::FontIdentity::New());
  identity->id = static_cast<uint32_t>(index);
  identity->ttc_index = result_identity.fTTCIndex;
  identity->str_representation = result_identity.fString.c_str();

  mojom::TypefaceStylePtr style(mojom::TypefaceStyle::New());
  style->weight = result_style.weight();
  style->width = result_style.width();
  style->slant = static_cast<mojom::TypefaceSlant>(result_style.slant());

  callback.Run(std::move(identity), result_family.c_str(), std::move(style));
}

void FontServiceApp::OpenStream(uint32_t id_number,
                                const OpenStreamCallback& callback) {
  mojo::ScopedHandle handle;
  if (id_number < static_cast<uint32_t>(paths_.size())) {
    handle = GetHandleForPath(base::FilePath(paths_[id_number].c_str()));
  }

  callback.Run(std::move(handle));
}

int FontServiceApp::FindOrAddPath(const SkString& path) {
  int count = paths_.size();
  for (int i = 0; i < count; ++i) {
    if (path == paths_[i])
      return i;
  }
  paths_.emplace_back(path);
  return count;
}

}  // namespace font_service
