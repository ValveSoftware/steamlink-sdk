// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/box_definitions.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_switches.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/formats/mp4/avc.h"
#include "media/formats/mp4/es_descriptor.h"
#include "media/formats/mp4/rcheck.h"
#include "media/media_features.h"

#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
#include "media/formats/mp4/hevc.h"
#endif

namespace media {
namespace mp4 {

FileType::FileType() {}
FileType::FileType(const FileType& other) = default;
FileType::~FileType() {}
FourCC FileType::BoxType() const { return FOURCC_FTYP; }

bool FileType::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFourCC(&major_brand) && reader->Read4(&minor_version));
  size_t num_brands = (reader->size() - reader->pos()) / sizeof(FourCC);
  return reader->SkipBytes(sizeof(FourCC) * num_brands);  // compatible_brands
}

ProtectionSystemSpecificHeader::ProtectionSystemSpecificHeader() {}
ProtectionSystemSpecificHeader::ProtectionSystemSpecificHeader(
    const ProtectionSystemSpecificHeader& other) = default;
ProtectionSystemSpecificHeader::~ProtectionSystemSpecificHeader() {}
FourCC ProtectionSystemSpecificHeader::BoxType() const { return FOURCC_PSSH; }

bool ProtectionSystemSpecificHeader::Parse(BoxReader* reader) {
  // Don't bother validating the box's contents.
  // Copy the entire box, including the header, for passing to EME as initData.
  DCHECK(raw_box.empty());
  raw_box.assign(reader->data(), reader->data() + reader->size());
  return true;
}

FullProtectionSystemSpecificHeader::FullProtectionSystemSpecificHeader() {}
FullProtectionSystemSpecificHeader::FullProtectionSystemSpecificHeader(
    const FullProtectionSystemSpecificHeader& other) = default;
FullProtectionSystemSpecificHeader::~FullProtectionSystemSpecificHeader() {}
FourCC FullProtectionSystemSpecificHeader::BoxType() const {
  return FOURCC_PSSH;
}

// The format of a 'pssh' box is as follows:
//   unsigned int(32) size;
//   unsigned int(32) type = "pssh";
//   if (size==1) {
//     unsigned int(64) largesize;
//   } else if (size==0) {
//     -- box extends to end of file
//   }
//   unsigned int(8) version;
//   bit(24) flags;
//   unsigned int(8)[16] SystemID;
//   if (version > 0)
//   {
//     unsigned int(32) KID_count;
//     {
//       unsigned int(8)[16] KID;
//     } [KID_count]
//   }
//   unsigned int(32) DataSize;
//   unsigned int(8)[DataSize] Data;

bool FullProtectionSystemSpecificHeader::Parse(mp4::BoxReader* reader) {
  RCHECK(reader->type() == BoxType() && reader->ReadFullBoxHeader());

  // Only versions 0 and 1 of the 'pssh' boxes are supported. Any other
  // versions are ignored.
  RCHECK(reader->version() == 0 || reader->version() == 1);
  RCHECK(reader->flags() == 0);
  RCHECK(reader->ReadVec(&system_id, 16));

  if (reader->version() > 0) {
    uint32_t kid_count;
    RCHECK(reader->Read4(&kid_count));
    for (uint32_t i = 0; i < kid_count; ++i) {
      std::vector<uint8_t> kid;
      RCHECK(reader->ReadVec(&kid, 16));
      key_ids.push_back(kid);
    }
  }

  uint32_t data_size;
  RCHECK(reader->Read4(&data_size));
  RCHECK(reader->ReadVec(&data, data_size));
  return true;
}

SampleAuxiliaryInformationOffset::SampleAuxiliaryInformationOffset() {}
SampleAuxiliaryInformationOffset::SampleAuxiliaryInformationOffset(
    const SampleAuxiliaryInformationOffset& other) = default;
SampleAuxiliaryInformationOffset::~SampleAuxiliaryInformationOffset() {}
FourCC SampleAuxiliaryInformationOffset::BoxType() const { return FOURCC_SAIO; }

bool SampleAuxiliaryInformationOffset::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->flags() & 1)
    RCHECK(reader->SkipBytes(8));

  uint32_t count;
  RCHECK(reader->Read4(&count) &&
         reader->HasBytes(count * (reader->version() == 1 ? 8 : 4)));
  offsets.resize(count);

  for (uint32_t i = 0; i < count; i++) {
    if (reader->version() == 1) {
      RCHECK(reader->Read8(&offsets[i]));
    } else {
      RCHECK(reader->Read4Into8(&offsets[i]));
    }
  }
  return true;
}

SampleAuxiliaryInformationSize::SampleAuxiliaryInformationSize()
  : default_sample_info_size(0), sample_count(0) {
}
SampleAuxiliaryInformationSize::SampleAuxiliaryInformationSize(
    const SampleAuxiliaryInformationSize& other) = default;
SampleAuxiliaryInformationSize::~SampleAuxiliaryInformationSize() {}
FourCC SampleAuxiliaryInformationSize::BoxType() const { return FOURCC_SAIZ; }

bool SampleAuxiliaryInformationSize::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->flags() & 1)
    RCHECK(reader->SkipBytes(8));

  RCHECK(reader->Read1(&default_sample_info_size) &&
         reader->Read4(&sample_count));
  if (default_sample_info_size == 0)
    return reader->ReadVec(&sample_info_sizes, sample_count);
  return true;
}

SampleEncryptionEntry::SampleEncryptionEntry() {}
SampleEncryptionEntry::SampleEncryptionEntry(
    const SampleEncryptionEntry& other) = default;
SampleEncryptionEntry::~SampleEncryptionEntry() {}

bool SampleEncryptionEntry::Parse(BufferReader* reader,
                                  uint8_t iv_size,
                                  bool has_subsamples) {
  // According to ISO/IEC FDIS 23001-7: CENC spec, IV should be either
  // 64-bit (8-byte) or 128-bit (16-byte).
  RCHECK(iv_size == 8 || iv_size == 16);

  memset(initialization_vector, 0, sizeof(initialization_vector));
  for (uint8_t i = 0; i < iv_size; i++)
    RCHECK(reader->Read1(initialization_vector + i));

  if (!has_subsamples) {
    subsamples.clear();
    return true;
  }

  uint16_t subsample_count;
  RCHECK(reader->Read2(&subsample_count));
  RCHECK(subsample_count > 0);
  subsamples.resize(subsample_count);
  for (SubsampleEntry& subsample : subsamples) {
    uint16_t clear_bytes;
    uint32_t cypher_bytes;
    RCHECK(reader->Read2(&clear_bytes) && reader->Read4(&cypher_bytes));
    subsample.clear_bytes = clear_bytes;
    subsample.cypher_bytes = cypher_bytes;
  }
  return true;
}

bool SampleEncryptionEntry::GetTotalSizeOfSubsamples(size_t* total_size) const {
  size_t size = 0;
  for (const SubsampleEntry& subsample : subsamples) {
    size += subsample.clear_bytes;
    RCHECK(size >= subsample.clear_bytes);  // overflow
    size += subsample.cypher_bytes;
    RCHECK(size >= subsample.cypher_bytes);  // overflow
  }
  *total_size = size;
  return true;
}

SampleEncryption::SampleEncryption() : use_subsample_encryption(false) {}
SampleEncryption::SampleEncryption(const SampleEncryption& other) = default;
SampleEncryption::~SampleEncryption() {}
FourCC SampleEncryption::BoxType() const {
  return FOURCC_SENC;
}

bool SampleEncryption::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  use_subsample_encryption = (reader->flags() & kUseSubsampleEncryption) != 0;
  sample_encryption_data.assign(reader->data() + reader->pos(),
                                reader->data() + reader->size());
  return true;
}

OriginalFormat::OriginalFormat() : format(FOURCC_NULL) {}
OriginalFormat::OriginalFormat(const OriginalFormat& other) = default;
OriginalFormat::~OriginalFormat() {}
FourCC OriginalFormat::BoxType() const { return FOURCC_FRMA; }

bool OriginalFormat::Parse(BoxReader* reader) {
  return reader->ReadFourCC(&format);
}

SchemeType::SchemeType() : type(FOURCC_NULL), version(0) {}
SchemeType::SchemeType(const SchemeType& other) = default;
SchemeType::~SchemeType() {}
FourCC SchemeType::BoxType() const { return FOURCC_SCHM; }

bool SchemeType::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->ReadFourCC(&type) &&
         reader->Read4(&version));
  return true;
}

TrackEncryption::TrackEncryption()
  : is_encrypted(false), default_iv_size(0) {
}
TrackEncryption::TrackEncryption(const TrackEncryption& other) = default;
TrackEncryption::~TrackEncryption() {}
FourCC TrackEncryption::BoxType() const { return FOURCC_TENC; }

bool TrackEncryption::Parse(BoxReader* reader) {
  uint8_t flag;
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->SkipBytes(2) &&
         reader->Read1(&flag) &&
         reader->Read1(&default_iv_size) &&
         reader->ReadVec(&default_kid, 16));
  is_encrypted = (flag != 0);
  if (is_encrypted) {
    RCHECK(default_iv_size == 8 || default_iv_size == 16);
  } else {
    RCHECK(default_iv_size == 0);
  }
  return true;
}

SchemeInfo::SchemeInfo() {}
SchemeInfo::SchemeInfo(const SchemeInfo& other) = default;
SchemeInfo::~SchemeInfo() {}
FourCC SchemeInfo::BoxType() const { return FOURCC_SCHI; }

bool SchemeInfo::Parse(BoxReader* reader) {
  return reader->ScanChildren() && reader->ReadChild(&track_encryption);
}

ProtectionSchemeInfo::ProtectionSchemeInfo() {}
ProtectionSchemeInfo::ProtectionSchemeInfo(const ProtectionSchemeInfo& other) =
    default;
ProtectionSchemeInfo::~ProtectionSchemeInfo() {}
FourCC ProtectionSchemeInfo::BoxType() const { return FOURCC_SINF; }

bool ProtectionSchemeInfo::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&format) &&
         reader->ReadChild(&type));
  if (type.type == FOURCC_CENC)
    RCHECK(reader->ReadChild(&info));
  // Other protection schemes are silently ignored. Since the protection scheme
  // type can't be determined until this box is opened, we return 'true' for
  // non-CENC protection scheme types. It is the parent box's responsibility to
  // ensure that this scheme type is a supported one.
  return true;
}

MovieHeader::MovieHeader()
    : creation_time(0),
      modification_time(0),
      timescale(0),
      duration(0),
      rate(-1),
      volume(-1),
      next_track_id(0) {}
MovieHeader::MovieHeader(const MovieHeader& other) = default;
MovieHeader::~MovieHeader() {}
FourCC MovieHeader::BoxType() const { return FOURCC_MVHD; }

bool MovieHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());

  if (reader->version() == 1) {
    RCHECK(reader->Read8(&creation_time) &&
           reader->Read8(&modification_time) &&
           reader->Read4(&timescale) &&
           reader->Read8(&duration));
  } else {
    RCHECK(reader->Read4Into8(&creation_time) &&
           reader->Read4Into8(&modification_time) &&
           reader->Read4(&timescale) &&
           reader->Read4Into8(&duration));
  }

  RCHECK(reader->Read4s(&rate) &&
         reader->Read2s(&volume) &&
         reader->SkipBytes(10) &&  // reserved
         reader->SkipBytes(36) &&  // matrix
         reader->SkipBytes(24) &&  // predefined zero
         reader->Read4(&next_track_id));
  return true;
}

TrackHeader::TrackHeader()
    : creation_time(0),
      modification_time(0),
      track_id(0),
      duration(0),
      layer(-1),
      alternate_group(-1),
      volume(-1),
      width(0),
      height(0) {}
TrackHeader::TrackHeader(const TrackHeader& other) = default;
TrackHeader::~TrackHeader() {}
FourCC TrackHeader::BoxType() const { return FOURCC_TKHD; }

bool TrackHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->version() == 1) {
    RCHECK(reader->Read8(&creation_time) &&
           reader->Read8(&modification_time) &&
           reader->Read4(&track_id) &&
           reader->SkipBytes(4) &&    // reserved
           reader->Read8(&duration));
  } else {
    RCHECK(reader->Read4Into8(&creation_time) &&
           reader->Read4Into8(&modification_time) &&
           reader->Read4(&track_id) &&
           reader->SkipBytes(4) &&   // reserved
           reader->Read4Into8(&duration));
  }

  RCHECK(reader->SkipBytes(8) &&  // reserved
         reader->Read2s(&layer) &&
         reader->Read2s(&alternate_group) &&
         reader->Read2s(&volume) &&
         reader->SkipBytes(2) &&  // reserved
         reader->SkipBytes(36) &&  // matrix
         reader->Read4(&width) &&
         reader->Read4(&height));

  // Round width and height to the nearest number.
  // Note: width and height are fixed-point 16.16 values. The following code
  // rounds a.1x to a + 1, and a.0x to a.
  width >>= 15;
  width += 1;
  width >>= 1;
  height >>= 15;
  height += 1;
  height >>= 1;

  return true;
}

SampleDescription::SampleDescription() : type(kInvalid) {}
SampleDescription::SampleDescription(const SampleDescription& other) = default;
SampleDescription::~SampleDescription() {}
FourCC SampleDescription::BoxType() const { return FOURCC_STSD; }

bool SampleDescription::Parse(BoxReader* reader) {
  uint32_t count;
  RCHECK(reader->SkipBytes(4) &&
         reader->Read4(&count));
  video_entries.clear();
  audio_entries.clear();

  // Note: this value is preset before scanning begins. See comments in the
  // Parse(Media*) function.
  if (type == kVideo) {
    RCHECK(reader->ReadAllChildren(&video_entries));
  } else if (type == kAudio) {
    RCHECK(reader->ReadAllChildren(&audio_entries));
  }
  return true;
}

SampleTable::SampleTable() {}
SampleTable::SampleTable(const SampleTable& other) = default;

SampleTable::~SampleTable() {}
FourCC SampleTable::BoxType() const { return FOURCC_STBL; }

bool SampleTable::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&description));
  // There could be multiple SampleGroupDescription boxes with different
  // grouping types. For common encryption, the relevant grouping type is
  // 'seig'. Continue reading until 'seig' is found, or until running out of
  // child boxes.
  while (reader->HasChild(&sample_group_description)) {
    RCHECK(reader->ReadChild(&sample_group_description));
    if (sample_group_description.grouping_type == FOURCC_SEIG)
      break;
    sample_group_description.entries.clear();
  }
  return true;
}

EditList::EditList() {}
EditList::EditList(const EditList& other) = default;
EditList::~EditList() {}
FourCC EditList::BoxType() const { return FOURCC_ELST; }

bool EditList::Parse(BoxReader* reader) {
  uint32_t count;
  RCHECK(reader->ReadFullBoxHeader() && reader->Read4(&count));

  if (reader->version() == 1) {
    RCHECK(reader->HasBytes(count * 20));
  } else {
    RCHECK(reader->HasBytes(count * 12));
  }
  edits.resize(count);

  for (std::vector<EditListEntry>::iterator edit = edits.begin();
       edit != edits.end(); ++edit) {
    if (reader->version() == 1) {
      RCHECK(reader->Read8(&edit->segment_duration) &&
             reader->Read8s(&edit->media_time));
    } else {
      RCHECK(reader->Read4Into8(&edit->segment_duration) &&
             reader->Read4sInto8s(&edit->media_time));
    }
    RCHECK(reader->Read2s(&edit->media_rate_integer) &&
           reader->Read2s(&edit->media_rate_fraction));
  }
  return true;
}

Edit::Edit() {}
Edit::Edit(const Edit& other) = default;
Edit::~Edit() {}
FourCC Edit::BoxType() const { return FOURCC_EDTS; }

bool Edit::Parse(BoxReader* reader) {
  return reader->ScanChildren() && reader->ReadChild(&list);
}

HandlerReference::HandlerReference() : type(kInvalid) {}
HandlerReference::HandlerReference(const HandlerReference& other) = default;
HandlerReference::~HandlerReference() {}
FourCC HandlerReference::BoxType() const { return FOURCC_HDLR; }

bool HandlerReference::Parse(BoxReader* reader) {
  FourCC hdlr_type;
  RCHECK(reader->ReadFullBoxHeader() && reader->SkipBytes(4) &&
         reader->ReadFourCC(&hdlr_type) && reader->SkipBytes(12));

  // Now we should be at the beginning of the |name| field of HDLR box. The
  // |name| is a zero-terminated ASCII string in ISO BMFF, but it was a
  // Pascal-style counted string in older QT/Mov formats. So we'll read the
  // remaining box bytes first, then if the last one is zero, we strip the last
  // zero byte, otherwise we'll string the first byte (containing the length of
  // the Pascal-style string).
  std::vector<uint8_t> name_bytes;
  RCHECK(reader->ReadVec(&name_bytes, reader->size() - reader->pos()));
  if (name_bytes.size() == 0) {
    name = "";
  } else if (name_bytes.back() == 0) {
    // This is a zero-terminated C-style string, exclude the last byte.
    name = std::string(name_bytes.begin(), name_bytes.end() - 1);
  } else {
    // Check that the length of the Pascal-style string is correct.
    RCHECK(name_bytes[0] == (name_bytes.size() - 1));
    // Skip the first byte, containing the length of the Pascal-string.
    name = std::string(name_bytes.begin() + 1, name_bytes.end());
  }

  if (hdlr_type == FOURCC_VIDE) {
    type = kVideo;
  } else if (hdlr_type == FOURCC_SOUN) {
    type = kAudio;
  } else if (hdlr_type == FOURCC_META || hdlr_type == FOURCC_SUBT ||
             hdlr_type == FOURCC_TEXT || hdlr_type == FOURCC_SBTL) {
    // For purposes of detection, we include 'sbtl' handler here. Note, though
    // that ISO-14496-12 and its 2012 Amendment 2, and the spec for sourcing
    // inband tracks all reference only 'text' or 'subt', and 14496-30
    // references only 'subt'. Yet ffmpeg can encode subtitles as 'sbtl'.
    type = kText;
  } else {
    type = kInvalid;
  }
  return true;
}

AVCDecoderConfigurationRecord::AVCDecoderConfigurationRecord()
    : version(0),
      profile_indication(0),
      profile_compatibility(0),
      avc_level(0),
      length_size(0) {}
AVCDecoderConfigurationRecord::AVCDecoderConfigurationRecord(
    const AVCDecoderConfigurationRecord& other) = default;
AVCDecoderConfigurationRecord::~AVCDecoderConfigurationRecord() {}
FourCC AVCDecoderConfigurationRecord::BoxType() const { return FOURCC_AVCC; }

bool AVCDecoderConfigurationRecord::Parse(BoxReader* reader) {
  return ParseInternal(reader, reader->media_log());
}

bool AVCDecoderConfigurationRecord::Parse(const uint8_t* data, int data_size) {
  BufferReader reader(data, data_size);
  return ParseInternal(&reader, new MediaLog());
}

bool AVCDecoderConfigurationRecord::ParseInternal(
    BufferReader* reader,
    const scoped_refptr<MediaLog>& media_log) {
  RCHECK(reader->Read1(&version) && version == 1 &&
         reader->Read1(&profile_indication) &&
         reader->Read1(&profile_compatibility) &&
         reader->Read1(&avc_level));

  uint8_t length_size_minus_one;
  RCHECK(reader->Read1(&length_size_minus_one));
  length_size = (length_size_minus_one & 0x3) + 1;

  RCHECK(length_size != 3); // Only values of 1, 2, and 4 are valid.

  uint8_t num_sps;
  RCHECK(reader->Read1(&num_sps));
  num_sps &= 0x1f;

  sps_list.resize(num_sps);
  for (int i = 0; i < num_sps; i++) {
    uint16_t sps_length;
    RCHECK(reader->Read2(&sps_length) &&
           reader->ReadVec(&sps_list[i], sps_length));
    RCHECK(sps_list[i].size() > 4);

    if (media_log.get()) {
      MEDIA_LOG(INFO, media_log) << "Video codec: avc1."
                                 << base::HexEncode(sps_list[i].data() + 1, 3);
    }
  }

  uint8_t num_pps;
  RCHECK(reader->Read1(&num_pps));

  pps_list.resize(num_pps);
  for (int i = 0; i < num_pps; i++) {
    uint16_t pps_length;
    RCHECK(reader->Read2(&pps_length) &&
           reader->ReadVec(&pps_list[i], pps_length));
  }

  return true;
}

VPCodecConfigurationRecord::VPCodecConfigurationRecord()
    : profile(VIDEO_CODEC_PROFILE_UNKNOWN) {}

VPCodecConfigurationRecord::VPCodecConfigurationRecord(
    const VPCodecConfigurationRecord& other) = default;

VPCodecConfigurationRecord::~VPCodecConfigurationRecord() {}

FourCC VPCodecConfigurationRecord::BoxType() const {
  return FOURCC_VPCC;
}

bool VPCodecConfigurationRecord::Parse(BoxReader* reader) {
  uint8_t profile_indication = 0;
  RCHECK(reader->ReadFullBoxHeader() && reader->Read1(&profile_indication));
  // The remaining fields are not parsed as we don't care about them for now.

  switch (profile_indication) {
    case 0:
      profile = VP9PROFILE_PROFILE0;
      break;
    case 1:
      profile = VP9PROFILE_PROFILE1;
      break;
    case 2:
      profile = VP9PROFILE_PROFILE2;
      break;
    case 3:
      profile = VP9PROFILE_PROFILE3;
      break;
    default:
      MEDIA_LOG(ERROR, reader->media_log()) << "Unsupported VP9 profile: "
                                            << profile_indication;
      return false;
  }
  return true;
}

PixelAspectRatioBox::PixelAspectRatioBox() : h_spacing(1), v_spacing(1) {}
PixelAspectRatioBox::PixelAspectRatioBox(const PixelAspectRatioBox& other) =
    default;
PixelAspectRatioBox::~PixelAspectRatioBox() {}
FourCC PixelAspectRatioBox::BoxType() const { return FOURCC_PASP; }

bool PixelAspectRatioBox::Parse(BoxReader* reader) {
  RCHECK(reader->Read4(&h_spacing) &&
         reader->Read4(&v_spacing));
  return true;
}

VideoSampleEntry::VideoSampleEntry()
    : format(FOURCC_NULL),
      data_reference_index(0),
      width(0),
      height(0),
      video_codec(kUnknownVideoCodec),
      video_codec_profile(VIDEO_CODEC_PROFILE_UNKNOWN) {}

VideoSampleEntry::VideoSampleEntry(const VideoSampleEntry& other) = default;

VideoSampleEntry::~VideoSampleEntry() {}
FourCC VideoSampleEntry::BoxType() const {
  DCHECK(false) << "VideoSampleEntry should be parsed according to the "
                << "handler type recovered in its Media ancestor.";
  return FOURCC_NULL;
}

bool VideoSampleEntry::Parse(BoxReader* reader) {
  format = reader->type();
  RCHECK(reader->SkipBytes(6) &&
         reader->Read2(&data_reference_index) &&
         reader->SkipBytes(16) &&
         reader->Read2(&width) &&
         reader->Read2(&height) &&
         reader->SkipBytes(50));

  RCHECK(reader->ScanChildren() &&
         reader->MaybeReadChild(&pixel_aspect));

  if (format == FOURCC_ENCV) {
    // Continue scanning until a recognized protection scheme is found, or until
    // we run out of protection schemes.
    while (sinf.type.type != FOURCC_CENC) {
      if (!reader->ReadChild(&sinf))
        return false;
    }
  }

  const FourCC actual_format =
      format == FOURCC_ENCV ? sinf.format.format : format;
  switch (actual_format) {
    case FOURCC_AVC1:
    case FOURCC_AVC3: {
      DVLOG(2) << __FUNCTION__
               << " reading AVCDecoderConfigurationRecord (avcC)";
      std::unique_ptr<AVCDecoderConfigurationRecord> avcConfig(
          new AVCDecoderConfigurationRecord());
      RCHECK(reader->ReadChild(avcConfig.get()));
      frame_bitstream_converter =
          make_scoped_refptr(new AVCBitstreamConverter(std::move(avcConfig)));
      video_codec = kCodecH264;
      video_codec_profile = H264PROFILE_MAIN;
      break;
    }
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
    case FOURCC_HEV1:
    case FOURCC_HVC1: {
      DVLOG(2) << __FUNCTION__
               << " parsing HEVCDecoderConfigurationRecord (hvcC)";
      std::unique_ptr<HEVCDecoderConfigurationRecord> hevcConfig(
          new HEVCDecoderConfigurationRecord());
      RCHECK(reader->ReadChild(hevcConfig.get()));
      frame_bitstream_converter =
          make_scoped_refptr(new HEVCBitstreamConverter(std::move(hevcConfig)));
      video_codec = kCodecHEVC;
      break;
    }
#endif
    case FOURCC_VP09:
      if (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableVp9InMp4)) {
        DVLOG(2) << __FUNCTION__
                 << " parsing VPCodecConfigurationRecord (vpcC)";
        std::unique_ptr<VPCodecConfigurationRecord> vp_config(
            new VPCodecConfigurationRecord());
        RCHECK(reader->ReadChild(vp_config.get()));
        frame_bitstream_converter = nullptr;
        video_codec = kCodecVP9;
        video_codec_profile = vp_config->profile;
      } else {
        MEDIA_LOG(ERROR, reader->media_log()) << "VP9 in MP4 is not enabled.";
        return false;
      }
      break;
    default:
      // Unknown/unsupported format
      MEDIA_LOG(ERROR, reader->media_log()) << __FUNCTION__
                                            << " unsupported video format "
                                            << FourCCToString(actual_format);
      return false;
  }

  return true;
}

bool VideoSampleEntry::IsFormatValid() const {
  const FourCC actual_format =
      format == FOURCC_ENCV ? sinf.format.format : format;
  switch (actual_format) {
    case FOURCC_AVC1:
    case FOURCC_AVC3:
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
    case FOURCC_HEV1:
    case FOURCC_HVC1:
#endif
      return true;
    case FOURCC_VP09:
      return base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableVp9InMp4);
    default:
      return false;
  }
}

ElementaryStreamDescriptor::ElementaryStreamDescriptor()
    : object_type(kForbidden) {}

ElementaryStreamDescriptor::ElementaryStreamDescriptor(
    const ElementaryStreamDescriptor& other) = default;

ElementaryStreamDescriptor::~ElementaryStreamDescriptor() {}

FourCC ElementaryStreamDescriptor::BoxType() const {
  return FOURCC_ESDS;
}

bool ElementaryStreamDescriptor::Parse(BoxReader* reader) {
  std::vector<uint8_t> data;
  ESDescriptor es_desc;

  RCHECK(reader->ReadFullBoxHeader());
  RCHECK(reader->ReadVec(&data, reader->size() - reader->pos()));
  RCHECK(es_desc.Parse(data));

  object_type = es_desc.object_type();

  if (object_type != 0x40) {
    MEDIA_LOG(INFO, reader->media_log()) << "Audio codec: mp4a." << std::hex
                                         << static_cast<int>(object_type);
  }

  if (es_desc.IsAAC(object_type))
    RCHECK(aac.Parse(es_desc.decoder_specific_info(), reader->media_log()));

  return true;
}

AudioSampleEntry::AudioSampleEntry()
    : format(FOURCC_NULL),
      data_reference_index(0),
      channelcount(0),
      samplesize(0),
      samplerate(0) {}

AudioSampleEntry::AudioSampleEntry(const AudioSampleEntry& other) = default;

AudioSampleEntry::~AudioSampleEntry() {}

FourCC AudioSampleEntry::BoxType() const {
  DCHECK(false) << "AudioSampleEntry should be parsed according to the "
                << "handler type recovered in its Media ancestor.";
  return FOURCC_NULL;
}

bool AudioSampleEntry::Parse(BoxReader* reader) {
  format = reader->type();
  RCHECK(reader->SkipBytes(6) &&
         reader->Read2(&data_reference_index) &&
         reader->SkipBytes(8) &&
         reader->Read2(&channelcount) &&
         reader->Read2(&samplesize) &&
         reader->SkipBytes(4) &&
         reader->Read4(&samplerate));
  // Convert from 16.16 fixed point to integer
  samplerate >>= 16;

  RCHECK(reader->ScanChildren());
  if (format == FOURCC_ENCA) {
    // Continue scanning until a recognized protection scheme is found, or until
    // we run out of protection schemes.
    while (sinf.type.type != FOURCC_CENC) {
      if (!reader->ReadChild(&sinf))
        return false;
    }
  }

  // ESDS is not valid in case of EAC3.
  RCHECK(reader->MaybeReadChild(&esds));
  return true;
}

MediaHeader::MediaHeader()
    : creation_time(0),
      modification_time(0),
      timescale(0),
      duration(0),
      language_code(0) {}
MediaHeader::MediaHeader(const MediaHeader& other) = default;
MediaHeader::~MediaHeader() {}
FourCC MediaHeader::BoxType() const { return FOURCC_MDHD; }

bool MediaHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());

  if (reader->version() == 1) {
    RCHECK(reader->Read8(&creation_time) && reader->Read8(&modification_time) &&
           reader->Read4(&timescale) && reader->Read8(&duration) &&
           reader->Read2(&language_code));
  } else {
    RCHECK(reader->Read4Into8(&creation_time) &&
           reader->Read4Into8(&modification_time) &&
           reader->Read4(&timescale) && reader->Read4Into8(&duration) &&
           reader->Read2(&language_code));
  }
  // ISO 639-2/T language code only uses 15 lower bits, so reset the 16th bit.
  language_code &= 0x7fff;
  // Skip playback quality information
  return reader->SkipBytes(2);
}

std::string MediaHeader::language() const {
  if (language_code == 0x7fff || language_code < 0x400) {
    return "und";
  }
  char lang_chars[4];
  lang_chars[3] = 0;
  lang_chars[2] = 0x60 + (language_code & 0x1f);
  lang_chars[1] = 0x60 + ((language_code >> 5) & 0x1f);
  lang_chars[0] = 0x60 + ((language_code >> 10) & 0x1f);

  if (lang_chars[0] < 'a' || lang_chars[0] > 'z' || lang_chars[1] < 'a' ||
      lang_chars[1] > 'z' || lang_chars[2] < 'a' || lang_chars[2] > 'z') {
    // Got unexpected characteds in ISO 639-2/T language code. Something must be
    // wrong with the input file, report 'und' language to be safe.
    DVLOG(2) << "Ignoring MDHD language_code (non ISO 639-2 compliant): "
             << lang_chars;
    lang_chars[0] = 'u';
    lang_chars[1] = 'n';
    lang_chars[2] = 'd';
  }

  return lang_chars;
}

MediaInformation::MediaInformation() {}
MediaInformation::MediaInformation(const MediaInformation& other) = default;
MediaInformation::~MediaInformation() {}
FourCC MediaInformation::BoxType() const { return FOURCC_MINF; }

bool MediaInformation::Parse(BoxReader* reader) {
  return reader->ScanChildren() &&
         reader->ReadChild(&sample_table);
}

Media::Media() {}
Media::Media(const Media& other) = default;
Media::~Media() {}
FourCC Media::BoxType() const { return FOURCC_MDIA; }

bool Media::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&header) &&
         reader->ReadChild(&handler));

  // Maddeningly, the HandlerReference box specifies how to parse the
  // SampleDescription box, making the latter the only box (of those that we
  // support) which cannot be parsed correctly on its own (or even with
  // information from its strict ancestor tree). We thus copy the handler type
  // to the sample description box *before* parsing it to provide this
  // information while parsing.
  information.sample_table.description.type = handler.type;
  RCHECK(reader->ReadChild(&information));
  return true;
}

Track::Track() {}
Track::Track(const Track& other) = default;
Track::~Track() {}
FourCC Track::BoxType() const { return FOURCC_TRAK; }

bool Track::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&header) &&
         reader->ReadChild(&media) &&
         reader->MaybeReadChild(&edit));
  return true;
}

MovieExtendsHeader::MovieExtendsHeader() : fragment_duration(0) {}
MovieExtendsHeader::MovieExtendsHeader(const MovieExtendsHeader& other) =
    default;
MovieExtendsHeader::~MovieExtendsHeader() {}
FourCC MovieExtendsHeader::BoxType() const { return FOURCC_MEHD; }

bool MovieExtendsHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->version() == 1) {
    RCHECK(reader->Read8(&fragment_duration));
  } else {
    RCHECK(reader->Read4Into8(&fragment_duration));
  }
  return true;
}

TrackExtends::TrackExtends()
    : track_id(0),
      default_sample_description_index(0),
      default_sample_duration(0),
      default_sample_size(0),
      default_sample_flags(0) {}
TrackExtends::TrackExtends(const TrackExtends& other) = default;
TrackExtends::~TrackExtends() {}
FourCC TrackExtends::BoxType() const { return FOURCC_TREX; }

bool TrackExtends::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->Read4(&track_id) &&
         reader->Read4(&default_sample_description_index) &&
         reader->Read4(&default_sample_duration) &&
         reader->Read4(&default_sample_size) &&
         reader->Read4(&default_sample_flags));
  return true;
}

MovieExtends::MovieExtends() {}
MovieExtends::MovieExtends(const MovieExtends& other) = default;
MovieExtends::~MovieExtends() {}
FourCC MovieExtends::BoxType() const { return FOURCC_MVEX; }

bool MovieExtends::Parse(BoxReader* reader) {
  header.fragment_duration = 0;
  return reader->ScanChildren() &&
         reader->MaybeReadChild(&header) &&
         reader->ReadChildren(&tracks);
}

Movie::Movie() : fragmented(false) {}
Movie::Movie(const Movie& other) = default;
Movie::~Movie() {}
FourCC Movie::BoxType() const { return FOURCC_MOOV; }

bool Movie::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() && reader->ReadChild(&header) &&
         reader->ReadChildren(&tracks));

  RCHECK_MEDIA_LOGGED(reader->ReadChild(&extends), reader->media_log(),
                      "Detected unfragmented MP4. Media Source Extensions "
                      "require ISO BMFF moov to contain mvex to indicate that "
                      "Movie Fragments are to be expected.");

  return reader->MaybeReadChildren(&pssh);
}

TrackFragmentDecodeTime::TrackFragmentDecodeTime() : decode_time(0) {}
TrackFragmentDecodeTime::TrackFragmentDecodeTime(
    const TrackFragmentDecodeTime& other) = default;
TrackFragmentDecodeTime::~TrackFragmentDecodeTime() {}
FourCC TrackFragmentDecodeTime::BoxType() const { return FOURCC_TFDT; }

bool TrackFragmentDecodeTime::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->version() == 1)
    return reader->Read8(&decode_time);
  else
    return reader->Read4Into8(&decode_time);
}

MovieFragmentHeader::MovieFragmentHeader() : sequence_number(0) {}
MovieFragmentHeader::MovieFragmentHeader(const MovieFragmentHeader& other) =
    default;
MovieFragmentHeader::~MovieFragmentHeader() {}
FourCC MovieFragmentHeader::BoxType() const { return FOURCC_MFHD; }

bool MovieFragmentHeader::Parse(BoxReader* reader) {
  return reader->SkipBytes(4) && reader->Read4(&sequence_number);
}

TrackFragmentHeader::TrackFragmentHeader()
    : track_id(0),
      sample_description_index(0),
      default_sample_duration(0),
      default_sample_size(0),
      default_sample_flags(0),
      has_default_sample_flags(false) {}

TrackFragmentHeader::TrackFragmentHeader(const TrackFragmentHeader& other) =
    default;
TrackFragmentHeader::~TrackFragmentHeader() {}
FourCC TrackFragmentHeader::BoxType() const { return FOURCC_TFHD; }

bool TrackFragmentHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() && reader->Read4(&track_id));

  // Media Source specific: reject tracks that set 'base-data-offset-present'.
  // Although the Media Source requires that 'default-base-is-moof' (14496-12
  // Amendment 2) be set, we omit this check as many otherwise-valid files in
  // the wild don't set it.
  //
  //  RCHECK((flags & 0x020000) && !(flags & 0x1));
  RCHECK(!(reader->flags() & 0x1));

  if (reader->flags() & 0x2) {
    RCHECK(reader->Read4(&sample_description_index));
  } else {
    sample_description_index = 0;
  }

  if (reader->flags() & 0x8) {
    RCHECK(reader->Read4(&default_sample_duration));
  } else {
    default_sample_duration = 0;
  }

  if (reader->flags() & 0x10) {
    RCHECK(reader->Read4(&default_sample_size));
  } else {
    default_sample_size = 0;
  }

  if (reader->flags() & 0x20) {
    RCHECK(reader->Read4(&default_sample_flags));
    has_default_sample_flags = true;
  } else {
    has_default_sample_flags = false;
  }

  return true;
}

TrackFragmentRun::TrackFragmentRun()
    : sample_count(0), data_offset(0) {}
TrackFragmentRun::TrackFragmentRun(const TrackFragmentRun& other) = default;
TrackFragmentRun::~TrackFragmentRun() {}
FourCC TrackFragmentRun::BoxType() const { return FOURCC_TRUN; }

bool TrackFragmentRun::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->Read4(&sample_count));
  const uint32_t flags = reader->flags();

  bool data_offset_present = (flags & 0x1) != 0;
  bool first_sample_flags_present = (flags & 0x4) != 0;
  bool sample_duration_present = (flags & 0x100) != 0;
  bool sample_size_present = (flags & 0x200) != 0;
  bool sample_flags_present = (flags & 0x400) != 0;
  bool sample_composition_time_offsets_present = (flags & 0x800) != 0;

  if (data_offset_present) {
    RCHECK(reader->Read4(&data_offset));
  } else {
    data_offset = 0;
  }

  uint32_t first_sample_flags = 0;
  if (first_sample_flags_present)
    RCHECK(reader->Read4(&first_sample_flags));

  int fields = sample_duration_present + sample_size_present +
      sample_flags_present + sample_composition_time_offsets_present;
  RCHECK(reader->HasBytes(fields * sample_count));

  if (sample_duration_present)
    sample_durations.resize(sample_count);
  if (sample_size_present)
    sample_sizes.resize(sample_count);
  if (sample_flags_present)
    sample_flags.resize(sample_count);
  if (sample_composition_time_offsets_present)
    sample_composition_time_offsets.resize(sample_count);

  for (uint32_t i = 0; i < sample_count; ++i) {
    if (sample_duration_present)
      RCHECK(reader->Read4(&sample_durations[i]));
    if (sample_size_present)
      RCHECK(reader->Read4(&sample_sizes[i]));
    if (sample_flags_present)
      RCHECK(reader->Read4(&sample_flags[i]));
    if (sample_composition_time_offsets_present)
      RCHECK(reader->Read4s(&sample_composition_time_offsets[i]));
  }

  if (first_sample_flags_present) {
    if (sample_flags.size() == 0) {
      sample_flags.push_back(first_sample_flags);
    } else {
      sample_flags[0] = first_sample_flags;
    }
  }
  return true;
}

SampleToGroup::SampleToGroup() : grouping_type(0), grouping_type_parameter(0) {}
SampleToGroup::SampleToGroup(const SampleToGroup& other) = default;
SampleToGroup::~SampleToGroup() {}
FourCC SampleToGroup::BoxType() const { return FOURCC_SBGP; }

bool SampleToGroup::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->Read4(&grouping_type));

  if (reader->version() == 1)
    RCHECK(reader->Read4(&grouping_type_parameter));

  if (grouping_type != FOURCC_SEIG) {
    DLOG(WARNING) << "SampleToGroup box with grouping_type '" << grouping_type
                  << "' is not supported.";
    return true;
  }

  uint32_t count;
  RCHECK(reader->Read4(&count));
  entries.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    RCHECK(reader->Read4(&entries[i].sample_count) &&
           reader->Read4(&entries[i].group_description_index));
  }
  return true;
}

CencSampleEncryptionInfoEntry::CencSampleEncryptionInfoEntry()
    : is_encrypted(false), iv_size(0) {}
CencSampleEncryptionInfoEntry::CencSampleEncryptionInfoEntry(
    const CencSampleEncryptionInfoEntry& other) = default;
CencSampleEncryptionInfoEntry::~CencSampleEncryptionInfoEntry() {}

SampleGroupDescription::SampleGroupDescription() : grouping_type(0) {}
SampleGroupDescription::SampleGroupDescription(
    const SampleGroupDescription& other) = default;
SampleGroupDescription::~SampleGroupDescription() {}
FourCC SampleGroupDescription::BoxType() const { return FOURCC_SGPD; }

bool SampleGroupDescription::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->Read4(&grouping_type));

  if (grouping_type != FOURCC_SEIG) {
    DLOG(WARNING) << "SampleGroupDescription box with grouping_type '"
                  << grouping_type << "' is not supported.";
    return true;
  }

  const uint8_t version = reader->version();

  const size_t kKeyIdSize = 16;
  const size_t kEntrySize = sizeof(uint32_t) + kKeyIdSize;
  uint32_t default_length = 0;
  if (version == 1) {
      RCHECK(reader->Read4(&default_length));
      RCHECK(default_length == 0 || default_length >= kEntrySize);
  }

  uint32_t count;
  RCHECK(reader->Read4(&count));
  entries.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    if (version == 1) {
      if (default_length == 0) {
        uint32_t description_length = 0;
        RCHECK(reader->Read4(&description_length));
        RCHECK(description_length >= kEntrySize);
      }
    }

    uint8_t flag;
    RCHECK(reader->SkipBytes(2) &&  // reserved.
           reader->Read1(&flag) &&
           reader->Read1(&entries[i].iv_size) &&
           reader->ReadVec(&entries[i].key_id, kKeyIdSize));

    entries[i].is_encrypted = (flag != 0);
    if (entries[i].is_encrypted) {
      RCHECK(entries[i].iv_size == 8 || entries[i].iv_size == 16);
    } else {
      RCHECK(entries[i].iv_size == 0);
    }
  }
  return true;
}

TrackFragment::TrackFragment() {}
TrackFragment::TrackFragment(const TrackFragment& other) = default;
TrackFragment::~TrackFragment() {}
FourCC TrackFragment::BoxType() const { return FOURCC_TRAF; }

bool TrackFragment::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&header) &&
         // Media Source specific: 'tfdt' required
         reader->ReadChild(&decode_time) &&
         reader->MaybeReadChildren(&runs) &&
         reader->MaybeReadChild(&auxiliary_offset) &&
         reader->MaybeReadChild(&auxiliary_size) &&
         reader->MaybeReadChild(&sdtp));

  // There could be multiple SampleGroupDescription and SampleToGroup boxes with
  // different grouping types. For common encryption, the relevant grouping type
  // is 'seig'. Continue reading until 'seig' is found, or until running out of
  // child boxes.
  while (reader->HasChild(&sample_group_description)) {
    RCHECK(reader->ReadChild(&sample_group_description));
    if (sample_group_description.grouping_type == FOURCC_SEIG)
      break;
    sample_group_description.entries.clear();
  }
  while (reader->HasChild(&sample_to_group)) {
    RCHECK(reader->ReadChild(&sample_to_group));
    if (sample_to_group.grouping_type == FOURCC_SEIG)
      break;
    sample_to_group.entries.clear();
  }
  return true;
}

MovieFragment::MovieFragment() {}
MovieFragment::MovieFragment(const MovieFragment& other) = default;
MovieFragment::~MovieFragment() {}
FourCC MovieFragment::BoxType() const { return FOURCC_MOOF; }

bool MovieFragment::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&header) &&
         reader->ReadChildren(&tracks) &&
         reader->MaybeReadChildren(&pssh));
  return true;
}

IndependentAndDisposableSamples::IndependentAndDisposableSamples() {}
IndependentAndDisposableSamples::IndependentAndDisposableSamples(
    const IndependentAndDisposableSamples& other) = default;
IndependentAndDisposableSamples::~IndependentAndDisposableSamples() {}
FourCC IndependentAndDisposableSamples::BoxType() const { return FOURCC_SDTP; }

bool IndependentAndDisposableSamples::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  RCHECK(reader->version() == 0);
  RCHECK(reader->flags() == 0);

  int sample_count = reader->size() - reader->pos();
  sample_depends_on_.resize(sample_count);
  for (int i = 0; i < sample_count; ++i) {
    uint8_t sample_info;
    RCHECK(reader->Read1(&sample_info));

    sample_depends_on_[i] =
        static_cast<SampleDependsOn>((sample_info >> 4) & 0x3);

    RCHECK(sample_depends_on_[i] != kSampleDependsOnReserved);
  }

  return true;
}

SampleDependsOn IndependentAndDisposableSamples::sample_depends_on(
    size_t i) const {
  if (i >= sample_depends_on_.size())
    return kSampleDependsOnUnknown;

  return sample_depends_on_[i];
}

}  // namespace mp4
}  // namespace media
