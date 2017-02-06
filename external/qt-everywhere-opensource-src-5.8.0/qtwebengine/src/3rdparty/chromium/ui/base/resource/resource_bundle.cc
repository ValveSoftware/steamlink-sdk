// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include <stdint.h>

#include <limits>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/zlib/zlib.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/data_pack.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/strings/grit/app_locale_settings.h"

#if defined(OS_ANDROID)
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/gfx/android/device_display_info.h"
#endif

#if defined(OS_CHROMEOS)
#include "ui/gfx/platform_font_linux.h"
#endif

#if defined(OS_WIN)
#include "ui/display/win/dpi.h"
#endif

namespace ui {

namespace {

// PNG-related constants.
const unsigned char kPngMagic[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
const size_t kPngChunkMetadataSize = 12;  // length, type, crc32
const unsigned char kPngScaleChunkType[4] = { 'c', 's', 'C', 'l' };
const unsigned char kPngDataChunkType[4] = { 'I', 'D', 'A', 'T' };

#if !defined(OS_MACOSX) || defined(TOOLKIT_QT)
const char kPakFileSuffix[] = ".pak";
#endif

ResourceBundle* g_shared_instance_ = NULL;

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

base::FilePath GetResourcesPakFilePath(const std::string& pak_name) {
  base::FilePath path;
  if (PathService::Get(base::DIR_MODULE, &path))
    return path.AppendASCII(pak_name.c_str());

  // Return just the name of the pak file.
#if defined(OS_WIN)
  return base::FilePath(base::ASCIIToUTF16(pak_name));
#else
  return base::FilePath(pak_name.c_str());
#endif  // OS_WIN
}

SkBitmap CreateEmptyBitmap() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(32, 32);
  bitmap.eraseARGB(255, 255, 255, 0);
  return bitmap;
}

// Decodes given gzip input via zlib.
base::RefCountedBytes* DecodeGzipData(const unsigned char* input_buffer,
                                      size_t input_size) {
  z_stream inflateStream;
  memset(&inflateStream, 0, sizeof(inflateStream));
  inflateStream.zalloc = Z_NULL;
  inflateStream.zfree = Z_NULL;
  inflateStream.opaque = Z_NULL;

  inflateStream.avail_in = input_size;
  inflateStream.next_in = const_cast<Bytef*>(input_buffer);

  CHECK(input_size >= 4);
  // Size of output comes from footer of gzip file format, found as the last 4
  // bytes in the compressed file, which are stored little endian.
  inflateStream.avail_out = base::ByteSwapToLE32(
      *reinterpret_cast<const uint32_t*>(&input_buffer[input_size - 4]));

  std::vector<unsigned char> output(inflateStream.avail_out);
  inflateStream.next_out = reinterpret_cast<Bytef*>(&output[0]);

  CHECK(inflateInit2(&inflateStream, 16) == Z_OK);
  CHECK(inflate(&inflateStream, Z_FINISH) == Z_STREAM_END);
  CHECK(inflateEnd(&inflateStream) == Z_OK);

  // Cannot use TakeVector since it puts the RefCounted* into a scoped_refptr,
  // and callers of this function return a raw pointer (not a scoped_refptr), so
  // the memory will be deallocated upon exit of the calling function.
  base::RefCountedBytes* returnVal = new base::RefCountedBytes();
  returnVal->data().swap(output);
  return returnVal;
}

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
  ~ResourceBundleImageSource() override {}

  // gfx::ImageSkiaSource overrides:
  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    SkBitmap image;
    bool fell_back_to_1x = false;
    ScaleFactor scale_factor = GetSupportedScaleFactor(scale);
    bool found = rb_->LoadBitmap(resource_id_, &scale_factor,
                                 &image, &fell_back_to_1x);
    if (!found) {
#if defined(OS_ANDROID)
      // TODO(oshima): Android unit_tests runs at DSF=3 with 100P assets.
      return gfx::ImageSkiaRep();
#else
      NOTREACHED() << "Unable to load image with id " << resource_id_
                   << ", scale=" << scale;
      return gfx::ImageSkiaRep(CreateEmptyBitmap(), scale);
#endif
    }

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

struct ResourceBundle::FontKey {
  FontKey(int in_size_delta,
          gfx::Font::FontStyle in_style,
          gfx::Font::Weight in_weight)
      : size_delta(in_size_delta), style(in_style), weight(in_weight) {}

  ~FontKey() {}

  bool operator==(const FontKey& rhs) const {
    return std::tie(size_delta, style, weight) ==
           std::tie(rhs.size_delta, rhs.style, rhs.weight);
  }

  bool operator<(const FontKey& rhs) const {
    return std::tie(size_delta, style, weight) <
           std::tie(rhs.size_delta, rhs.style, rhs.weight);
  }

  int size_delta;
  gfx::Font::FontStyle style;
  gfx::Font::Weight weight;
};

// static
std::string ResourceBundle::InitSharedInstanceWithLocale(
    const std::string& pref_locale,
    Delegate* delegate,
    LoadResources load_resources) {
  InitSharedInstance(delegate);
  if (load_resources == LOAD_COMMON_RESOURCES)
    g_shared_instance_->LoadCommonResources();
  std::string result = g_shared_instance_->LoadLocaleResources(pref_locale);
  g_shared_instance_->InitDefaultFontList();
  return result;
}

// static
void ResourceBundle::InitSharedInstanceWithPakFileRegion(
    base::File pak_file,
    const base::MemoryMappedFile::Region& region) {
  InitSharedInstance(NULL);
  std::unique_ptr<DataPack> data_pack(new DataPack(SCALE_FACTOR_100P));
  if (!data_pack->LoadFromFileRegion(std::move(pak_file), region)) {
    NOTREACHED() << "failed to load pak file";
    return;
  }
  g_shared_instance_->locale_resources_data_.reset(data_pack.release());
  g_shared_instance_->InitDefaultFontList();
}

// static
void ResourceBundle::InitSharedInstanceWithPakPath(const base::FilePath& path) {
  InitSharedInstance(NULL);
  g_shared_instance_->LoadTestResources(path, path);

  g_shared_instance_->InitDefaultFontList();
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

#if !defined(OS_ANDROID) && !defined(TOOLKIT_QT)
bool ResourceBundle::LocaleDataPakExists(const std::string& locale) {
  return !GetLocaleFilePath(locale, true).empty();
}
#endif  // !defined(OS_ANDROID)

void ResourceBundle::AddDataPackFromPath(const base::FilePath& path,
                                         ScaleFactor scale_factor) {
  AddDataPackFromPathInternal(path, scale_factor, false, false);
}

void ResourceBundle::AddOptionalDataPackFromPath(const base::FilePath& path,
                                         ScaleFactor scale_factor) {
  AddDataPackFromPathInternal(path, scale_factor, true, false);
}

void ResourceBundle::AddMaterialDesignDataPackFromPath(
    const base::FilePath& path,
    ScaleFactor scale_factor) {
  AddDataPackFromPathInternal(path, scale_factor, false, true);
}

void ResourceBundle::AddOptionalMaterialDesignDataPackFromPath(
    const base::FilePath& path,
    ScaleFactor scale_factor) {
  AddDataPackFromPathInternal(path, scale_factor, true, true);
}

void ResourceBundle::AddDataPackFromFile(base::File file,
                                         ScaleFactor scale_factor) {
  AddDataPackFromFileRegion(std::move(file),
                            base::MemoryMappedFile::Region::kWholeFile,
                            scale_factor);
}

void ResourceBundle::AddDataPackFromFileRegion(
    base::File file,
    const base::MemoryMappedFile::Region& region,
    ScaleFactor scale_factor) {
  std::unique_ptr<DataPack> data_pack(new DataPack(scale_factor));
  if (data_pack->LoadFromFileRegion(std::move(file), region)) {
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

  if (!locale_file_path.empty()) {
    locale_file_path =
        locale_file_path.AppendASCII(app_locale + kPakFileSuffix);
  }

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

#if !defined(OS_ANDROID) && !defined(TOOLKIT_QT)
std::string ResourceBundle::LoadLocaleResources(
    const std::string& pref_locale) {
  DCHECK(!locale_resources_data_.get()) << "locale.pak already loaded";
  std::string app_locale = l10n_util::GetApplicationLocale(pref_locale);
  base::FilePath locale_file_path = GetOverriddenPakPath();
  if (locale_file_path.empty())
    locale_file_path = GetLocaleFilePath(app_locale, true);

  if (locale_file_path.empty()) {
    // It's possible that there is no locale.pak.
    LOG(WARNING) << "locale_file_path.empty() for locale " << app_locale;
    return std::string();
  }

  std::unique_ptr<DataPack> data_pack(new DataPack(SCALE_FACTOR_100P));
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
#endif  // defined(OS_ANDROID)

void ResourceBundle::LoadTestResources(const base::FilePath& path,
                                       const base::FilePath& locale_path) {
  is_test_resources_ = true;
  DCHECK(!ui::GetSupportedScaleFactors().empty());
  const ScaleFactor scale_factor(ui::GetSupportedScaleFactors()[0]);
  // Use the given resource pak for both common and localized resources.
  std::unique_ptr<DataPack> data_pack(new DataPack(scale_factor));
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

void ResourceBundle::OverrideLocaleStringResource(
    int message_id,
    const base::string16& string) {
  overridden_locale_strings_[message_id] = string;
}

const base::FilePath& ResourceBundle::GetOverriddenPakPath() {
  return overridden_pak_path_;
}

std::string ResourceBundle::ReloadLocaleResources(
    const std::string& pref_locale) {
  base::AutoLock lock_scope(*locale_resources_data_lock_);

  // Remove all overriden strings, as they will not be valid for the new locale.
  overridden_locale_strings_.clear();

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

#if defined(OS_CHROMEOS)
    ui::ScaleFactor scale_factor_to_load = GetMaxScaleFactor();
#elif defined(OS_WIN)
    ui::ScaleFactor scale_factor_to_load =
        display::win::GetDPIScale() > 1.25
            ? GetMaxScaleFactor()
            : ui::SCALE_FACTOR_100P;
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

base::RefCountedMemory* ResourceBundle::LoadDataResourceBytes(
    int resource_id) const {
  return LoadDataResourceBytesForScale(resource_id, ui::SCALE_FACTOR_NONE);
}

base::RefCountedMemory* ResourceBundle::LoadDataResourceBytesForScale(
    int resource_id,
    ScaleFactor scale_factor) const {
  base::RefCountedMemory* bytes = NULL;
  if (delegate_)
    bytes = delegate_->LoadDataResourceBytes(resource_id, scale_factor);

  if (!bytes) {
    base::StringPiece data =
        GetRawDataResourceForScaleImpl(resource_id, scale_factor);
    if (!data.empty()) {
      if (data.starts_with(CUSTOM_GZIP_HEADER)) {
        // Jump past special identification byte prepended to header
        const unsigned char* gzip_start =
            reinterpret_cast<const unsigned char*>(data.data()) + 1;
        bytes = DecodeGzipData(gzip_start, data.length() - 1);
      } else {
        bytes = new base::RefCountedStaticMemory(data.data(), data.length());
      }
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
  base::StringPiece data =
      GetRawDataResourceForScaleImpl(resource_id, scale_factor);

  // Do not allow this function to retrieve gzip compressed resources.
  CHECK(!data.starts_with(CUSTOM_GZIP_HEADER));

  return data;
}

base::string16 ResourceBundle::GetLocalizedString(int message_id) {
  base::string16 string;
  if (delegate_ && delegate_->GetLocalizedString(message_id, &string))
    return string;

  // Ensure that ReloadLocaleResources() doesn't drop the resources while
  // we're using them.
  base::AutoLock lock_scope(*locale_resources_data_lock_);

  IdToStringMap::const_iterator it =
      overridden_locale_strings_.find(message_id);
  if (it != overridden_locale_strings_.end())
    return it->second;

  // If for some reason we were unable to load the resources , return an empty
  // string (better than crashing).
  if (!locale_resources_data_.get()) {
    LOG(WARNING) << "locale resources are not loaded";
    return base::string16();
  }

  base::StringPiece data;
  if (!locale_resources_data_->GetStringPiece(static_cast<uint16_t>(message_id),
                                              &data)) {
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

const gfx::FontList& ResourceBundle::GetFontListWithDelta(
    int size_delta,
    gfx::Font::FontStyle style,
    gfx::Font::Weight weight) {
  base::AutoLock lock_scope(*images_and_fonts_lock_);

  const FontKey styled_key(size_delta, style, weight);

  auto found = font_cache_.find(styled_key);
  if (found != font_cache_.end())
    return found->second;

  const FontKey base_key(0, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);
  gfx::FontList& base = font_cache_[base_key];
  if (styled_key == base_key)
    return base;

  // Fonts of a given style are derived from the unstyled font of the same size.
  // Cache the unstyled font by first inserting a default-constructed font list.
  // Then, derive it for the initial insertion, or use the iterator that points
  // to the existing entry that the insertion collided with.
  const FontKey sized_key(size_delta, gfx::Font::NORMAL,
                          gfx::Font::Weight::NORMAL);
  auto sized = font_cache_.insert(std::make_pair(sized_key, gfx::FontList()));
  if (sized.second)
    sized.first->second = base.DeriveWithSizeDelta(size_delta);
  if (styled_key == sized_key)
    return sized.first->second;

  auto styled = font_cache_.insert(std::make_pair(styled_key, gfx::FontList()));
  DCHECK(styled.second);  // Otherwise font_cache_.find(..) would have found it.
  styled.first->second = sized.first->second.Derive(
      0, sized.first->second.GetFontStyle() | style, weight);

  return styled.first->second;
}

const gfx::Font& ResourceBundle::GetFontWithDelta(int size_delta,
                                                  gfx::Font::FontStyle style,
                                                  gfx::Font::Weight weight) {
  return GetFontListWithDelta(size_delta, style, weight).GetPrimaryFont();
}

const gfx::FontList& ResourceBundle::GetFontList(FontStyle legacy_style) {
  gfx::Font::Weight font_weight = gfx::Font::Weight::NORMAL;
  if (legacy_style == BoldFont || legacy_style == SmallBoldFont ||
      legacy_style == MediumBoldFont || legacy_style == LargeBoldFont)
    font_weight = gfx::Font::Weight::BOLD;

  int size_delta = 0;
  switch (legacy_style) {
    case SmallFont:
    case SmallBoldFont:
      size_delta = kSmallFontDelta;
      break;
    case MediumFont:
    case MediumBoldFont:
      size_delta = kMediumFontDelta;
      break;
    case LargeFont:
    case LargeBoldFont:
      size_delta = kLargeFontDelta;
      break;
    case BaseFont:
    case BoldFont:
      break;
  }

  return GetFontListWithDelta(size_delta, gfx::Font::NORMAL, font_weight);
}

const gfx::Font& ResourceBundle::GetFont(FontStyle style) {
  return GetFontList(style).GetPrimaryFont();
}

void ResourceBundle::ReloadFonts() {
  base::AutoLock lock_scope(*images_and_fonts_lock_);
  InitDefaultFontList();
  font_cache_.clear();
}

ScaleFactor ResourceBundle::GetMaxScaleFactor() const {
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_LINUX)
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
#if !defined(OS_IOS)
  // On platforms other than iOS, 100P is always a supported scale factor.
  // For Windows we have a separate case in this function.
  supported_scale_factors.push_back(SCALE_FACTOR_100P);
#endif
#if defined(OS_ANDROID)
  float display_density;
  if (display::Display::HasForceDeviceScaleFactor()) {
    display_density = display::Display::GetForcedDeviceScaleFactor();
  } else {
    gfx::DeviceDisplayInfo device_info;
    display_density = device_info.GetDIPScale();
  }
  const ScaleFactor closest = FindClosestScaleFactorUnsafe(display_density);
  if (closest != SCALE_FACTOR_100P)
    supported_scale_factors.push_back(closest);
#elif defined(OS_IOS)
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  if (display.device_scale_factor() > 2.0) {
    DCHECK_EQ(3.0, display.device_scale_factor());
    supported_scale_factors.push_back(SCALE_FACTOR_300P);
  } else if (display.device_scale_factor() > 1.0) {
    DCHECK_EQ(2.0, display.device_scale_factor());
    supported_scale_factors.push_back(SCALE_FACTOR_200P);
  } else {
    supported_scale_factors.push_back(SCALE_FACTOR_100P);
  }
#elif defined(OS_MACOSX) || defined(OS_CHROMEOS) || defined(OS_LINUX) || \
    defined(OS_WIN)
  supported_scale_factors.push_back(SCALE_FACTOR_200P);
#endif
  ui::SetSupportedScaleFactors(supported_scale_factors);
}

void ResourceBundle::FreeImages() {
  images_.clear();
}

void ResourceBundle::LoadChromeResources() {
// TODO(estade): remove material design specific resources.
// See crbug.com/613593
#if defined(OS_MACOSX)
  // The material design data packs contain some of the same asset IDs as in
  // the non-material design data packs. Add these to the ResourceBundle
  // first so that they are searched first when a request for an asset is
  // made.
  if (MaterialDesignController::IsModeMaterial()) {
    if (IsScaleFactorSupported(SCALE_FACTOR_100P)) {
      AddMaterialDesignDataPackFromPath(
          GetResourcesPakFilePath("chrome_material_100_percent.pak"),
          SCALE_FACTOR_100P);
    }

    if (IsScaleFactorSupported(SCALE_FACTOR_200P)) {
      AddOptionalMaterialDesignDataPackFromPath(
          GetResourcesPakFilePath("chrome_material_200_percent.pak"),
          SCALE_FACTOR_200P);
    }
  }
#endif

  // Always load the 1x data pack first as the 2x data pack contains both 1x and
  // 2x images. The 1x data pack only has 1x images, thus passes in an accurate
  // scale factor to gfx::ImageSkia::AddRepresentation.
  if (IsScaleFactorSupported(SCALE_FACTOR_100P)) {
    AddDataPackFromPath(GetResourcesPakFilePath(
        "chrome_100_percent.pak"), SCALE_FACTOR_100P);
  }

  if (IsScaleFactorSupported(SCALE_FACTOR_200P)) {
    AddOptionalDataPackFromPath(GetResourcesPakFilePath(
        "chrome_200_percent.pak"), SCALE_FACTOR_200P);
  }
}

void ResourceBundle::AddDataPackFromPathInternal(
    const base::FilePath& path,
    ScaleFactor scale_factor,
    bool optional,
    bool has_only_material_assets) {
  // Do not pass an empty |path| value to this method. If the absolute path is
  // unknown pass just the pack file name.
  DCHECK(!path.empty());

  base::FilePath pack_path = path;
  if (delegate_)
    pack_path = delegate_->GetPathForResourcePack(pack_path, scale_factor);

  // Don't try to load empty values or values that are not absolute paths.
  if (pack_path.empty() || !pack_path.IsAbsolute())
    return;

  std::unique_ptr<DataPack> data_pack(new DataPack(scale_factor));
  data_pack->set_has_only_material_design_assets(has_only_material_assets);
  if (data_pack->LoadFromPath(pack_path)) {
    AddDataPack(data_pack.release());
  } else if (!optional) {
    LOG(ERROR) << "Failed to load " << pack_path.value()
               << "\nSome features may not be available.";
  }
}

void ResourceBundle::AddDataPack(DataPack* data_pack) {
#if DCHECK_IS_ON()
  data_pack->CheckForDuplicateResources(data_packs_);
#endif
  data_packs_.push_back(data_pack);

  if (GetScaleForScaleFactor(data_pack->GetScaleFactor()) >
      GetScaleForScaleFactor(max_scale_factor_))
    max_scale_factor_ = data_pack->GetScaleFactor();
}

void ResourceBundle::InitDefaultFontList() {
#if defined(OS_CHROMEOS)
  std::string font_family = base::UTF16ToUTF8(
      GetLocalizedString(IDS_UI_FONT_FAMILY_CROS));
  gfx::FontList::SetDefaultFontDescription(font_family);

  // TODO(yukishiino): Remove SetDefaultFontDescription() once the migration to
  // the font list is done.  We will no longer need SetDefaultFontDescription()
  // after every client gets started using a FontList instead of a Font.
  gfx::PlatformFontLinux::SetDefaultFontDescription(font_family);
#else
  // Use a single default font as the default font list.
  gfx::FontList::SetDefaultFontDescription(std::string());
#endif
}

bool ResourceBundle::LoadBitmap(const ResourceHandle& data_handle,
                                int resource_id,
                                SkBitmap* bitmap,
                                bool* fell_back_to_1x) const {
  DCHECK(fell_back_to_1x);
  scoped_refptr<base::RefCountedMemory> memory(
      data_handle.GetStaticMemory(static_cast<uint16_t>(resource_id)));
  if (!memory.get())
    return false;

  if (DecodePNG(memory->front(), memory->size(), bitmap, fell_back_to_1x))
    return true;

#if !defined(OS_IOS)
  // iOS does not compile or use the JPEG codec.  On other platforms,
  // 99% of our assets are PNGs, however fallback to JPEG.
  std::unique_ptr<SkBitmap> jpeg_bitmap(
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
  ResourceHandle* data_handle_100_percent = nullptr;
  for (size_t i = 0; i < data_packs_.size(); ++i) {
    if (data_packs_[i]->GetScaleFactor() == ui::SCALE_FACTOR_NONE &&
        LoadBitmap(*data_packs_[i], resource_id, bitmap, fell_back_to_1x)) {
      DCHECK(!*fell_back_to_1x);
      *scale_factor = ui::SCALE_FACTOR_NONE;
      return true;
    }
    if (data_packs_[i]->GetScaleFactor() == ui::SCALE_FACTOR_100P)
      data_handle_100_percent = data_packs_[i];

    if (data_packs_[i]->GetScaleFactor() == *scale_factor &&
        LoadBitmap(*data_packs_[i], resource_id, bitmap, fell_back_to_1x)) {
      return true;
    }
  }

  // Unit tests may only have 1x data pack. Allow them to fallback to 1x
  // resources.
  if (*scale_factor != ui::SCALE_FACTOR_100P && is_test_resources_ &&
      data_handle_100_percent &&
      LoadBitmap(*data_handle_100_percent, resource_id, bitmap,
                 fell_back_to_1x)) {
    *fell_back_to_1x = true;
    return true;
  }

  return false;
}

base::StringPiece ResourceBundle::GetRawDataResourceForScaleImpl(
    int resource_id,
    ScaleFactor scale_factor) const {
  base::StringPiece data;
  if (delegate_ &&
      delegate_->GetRawDataResource(resource_id, scale_factor, &data))
    return data;

  if (scale_factor != ui::SCALE_FACTOR_100P) {
    for (size_t i = 0; i < data_packs_.size(); i++) {
      if (data_packs_[i]->GetScaleFactor() == scale_factor &&
          data_packs_[i]->GetStringPiece(static_cast<uint16_t>(resource_id),
                                         &data))
        return data;
    }
  }

  for (size_t i = 0; i < data_packs_.size(); i++) {
    if ((data_packs_[i]->GetScaleFactor() == ui::SCALE_FACTOR_100P ||
         data_packs_[i]->GetScaleFactor() == ui::SCALE_FACTOR_200P ||
         data_packs_[i]->GetScaleFactor() == ui::SCALE_FACTOR_300P ||
         data_packs_[i]->GetScaleFactor() == ui::SCALE_FACTOR_NONE) &&
        data_packs_[i]->GetStringPiece(static_cast<uint16_t>(resource_id),
                                       &data))
      return data;
  }

  return base::StringPiece();
}

gfx::Image& ResourceBundle::GetEmptyImage() {
  base::AutoLock lock(*images_and_fonts_lock_);

  if (empty_image_.IsEmpty()) {
    // The placeholder bitmap is bright red so people notice the problem.
    SkBitmap bitmap = CreateEmptyBitmap();
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
    uint32_t length = 0;
    base::ReadBigEndian(reinterpret_cast<const char*>(buf + pos), &length);
    if (size - pos - kPngChunkMetadataSize < length)
      break;
    if (length == 0 &&
        memcmp(buf + pos + sizeof(uint32_t), kPngScaleChunkType,
               arraysize(kPngScaleChunkType)) == 0) {
      return true;
    }
    if (memcmp(buf + pos + sizeof(uint32_t), kPngDataChunkType,
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
