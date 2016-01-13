// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// This sample application demonstrates how to use the Matroska parser
// library, which allows clients to handle a Matroska format file.

#include <memory>

#include "mkvreader.hpp"
#include "mkvparser.hpp"

#ifdef _MSC_VER
// Disable MSVC warnings that suggest making code non-portable.
#pragma warning(disable : 4996)
#endif

static const wchar_t* utf8towcs(const char* str) {
  if (str == NULL)
    return NULL;

  // TODO: this probably requires that the locale be
  // configured somehow:

  const size_t size = mbstowcs(NULL, str, 0);

  if (size == 0)
    return NULL;

  wchar_t* const val = new wchar_t[size + 1];

  mbstowcs(val, str, size);
  val[size] = L'\0';

  return val;
}

bool InputHasCues(const mkvparser::Segment* const segment) {
  const mkvparser::Cues* const cues = segment->GetCues();
  if (cues == NULL)
    return false;

  while (!cues->DoneParsing())
    cues->LoadCuePoint();

  const mkvparser::CuePoint* const cue_point = cues->GetFirst();
  if (cue_point == NULL)
    return false;

  return true;
}

int main(int argc, char* argv[]) {
  if (argc == 1) {
    printf("\t\t\tMkv Parser Sample Application\n");
    printf("\t\t\tUsage: \n");
    printf("\t\t\t  ./sample [input file] \n");
    printf("\t\t\t  ./sample sample.mkv \n");
    return -1;
  }

  using namespace mkvparser;

  MkvReader reader;

  if (reader.Open(argv[1])) {
    printf("\n Filename is invalid or error while opening.\n");
    return -1;
  }

  int maj, min, build, rev;

  GetVersion(maj, min, build, rev);
  printf("\t\t libmkv verison: %d.%d.%d.%d\n", maj, min, build, rev);

  long long pos = 0;

  EBMLHeader ebmlHeader;

  ebmlHeader.Parse(&reader, pos);

  printf("\t\t\t    EBML Header\n");
  printf("\t\tEBML Version\t\t: %lld\n", ebmlHeader.m_version);
  printf("\t\tEBML MaxIDLength\t: %lld\n", ebmlHeader.m_maxIdLength);
  printf("\t\tEBML MaxSizeLength\t: %lld\n", ebmlHeader.m_maxSizeLength);
  printf("\t\tDoc Type\t\t: %s\n", ebmlHeader.m_docType);
  printf("\t\tPos\t\t\t: %lld\n", pos);

  typedef mkvparser::Segment seg_t;
  seg_t* pSegment_;

  long long ret = seg_t::CreateInstance(&reader, pos, pSegment_);
  if (ret) {
    printf("\n Segment::CreateInstance() failed.");
    return -1;
  }

  const std::auto_ptr<seg_t> pSegment(pSegment_);

  ret = pSegment->Load();
  if (ret < 0) {
    printf("\n Segment::Load() failed.");
    return -1;
  }

  const SegmentInfo* const pSegmentInfo = pSegment->GetInfo();

  const long long timeCodeScale = pSegmentInfo->GetTimeCodeScale();
  const long long duration_ns = pSegmentInfo->GetDuration();

  const char* const pTitle_ = pSegmentInfo->GetTitleAsUTF8();
  const wchar_t* const pTitle = utf8towcs(pTitle_);

  const char* const pMuxingApp_ = pSegmentInfo->GetMuxingAppAsUTF8();
  const wchar_t* const pMuxingApp = utf8towcs(pMuxingApp_);

  const char* const pWritingApp_ = pSegmentInfo->GetWritingAppAsUTF8();
  const wchar_t* const pWritingApp = utf8towcs(pWritingApp_);

  printf("\n");
  printf("\t\t\t   Segment Info\n");
  printf("\t\tTimeCodeScale\t\t: %lld \n", timeCodeScale);
  printf("\t\tDuration\t\t: %lld\n", duration_ns);

  const double duration_sec = double(duration_ns) / 1000000000;
  printf("\t\tDuration(secs)\t\t: %7.3lf\n", duration_sec);

  if (pTitle == NULL)
    printf("\t\tTrack Name\t\t: NULL\n");
  else {
    printf("\t\tTrack Name\t\t: %ls\n", pTitle);
    delete[] pTitle;
  }

  if (pMuxingApp == NULL)
    printf("\t\tMuxing App\t\t: NULL\n");
  else {
    printf("\t\tMuxing App\t\t: %ls\n", pMuxingApp);
    delete[] pMuxingApp;
  }

  if (pWritingApp == NULL)
    printf("\t\tWriting App\t\t: NULL\n");
  else {
    printf("\t\tWriting App\t\t: %ls\n", pWritingApp);
    delete[] pWritingApp;
  }

  // pos of segment payload
  printf("\t\tPosition(Segment)\t: %lld\n", pSegment->m_start);

  // size of segment payload
  printf("\t\tSize(Segment)\t\t: %lld\n", pSegment->m_size);

  const mkvparser::Tracks* pTracks = pSegment->GetTracks();

  unsigned long track_num = 0;
  const unsigned long num_tracks = pTracks->GetTracksCount();

  printf("\n\t\t\t   Track Info\n");

  while (track_num != num_tracks) {
    const Track* const pTrack = pTracks->GetTrackByIndex(track_num++);

    if (pTrack == NULL)
      continue;

    const long trackType = pTrack->GetType();
    const long trackNumber = pTrack->GetNumber();
    const unsigned long long trackUid = pTrack->GetUid();
    const wchar_t* const pTrackName = utf8towcs(pTrack->GetNameAsUTF8());

    printf("\t\tTrack Type\t\t: %ld\n", trackType);
    printf("\t\tTrack Number\t\t: %ld\n", trackNumber);
    printf("\t\tTrack Uid\t\t: %lld\n", trackUid);

    if (pTrackName == NULL)
      printf("\t\tTrack Name\t\t: NULL\n");
    else {
      printf("\t\tTrack Name\t\t: %ls \n", pTrackName);
      delete[] pTrackName;
    }

    const char* const pCodecId = pTrack->GetCodecId();

    if (pCodecId == NULL)
      printf("\t\tCodec Id\t\t: NULL\n");
    else
      printf("\t\tCodec Id\t\t: %s\n", pCodecId);

    const char* const pCodecName_ = pTrack->GetCodecNameAsUTF8();
    const wchar_t* const pCodecName = utf8towcs(pCodecName_);

    if (pCodecName == NULL)
      printf("\t\tCodec Name\t\t: NULL\n");
    else {
      printf("\t\tCodec Name\t\t: %ls\n", pCodecName);
      delete[] pCodecName;
    }

    if (trackType == mkvparser::Track::kVideo) {
      const VideoTrack* const pVideoTrack =
          static_cast<const VideoTrack*>(pTrack);

      const long long width = pVideoTrack->GetWidth();
      printf("\t\tVideo Width\t\t: %lld\n", width);

      const long long height = pVideoTrack->GetHeight();
      printf("\t\tVideo Height\t\t: %lld\n", height);

      const double rate = pVideoTrack->GetFrameRate();
      printf("\t\tVideo Rate\t\t: %f\n", rate);
    }

    if (trackType == mkvparser::Track::kAudio) {
      const AudioTrack* const pAudioTrack =
          static_cast<const AudioTrack*>(pTrack);

      const long long channels = pAudioTrack->GetChannels();
      printf("\t\tAudio Channels\t\t: %lld\n", channels);

      const long long bitDepth = pAudioTrack->GetBitDepth();
      printf("\t\tAudio BitDepth\t\t: %lld\n", bitDepth);

      const double sampleRate = pAudioTrack->GetSamplingRate();
      printf("\t\tAddio Sample Rate\t: %.3f\n", sampleRate);

      const long long codecDelay = pAudioTrack->GetCodecDelay();
      printf("\t\tAudio Codec Delay\t\t: %lld\n", codecDelay);

      const long long seekPreRoll = pAudioTrack->GetSeekPreRoll();
      printf("\t\tAudio Seek Pre Roll\t\t: %lld\n", seekPreRoll);
    }
  }

  printf("\n\n\t\t\t   Cluster Info\n");
  const unsigned long clusterCount = pSegment->GetCount();

  printf("\t\tCluster Count\t: %ld\n\n", clusterCount);

  if (clusterCount == 0) {
    printf("\t\tSegment has no clusters.\n");
    return -1;
  }

  const mkvparser::Cluster* pCluster = pSegment->GetFirst();

  while ((pCluster != NULL) && !pCluster->EOS()) {
    const long long timeCode = pCluster->GetTimeCode();
    printf("\t\tCluster Time Code\t: %lld\n", timeCode);

    const long long time_ns = pCluster->GetTime();
    printf("\t\tCluster Time (ns)\t: %lld\n", time_ns);

    const BlockEntry* pBlockEntry;

    long status = pCluster->GetFirst(pBlockEntry);

    if (status < 0)  // error
    {
      printf("\t\tError parsing first block of cluster\n");
      fflush(stdout);
      return -1;
    }

    while ((pBlockEntry != NULL) && !pBlockEntry->EOS()) {
      const Block* const pBlock = pBlockEntry->GetBlock();
      const long long trackNum = pBlock->GetTrackNumber();
      const unsigned long tn = static_cast<unsigned long>(trackNum);
      const Track* const pTrack = pTracks->GetTrackByNumber(tn);

      if (pTrack == NULL)
        printf("\t\t\tBlock\t\t:UNKNOWN TRACK TYPE\n");
      else {
        const long long trackType = pTrack->GetType();
        const int frameCount = pBlock->GetFrameCount();
        const long long time_ns = pBlock->GetTime(pCluster);
        const long long discard_padding = pBlock->GetDiscardPadding();

        printf("\t\t\tBlock\t\t:%s,%s,%15lld,%lld\n",
               (trackType == mkvparser::Track::kVideo) ? "V" : "A",
               pBlock->IsKey() ? "I" : "P", time_ns, discard_padding);

        for (int i = 0; i < frameCount; ++i) {
          const Block::Frame& theFrame = pBlock->GetFrame(i);
          const long size = theFrame.len;
          const long long offset = theFrame.pos;
          printf("\t\t\t %15ld,%15llx\n", size, offset);
        }
      }

      status = pCluster->GetNext(pBlockEntry, pBlockEntry);

      if (status < 0) {
        printf("\t\t\tError parsing next block of cluster\n");
        fflush(stdout);
        return -1;
      }
    }

    pCluster = pSegment->GetNext(pCluster);
  }

  if (InputHasCues(pSegment.get())) {
    // Walk them.
    const mkvparser::Cues* const cues = pSegment->GetCues();
    const mkvparser::CuePoint* cue = cues->GetFirst();
    int cue_point_num = 1;

    printf("\t\tCues\n");
    do {
      for (track_num = 1; track_num != num_tracks; ++track_num) {
        const mkvparser::Track* const track =
            pTracks->GetTrackByNumber(track_num);
        const mkvparser::CuePoint::TrackPosition* const track_pos =
            cue->Find(track);

        if (track_pos != NULL) {
          const char track_type =
              (track->GetType() == mkvparser::Track::kVideo) ? 'V' : 'A';
          printf(
              "\t\t\tCue Point %4d Track %3lu(%c) Time %14lld "
              "Block %4lld Pos %8llx\n",
              cue_point_num, track_num, track_type,
              cue->GetTime(pSegment.get()), track_pos->m_block,
              track_pos->m_pos);
        }
      }

      cue = cues->GetNext(cue);
      ++cue_point_num;
    } while (cue != NULL);
  }

  fflush(stdout);
  return 0;
}