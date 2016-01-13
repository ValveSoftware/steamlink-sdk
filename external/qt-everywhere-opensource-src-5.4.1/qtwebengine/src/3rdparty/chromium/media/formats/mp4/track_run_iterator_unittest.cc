// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_split.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/rcheck.h"
#include "media/formats/mp4/track_run_iterator.h"
#include "testing/gtest/include/gtest/gtest.h"

// The sum of the elements in a vector initialized with SumAscending,
// less the value of the last element.
static const int kSumAscending1 = 45;

static const int kAudioScale = 48000;
static const int kVideoScale = 25;

static const uint8 kAuxInfo[] = {
  0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x31,
  0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x32,
  0x00, 0x02,
  0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x03, 0x00, 0x00, 0x00, 0x04
};

static const char kIv1[] = {
  0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x31,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8 kKeyId[] = {
  0x41, 0x47, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x54,
  0x65, 0x73, 0x74, 0x4b, 0x65, 0x79, 0x49, 0x44
};

static const uint8 kCencSampleGroupKeyId[] = {
  0x46, 0x72, 0x61, 0x67, 0x53, 0x61, 0x6d, 0x70,
  0x6c, 0x65, 0x47, 0x72, 0x6f, 0x75, 0x70, 0x4b
};

namespace media {
namespace mp4 {

class TrackRunIteratorTest : public testing::Test {
 public:
  TrackRunIteratorTest() {
    CreateMovie();
  }

 protected:
  Movie moov_;
  LogCB log_cb_;
  scoped_ptr<TrackRunIterator> iter_;

  void CreateMovie() {
    moov_.header.timescale = 1000;
    moov_.tracks.resize(3);
    moov_.extends.tracks.resize(2);
    moov_.tracks[0].header.track_id = 1;
    moov_.tracks[0].media.header.timescale = kAudioScale;
    SampleDescription& desc1 =
        moov_.tracks[0].media.information.sample_table.description;
    AudioSampleEntry aud_desc;
    aud_desc.format = FOURCC_MP4A;
    aud_desc.sinf.info.track_encryption.is_encrypted = false;
    desc1.type = kAudio;
    desc1.audio_entries.push_back(aud_desc);
    moov_.extends.tracks[0].track_id = 1;
    moov_.extends.tracks[0].default_sample_description_index = 1;
    moov_.tracks[0].media.information.sample_table.sync_sample.is_present =
        false;
    moov_.tracks[1].header.track_id = 2;
    moov_.tracks[1].media.header.timescale = kVideoScale;
    SampleDescription& desc2 =
        moov_.tracks[1].media.information.sample_table.description;
    VideoSampleEntry vid_desc;
    vid_desc.format = FOURCC_AVC1;
    vid_desc.sinf.info.track_encryption.is_encrypted = false;
    desc2.type = kVideo;
    desc2.video_entries.push_back(vid_desc);
    moov_.extends.tracks[1].track_id = 2;
    moov_.extends.tracks[1].default_sample_description_index = 1;
    SyncSample& video_sync_sample =
        moov_.tracks[1].media.information.sample_table.sync_sample;
    video_sync_sample.is_present = true;
    video_sync_sample.entries.resize(1);
    video_sync_sample.entries[0] = 0;

    moov_.tracks[2].header.track_id = 3;
    moov_.tracks[2].media.information.sample_table.description.type = kHint;
  }

  uint32 ToSampleFlags(const std::string& str) {
    CHECK_EQ(str.length(), 2u);

    SampleDependsOn sample_depends_on = kSampleDependsOnReserved;
    bool is_non_sync_sample = false;
    switch(str[0]) {
      case 'U':
        sample_depends_on = kSampleDependsOnUnknown;
        break;
      case 'O':
        sample_depends_on = kSampleDependsOnOthers;
        break;
      case 'N':
        sample_depends_on = kSampleDependsOnNoOther;
        break;
      default:
        CHECK(false) << "Invalid sample dependency character '"
                     << str[0] << "'";
        break;
    }

    switch(str[1]) {
      case 'S':
        is_non_sync_sample = false;
        break;
      case 'N':
        is_non_sync_sample = true;
        break;
      default:
        CHECK(false) << "Invalid sync sample character '"
                     << str[1] << "'";
        break;
    }
    uint32 flags = static_cast<uint32>(sample_depends_on) << 24;
    if (is_non_sync_sample)
      flags |= kSampleIsNonSyncSample;
    return flags;
  }

  void SetFlagsOnSamples(const std::string& sample_info,
                         TrackFragmentRun* trun) {
    // US - SampleDependsOnUnknown & IsSyncSample
    // UN - SampleDependsOnUnknown & IsNonSyncSample
    // OS - SampleDependsOnOthers & IsSyncSample
    // ON - SampleDependsOnOthers & IsNonSyncSample
    // NS - SampleDependsOnNoOthers & IsSyncSample
    // NN - SampleDependsOnNoOthers & IsNonSyncSample
    std::vector<std::string> flags_data;
    base::SplitString(sample_info, ' ', &flags_data);

    if (flags_data.size() == 1u) {
      // Simulates the first_sample_flags_present set scenario,
      // where only one sample_flag value is set and the default
      // flags are used for everything else.
      ASSERT_GE(trun->sample_count, flags_data.size());
    } else {
      ASSERT_EQ(trun->sample_count, flags_data.size());
    }

    trun->sample_flags.resize(flags_data.size());
    for (size_t i = 0; i < flags_data.size(); i++)
      trun->sample_flags[i] = ToSampleFlags(flags_data[i]);
  }

  std::string KeyframeAndRAPInfo(TrackRunIterator* iter) {
    CHECK(iter->IsRunValid());
    std::stringstream ss;
    ss << iter->track_id();

    while (iter->IsSampleValid()) {
      ss << " " << (iter->is_keyframe() ? "K" : "P");
      if (iter->is_random_access_point())
        ss << "R";
      iter->AdvanceSample();
    }

    return ss.str();
  }

  MovieFragment CreateFragment() {
    MovieFragment moof;
    moof.tracks.resize(2);
    moof.tracks[0].decode_time.decode_time = 0;
    moof.tracks[0].header.track_id = 1;
    moof.tracks[0].header.has_default_sample_flags = true;
    moof.tracks[0].header.default_sample_flags = ToSampleFlags("US");
    moof.tracks[0].header.default_sample_duration = 1024;
    moof.tracks[0].header.default_sample_size = 4;
    moof.tracks[0].runs.resize(2);
    moof.tracks[0].runs[0].sample_count = 10;
    moof.tracks[0].runs[0].data_offset = 100;
    SetAscending(&moof.tracks[0].runs[0].sample_sizes);

    moof.tracks[0].runs[1].sample_count = 10;
    moof.tracks[0].runs[1].data_offset = 10000;

    moof.tracks[1].header.track_id = 2;
    moof.tracks[1].header.has_default_sample_flags = false;
    moof.tracks[1].decode_time.decode_time = 10;
    moof.tracks[1].runs.resize(1);
    moof.tracks[1].runs[0].sample_count = 10;
    moof.tracks[1].runs[0].data_offset = 200;
    SetAscending(&moof.tracks[1].runs[0].sample_sizes);
    SetAscending(&moof.tracks[1].runs[0].sample_durations);
    SetFlagsOnSamples("US UN UN UN UN UN UN UN UN UN", &moof.tracks[1].runs[0]);

    return moof;
  }

  // Update the first sample description of a Track to indicate encryption
  void AddEncryption(Track* track) {
    SampleDescription* stsd =
        &track->media.information.sample_table.description;
    ProtectionSchemeInfo* sinf;
    if (!stsd->video_entries.empty()) {
       sinf = &stsd->video_entries[0].sinf;
    } else {
       sinf = &stsd->audio_entries[0].sinf;
    }

    sinf->type.type = FOURCC_CENC;
    sinf->info.track_encryption.is_encrypted = true;
    sinf->info.track_encryption.default_iv_size = 8;
    sinf->info.track_encryption.default_kid.assign(kKeyId,
                                                   kKeyId + arraysize(kKeyId));
  }

  // Add SampleGroupDescription Box with two entries (an unencrypted entry and
  // an encrypted entry). Populate SampleToGroup Box from input array.
  void AddCencSampleGroup(TrackFragment* frag,
                          const SampleToGroupEntry* sample_to_group_entries,
                          size_t num_entries) {
    frag->sample_group_description.grouping_type = FOURCC_SEIG;
    frag->sample_group_description.entries.resize(2);
    frag->sample_group_description.entries[0].is_encrypted = false;
    frag->sample_group_description.entries[0].iv_size = 0;
    frag->sample_group_description.entries[1].is_encrypted = true;
    frag->sample_group_description.entries[1].iv_size = 8;
    frag->sample_group_description.entries[1].key_id.assign(
        kCencSampleGroupKeyId,
        kCencSampleGroupKeyId + arraysize(kCencSampleGroupKeyId));

    frag->sample_to_group.grouping_type = FOURCC_SEIG;
    frag->sample_to_group.entries.assign(sample_to_group_entries,
                                         sample_to_group_entries + num_entries);
  }

  // Add aux info covering the first track run to a TrackFragment, and update
  // the run to ensure it matches length and subsample information.
  void AddAuxInfoHeaders(int offset, TrackFragment* frag) {
    frag->auxiliary_offset.offsets.push_back(offset);
    frag->auxiliary_size.sample_count = 2;
    frag->auxiliary_size.sample_info_sizes.push_back(8);
    frag->auxiliary_size.sample_info_sizes.push_back(22);
    frag->runs[0].sample_count = 2;
    frag->runs[0].sample_sizes[1] = 10;
  }

  bool InitMoofWithArbitraryAuxInfo(MovieFragment* moof) {
    // Add aux info header (equal sized aux info for every sample).
    for (uint32 i = 0; i < moof->tracks.size(); ++i) {
      moof->tracks[i].auxiliary_offset.offsets.push_back(50);
      moof->tracks[i].auxiliary_size.sample_count = 10;
      moof->tracks[i].auxiliary_size.default_sample_info_size = 8;
    }

    // We don't care about the actual data in aux.
    std::vector<uint8> aux_info(1000);
    return iter_->Init(*moof) &&
           iter_->CacheAuxInfo(&aux_info[0], aux_info.size());
  }

  void SetAscending(std::vector<uint32>* vec) {
    vec->resize(10);
    for (size_t i = 0; i < vec->size(); i++)
      (*vec)[i] = i+1;
  }
};

TEST_F(TrackRunIteratorTest, NoRunsTest) {
  iter_.reset(new TrackRunIterator(&moov_, log_cb_));
  ASSERT_TRUE(iter_->Init(MovieFragment()));
  EXPECT_FALSE(iter_->IsRunValid());
  EXPECT_FALSE(iter_->IsSampleValid());
}

TEST_F(TrackRunIteratorTest, BasicOperationTest) {
  iter_.reset(new TrackRunIterator(&moov_, log_cb_));
  MovieFragment moof = CreateFragment();

  // Test that runs are sorted correctly, and that properties of the initial
  // sample of the first run are correct
  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_TRUE(iter_->IsRunValid());
  EXPECT_FALSE(iter_->is_encrypted());
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->sample_offset(), 100);
  EXPECT_EQ(iter_->sample_size(), 1);
  EXPECT_EQ(iter_->dts(), TimeDeltaFromRational(0, kAudioScale));
  EXPECT_EQ(iter_->cts(), TimeDeltaFromRational(0, kAudioScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromRational(1024, kAudioScale));
  EXPECT_TRUE(iter_->is_keyframe());

  // Advance to the last sample in the current run, and test its properties
  for (int i = 0; i < 9; i++) iter_->AdvanceSample();
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->sample_offset(), 100 + kSumAscending1);
  EXPECT_EQ(iter_->sample_size(), 10);
  EXPECT_EQ(iter_->dts(), TimeDeltaFromRational(1024 * 9, kAudioScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromRational(1024, kAudioScale));
  EXPECT_TRUE(iter_->is_keyframe());

  // Test end-of-run
  iter_->AdvanceSample();
  EXPECT_FALSE(iter_->IsSampleValid());

  // Test last sample of next run
  iter_->AdvanceRun();
  EXPECT_TRUE(iter_->is_keyframe());
  for (int i = 0; i < 9; i++) iter_->AdvanceSample();
  EXPECT_EQ(iter_->track_id(), 2u);
  EXPECT_EQ(iter_->sample_offset(), 200 + kSumAscending1);
  EXPECT_EQ(iter_->sample_size(), 10);
  int64 base_dts = kSumAscending1 + moof.tracks[1].decode_time.decode_time;
  EXPECT_EQ(iter_->dts(), TimeDeltaFromRational(base_dts, kVideoScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromRational(10, kVideoScale));
  EXPECT_FALSE(iter_->is_keyframe());

  // Test final run
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->dts(), TimeDeltaFromRational(1024 * 10, kAudioScale));
  iter_->AdvanceSample();
  EXPECT_EQ(moof.tracks[0].runs[1].data_offset +
            moof.tracks[0].header.default_sample_size,
            iter_->sample_offset());
  iter_->AdvanceRun();
  EXPECT_FALSE(iter_->IsRunValid());
}

TEST_F(TrackRunIteratorTest, TrackExtendsDefaultsTest) {
  moov_.extends.tracks[0].default_sample_duration = 50;
  moov_.extends.tracks[0].default_sample_size = 3;
  moov_.extends.tracks[0].default_sample_flags = ToSampleFlags("UN");
  iter_.reset(new TrackRunIterator(&moov_, log_cb_));
  MovieFragment moof = CreateFragment();
  moof.tracks[0].header.has_default_sample_flags = false;
  moof.tracks[0].header.default_sample_size = 0;
  moof.tracks[0].header.default_sample_duration = 0;
  moof.tracks[0].runs[0].sample_sizes.clear();
  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceSample();
  EXPECT_FALSE(iter_->is_keyframe());
  EXPECT_EQ(iter_->sample_size(), 3);
  EXPECT_EQ(iter_->sample_offset(), moof.tracks[0].runs[0].data_offset + 3);
  EXPECT_EQ(iter_->duration(), TimeDeltaFromRational(50, kAudioScale));
  EXPECT_EQ(iter_->dts(), TimeDeltaFromRational(50, kAudioScale));
}

TEST_F(TrackRunIteratorTest, FirstSampleFlagTest) {
  // Ensure that keyframes are flagged correctly in the face of BMFF boxes which
  // explicitly specify the flags for the first sample in a run and rely on
  // defaults for all subsequent samples
  iter_.reset(new TrackRunIterator(&moov_, log_cb_));
  MovieFragment moof = CreateFragment();
  moof.tracks[1].header.has_default_sample_flags = true;
  moof.tracks[1].header.default_sample_flags = ToSampleFlags("UN");
  SetFlagsOnSamples("US", &moof.tracks[1].runs[0]);

  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_EQ("1 KR KR KR KR KR KR KR KR KR KR", KeyframeAndRAPInfo(iter_.get()));

  iter_->AdvanceRun();
  EXPECT_EQ("2 KR P P P P P P P P P", KeyframeAndRAPInfo(iter_.get()));
}

TEST_F(TrackRunIteratorTest, ReorderingTest) {
  // Test frame reordering and edit list support. The frames have the following
  // decode timestamps:
  //
  //   0ms 40ms   120ms     240ms
  //   | 0 | 1  - | 2  -  - |
  //
  // ...and these composition timestamps, after edit list adjustment:
  //
  //   0ms 40ms       160ms  240ms
  //   | 0 | 2  -  -  | 1 - |

  // Create an edit list with one entry, with an initial start time of 80ms
  // (that is, 2 / kVideoTimescale) and a duration of zero (which is treated as
  // infinite according to 14496-12:2012). This will cause the first 80ms of the
  // media timeline - which will be empty, due to CTS biasing - to be discarded.
  iter_.reset(new TrackRunIterator(&moov_, log_cb_));
  EditListEntry entry;
  entry.segment_duration = 0;
  entry.media_time = 2;
  entry.media_rate_integer = 1;
  entry.media_rate_fraction = 0;
  moov_.tracks[1].edit.list.edits.push_back(entry);

  // Add CTS offsets. Without bias, the CTS offsets for the first three frames
  // would simply be [0, 3, -2]. Since CTS offsets should be non-negative for
  // maximum compatibility, these values are biased up to [2, 5, 0], and the
  // extra 80ms is removed via the edit list.
  MovieFragment moof = CreateFragment();
  std::vector<int32>& cts_offsets =
    moof.tracks[1].runs[0].sample_composition_time_offsets;
  cts_offsets.resize(10);
  cts_offsets[0] = 2;
  cts_offsets[1] = 5;
  cts_offsets[2] = 0;
  moof.tracks[1].decode_time.decode_time = 0;

  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->dts(), TimeDeltaFromRational(0, kVideoScale));
  EXPECT_EQ(iter_->cts(), TimeDeltaFromRational(0, kVideoScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromRational(1, kVideoScale));
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->dts(), TimeDeltaFromRational(1, kVideoScale));
  EXPECT_EQ(iter_->cts(), TimeDeltaFromRational(4, kVideoScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromRational(2, kVideoScale));
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->dts(), TimeDeltaFromRational(3, kVideoScale));
  EXPECT_EQ(iter_->cts(), TimeDeltaFromRational(1, kVideoScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromRational(3, kVideoScale));
}

TEST_F(TrackRunIteratorTest, IgnoreUnknownAuxInfoTest) {
  iter_.reset(new TrackRunIterator(&moov_, log_cb_));
  MovieFragment moof = CreateFragment();
  moof.tracks[1].auxiliary_offset.offsets.push_back(50);
  moof.tracks[1].auxiliary_size.default_sample_info_size = 2;
  moof.tracks[1].auxiliary_size.sample_count = 2;
  moof.tracks[1].runs[0].sample_count = 2;
  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceRun();
  EXPECT_FALSE(iter_->AuxInfoNeedsToBeCached());
}

TEST_F(TrackRunIteratorTest, DecryptConfigTest) {
  AddEncryption(&moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_, log_cb_));

  MovieFragment moof = CreateFragment();
  AddAuxInfoHeaders(50, &moof.tracks[1]);

  ASSERT_TRUE(iter_->Init(moof));

  // The run for track 2 will be first, since its aux info offset is the first
  // element in the file.
  EXPECT_EQ(iter_->track_id(), 2u);
  EXPECT_TRUE(iter_->is_encrypted());
  EXPECT_TRUE(iter_->AuxInfoNeedsToBeCached());
  EXPECT_EQ(static_cast<uint32>(iter_->aux_info_size()), arraysize(kAuxInfo));
  EXPECT_EQ(iter_->aux_info_offset(), 50);
  EXPECT_EQ(iter_->GetMaxClearOffset(), 50);
  EXPECT_FALSE(iter_->CacheAuxInfo(NULL, 0));
  EXPECT_FALSE(iter_->CacheAuxInfo(kAuxInfo, 3));
  EXPECT_TRUE(iter_->AuxInfoNeedsToBeCached());
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, arraysize(kAuxInfo)));
  EXPECT_FALSE(iter_->AuxInfoNeedsToBeCached());
  EXPECT_EQ(iter_->sample_offset(), 200);
  EXPECT_EQ(iter_->GetMaxClearOffset(), moof.tracks[0].runs[0].data_offset);
  scoped_ptr<DecryptConfig> config = iter_->GetDecryptConfig();
  ASSERT_EQ(arraysize(kKeyId), config->key_id().size());
  EXPECT_TRUE(!memcmp(kKeyId, config->key_id().data(),
                      config->key_id().size()));
  ASSERT_EQ(arraysize(kIv1), config->iv().size());
  EXPECT_TRUE(!memcmp(kIv1, config->iv().data(), config->iv().size()));
  EXPECT_TRUE(config->subsamples().empty());
  iter_->AdvanceSample();
  config = iter_->GetDecryptConfig();
  EXPECT_EQ(config->subsamples().size(), 2u);
  EXPECT_EQ(config->subsamples()[0].clear_bytes, 1u);
  EXPECT_EQ(config->subsamples()[1].cypher_bytes, 4u);
}

TEST_F(TrackRunIteratorTest, CencSampleGroupTest) {
  MovieFragment moof = CreateFragment();

  const SampleToGroupEntry kSampleToGroupTable[] = {
      // Associated with the second entry in SampleGroupDescription Box.
      {1, SampleToGroupEntry::kFragmentGroupDescriptionIndexBase + 2},
      // Associated with the first entry in SampleGroupDescription Box.
      {1, SampleToGroupEntry::kFragmentGroupDescriptionIndexBase + 1}};
  AddCencSampleGroup(
      &moof.tracks[0], kSampleToGroupTable, arraysize(kSampleToGroupTable));

  iter_.reset(new TrackRunIterator(&moov_, log_cb_));
  ASSERT_TRUE(InitMoofWithArbitraryAuxInfo(&moof));

  std::string cenc_sample_group_key_id(
      kCencSampleGroupKeyId,
      kCencSampleGroupKeyId + arraysize(kCencSampleGroupKeyId));
  // The first sample is encrypted and the second sample is unencrypted.
  EXPECT_TRUE(iter_->is_encrypted());
  EXPECT_EQ(cenc_sample_group_key_id, iter_->GetDecryptConfig()->key_id());
  iter_->AdvanceSample();
  EXPECT_FALSE(iter_->is_encrypted());
}

TEST_F(TrackRunIteratorTest, CencSampleGroupWithTrackEncryptionBoxTest) {
  // Add TrackEncryption Box.
  AddEncryption(&moov_.tracks[0]);

  MovieFragment moof = CreateFragment();

  const SampleToGroupEntry kSampleToGroupTable[] = {
      // Associated with the second entry in SampleGroupDescription Box.
      {2, SampleToGroupEntry::kFragmentGroupDescriptionIndexBase + 2},
      // Associated with the default values specified in TrackEncryption Box.
      {4, 0},
      // Associated with the first entry in SampleGroupDescription Box.
      {3, SampleToGroupEntry::kFragmentGroupDescriptionIndexBase + 1}};
  AddCencSampleGroup(
      &moof.tracks[0], kSampleToGroupTable, arraysize(kSampleToGroupTable));

  iter_.reset(new TrackRunIterator(&moov_, log_cb_));
  ASSERT_TRUE(InitMoofWithArbitraryAuxInfo(&moof));

  std::string track_encryption_key_id(kKeyId, kKeyId + arraysize(kKeyId));
  std::string cenc_sample_group_key_id(
      kCencSampleGroupKeyId,
      kCencSampleGroupKeyId + arraysize(kCencSampleGroupKeyId));

  for (size_t i = 0; i < kSampleToGroupTable[0].sample_count; ++i) {
    EXPECT_TRUE(iter_->is_encrypted());
    EXPECT_EQ(cenc_sample_group_key_id, iter_->GetDecryptConfig()->key_id());
    iter_->AdvanceSample();
  }

  for (size_t i = 0; i < kSampleToGroupTable[1].sample_count; ++i) {
    EXPECT_TRUE(iter_->is_encrypted());
    EXPECT_EQ(track_encryption_key_id, iter_->GetDecryptConfig()->key_id());
    iter_->AdvanceSample();
  }

  for (size_t i = 0; i < kSampleToGroupTable[2].sample_count; ++i) {
    EXPECT_FALSE(iter_->is_encrypted());
    iter_->AdvanceSample();
  }

  // The remaining samples should be associated with the default values
  // specified in TrackEncryption Box.
  EXPECT_TRUE(iter_->is_encrypted());
  EXPECT_EQ(track_encryption_key_id, iter_->GetDecryptConfig()->key_id());
}

// It is legal for aux info blocks to be shared among multiple formats.
TEST_F(TrackRunIteratorTest, SharedAuxInfoTest) {
  AddEncryption(&moov_.tracks[0]);
  AddEncryption(&moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_, log_cb_));

  MovieFragment moof = CreateFragment();
  moof.tracks[0].runs.resize(1);
  AddAuxInfoHeaders(50, &moof.tracks[0]);
  AddAuxInfoHeaders(50, &moof.tracks[1]);
  moof.tracks[0].auxiliary_size.default_sample_info_size = 8;

  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->aux_info_offset(), 50);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, arraysize(kAuxInfo)));
  scoped_ptr<DecryptConfig> config = iter_->GetDecryptConfig();
  ASSERT_EQ(arraysize(kIv1), config->iv().size());
  EXPECT_TRUE(!memcmp(kIv1, config->iv().data(), config->iv().size()));
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->GetMaxClearOffset(), 50);
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->GetMaxClearOffset(), 50);
  EXPECT_EQ(iter_->aux_info_offset(), 50);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, arraysize(kAuxInfo)));
  EXPECT_EQ(iter_->GetMaxClearOffset(), 200);
  ASSERT_EQ(arraysize(kIv1), config->iv().size());
  EXPECT_TRUE(!memcmp(kIv1, config->iv().data(), config->iv().size()));
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->GetMaxClearOffset(), 201);
}

// Sensible files are expected to place auxiliary information for a run
// immediately before the main data for that run. Alternative schemes are
// possible, however, including the somewhat reasonable behavior of placing all
// aux info at the head of the 'mdat' box together, and the completely
// unreasonable behavior demonstrated here:
//  byte 50: track 2, run 1 aux info
//  byte 100: track 1, run 1 data
//  byte 200: track 2, run 1 data
//  byte 201: track 1, run 2 aux info (*inside* track 2, run 1 data)
//  byte 10000: track 1, run 2 data
//  byte 20000: track 1, run 1 aux info
TEST_F(TrackRunIteratorTest, UnexpectedOrderingTest) {
  AddEncryption(&moov_.tracks[0]);
  AddEncryption(&moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_, log_cb_));

  MovieFragment moof = CreateFragment();
  AddAuxInfoHeaders(20000, &moof.tracks[0]);
  moof.tracks[0].auxiliary_offset.offsets.push_back(201);
  moof.tracks[0].auxiliary_size.sample_count += 2;
  moof.tracks[0].auxiliary_size.default_sample_info_size = 8;
  moof.tracks[0].runs[1].sample_count = 2;
  AddAuxInfoHeaders(50, &moof.tracks[1]);
  moof.tracks[1].runs[0].sample_sizes[0] = 5;

  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_EQ(iter_->track_id(), 2u);
  EXPECT_EQ(iter_->aux_info_offset(), 50);
  EXPECT_EQ(iter_->sample_offset(), 200);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, arraysize(kAuxInfo)));
  EXPECT_EQ(iter_->GetMaxClearOffset(), 100);
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->aux_info_offset(), 20000);
  EXPECT_EQ(iter_->sample_offset(), 100);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, arraysize(kAuxInfo)));
  EXPECT_EQ(iter_->GetMaxClearOffset(), 100);
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->GetMaxClearOffset(), 101);
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->aux_info_offset(), 201);
  EXPECT_EQ(iter_->sample_offset(), 10000);
  EXPECT_EQ(iter_->GetMaxClearOffset(), 201);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, arraysize(kAuxInfo)));
  EXPECT_EQ(iter_->GetMaxClearOffset(), 10000);
}

TEST_F(TrackRunIteratorTest, MissingAndEmptyStss) {
  MovieFragment moof = CreateFragment();

  // Setup track 0 to not have an stss box, which means that all samples should
  // be marked as random access points unless the kSampleIsNonSyncSample flag is
  // set in the sample flags.
  moov_.tracks[0].media.information.sample_table.sync_sample.is_present = false;
  moov_.tracks[0].media.information.sample_table.sync_sample.entries.resize(0);
  moof.tracks[0].runs.resize(1);
  moof.tracks[0].runs[0].sample_count = 6;
  moof.tracks[0].runs[0].data_offset = 100;
  SetFlagsOnSamples("US UN OS ON NS NN", &moof.tracks[0].runs[0]);

  // Setup track 1 to have an stss box with no entries, which normally means
  // that none of the samples should be random access points. If the
  // kSampleIsNonSyncSample flag is NOT set though, the sample should be
  // considered a random access point.
  moov_.tracks[1].media.information.sample_table.sync_sample.is_present = true;
  moov_.tracks[1].media.information.sample_table.sync_sample.entries.resize(0);
  moof.tracks[1].runs.resize(1);
  moof.tracks[1].runs[0].sample_count = 6;
  moof.tracks[1].runs[0].data_offset = 200;
  SetFlagsOnSamples("US UN OS ON NS NN", &moof.tracks[1].runs[0]);

  iter_.reset(new TrackRunIterator(&moov_, log_cb_));

  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_TRUE(iter_->IsRunValid());

  // Verify that all samples except for the ones that have the
  // kSampleIsNonSyncSample flag set are marked as random access points.
  EXPECT_EQ("1 KR P PR P KR K", KeyframeAndRAPInfo(iter_.get()));

  iter_->AdvanceRun();

  // Verify that nothing is marked as a random access point.
  EXPECT_EQ("2 KR P PR P KR K", KeyframeAndRAPInfo(iter_.get()));
}


}  // namespace mp4
}  // namespace media
