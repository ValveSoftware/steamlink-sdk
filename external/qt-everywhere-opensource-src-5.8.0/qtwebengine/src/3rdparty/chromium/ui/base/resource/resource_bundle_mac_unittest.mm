// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#import <AppKit/AppKit.h>
#include <stddef.h>
#include <stdint.h>

#include "base/base_paths.h"
#include "base/big_endian.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/data_pack.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/app_locale_settings.h"

namespace ui {

extern const char kEmptyPakContents[];
extern const size_t kEmptyPakSize;

namespace {

const unsigned char kPngMagic[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
const size_t kPngChunkMetadataSize = 12;
const unsigned char kPngIHDRChunkType[4] = { 'I', 'H', 'D', 'R' };

// Returns |bitmap_data| with |custom_chunk| inserted after the IHDR chunk.
void AddCustomChunk(const base::StringPiece& custom_chunk,
                    std::vector<unsigned char>* bitmap_data) {
  EXPECT_LT(arraysize(kPngMagic) + kPngChunkMetadataSize, bitmap_data->size());
  EXPECT_TRUE(std::equal(
      bitmap_data->begin(),
      bitmap_data->begin() + arraysize(kPngMagic),
      kPngMagic));
  std::vector<unsigned char>::iterator ihdr_start =
      bitmap_data->begin() + arraysize(kPngMagic);
  char ihdr_length_data[sizeof(uint32_t)];
  for (size_t i = 0; i < sizeof(uint32_t); ++i)
    ihdr_length_data[i] = *(ihdr_start + i);
  uint32_t ihdr_chunk_length = 0;
  base::ReadBigEndian(reinterpret_cast<char*>(ihdr_length_data),
                      &ihdr_chunk_length);
  EXPECT_TRUE(
      std::equal(ihdr_start + sizeof(uint32_t),
                 ihdr_start + sizeof(uint32_t) + sizeof(kPngIHDRChunkType),
                 kPngIHDRChunkType));

  bitmap_data->insert(ihdr_start + kPngChunkMetadataSize + ihdr_chunk_length,
                      custom_chunk.begin(), custom_chunk.end());
}

// Creates datapack at |path| with a single bitmap at resource ID 3
// which is |edge_size|x|edge_size| pixels.
// If |custom_chunk| is non empty, adds it after the IHDR chunk
// in the encoded bitmap data.
void CreateDataPackWithSingleBitmap(const base::FilePath& path,
                                    int edge_size,
                                    const base::StringPiece& custom_chunk) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(edge_size, edge_size);
  bitmap.eraseColor(SK_ColorWHITE);
  std::vector<unsigned char> bitmap_data;
  EXPECT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &bitmap_data));

  if (custom_chunk.size() > 0)
    AddCustomChunk(custom_chunk, &bitmap_data);

  std::map<uint16_t, base::StringPiece> resources;
  resources[3u] = base::StringPiece(
      reinterpret_cast<const char*>(&bitmap_data[0]), bitmap_data.size());
  DataPack::WritePack(path, resources, ui::DataPack::BINARY);
}

}  // namespace

class ResourceBundleMacImageTest : public testing::Test {
 public:
  ResourceBundleMacImageTest() : resource_bundle_(NULL) {}

  ~ResourceBundleMacImageTest() override {}

  void SetUp() override {
    // Create a temporary directory to write test resource bundles to.
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
  }

  // Overridden from testing::Test:
  void TearDown() override { delete resource_bundle_; }

  // Returns new ResoureBundle with the specified |delegate|. The
  // ResourceBundleTest class manages the lifetime of the returned
  // ResourceBundle.
  ResourceBundle* CreateResourceBundle(ResourceBundle::Delegate* delegate) {
    DCHECK(!resource_bundle_);

    resource_bundle_ = new ResourceBundle(delegate);
    return resource_bundle_;
  }

  // Returns resource bundle which uses an empty data pak for locale data.
  ui::ResourceBundle* CreateResourceBundleWithEmptyLocalePak() {
    // Write an empty data pak for locale data.
    const base::FilePath& locale_path = dir_path().Append(
        FILE_PATH_LITERAL("locale.pak"));
    EXPECT_EQ(base::WriteFile(locale_path, kEmptyPakContents, kEmptyPakSize),
              static_cast<int>(kEmptyPakSize));

    ui::ResourceBundle* resource_bundle = CreateResourceBundle(NULL);

    // Load the empty locale data pak.
    resource_bundle->LoadTestResources(base::FilePath(), locale_path);
    return resource_bundle;
  }

  // Returns the path of temporary directory to write test data packs into.
  const base::FilePath& dir_path() { return dir_.path(); }

  // Returns the number of DataPacks managed by |resource_bundle| which are
  // flagged as containing only material design resources.
  size_t NumberOfMaterialDesignDataPacksInResourceBundle(
      ResourceBundle* resource_bundle) {
    DCHECK(resource_bundle);
    size_t num_material_packs = 0;
    for (size_t i = 0; i < resource_bundle->data_packs_.size(); i++) {
      if (resource_bundle->data_packs_[i]->HasOnlyMaterialDesignAssets())
        num_material_packs++;
    }

    return num_material_packs;
  }

protected:
  ResourceBundle* resource_bundle_;

 private:
  std::unique_ptr<DataPack> locale_pack_;
  base::ScopedTempDir dir_;

  DISALLOW_COPY_AND_ASSIGN(ResourceBundleMacImageTest);
};

// Verifies that ResourceBundle searches the Material Design data pack before
// the default data pack, and that the returned image contains only the
// representation from the Material Design pack.
TEST_F(ResourceBundleMacImageTest, CheckImageFromMaterialDesign) {
  // Create two .pak files, each containing a single image with the
  // same asset ID but different sizes (note that the images must be
  // different sizes in this test in order to correctly determine
  // from which data pack the asset was pulled). Note also that the value
  // of |material_size| was chosen to be divisible by 3, since iOS may
  // use this scale factor.
  const int default_size = 10;
  const int material_size = 48;
  ASSERT_NE(default_size, material_size);
  base::FilePath default_path = dir_path().AppendASCII("default.pak");
  base::FilePath material_path = dir_path().AppendASCII("material.pak");
  CreateDataPackWithSingleBitmap(default_path,
                                 default_size,
                                 base::StringPiece());
  CreateDataPackWithSingleBitmap(material_path,
                                 material_size,
                                 base::StringPiece());

  ScaleFactor scale_factor = SCALE_FACTOR_100P;
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();

  // Load the 'material' data pack into ResourceBundle first.
  resource_bundle->AddMaterialDesignDataPackFromPath(material_path,
                                                     scale_factor);
  resource_bundle->AddDataPackFromPath(default_path, scale_factor);

  // Confirm that there are two data packs available.
  const unsigned int data_packs_size = resource_bundle->data_packs_.size();
  const unsigned int expected_size = 2;
  EXPECT_EQ(expected_size, data_packs_size);

  const size_t md_data_pack_count =
      NumberOfMaterialDesignDataPacksInResourceBundle(resource_bundle);
  const size_t expected_pack_count = 1;
  EXPECT_EQ(expected_pack_count, md_data_pack_count);

  // Normally with two packs containing the same image, GetNativeImageNamed()
  // returns an NSImage that contains both representations. With the Material
  // Design pack, any images it contains should override the ones in the default
  // pack, so if both packs contain the same image, the returned NSImage should
  // contain just a single representation.
  NSImage* icon = resource_bundle->GetNativeImageNamed(3).ToNSImage();
  const unsigned long representations_count = [[icon representations] count];
  const unsigned long expected_count = 1;
  EXPECT_EQ(expected_count, representations_count);
}

}  // namespace ui
