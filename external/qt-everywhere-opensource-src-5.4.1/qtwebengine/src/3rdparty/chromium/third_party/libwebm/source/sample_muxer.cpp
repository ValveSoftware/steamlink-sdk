// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <string>

// libwebm parser includes
#include "mkvreader.hpp"
#include "mkvparser.hpp"

// libwebm muxer includes
#include "mkvmuxer.hpp"
#include "mkvwriter.hpp"
#include "mkvmuxerutil.hpp"

#include "sample_muxer_metadata.h"

using mkvmuxer::int64;
using mkvmuxer::uint64;

#ifdef _MSC_VER
// Disable MSVC warnings that suggest making code non-portable.
#pragma warning(disable : 4996)
#endif

namespace {

void Usage() {
  printf("Usage: sample_muxer -i input -o output [options]\n");
  printf("\n");
  printf("Main options:\n");
  printf("  -h | -?                     show help\n");
  printf("  -video <int>                >0 outputs video\n");
  printf("  -audio <int>                >0 outputs audio\n");
  printf("  -live <int>                 >0 puts the muxer into live mode\n");
  printf("                              0 puts the muxer into file mode\n");
  printf("  -output_cues <int>          >0 outputs cues element\n");
  printf("  -cues_on_video_track <int>  >0 outputs cues on video track\n");
  printf("  -cues_on_audio_track <int>  >0 outputs cues on audio track\n");
  printf("  -max_cluster_duration <double> in seconds\n");
  printf("  -max_cluster_size <int>     in bytes\n");
  printf("  -switch_tracks <int>        >0 switches tracks in output\n");
  printf("  -audio_track_number <int>   >0 Changes the audio track number\n");
  printf("  -video_track_number <int>   >0 Changes the video track number\n");
  printf("  -chunking <string>          Chunk output\n");
  printf("\n");
  printf("Video options:\n");
  printf("  -display_width <int>        Display width in pixels\n");
  printf("  -display_height <int>       Display height in pixels\n");
  printf("  -stereo_mode <int>          3D video mode\n");
  printf("\n");
  printf("Cues options:\n");
  printf("  -output_cues_block_number <int> >0 outputs cue block number\n");
  printf("  -cues_before_clusters <int> >0 puts Cues before Clusters\n");
  printf("\n");
  printf("Metadata options:\n");
  printf("  -webvtt-subtitles <vttfile>    ");
  printf("add WebVTT subtitles as metadata track\n");
  printf("  -webvtt-captions <vttfile>     ");
  printf("add WebVTT captions as metadata track\n");
  printf("  -webvtt-descriptions <vttfile> ");
  printf("add WebVTT descriptions as metadata track\n");
  printf("  -webvtt-metadata <vttfile>     ");
  printf("add WebVTT subtitles as metadata track\n");
  printf("  -webvtt-chapters <vttfile>     ");
  printf("add WebVTT chapters as MKV chapters element\n");
}

struct MetadataFile {
  const char* name;
  SampleMuxerMetadata::Kind kind;
};

typedef std::list<MetadataFile> metadata_files_t;

// Cache the WebVTT filenames specified as command-line args.
bool LoadMetadataFiles(const metadata_files_t& files,
                       SampleMuxerMetadata* metadata) {
  typedef metadata_files_t::const_iterator iter_t;

  iter_t i = files.begin();
  const iter_t j = files.end();

  while (i != j) {
    const metadata_files_t::value_type& v = *i++;

    if (!metadata->Load(v.name, v.kind))
      return false;
  }

  return true;
}

int ParseArgWebVTT(char* argv[], int* argv_index, int argc_check,
                   metadata_files_t* metadata_files) {
  int& i = *argv_index;

  enum { kCount = 5 };
  struct Arg {
    const char* name;
    SampleMuxerMetadata::Kind kind;
  };
  const Arg args[kCount] = {
      {"-webvtt-subtitles", SampleMuxerMetadata::kSubtitles},
      {"-webvtt-captions", SampleMuxerMetadata::kCaptions},
      {"-webvtt-descriptions", SampleMuxerMetadata::kDescriptions},
      {"-webvtt-metadata", SampleMuxerMetadata::kMetadata},
      {"-webvtt-chapters", SampleMuxerMetadata::kChapters}};

  for (int idx = 0; idx < kCount; ++idx) {
    const Arg& arg = args[idx];

    if (strcmp(arg.name, argv[i]) != 0)  // no match
      continue;

    ++i;  // consume arg name here

    if (i > argc_check) {
      printf("missing value for %s\n", arg.name);
      return -1;  // error
    }

    MetadataFile f;
    f.name = argv[i];  // arg value is consumed via caller's loop idx
    f.kind = arg.kind;

    metadata_files->push_back(f);
    return 1;  // successfully parsed WebVTT arg
  }

  return 0;  // not a WebVTT arg
}

}  // end namespace

int main(int argc, char* argv[]) {
  char* input = NULL;
  char* output = NULL;

  // Segment variables
  bool output_video = true;
  bool output_audio = true;
  bool live_mode = false;
  bool output_cues = true;
  bool cues_before_clusters = false;
  bool cues_on_video_track = true;
  bool cues_on_audio_track = false;
  uint64 max_cluster_duration = 0;
  uint64 max_cluster_size = 0;
  bool switch_tracks = false;
  int audio_track_number = 0;  // 0 tells muxer to decide.
  int video_track_number = 0;  // 0 tells muxer to decide.
  bool chunking = false;
  const char* chunk_name = NULL;

  bool output_cues_block_number = true;

  uint64 display_width = 0;
  uint64 display_height = 0;
  uint64 stereo_mode = 0;

  metadata_files_t metadata_files;

  const int argc_check = argc - 1;
  for (int i = 1; i < argc; ++i) {
    char* end;

    if (!strcmp("-h", argv[i]) || !strcmp("-?", argv[i])) {
      Usage();
      return EXIT_SUCCESS;
    } else if (!strcmp("-i", argv[i]) && i < argc_check) {
      input = argv[++i];
    } else if (!strcmp("-o", argv[i]) && i < argc_check) {
      output = argv[++i];
    } else if (!strcmp("-video", argv[i]) && i < argc_check) {
      output_video = strtol(argv[++i], &end, 10) == 0 ? false : true;
    } else if (!strcmp("-audio", argv[i]) && i < argc_check) {
      output_audio = strtol(argv[++i], &end, 10) == 0 ? false : true;
    } else if (!strcmp("-live", argv[i]) && i < argc_check) {
      live_mode = strtol(argv[++i], &end, 10) == 0 ? false : true;
    } else if (!strcmp("-output_cues", argv[i]) && i < argc_check) {
      output_cues = strtol(argv[++i], &end, 10) == 0 ? false : true;
    } else if (!strcmp("-cues_before_clusters", argv[i]) && i < argc_check) {
      cues_before_clusters = strtol(argv[++i], &end, 10) == 0 ? false : true;
    } else if (!strcmp("-cues_on_video_track", argv[i]) && i < argc_check) {
      cues_on_video_track = strtol(argv[++i], &end, 10) == 0 ? false : true;
      if (cues_on_video_track)
        cues_on_audio_track = false;
    } else if (!strcmp("-cues_on_audio_track", argv[i]) && i < argc_check) {
      cues_on_audio_track = strtol(argv[++i], &end, 10) == 0 ? false : true;
      if (cues_on_audio_track)
        cues_on_video_track = false;
    } else if (!strcmp("-max_cluster_duration", argv[i]) && i < argc_check) {
      const double seconds = strtod(argv[++i], &end);
      max_cluster_duration = static_cast<uint64>(seconds * 1000000000.0);
    } else if (!strcmp("-max_cluster_size", argv[i]) && i < argc_check) {
      max_cluster_size = strtol(argv[++i], &end, 10);
    } else if (!strcmp("-switch_tracks", argv[i]) && i < argc_check) {
      switch_tracks = strtol(argv[++i], &end, 10) == 0 ? false : true;
    } else if (!strcmp("-audio_track_number", argv[i]) && i < argc_check) {
      audio_track_number = strtol(argv[++i], &end, 10);
    } else if (!strcmp("-video_track_number", argv[i]) && i < argc_check) {
      video_track_number = strtol(argv[++i], &end, 10);
    } else if (!strcmp("-chunking", argv[i]) && i < argc_check) {
      chunking = true;
      chunk_name = argv[++i];
    } else if (!strcmp("-display_width", argv[i]) && i < argc_check) {
      display_width = strtol(argv[++i], &end, 10);
    } else if (!strcmp("-display_height", argv[i]) && i < argc_check) {
      display_height = strtol(argv[++i], &end, 10);
    } else if (!strcmp("-stereo_mode", argv[i]) && i < argc_check) {
      stereo_mode = strtol(argv[++i], &end, 10);
    } else if (!strcmp("-output_cues_block_number", argv[i]) &&
               i < argc_check) {
      output_cues_block_number =
          strtol(argv[++i], &end, 10) == 0 ? false : true;
    } else if (int e = ParseArgWebVTT(argv, &i, argc_check, &metadata_files)) {
      if (e < 0)
        return EXIT_FAILURE;
    }
  }

  if (input == NULL || output == NULL) {
    Usage();
    return EXIT_FAILURE;
  }

  // Get parser header info
  mkvparser::MkvReader reader;

  if (reader.Open(input)) {
    printf("\n Filename is invalid or error while opening.\n");
    return EXIT_FAILURE;
  }

  long long pos = 0;
  mkvparser::EBMLHeader ebml_header;
  ebml_header.Parse(&reader, pos);

  mkvparser::Segment* parser_segment;
  long long ret =
      mkvparser::Segment::CreateInstance(&reader, pos, parser_segment);
  if (ret) {
    printf("\n Segment::CreateInstance() failed.");
    return EXIT_FAILURE;
  }

  ret = parser_segment->Load();
  if (ret < 0) {
    printf("\n Segment::Load() failed.");
    return EXIT_FAILURE;
  }

  const mkvparser::SegmentInfo* const segment_info = parser_segment->GetInfo();
  const long long timeCodeScale = segment_info->GetTimeCodeScale();

  // Set muxer header info
  mkvmuxer::MkvWriter writer;

  char* temp_file = tmpnam(NULL);
  if (!writer.Open(cues_before_clusters ? temp_file : output)) {
    printf("\n Filename is invalid or error while opening.\n");
    return EXIT_FAILURE;
  }

  // Set Segment element attributes
  mkvmuxer::Segment muxer_segment;

  if (!muxer_segment.Init(&writer)) {
    printf("\n Could not initialize muxer segment!\n");
    return EXIT_FAILURE;
  }

  if (live_mode)
    muxer_segment.set_mode(mkvmuxer::Segment::kLive);
  else
    muxer_segment.set_mode(mkvmuxer::Segment::kFile);

  if (chunking)
    muxer_segment.SetChunking(true, chunk_name);

  if (max_cluster_duration > 0)
    muxer_segment.set_max_cluster_duration(max_cluster_duration);
  if (max_cluster_size > 0)
    muxer_segment.set_max_cluster_size(max_cluster_size);
  muxer_segment.OutputCues(output_cues);

  // Set SegmentInfo element attributes
  mkvmuxer::SegmentInfo* const info = muxer_segment.GetSegmentInfo();
  info->set_timecode_scale(timeCodeScale);
  info->set_writing_app("sample_muxer");

  // Set Tracks element attributes
  const mkvparser::Tracks* const parser_tracks = parser_segment->GetTracks();
  unsigned long i = 0;
  uint64 vid_track = 0;  // no track added
  uint64 aud_track = 0;  // no track added

  using mkvparser::Track;

  while (i != parser_tracks->GetTracksCount()) {
    int track_num = i++;
    if (switch_tracks)
      track_num = i % parser_tracks->GetTracksCount();

    const mkvparser::Track* const parser_track =
        parser_tracks->GetTrackByIndex(track_num);

    if (parser_track == NULL)
      continue;

    // TODO(fgalligan): Add support for language to parser.
    const char* const track_name = parser_track->GetNameAsUTF8();

    const long long track_type = parser_track->GetType();

    if (track_type == Track::kVideo && output_video) {
      // Get the video track from the parser
      const mkvparser::VideoTrack* const pVideoTrack =
          static_cast<const mkvparser::VideoTrack*>(parser_track);
      const long long width = pVideoTrack->GetWidth();
      const long long height = pVideoTrack->GetHeight();

      // Add the video track to the muxer
      vid_track = muxer_segment.AddVideoTrack(static_cast<int>(width),
                                              static_cast<int>(height),
                                              video_track_number);
      if (!vid_track) {
        printf("\n Could not add video track.\n");
        return EXIT_FAILURE;
      }

      mkvmuxer::VideoTrack* const video = static_cast<mkvmuxer::VideoTrack*>(
          muxer_segment.GetTrackByNumber(vid_track));
      if (!video) {
        printf("\n Could not get video track.\n");
        return EXIT_FAILURE;
      }

      if (track_name)
        video->set_name(track_name);

      video->set_codec_id(pVideoTrack->GetCodecId());

      if (display_width > 0)
        video->set_display_width(display_width);
      if (display_height > 0)
        video->set_display_height(display_height);
      if (stereo_mode > 0)
        video->SetStereoMode(stereo_mode);

      const double rate = pVideoTrack->GetFrameRate();
      if (rate > 0.0) {
        video->set_frame_rate(rate);
      }
    } else if (track_type == Track::kAudio && output_audio) {
      // Get the audio track from the parser
      const mkvparser::AudioTrack* const pAudioTrack =
          static_cast<const mkvparser::AudioTrack*>(parser_track);
      const long long channels = pAudioTrack->GetChannels();
      const double sample_rate = pAudioTrack->GetSamplingRate();

      // Add the audio track to the muxer
      aud_track = muxer_segment.AddAudioTrack(static_cast<int>(sample_rate),
                                              static_cast<int>(channels),
                                              audio_track_number);
      if (!aud_track) {
        printf("\n Could not add audio track.\n");
        return EXIT_FAILURE;
      }

      mkvmuxer::AudioTrack* const audio = static_cast<mkvmuxer::AudioTrack*>(
          muxer_segment.GetTrackByNumber(aud_track));
      if (!audio) {
        printf("\n Could not get audio track.\n");
        return EXIT_FAILURE;
      }

      if (track_name)
        audio->set_name(track_name);

      audio->set_codec_id(pAudioTrack->GetCodecId());

      size_t private_size;
      const unsigned char* const private_data =
          pAudioTrack->GetCodecPrivate(private_size);
      if (private_size > 0) {
        if (!audio->SetCodecPrivate(private_data, private_size)) {
          printf("\n Could not add audio private data.\n");
          return EXIT_FAILURE;
        }
      }

      const long long bit_depth = pAudioTrack->GetBitDepth();
      if (bit_depth > 0)
        audio->set_bit_depth(bit_depth);

      if (pAudioTrack->GetCodecDelay())
        audio->set_codec_delay(pAudioTrack->GetCodecDelay());
      if (pAudioTrack->GetSeekPreRoll())
        audio->set_seek_pre_roll(pAudioTrack->GetSeekPreRoll());
    }
  }

  // We have created all the video and audio tracks.  If any WebVTT
  // files were specified as command-line args, then parse them and
  // add a track to the output file corresponding to each metadata
  // input file.

  SampleMuxerMetadata metadata;

  if (!metadata.Init(&muxer_segment)) {
    printf("\n Could not initialize metadata cache.\n");
    return EXIT_FAILURE;
  }

  if (!LoadMetadataFiles(metadata_files, &metadata))
    return EXIT_FAILURE;

  if (!metadata.AddChapters())
    return EXIT_FAILURE;

  // Set Cues element attributes
  mkvmuxer::Cues* const cues = muxer_segment.GetCues();
  cues->set_output_block_number(output_cues_block_number);
  if (cues_on_video_track && vid_track)
    muxer_segment.CuesTrack(vid_track);
  if (cues_on_audio_track && aud_track)
    muxer_segment.CuesTrack(aud_track);

  // Write clusters
  unsigned char* data = NULL;
  int data_len = 0;

  const mkvparser::Cluster* cluster = parser_segment->GetFirst();

  while ((cluster != NULL) && !cluster->EOS()) {
    const mkvparser::BlockEntry* block_entry;

    long status = cluster->GetFirst(block_entry);

    if (status) {
      printf("\n Could not get first block of cluster.\n");
      return EXIT_FAILURE;
    }

    while ((block_entry != NULL) && !block_entry->EOS()) {
      const mkvparser::Block* const block = block_entry->GetBlock();
      const long long trackNum = block->GetTrackNumber();
      const mkvparser::Track* const parser_track =
          parser_tracks->GetTrackByNumber(static_cast<unsigned long>(trackNum));

      // When |parser_track| is NULL, it means that the track number in the
      // Block is invalid (i.e.) the was no TrackEntry corresponding to the
      // track number. So we reject the file.
      if (!parser_track) {
        return EXIT_FAILURE;
      }

      const long long track_type = parser_track->GetType();
      const long long time_ns = block->GetTime(cluster);

      // Flush any metadata frames to the output file, before we write
      // the current block.
      if (!metadata.Write(time_ns))
        return EXIT_FAILURE;

      if ((track_type == Track::kAudio && output_audio) ||
          (track_type == Track::kVideo && output_video)) {
        const int frame_count = block->GetFrameCount();
        const bool is_key = block->IsKey();
        const int64 discard_padding = block->GetDiscardPadding();

        for (int i = 0; i < frame_count; ++i) {
          const mkvparser::Block::Frame& frame = block->GetFrame(i);

          if (frame.len > data_len) {
            delete[] data;
            data = new unsigned char[frame.len];
            if (!data)
              return EXIT_FAILURE;
            data_len = frame.len;
          }

          if (frame.Read(&reader, data))
            return EXIT_FAILURE;

          uint64 track_num = vid_track;
          if (track_type == Track::kAudio)
            track_num = aud_track;

          bool frame_added = false;
          if (discard_padding) {
            frame_added = muxer_segment.AddFrameWithDiscardPadding(
                data, frame.len, discard_padding, track_num, time_ns, is_key);
          } else {
            frame_added = muxer_segment.AddFrame(data, frame.len, track_num,
                                                 time_ns, is_key);
          }
          if (!frame_added) {
            printf("\n Could not add frame.\n");
            return EXIT_FAILURE;
          }
        }
      }

      status = cluster->GetNext(block_entry, block_entry);

      if (status) {
        printf("\n Could not get next block of cluster.\n");
        return EXIT_FAILURE;
      }
    }

    cluster = parser_segment->GetNext(cluster);
  }

  // We have exhausted all video and audio frames in the input file.
  // Flush any remaining metadata frames to the output file.
  if (!metadata.Write(-1))
    return EXIT_FAILURE;

  if (!muxer_segment.Finalize()) {
    printf("Finalization of segment failed.\n");
    return EXIT_FAILURE;
  }

  reader.Close();
  writer.Close();

  if (cues_before_clusters) {
    if (reader.Open(temp_file)) {
      printf("\n Filename is invalid or error while opening.\n");
      return EXIT_FAILURE;
    }
    if (!writer.Open(output)) {
      printf("\n Filename is invalid or error while opening.\n");
      return EXIT_FAILURE;
    }
    if (!muxer_segment.CopyAndMoveCuesBeforeClusters(&reader, &writer)) {
      printf("\n Unable to copy and move cues before clusters.\n");
      return EXIT_FAILURE;
    }
    reader.Close();
    writer.Close();
    remove(temp_file);
  }

  delete[] data;
  delete parser_segment;

  return EXIT_SUCCESS;
}
