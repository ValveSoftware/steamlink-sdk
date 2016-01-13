// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include <limits>
#include <vector>

#include "base/big_endian.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "grit/app_locale_settings.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/data_pack.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/safe_integer_conversions.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"

#if defined(OS_CHROMEOS)
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/platform_font_pango.h"
#endif

#if defined(OS_WIN)
#include "ui/base/win/dpi_setup.h"
#include "ui/gfx/win/dpi.h"
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#endif

namespace ui {

namespace {

// Font sizes relative to base font.
const int kSmallFontSizeDelta = -1;
const int kMediumFontSizeDelta = 3;
const int kLargeFontSizeDelta = 8;

// PNG-related constants.
const unsigned char kPngMagic[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
const size_t kPngChunkMetadataSize = 12;  // length, type, crc32
const unsigned char kPngScaleChunkType[4] = { 'c', 's', 'C', 'l' };
const unsigned char kPngDataChunkType[4] = { 'I', 'D', 'A', 'T' };

ResourceBundle* g_shared_instance_ = NULL;

void InitDefaultFontList() {
#if defined(OS_CHROMEOS)
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  std::string font_family = base::UTF16ToUTF8(
      rb.GetLocalizedString(IDS_UI_FONT_FAMILY_CROS));
  gfx::FontList::SetDefaultFontDescription(font_family);

  // TODO(yukishiino): Remove SetDefaultFontDescription() once the migration to
  // the font list is done.  We will no longer need SetDefaultFontDescription()
  // after every client gets started using a FontList instead of a Font.
  gfx::PlatformFontPango::SetDefaultFontDescription(font_family);
#else
  // Use a single default font as the default font list.
  gfx::FontList::SetDefaultFontDescription(std::string());
#endif
}

#if defined(OS_ANDROID)
// Returns the scale factor closest to |scale| from the full list of factors.
// Note that it does NOT rely on the list of supported scale factors.
// Finding the closest match is inefficient and shouldn't be done frequently.
ScaleFactor FindClosestScaleFactorUnsafe(float scale) {
  float smallest_diff =  std::numeric_limits<float>::max();
  ScaleFactor closest_match = SCALE_FACTOR_100P;
  for (int i = SCALE_FACTOR_100P; i < NUM_SCALE_FACTORS; ++i) {
    const ScaleFactor scale_factor = static_cast<ScaleFactor>(i);
    float diff = std::abs(GetScaleForScaleFactor(scale_factor) - scale);
    if (diff < smallest_diff) {
      closest_match = scale_factor;
      smallest_diff = diff;
    }
  }
  return closest_match;
}
#endif  // OS_ANDROID

}  // namespace

// An ImageSkiaSource that loads bitmaps for the requested scale factor from
// ResourceBundle on demand for a given |resource_id|. If the bitmap for the
// requested scale factor does not exist, it will return the 1x bitmap scaled
// by the scale factor. This may lead to broken UI if the correct size of the
// scaled image is not exactly |scale_factor| * the size of the 1x resource.
// When --highlight-missing-scaled-resources flag is specified, scaled 1x images
// are higlighted by blending them with red.
class ResourceBundle::ResourceBundleImageSource : public gfx::ImageSkiaSource {
 public:
  ResourceBundleImageSource(ResourceBundle* rb, int resource_id)
      : rb_(rb), resource_id_(resource_id) {}
  virtual ~ResourceBundleImageSource() {}

  // gfx::ImageSkiaSource overrides:
  virtual gfx::ImageSkiaRep GetImageForScale(float scale) OVERRIDE {
    SkBitmap image;
    bool fell_back_to_1x = false;
    ScaleFactor scale_factor = GetSupportedScaleFactor(scale);
    bool found = rb_->LoadBitmap(resource_id_, &scale_factor,
                                 &image, &fell_back_to_1x);
    if (!found)
      return gfx::ImageSkiaRep();

    // If the resource is in the package with SCALE_FACTOR_NONE, it
    // can be used in any scale factor. The image is maked as "unscaled"
    // so that the ImageSkia do not automatically scale.
    if (scale_factor == ui::SCALE_FACTOR_NONE)
      return gfx::ImageSkiaRep(image, 0.0f);

    if (fell_back_to_1x) {
      // GRIT fell back to the 100% image, so rescale it to the correct size.
      image = skia::ImageOperations::Resize(
          image,
          skia::ImageOperations::RESIZE_LANCZOS3,
          gfx::ToCeiledInt(image.width() * scale),
          gfx::ToCeiledInt(image.height() * scale));
    } else {
      scale = GetScaleForScaleFactor(scale_factor);
    }
    return gfx::ImageSkiaRep(image, scale);
  }

 private:
  ResourceBundle* rb_;
  const int resource_id_;

  DISALLOW_COPY_AND_ASSIGN(ResourceBundleImageSource);
};

// static
std::string ResourceBundle::InitSharedInstanceWithLocale(
    const std::string& pref_locale, Delegate* delegate) {
  InitSharedInstance(delegate);
  g_shared_instance_->LoadCommonResources();
  std::string result = g_shared_instance_->LoadLocaleResources(pref_locale);
  InitDefaultFontList();
  return result;
}

// static
std::string ResourceBundle::InitSharedInstanceLocaleOnly(
    const std::string& pref_locale, Delegate* delegate) {
  InitSharedInstance(delegate);
  std::string result = g_shared_instance_->LoadLocaleResources(pref_locale);
  InitDefaultFontList();
  return result;
}

// static
void ResourceBundle::InitSharedInstanceWithPakFile(
    base::File pak_file, bool should_load_common_resources) {
  InitSharedInstance(NULL);
  if (should_load_common_resources)
    g_shared_instance_->LoadCommonResources();

  scoped_ptr<DataPack> data_pack(
      new DataPack(SCALE_FACTOR_100P));
  if (!data_pack->LoadFromFile(pak_file.Pass())) {
    NOTREACHED() << "failed to load pak file";
    return;
  }
  g_shared_instance_->locale_resources_data_.reset(data_pack.release());
  InitDefaultFontList();
}

// static
void ResourceBundle::InitSharedInstanceWithPakPath(const base::FilePath& path) {
  InitSharedInstance(NULL);
  g_shared_instance_->LoadTestResources(path, path);

  InitDefaultFontList();
}

// static
void ResourceBundle::CleanupSharedInstance() {
  if (g_shared_instance_) {
    delete g_shared_instance_;
    g_shared_instance_ = NULL;
  }
}

// static
bool ResourceBundle::HasSharedInstance() {
  return g_shared_instance_ != NULL;
}

// static
ResourceBundle& ResourceBundle::GetSharedInstance() {
  // Must call InitSharedInstance before this function.
  CHECK(g_shared_instance_ != NULL);
  return *g_shared_instance_;
}

bool ResourceBundle::LocaleDataPakExists(const std::string& locale) {
  return !GetLocaleFilePath(locale, true).empty();
}

void ResourceBundle::AddDataPackFromPath(const base::FilePath& path,
                                         ScaleFactor scale_factor) {
  AddDataPackFromPathInternal(path, scale_factor, false);
}

void ResourceBundle::AddOptionalDataPackFromPath(const base::FilePath& path,
                                         ScaleFactor scale_factor) {
  AddDataPackFromPathInternal(path, scale_factor, true);
}

void ResourceBundle::AddDataPackFromFile(base::File file,
                                         ScaleFactor scale_factor) {
  scoped_ptr<DataPack> data_pack(
      new DataPack(scale_factor));
  if (data_pack->LoadFromFile(file.Pass())) {
    AddDataPack(data_pack.release());
  } else {
    LOG(ERROR) << "Failed to load data pack from file."
               << "\nSome features may not be available.";
  }
}

#if !defined(OS_MACOSX) || defined(TOOLKIT_QT)
base::FilePath ResourceBundle::GetLocaleFilePath(const std::string& app_locale,
                                                 bool test_file_exists) {
  if (app_locale.empty())
    return base::FilePath();

  base::FilePath locale_file_path;

  PathService::Get(ui::DIR_LOCALES, &locale_file_path);

  if (!locale_file_path.empty())
    locale_file_path = locale_file_path.AppendASCII(app_locale + ".pak");

  if (delegate_) {
    locale_file_path =
        delegate_->GetPathForLocalePack(locale_file_path, app_locale);
  }

  // Don't try to load empty values or values that are not absolute paths.
  if (locale_file_path.empty() || !locale_file_path.IsAbsolute())
    return base::FilePath();

  if (test_file_exists && !base::PathExists(locale_file_path))
    return base::FilePath();

  return locale_file_path;
}
#endif

std::string ResourceBundle::LoadLocaleResources(
    const std::string& pref_locale) {
  DCHECK(!locale_resources_data_.get()) << "locale.pak already loaded";
  std::string app_locale = l10n_util::GetApplicationLocale(pref_locale);
  base::FilePath locale_file_path = GetOverriddenPakPath();
  if (locale_file_path.empty())
    locale_file_path = GetLocaleFilePath(app_locale, true);

  if (locale_file_path.empty()) {
    // It's possible that there is no locale.pak.
    LOG(WARNING) << "locale_file_path.empty()";
    return std::string();
  }

  scoped_ptr<DataPack> data_pack(
      new DataPack(SCALE_FACTOR_100P));
  if (!data_pack->LoadFromPath(locale_file_path)) {
    UMA_HISTOGRAM_ENUMERATION("ResourceBundle.LoadLocaleResourcesError",
                              logging::GetLastSystemErrorCode(), 16000);
    LOG(ERROR) << "failed to load locale.pak";
    NOTREACHED();
    return std::string();
  }

  locale_resources_data_.reset(data_pack.release());
  return app_locale;
}

void ResourceBundle::LoadTestResources(const base::FilePath& path,
                                       const base::FilePath& locale_path) {
  // Use the given resource pak for both common and localized resources.
  scoped_ptr<DataPack> data_pack(new DataPack(SCALE_FACTOR_100P));
  if (!path.empty() && data_pack->LoadFromPath(path))
    AddDataPack(data_pack.release());

  data_pack.reset(new DataPack(ui::SCALE_FACTOR_NONE));
  if (!locale_path.empty() && data_pack->LoadFromPath(locale_path)) {
    locale_resources_data_.reset(data_pack.release());
  } else {
    locale_resources_data_.reset(new DataPack(ui::SCALE_FACTOR_NONE));
  }
}

void ResourceBundle::UnloadLocaleResources() {
  locale_resources_data_.reset();
}

void ResourceBundle::OverrideLocalePakForTest(const base::FilePath& pak_path) {
  overridden_pak_path_ = pak_path;
}

const base::FilePath& ResourceBundle::GetOverriddenPakPath() {
  return overridden_pak_path_;
}

std::string ResourceBundle::ReloadLocaleResources(
    const std::string& pref_locale) {
  base::AutoLock lock_scope(*locale_resources_data_lock_);
  UnloadLocaleResources();
  return LoadLocaleResources(pref_locale);
}

gfx::ImageSkia* ResourceBundle::GetImageSkiaNamed(int resource_id) {
  const gfx::ImageSkia* image = GetImageNamed(resource_id).ToImageSkia();
  return const_cast<gfx::ImageSkia*>(image);
}

gfx::Image& ResourceBundle::GetImageNamed(int resource_id) {
  // Check to see if the image is already in the cache.
  {
    base::AutoLock lock_scope(*images_and_fonts_lock_);
    if (images_.count(resource_id))
      return images_[resource_id];
  }

  gfx::Image image;
  if (delegate_)
    image = delegate_->GetImageNamed(resource_id);

  if (image.IsEmpty()) {
    DCHECK(!data_packs_.empty()) <<
        "Missing call to SetResourcesDataDLL?";

#if defined(OS_CHROMEOS) || defined(OS_WIN)
  ui::ScaleFactor scale_factor_to_load = GetMaxScaleFactor();
#else
  ui::ScaleFactor scale_factor_to_load = ui::SCALE_FACTOR_100P;
#endif

    // TODO(oshima): Consider reading the image size from png IHDR chunk and
    // skip decoding here and remove #ifdef below.
    // ResourceBundle::GetSharedInstance() is destroyed after the
    // BrowserMainLoop has finished running. |image_skia| is guaranteed to be
    // destroyed before the resource bundle is destroyed.
    gfx::ImageSkia image_skia(new ResourceBundleImageSource(this, resource_id),
                              GetScaleForScaleFactor(scale_factor_to_load));
    if (image_skia.isNull()) {
      LOG(WARNING) << "Unable to load image with id " << resource_id;
      NOTREACHED();  // Want to assert in debug mode.
      // The load failed to retrieve the image; show a debugging red square.
      return GetEmptyImage();
    }
    image_skia.SetReadOnly();
    image = gfx::Image(image_skia);
  }

  // The load was successful, so cache the image.
  base::AutoLock lock_scope(*images_and_fonts_lock_);

  // Another thread raced the load and has already cached the image.
  if (images_.count(resource_id))
    return images_[resource_id];

  images_[resource_id] = image;
  return images_[resource_id];
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  return GetNativeImageNamed(resource_id, RTL_DISABLED);
}

base::RefCountedStaticMemory* ResourceBundle::LoadDataResourceBytes(
    int resource_id) const {
  return LoadDataResourceBytesForScale(resource_id, ui::SCALE_FACTOR_NONE);
}

base::RefCountedStaticMemory* ResourceBundle::LoadDataResourceBytesForScale(
    int resource_id,
    ScaleFactor scale_factor) const {
  base::RefCountedStaticMemory* bytes = NULL;
  if (delegate_)
    bytes = delegate_->LoadDataResourceBytes(resource_id, scale_factor);

  if (!bytes) {
    base::StringPiece data =
        GetRawDataResourceForScale(resource_id, scale_factor);
    if (!data.empty()) {
      bytes = new base::RefCountedStaticMemory(data.data(), data.length());
    }
  }

  return bytes;
}

base::StringPiece ResourceBundle::GetRawDataResource(int resource_id) const {
  return GetRawDataResourceForScale(resource_id, ui::SCALE_FACTOR_NONE);
}

base::StringPiece ResourceBundle::GetRawDataResourceForScale(
    int resource_id,
    ScaleFactor scale_factor) const {
  base::StringPiece data;
  if (delegate_ &&
      delegate_->GetRawDataResource(resource_id, scale_factor, &data))
    return data;

  if (scale_factor != ui::SCALE_FACTOR_100P) {
    for (size_t i = 0; i < data_packs_.size(); i++) {
      if (data_packs_[i]->GetScaleFactor() == scale_factor &&
          data_packs_[i]->GetStringPiece(resource_id, &data))
        return data;
    }
  }
  for (size_t i = 0; i < data_packs_.size(); i++) {
    if ((data_packs_[i]->GetScaleFactor() == ui::SCALE_FACTOR_100P ||
         data_packs_[i]->GetScaleFactor() == ui::SCALE_FACTOR_200P ||
         data_packs_[i]->GetScaleFactor() == ui::SCALE_FACTOR_NONE) &&
        data_packs_[i]->GetStringPiece(resource_id, &data))
      return data;
  }

  return base::StringPiece();
}

base::string16 ResourceBundle::GetLocalizedString(int message_id) {
  base::string16 string;
  if (delegate_ && delegate_->GetLocalizedString(message_id, &string))
    return string;

  // Ensure that ReloadLocaleResources() doesn't drop the resources while
  // we're using them.
  base::AutoLock lock_scope(*locale_resources_data_lock_);

  // If for some reason we were unable to load the resources , return an empty
  // string (better than crashing).
  if (!locale_resources_data_.get()) {
    LOG(WARNING) << "locale resources are not loaded";
    return base::string16();
  }

  base::StringPiece data;
  if (!locale_resources_data_->GetStringPiece(message_id, &data)) {
    // Fall back on the main data pack (shouldn't be any strings here except in
    // unittests).
    data = GetRawDataResource(message_id);
    if (data.empty()) {
      NOTREACHED() << "unable to find resource: " << message_id;
      return base::string16();
    }
  }

  // Strings should not be loaded from a data pack that contains binary data.
  ResourceHandle::TextEncodingType encoding =
      locale_resources_data_->GetTextEncodingType();
  DCHECK(encoding == ResourceHandle::UTF16 || encoding == ResourceHandle::UTF8)
      << "requested localized string from binary pack file";

  // Data pack encodes strings as either UTF8 or UTF16.
  base::string16 msg;
  if (encoding == ResourceHandle::UTF16) {
    msg = base::string16(reinterpret_cast<const base::char16*>(data.data()),
                         data.length() / 2);
  } else if (encoding == ResourceHandle::UTF8) {
    msg = base::UTF8ToUTF16(data);
  }
  return msg;
}

const gfx::FontList& ResourceBundle::GetFontList(FontStyle style) {
  {
    base::AutoLock lock_scope(*images_and_fonts_lock_);
    LoadFontsIfNecessary();
  }
  switch (style) {
    case BoldFont:
      return *bold_font_list_;
    case SmallFont:
      return *small_font_list_;
    case MediumFont:
      return *medium_font_list_;
    case SmallBoldFont:
      return *small_bold_font_list_;
    case MediumBoldFont:
      return *medium_bold_font_list_;
    case LargeFont:
      return *large_font_list_;
    case LargeBoldFont:
      return *large_bold_font_list_;
    default:
      return *base_font_list_;
  }
}

const gfx::Font& ResourceBundle::GetFont(FontStyle style) {
  return GetFontList(style).GetPrimaryFont();
}

void ResourceBundle::ReloadFonts() {
  base::AutoLock lock_scope(*images_and_fonts_lock_);
  base_font_list_.reset();
  LoadFontsIfNecessary();
}

ScaleFactor ResourceBundle::GetMaxScaleFactor() const {
#if defined(OS_CHROMEOS) || defined(OS_WIN)
  return max_scale_factor_;
#else
  return GetSupportedScaleFactors().back();
#endif
}

bool ResourceBundle::IsScaleFactorSupported(ScaleFactor scale_factor) {
  const std::vector<ScaleFactor>& supported_scale_factors =
      ui::GetSupportedScaleFactors();
  return std::find(supported_scale_factors.begin(),
                   supported_scale_factors.end(),
                   scale_factor) != supported_scale_factors.end();
}

ResourceBundle::ResourceBundle(Delegate* delegate)
    : delegate_(delegate),
      images_and_fonts_lock_(new base::Lock),
      locale_resources_data_lock_(new base::Lock),
      max_scale_factor_(SCALE_FACTOR_100P) {
}

ResourceBundle::~ResourceBundle() {
  FreeImages();
  UnloadLocaleResources();
}

// static
void ResourceBundle::InitSharedInstance(Delegate* delegate) {
  DCHECK(g_shared_instance_ == NULL) << "ResourceBundle initialized twice";
  g_shared_instance_ = new ResourceBundle(delegate);
  static std::vector<ScaleFactor> supported_scale_factors;
#if !defined(OS_IOS) && !defined(OS_WIN)
  // On platforms other than iOS, 100P is always a supported scale factor.
  // For Windows we have a separate case in this function.
  supported_scale_factors.push_back(SCALE_FACTOR_100P);
#endif
#if defined(OS_ANDROID)
  const gfx::Display display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  const float display_density = display.device_scale_factor();
  const ScaleFactor closest = FindClosestScaleFactorUnsafe(display_density);
  if (closest != SCALE_FACTOR_100P)
    supported_scale_factors.push_back(closest);
#elif defined(OS_IOS)
    gfx::Display display = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  if (display.device_scale_factor() > 1.0) {
    DCHECK_EQ(2.0, display.device_scale_factor());
    supported_scale_factors.push_back(SCALE_FACTOR_200P);
  } else {
    supported_scale_factors.push_back(SCALE_FACTOR_100P);
  }
#elif defined(OS_MACOSX)
  if (base::mac::IsOSLionOrLater())
    supported_scale_factors.push_back(SCALE_FACTOR_200P);
#elif defined(OS_CHROMEOS)
  // TODO(oshima): Include 200P only if the device support 200P
  supported_scale_factors.push_back(SCALE_FACTOR_200P);
#elif defined(OS_LINUX) && defined(ENABLE_HIDPI)
  supported_scale_factors.push_back(SCALE_FACTOR_200P);
#elif defined(OS_WIN)
  bool default_to_100P = true;
  if (gfx::IsHighDPIEnabled()) {
    // On Windows if the dpi scale is greater than 1.25 on high dpi machines
    // downscaling from 200 percent looks better than scaling up from 100
    // percent.
    if (gfx::GetDPIScale() > 1.25) {
      supported_scale_factors.push_back(SCALE_FACTOR_200P);
      default_to_100P = false;
    }
  }
  if (default_to_100P)
    supported_scale_factors.push_back(SCALE_FACTOR_100P);
#endif
  ui::SetSupportedScaleFactors(supported_scale_factors);
#if defined(OS_WIN)
  // Must be called _after_ supported scale factors are set since it
  // uses them.
  // Don't initialize the device scale factor if it has already been
  // initialized.
  if (!gfx::win::IsDeviceScaleFactorSet())
    ui::win::InitDeviceScaleFactor();
#endif
}

void ResourceBundle::FreeImages() {
  images_.clear();
}

void ResourceBundle::AddDataPackFromPathInternal(const base::FilePath& path,
                                                 ScaleFactor scale_factor,
                                                 bool optional) {
  // Do not pass an empty |path| value to this method. If the absolute path is
  // unknown pass just the pack file name.
  DCHECK(!path.empty());

  base::FilePath pack_path = path;
  if (delegate_)
    pack_path = delegate_->GetPathForResourcePack(pack_path, scale_factor);

  // Don't try to load empty values or values that are not absolute paths.
  if (pack_path.empty() || !pack_path.IsAbsolute())
    return;

  scoped_ptr<DataPack> data_pack(
      new DataPack(scale_factor));
  if (data_pack->LoadFromPath(pack_path)) {
    AddDataPack(data_pack.release());
  } else if (!optional) {
    LOG(ERROR) << "Failed to load " << pack_path.value()
               << "\nSome features may not be available.";
  }
}

void ResourceBundle::AddDataPack(DataPack* data_pack) {
  data_packs_.push_back(data_pack);

  if (GetScaleForScaleFactor(data_pack->GetScaleFactor()) >
      GetScaleForScaleFactor(max_scale_factor_))
    max_scale_factor_ = data_pack->GetScaleFactor();
}

void ResourceBundle::LoadFontsIfNecessary() {
  images_and_fonts_lock_->AssertAcquired();
  if (!base_font_list_.get()) {
    if (delegate_) {
      base_font_list_ = GetFontListFromDelegate(BaseFont);
      bold_font_list_ = GetFontListFromDelegate(BoldFont);
      small_font_list_ = GetFontListFromDelegate(SmallFont);
      small_bold_font_list_ = GetFontListFromDelegate(SmallBoldFont);
      medium_font_list_ = GetFontListFromDelegate(MediumFont);
      medium_bold_font_list_ = GetFontListFromDelegate(MediumBoldFont);
      large_font_list_ = GetFontListFromDelegate(LargeFont);
      large_bold_font_list_ = GetFontListFromDelegate(LargeBoldFont);
    }

    if (!base_font_list_.get())
      base_font_list_.reset(new gfx::FontList());

    if (!bold_font_list_.get()) {
      bold_font_list_.reset(new gfx::FontList());
      *bold_font_list_ = base_font_list_->DeriveWithStyle(
          base_font_list_->GetFontStyle() | gfx::Font::BOLD);
    }

    if (!small_font_list_.get()) {
      small_font_list_.reset(new gfx::FontList());
      *small_font_list_ =
          base_font_list_->DeriveWithSizeDelta(kSmallFontSizeDelta);
    }

    if (!small_bold_font_list_.get()) {
      small_bold_font_list_.reset(new gfx::FontList());
      *small_bold_font_list_ = small_font_list_->DeriveWithStyle(
          small_font_list_->GetFontStyle() | gfx::Font::BOLD);
    }

    if (!medium_font_list_.get()) {
      medium_font_list_.reset(new gfx::FontList());
      *medium_font_list_ =
          base_font_list_->DeriveWithSizeDelta(kMediumFontSizeDelta);
    }

    if (!medium_bold_font_list_.get()) {
      medium_bold_font_list_.reset(new gfx::FontList());
      *medium_bold_font_list_ = medium_font_list_->DeriveWithStyle(
          medium_font_list_->GetFontStyle() | gfx::Font::BOLD);
    }

    if (!large_font_list_.get()) {
      large_font_list_.reset(new gfx::FontList());
      *large_font_list_ =
          base_font_list_->DeriveWithSizeDelta(kLargeFontSizeDelta);
    }

    if (!large_bold_font_list_.get()) {
      large_bold_font_list_.reset(new gfx::FontList());
      *large_bold_font_list_ = large_font_list_->DeriveWithStyle(
          large_font_list_->GetFontStyle() | gfx::Font::BOLD);
    }
  }
}

scoped_ptr<gfx::FontList> ResourceBundle::GetFontListFromDelegate(
    FontStyle style) {
  DCHECK(delegate_);
  scoped_ptr<gfx::Font> font = delegate_->GetFont(style);
  if (font.get())
    return scoped_ptr<gfx::FontList>(new gfx::FontList(*font));
  return scoped_ptr<gfx::FontList>();
}

bool ResourceBundle::LoadBitmap(const ResourceHandle& data_handle,
                                int resource_id,
                                SkBitmap* bitmap,
                                bool* fell_back_to_1x) const {
  DCHECK(fell_back_to_1x);
  scoped_refptr<base::RefCountedMemory> memory(
      data_handle.GetStaticMemory(resource_id));
  if (!memory.get())
    return false;

  if (DecodePNG(memory->front(), memory->size(), bitmap, fell_back_to_1x))
    return true;

#if !defined(OS_IOS)
  // iOS does not compile or use the JPEG codec.  On other platforms,
  // 99% of our assets are PNGs, however fallback to JPEG.
  scoped_ptr<SkBitmap> jpeg_bitmap(
      gfx::JPEGCodec::Decode(memory->front(), memory->size()));
  if (jpeg_bitmap.get()) {
    bitmap->swap(*jpeg_bitmap.get());
    *fell_back_to_1x = false;
    return true;
  }
#endif

  NOTREACHED() << "Unable to decode theme image resource " << resource_id;
  return false;
}

bool ResourceBundle::LoadBitmap(int resource_id,
                                ScaleFactor* scale_factor,
                                SkBitmap* bitmap,
                                bool* fell_back_to_1x) const {
  DCHECK(fell_back_to_1x);
  for (size_t i = 0; i < data_packs_.size(); ++i) {
    if (data_packs_[i]->GetScaleFactor() == ui::SCALE_FACTOR_NONE &&
        LoadBitmap(*data_packs_[i], resource_id, bitmap, fell_back_to_1x)) {
      DCHECK(!*fell_back_to_1x);
      *scale_factor = ui::SCALE_FACTOR_NONE;
      return true;
    }
    if (data_packs_[i]->GetScaleFactor() == *scale_factor &&
        LoadBitmap(*data_packs_[i], resource_id, bitmap, fell_back_to_1x)) {
      return true;
    }
  }
  return false;
}

gfx::Image& ResourceBundle::GetEmptyImage() {
  base::AutoLock lock(*images_and_fonts_lock_);

  if (empty_image_.IsEmpty()) {
    // The placeholder bitmap is bright red so people notice the problem.
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 32, 32);
    bitmap.allocPixels();
    bitmap.eraseARGB(255, 255, 0, 0);
    empty_image_ = gfx::Image::CreateFrom1xBitmap(bitmap);
  }
  return empty_image_;
}

// static
bool ResourceBundle::PNGContainsFallbackMarker(const unsigned char* buf,
                                               size_t size) {
  if (size < arraysize(kPngMagic) ||
      memcmp(buf, kPngMagic, arraysize(kPngMagic)) != 0) {
    // Data invalid or a JPEG.
    return false;
  }
  size_t pos = arraysize(kPngMagic);

  // Scan for custom chunks until we find one, find the IDAT chunk, or run out
  // of chunks.
  for (;;) {
    if (size - pos < kPngChunkMetadataSize)
      break;
    uint32 length = 0;
    base::ReadBigEndian(reinterpret_cast<const char*>(buf + pos), &length);
    if (size - pos - kPngChunkMetadataSize < length)
      break;
    if (length == 0 && memcmp(buf + pos + sizeof(uint32), kPngScaleChunkType,
                              arraysize(kPngScaleChunkType)) == 0) {
      return true;
    }
    if (memcmp(buf + pos + sizeof(uint32), kPngDataChunkType,
               arraysize(kPngDataChunkType)) == 0) {
      // Stop looking for custom chunks, any custom chunks should be before an
      // IDAT chunk.
      break;
    }
    pos += length + kPngChunkMetadataSize;
  }
  return false;
}

// static
bool ResourceBundle::DecodePNG(const unsigned char* buf,
                               size_t size,
                               SkBitmap* bitmap,
                               bool* fell_back_to_1x) {
  *fell_back_to_1x = PNGContainsFallbackMarker(buf, size);
  return gfx::PNGCodec::Decode(buf, size, bitmap);
}

}  // namespace ui
