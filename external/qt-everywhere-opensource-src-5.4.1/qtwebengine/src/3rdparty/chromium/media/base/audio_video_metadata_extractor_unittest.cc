// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/sha1.h"
#include "build/build_config.h"
#include "media/base/audio_video_metadata_extractor.h"
#include "media/base/test_data_util.h"
#include "media/filters/file_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

scoped_ptr<AudioVideoMetadataExtractor> GetExtractor(
    const std::string& filename,
    bool extract_attached_images,
    bool expected_result,
    double expected_duration,
    int expected_width,
    int expected_height) {
  FileDataSource source;
  EXPECT_TRUE(source.Initialize(GetTestDataFilePath(filename)));

  scoped_ptr<AudioVideoMetadataExtractor> extractor(
      new AudioVideoMetadataExtractor);
  bool extracted = extractor->Extract(&source, extract_attached_images);
  EXPECT_EQ(expected_result, extracted);

  if (!extracted)
    return extractor.Pass();

  EXPECT_EQ(expected_duration, extractor->duration());

  EXPECT_EQ(expected_width, extractor->width());
  EXPECT_EQ(expected_height, extractor->height());

  return extractor.Pass();
}

TEST(AudioVideoMetadataExtractorTest, InvalidFile) {
  GetExtractor("ten_byte_file", true, false, 0, -1, -1);
}

TEST(AudioVideoMetadataExtractorTest, AudioOGG) {
  scoped_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("9ch.ogg", true, true, 0, -1, -1);
  EXPECT_EQ("Processed by SoX", extractor->comment());

  EXPECT_EQ("ogg", extractor->stream_infos()[0].type);
  EXPECT_EQ(2u, extractor->stream_infos().size());

  EXPECT_EQ(0u, extractor->stream_infos()[0].tags.size());

  EXPECT_EQ(1u, extractor->stream_infos()[1].tags.size());
  EXPECT_EQ("vorbis", extractor->stream_infos()[1].type);
  EXPECT_EQ("Processed by SoX",
            extractor->stream_infos()[1].tags.find("COMMENT")->second);

  EXPECT_EQ(0u, extractor->attached_images_bytes().size());
}

TEST(AudioVideoMetadataExtractorTest, AudioWAV) {
  scoped_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("sfx_u8.wav", true, true, 0, -1, -1);
  EXPECT_EQ("Lavf54.37.100", extractor->encoder());
  EXPECT_EQ("Amadeus Pro", extractor->encoded_by());

  EXPECT_EQ("wav", extractor->stream_infos()[0].type);
  EXPECT_EQ(2u, extractor->stream_infos().size());

  EXPECT_EQ(2u, extractor->stream_infos()[0].tags.size());
  EXPECT_EQ("Lavf54.37.100",
            extractor->stream_infos()[0].tags.find("encoder")->second);
  EXPECT_EQ("Amadeus Pro",
            extractor->stream_infos()[0].tags.find("encoded_by")->second);

  EXPECT_EQ("pcm_u8", extractor->stream_infos()[1].type);
  EXPECT_EQ(0u, extractor->stream_infos()[1].tags.size());

  EXPECT_EQ(0u, extractor->attached_images_bytes().size());
}

TEST(AudioVideoMetadataExtractorTest, VideoWebM) {
  scoped_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("bear-320x240-multitrack.webm", true, true, 2, 320, 240);
  EXPECT_EQ("Lavf53.9.0", extractor->encoder());

  EXPECT_EQ(6u, extractor->stream_infos().size());

  EXPECT_EQ("matroska,webm", extractor->stream_infos()[0].type);
  EXPECT_EQ(1u, extractor->stream_infos()[0].tags.size());
  EXPECT_EQ("Lavf53.9.0",
            extractor->stream_infos()[0].tags.find("ENCODER")->second);

  EXPECT_EQ("vp8", extractor->stream_infos()[1].type);
  EXPECT_EQ(0u, extractor->stream_infos()[1].tags.size());

  EXPECT_EQ("vorbis", extractor->stream_infos()[2].type);
  EXPECT_EQ(0u, extractor->stream_infos()[2].tags.size());

  EXPECT_EQ("subrip", extractor->stream_infos()[3].type);
  EXPECT_EQ(0u, extractor->stream_infos()[3].tags.size());

  EXPECT_EQ("theora", extractor->stream_infos()[4].type);
  EXPECT_EQ(0u, extractor->stream_infos()[4].tags.size());

  EXPECT_EQ("pcm_s16le", extractor->stream_infos()[5].type);
  EXPECT_EQ(1u, extractor->stream_infos()[5].tags.size());
  EXPECT_EQ("Lavc52.32.0",
            extractor->stream_infos()[5].tags.find("ENCODER")->second);

  EXPECT_EQ(0u, extractor->attached_images_bytes().size());
}

#if defined(USE_PROPRIETARY_CODECS)
TEST(AudioVideoMetadataExtractorTest, AndroidRotatedMP4Video) {
  scoped_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("90rotation.mp4", true, true, 0, 1920, 1080);

  EXPECT_EQ(90, extractor->rotation());

  EXPECT_EQ(3u, extractor->stream_infos().size());

  EXPECT_EQ("mov,mp4,m4a,3gp,3g2,mj2", extractor->stream_infos()[0].type);
  EXPECT_EQ(4u, extractor->stream_infos()[0].tags.size());
  EXPECT_EQ(
      "isom3gp4",
      extractor->stream_infos()[0].tags.find("compatible_brands")->second);
  EXPECT_EQ(
      "2014-02-11 00:39:25",
      extractor->stream_infos()[0].tags.find("creation_time")->second);
  EXPECT_EQ("isom",
            extractor->stream_infos()[0].tags.find("major_brand")->second);
  EXPECT_EQ("0",
            extractor->stream_infos()[0].tags.find("minor_version")->second);

  EXPECT_EQ("h264", extractor->stream_infos()[1].type);
  EXPECT_EQ(5u, extractor->stream_infos()[1].tags.size());
  EXPECT_EQ("2014-02-11 00:39:25",
            extractor->stream_infos()[1].tags.find("creation_time")->second);
  EXPECT_EQ("VideoHandle",
            extractor->stream_infos()[1].tags.find("handler_name")->second);
  EXPECT_EQ("eng", extractor->stream_infos()[1].tags.find("language")->second);
  EXPECT_EQ("90", extractor->stream_infos()[1].tags.find("rotate")->second);

  EXPECT_EQ("aac", extractor->stream_infos()[2].type);
  EXPECT_EQ(3u, extractor->stream_infos()[2].tags.size());
  EXPECT_EQ("2014-02-11 00:39:25",
            extractor->stream_infos()[2].tags.find("creation_time")->second);
  EXPECT_EQ("SoundHandle",
            extractor->stream_infos()[2].tags.find("handler_name")->second);
  EXPECT_EQ("eng", extractor->stream_infos()[2].tags.find("language")->second);

  EXPECT_EQ(0u, extractor->attached_images_bytes().size());
}

TEST(AudioVideoMetadataExtractorTest, AudioMP3) {
  scoped_ptr<AudioVideoMetadataExtractor> extractor =
      GetExtractor("id3_png_test.mp3", true, true, 1, -1, -1);

  EXPECT_EQ("Airbag", extractor->title());
  EXPECT_EQ("Radiohead", extractor->artist());
  EXPECT_EQ("OK Computer", extractor->album());
  EXPECT_EQ(1, extractor->track());
  EXPECT_EQ("Alternative", extractor->genre());
  EXPECT_EQ("1997", extractor->date());
  EXPECT_EQ("Lavf54.4.100", extractor->encoder());

  EXPECT_EQ(3u, extractor->stream_infos().size());

  EXPECT_EQ("mp3", extractor->stream_infos()[0].type);
  EXPECT_EQ(7u, extractor->stream_infos()[0].tags.size());
  EXPECT_EQ("OK Computer",
            extractor->stream_infos()[0].tags.find("album")->second);
  EXPECT_EQ("Radiohead",
            extractor->stream_infos()[0].tags.find("artist")->second);
  EXPECT_EQ("1997", extractor->stream_infos()[0].tags.find("date")->second);
  EXPECT_EQ("Lavf54.4.100",
            extractor->stream_infos()[0].tags.find("encoder")->second);
  EXPECT_EQ("Alternative",
            extractor->stream_infos()[0].tags.find("genre")->second);
  EXPECT_EQ("Airbag", extractor->stream_infos()[0].tags.find("title")->second);
  EXPECT_EQ("1", extractor->stream_infos()[0].tags.find("track")->second);

  EXPECT_EQ("mp3", extractor->stream_infos()[1].type);
  EXPECT_EQ(0u, extractor->stream_infos()[1].tags.size());

  EXPECT_EQ("png", extractor->stream_infos()[2].type);
  EXPECT_EQ(2u, extractor->stream_infos()[2].tags.size());
  EXPECT_EQ("Other", extractor->stream_infos()[2].tags.find("comment")->second);
  EXPECT_EQ("", extractor->stream_infos()[2].tags.find("title")->second);

  EXPECT_EQ(1u, extractor->attached_images_bytes().size());
  EXPECT_EQ(155752u, extractor->attached_images_bytes()[0].size());

  EXPECT_EQ("\x89PNG\r\n\x1a\n",
            extractor->attached_images_bytes()[0].substr(0, 8));
  EXPECT_EQ("IEND\xae\x42\x60\x82",
            extractor->attached_images_bytes()[0].substr(
                extractor->attached_images_bytes()[0].size() - 8, 8));
  EXPECT_EQ("\xF3\xED\x8F\xC7\xC7\x98\xB9V|p\xC0u!\xB5\x82\xCF\x95\xF0\xCD\xCE",
            base::SHA1HashString(extractor->attached_images_bytes()[0]));
}
#endif

}  // namespace media
