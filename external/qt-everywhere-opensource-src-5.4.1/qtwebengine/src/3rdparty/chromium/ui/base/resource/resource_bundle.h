// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_BUNDLE_H_
#define UI_BASE_RESOURCE_RESOURCE_BUNDLE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_export.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

class SkBitmap;

namespace base {
class File;
class Lock;
class RefCountedStaticMemory;
}

namespace ui {

class DataPack;
class ResourceHandle;

// ResourceBundle is a central facility to load images and other resources,
// such as theme graphics. Every resource is loaded only once.
class UI_BASE_EXPORT ResourceBundle {
 public:
  // An enumeration of the various font styles used throughout Chrome.
  // The following holds true for the font sizes:
  // Small <= SmallBold <= Base <= Bold <= Medium <= MediumBold <= Large.
  enum FontStyle {
    // NOTE: depending upon the locale, using one of the *BoldFont below
    // may *not* actually result in a bold font.
    SmallFont,
    SmallBoldFont,
    BaseFont,
    BoldFont,
    MediumFont,
    MediumBoldFont,
    LargeFont,
    LargeBoldFont,
  };

  enum ImageRTL {
    // Images are flipped in RTL locales.
    RTL_ENABLED,
    // Images are never flipped.
    RTL_DISABLED,
  };

  // Delegate class that allows interception of pack file loading and resource
  // requests. The methods of this class may be called on multiple threads.
  class Delegate {
   public:
    // Called before a resource pack file is loaded. Return the full path for
    // the pack file to continue loading or an empty value to cancel loading.
    // |pack_path| will contain the complete default path for the pack file if
    // known or just the pack file name otherwise.
    virtual base::FilePath GetPathForResourcePack(
        const base::FilePath& pack_path,
        ScaleFactor scale_factor) = 0;

    // Called before a locale pack file is loaded. Return the full path for
    // the pack file to continue loading or an empty value to cancel loading.
    // |pack_path| will contain the complete default path for the pack file if
    // known or just the pack file name otherwise.
    virtual base::FilePath GetPathForLocalePack(
        const base::FilePath& pack_path,
        const std::string& locale) = 0;

    // Return an image resource or an empty value to attempt retrieval of the
    // default resource.
    virtual gfx::Image GetImageNamed(int resource_id) = 0;

    // Return an image resource or an empty value to attempt retrieval of the
    // default resource.
    virtual gfx::Image GetNativeImageNamed(int resource_id, ImageRTL rtl) = 0;

    // Return a static memory resource or NULL to attempt retrieval of the
    // default resource.
    virtual base::RefCountedStaticMemory* LoadDataResourceBytes(
        int resource_id,
        ScaleFactor scale_factor) = 0;

    // Retrieve a raw data resource. Return true if a resource was provided or
    // false to attempt retrieval of the default resource.
    virtual bool GetRawDataResource(int resource_id,
                                    ScaleFactor scale_factor,
                                    base::StringPiece* value) = 0;

    // Retrieve a localized string. Return true if a string was provided or
    // false to attempt retrieval of the default string.
    virtual bool GetLocalizedString(int message_id, base::string16* value) = 0;

    // Returns a font or NULL to attempt retrieval of the default resource.
    virtual scoped_ptr<gfx::Font> GetFont(FontStyle style) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Initialize the ResourceBundle for this process. Does not take ownership of
  // the |delegate| value. Returns the language selected.
  // NOTE: Mac ignores this and always loads up resources for the language
  // defined by the Cocoa UI (i.e., NSBundle does the language work).
  //
  // TODO(sergeyu): This method also loads common resources (i.e. chrome.pak).
  // There is no way to specify which resource files are loaded, i.e. names of
  // the files are hardcoded in ResourceBundle. Fix it to allow to specify which
  // files are loaded (e.g. add a new method in Delegate).
  static std::string InitSharedInstanceWithLocale(
      const std::string& pref_locale, Delegate* delegate);

  // Same as InitSharedInstanceWithLocale(), but loads only localized resources,
  // without default resource packs.
  static std::string InitSharedInstanceLocaleOnly(
      const std::string& pref_locale, Delegate* delegate);

  // Initialize the ResourceBundle using given file. The second argument
  // controls whether or not ResourceBundle::LoadCommonResources is called.
  // This allows the use of this function in a sandbox without local file
  // access (as on Android).
  static void InitSharedInstanceWithPakFile(base::File file,
                                            bool should_load_common_resources);

  // Initialize the ResourceBundle using given data pack path for testing.
  static void InitSharedInstanceWithPakPath(const base::FilePath& path);

  // Delete the ResourceBundle for this process if it exists.
  static void CleanupSharedInstance();

  // Returns true after the global resource loader instance has been created.
  static bool HasSharedInstance();

  // Return the global resource loader instance.
  static ResourceBundle& GetSharedInstance();

  // Check if the .pak for the given locale exists.
  bool LocaleDataPakExists(const std::string& locale);

  // Registers additional data pack files with this ResourceBundle.  When
  // looking for a DataResource, we will search these files after searching the
  // main module. |path| should be the complete path to the pack file if known
  // or just the pack file name otherwise (the delegate may optionally override
  // this value). |scale_factor| is the scale of images in this resource pak
  // relative to the images in the 1x resource pak. This method is not thread
  // safe! You should call it immediately after calling InitSharedInstance.
  void AddDataPackFromPath(const base::FilePath& path,
                           ScaleFactor scale_factor);

  // Same as above but using an already open file.
  void AddDataPackFromFile(base::File file, ScaleFactor scale_factor);

  // Same as AddDataPackFromPath but does not log an error if the pack fails to
  // load.
  void AddOptionalDataPackFromPath(const base::FilePath& path,
                                   ScaleFactor scale_factor);

  // Changes the locale for an already-initialized ResourceBundle, returning the
  // name of the newly-loaded locale.  Future calls to get strings will return
  // the strings for this new locale.  This has no effect on existing or future
  // image resources.  |locale_resources_data_| is protected by a lock for the
  // duration of the swap, as GetLocalizedString() may be concurrently invoked
  // on another thread.
  std::string ReloadLocaleResources(const std::string& pref_locale);

  // Gets image with the specified resource_id from the current module data.
  // Returns a pointer to a shared instance of gfx::ImageSkia. This shared
  // instance is owned by the resource bundle and should not be freed.
  // TODO(pkotwicz): Make method return const gfx::ImageSkia*
  //
  // NOTE: GetNativeImageNamed is preferred for cross-platform gfx::Image use.
  gfx::ImageSkia* GetImageSkiaNamed(int resource_id);

  // Gets an image resource from the current module data. This will load the
  // image in Skia format by default. The ResourceBundle owns this.
  gfx::Image& GetImageNamed(int resource_id);

  // Similar to GetImageNamed, but rather than loading the image in Skia format,
  // it will load in the native platform type. This can avoid conversion from
  // one image type to another. ResourceBundle owns the result.
  //
  // Note that if the same resource has already been loaded in GetImageNamed(),
  // gfx::Image will perform a conversion, rather than using the native image
  // loading code of ResourceBundle.
  //
  // If |rtl| is RTL_ENABLED then the image is flipped in RTL locales.
  gfx::Image& GetNativeImageNamed(int resource_id, ImageRTL rtl);

  // Same as GetNativeImageNamed() except that RTL is not enabled.
  gfx::Image& GetNativeImageNamed(int resource_id);

  // Loads the raw bytes of a scale independent data resource.
  base::RefCountedStaticMemory* LoadDataResourceBytes(int resource_id) const;

  // Loads the raw bytes of a data resource nearest the scale factor
  // |scale_factor| into |bytes|, without doing any processing or
  // interpretation of the resource. Use ResourceHandle::SCALE_FACTOR_NONE
  // for scale independent image resources (such as wallpaper).
  // Returns NULL if we fail to read the resource.
  base::RefCountedStaticMemory* LoadDataResourceBytesForScale(
      int resource_id,
      ScaleFactor scale_factor) const;

  // Return the contents of a scale independent resource in a
  // StringPiece given the resource id
  base::StringPiece GetRawDataResource(int resource_id) const;

  // Return the contents of a resource in a StringPiece given the resource id
  // nearest the scale factor |scale_factor|.
  // Use ResourceHandle::SCALE_FACTOR_NONE for scale independent image resources
  // (such as wallpaper).
  base::StringPiece GetRawDataResourceForScale(int resource_id,
                                               ScaleFactor scale_factor) const;

  // Get a localized string given a message id.  Returns an empty
  // string if the message_id is not found.
  base::string16 GetLocalizedString(int message_id);

  // Returns the font list for the specified style.
  const gfx::FontList& GetFontList(FontStyle style);

  // Returns the font for the specified style.
  const gfx::Font& GetFont(FontStyle style);

  // Resets and reloads the cached fonts.  This is useful when the fonts of the
  // system have changed, for example, when the locale has changed.
  void ReloadFonts();

  // Overrides the path to the pak file from which the locale resources will be
  // loaded. Pass an empty path to undo.
  void OverrideLocalePakForTest(const base::FilePath& pak_path);

  // Returns the full pathname of the locale file to load.  May return an empty
  // string if no locale data files are found and |test_file_exists| is true.
  // Used on Android to load the local file in the browser process and pass it
  // to the sandboxed renderer process.
  base::FilePath GetLocaleFilePath(const std::string& app_locale,
                                   bool test_file_exists);

  // Returns the maximum scale factor currently loaded.
  // Returns SCALE_FACTOR_100P if no resource is loaded.
  ScaleFactor GetMaxScaleFactor() const;

 protected:
  // Returns true if |scale_factor| is supported by this platform.
  static bool IsScaleFactorSupported(ScaleFactor scale_factor);

 private:
  FRIEND_TEST_ALL_PREFIXES(ResourceBundleTest, DelegateGetPathForLocalePack);
  FRIEND_TEST_ALL_PREFIXES(ResourceBundleTest, DelegateGetImageNamed);
  FRIEND_TEST_ALL_PREFIXES(ResourceBundleTest, DelegateGetNativeImageNamed);

  friend class ResourceBundleImageTest;
  friend class ResourceBundleTest;

  class ResourceBundleImageSource;
  friend class ResourceBundleImageSource;

  // Ctor/dtor are private, since we're a singleton.
  explicit ResourceBundle(Delegate* delegate);
  ~ResourceBundle();

  // Shared initialization.
  static void InitSharedInstance(Delegate* delegate);

  // Free skia_images_.
  void FreeImages();

  // Load the main resources.
  void LoadCommonResources();

  // Implementation for AddDataPackFromPath and AddOptionalDataPackFromPath, if
  // the pack is not |optional| logs an error on failure to load.
  void AddDataPackFromPathInternal(const base::FilePath& path,
                                   ScaleFactor scale_factor,
                                   bool optional);

  // Inserts |data_pack| to |data_pack_| and updates |max_scale_factor_|
  // accordingly.
  void AddDataPack(DataPack* data_pack);

  // Try to load the locale specific strings from an external data module.
  // Returns the locale that is loaded.
  std::string LoadLocaleResources(const std::string& pref_locale);

  // Load test resources in given paths. If either path is empty an empty
  // resource pack is loaded.
  void LoadTestResources(const base::FilePath& path,
                         const base::FilePath& locale_path);

  // Unload the locale specific strings and prepares to load new ones. See
  // comments for ReloadLocaleResources().
  void UnloadLocaleResources();

  // Initializes all the gfx::FontList members if they haven't yet been
  // initialized.
  void LoadFontsIfNecessary();

  // Returns a FontList or NULL by calling Delegate::GetFont and converting
  // scoped_ptr<gfx::Font> to scoped_ptr<gfx::FontList>.
  scoped_ptr<gfx::FontList> GetFontListFromDelegate(FontStyle style);

  // Fills the |bitmap| given the data file to look in and the |resource_id|.
  // Returns false if the resource does not exist.
  //
  // If the call succeeds, |fell_back_to_1x| indicates whether Chrome's custom
  // csCl PNG chunk is present (added by GRIT if it falls back to a 100% image).
  bool LoadBitmap(const ResourceHandle& data_handle,
                  int resource_id,
                  SkBitmap* bitmap,
                  bool* fell_back_to_1x) const;

  // Fills the |bitmap| given the |resource_id| and |scale_factor|.
  // Returns false if the resource does not exist. This may fall back to
  // the data pack with SCALE_FACTOR_NONE, and when this happens,
  // |scale_factor| will be set to SCALE_FACTOR_100P.
  bool LoadBitmap(int resource_id,
                  ScaleFactor* scale_factor,
                  SkBitmap* bitmap,
                  bool* fell_back_to_1x) const;

  // Returns true if missing scaled resources should be visually indicated when
  // drawing the fallback (e.g., by tinting the image).
  static bool ShouldHighlightMissingScaledResources();

  // Returns true if the data in |buf| is a PNG that has the special marker
  // added by GRIT that indicates that the image is actually 1x data.
  static bool PNGContainsFallbackMarker(const unsigned char* buf, size_t size);

  // A wrapper for PNGCodec::Decode that returns information about custom
  // chunks. For security reasons we can't alter PNGCodec to return this
  // information. Our PNG files are preprocessed by GRIT, and any special chunks
  // should occur immediately after the IHDR chunk.
  static bool DecodePNG(const unsigned char* buf,
                        size_t size,
                        SkBitmap* bitmap,
                        bool* fell_back_to_1x);

  // Returns an empty image for when a resource cannot be loaded. This is a
  // bright red bitmap.
  gfx::Image& GetEmptyImage();

  const base::FilePath& GetOverriddenPakPath();

  // This pointer is guaranteed to outlive the ResourceBundle instance and may
  // be NULL.
  Delegate* delegate_;

  // Protects |images_| and font-related members.
  scoped_ptr<base::Lock> images_and_fonts_lock_;

  // Protects |locale_resources_data_|.
  scoped_ptr<base::Lock> locale_resources_data_lock_;

  // Handles for data sources.
  scoped_ptr<ResourceHandle> locale_resources_data_;
  ScopedVector<ResourceHandle> data_packs_;

  // The maximum scale factor currently loaded.
  ScaleFactor max_scale_factor_;

  // Cached images. The ResourceBundle caches all retrieved images and keeps
  // ownership of the pointers.
  typedef std::map<int, gfx::Image> ImageMap;
  ImageMap images_;

  gfx::Image empty_image_;

  // The various font lists used. Cached to avoid repeated GDI
  // creation/destruction.
  scoped_ptr<gfx::FontList> base_font_list_;
  scoped_ptr<gfx::FontList> bold_font_list_;
  scoped_ptr<gfx::FontList> small_font_list_;
  scoped_ptr<gfx::FontList> small_bold_font_list_;
  scoped_ptr<gfx::FontList> medium_font_list_;
  scoped_ptr<gfx::FontList> medium_bold_font_list_;
  scoped_ptr<gfx::FontList> large_font_list_;
  scoped_ptr<gfx::FontList> large_bold_font_list_;
  scoped_ptr<gfx::FontList> web_font_list_;

  base::FilePath overridden_pak_path_;

  DISALLOW_COPY_AND_ASSIGN(ResourceBundle);
};

}  // namespace ui

// TODO(beng): Someday, maybe, get rid of this.
using ui::ResourceBundle;

#endif  // UI_BASE_RESOURCE_RESOURCE_BUNDLE_H_
