// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/file_video_capture_device.h"

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"

namespace media {
static const int kY4MHeaderMaxSize = 200;
static const char kY4MSimpleFrameDelimiter[] = "FRAME";
static const int kY4MSimpleFrameDelimiterSize = 6;

int ParseY4MInt(const base::StringPiece& token) {
  int temp_int;
  CHECK(base::StringToInt(token, &temp_int)) << token;
  return temp_int;
}

// Extract numerator and denominator out of a token that must have the aspect
// numerator:denominator, both integer numbers.
void ParseY4MRational(const base::StringPiece& token,
                      int* numerator,
                      int* denominator) {
  size_t index_divider = token.find(':');
  CHECK_NE(index_divider, token.npos);
  *numerator = ParseY4MInt(token.substr(0, index_divider));
  *denominator = ParseY4MInt(token.substr(index_divider + 1, token.length()));
  CHECK(*denominator);
}

// This function parses the ASCII string in |header| as belonging to a Y4M file,
// returning the collected format in |video_format|. For a non authoritative
// explanation of the header format, check
// http://wiki.multimedia.cx/index.php?title=YUV4MPEG2
// Restrictions: Only interlaced I420 pixel format is supported, and pixel
// aspect ratio is ignored.
// Implementation notes: Y4M header should end with an ASCII 0x20 (whitespace)
// character, however all examples mentioned in the Y4M header description end
// with a newline character instead. Also, some headers do _not_ specify pixel
// format, in this case it means I420.
// This code was inspired by third_party/libvpx/.../y4minput.* .
void ParseY4MTags(const std::string& file_header,
                  media::VideoCaptureFormat* video_format) {
  video_format->pixel_format = media::PIXEL_FORMAT_I420;
  video_format->frame_size.set_width(0);
  video_format->frame_size.set_height(0);
  size_t index = 0;
  size_t blank_position = 0;
  base::StringPiece token;
  while ((blank_position = file_header.find_first_of("\n ", index)) !=
         std::string::npos) {
    // Every token is supposed to have an identifier letter and a bunch of
    // information immediately after, which we extract into a |token| here.
    token =
        base::StringPiece(&file_header[index + 1], blank_position - index - 1);
    CHECK(!token.empty());
    switch (file_header[index]) {
      case 'W':
        video_format->frame_size.set_width(ParseY4MInt(token));
        break;
      case 'H':
        video_format->frame_size.set_height(ParseY4MInt(token));
        break;
      case 'F': {
        // If the token is "FRAME", it means we have finished with the header.
        if (token[0] == 'R')
          break;
        int fps_numerator, fps_denominator;
        ParseY4MRational(token, &fps_numerator, &fps_denominator);
        video_format->frame_rate = fps_numerator / fps_denominator;
        break;
      }
      case 'I':
        // Interlacing is ignored, but we don't like mixed modes.
        CHECK_NE(token[0], 'm');
        break;
      case 'A':
        // Pixel aspect ratio ignored.
        break;
      case 'C':
        CHECK(token == "420" || token == "420jpeg" || token == "420paldv")
            << token;  // Only I420 is supported, and we fudge the variants.
        break;
      default:
        break;
    }
    // We're done if we have found a newline character right after the token.
    if (file_header[blank_position] == '\n')
      break;
    index = blank_position + 1;
  }
  // Last video format semantic correctness check before sending it back.
  CHECK(video_format->IsValid());
}

// Reads and parses the header of a Y4M |file|, returning the collected pixel
// format in |video_format|. Returns the index of the first byte of the first
// video frame.
// Restrictions: Only trivial per-frame headers are supported.
// static
int64 FileVideoCaptureDevice::ParseFileAndExtractVideoFormat(
    base::File* file,
    media::VideoCaptureFormat* video_format) {
  std::string header(kY4MHeaderMaxSize, 0);
  file->Read(0, &header[0], kY4MHeaderMaxSize - 1);

  size_t header_end = header.find(kY4MSimpleFrameDelimiter);
  CHECK_NE(header_end, header.npos);

  ParseY4MTags(header, video_format);
  return header_end + kY4MSimpleFrameDelimiterSize;
}

// Opens a given file for reading, and returns the file to the caller, who is
// responsible for closing it.
// static
base::File FileVideoCaptureDevice::OpenFileForRead(
    const base::FilePath& file_path) {
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  CHECK(file.IsValid()) << file_path.value();
  return file.Pass();
}

FileVideoCaptureDevice::FileVideoCaptureDevice(const base::FilePath& file_path)
    : capture_thread_("CaptureThread"),
      file_path_(file_path),
      frame_size_(0),
      current_byte_index_(0),
      first_frame_byte_index_(0) {}

FileVideoCaptureDevice::~FileVideoCaptureDevice() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Check if the thread is running.
  // This means that the device have not been DeAllocated properly.
  CHECK(!capture_thread_.IsRunning());
}

void FileVideoCaptureDevice::AllocateAndStart(
    const VideoCaptureParams& params,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(!capture_thread_.IsRunning());

  capture_thread_.Start();
  capture_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&FileVideoCaptureDevice::OnAllocateAndStart,
                 base::Unretained(this),
                 params,
                 base::Passed(&client)));
}

void FileVideoCaptureDevice::StopAndDeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(capture_thread_.IsRunning());

  capture_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&FileVideoCaptureDevice::OnStopAndDeAllocate,
                 base::Unretained(this)));
  capture_thread_.Stop();
}

int FileVideoCaptureDevice::CalculateFrameSize() {
  DCHECK_EQ(capture_format_.pixel_format, PIXEL_FORMAT_I420);
  DCHECK_EQ(capture_thread_.message_loop(), base::MessageLoop::current());
  return capture_format_.frame_size.GetArea() * 12 / 8;
}

void FileVideoCaptureDevice::OnAllocateAndStart(
    const VideoCaptureParams& params,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  DCHECK_EQ(capture_thread_.message_loop(), base::MessageLoop::current());

  client_ = client.Pass();

  // Open the file and parse the header. Get frame size and format.
  DCHECK(!file_.IsValid());
  file_ = OpenFileForRead(file_path_);
  first_frame_byte_index_ =
      ParseFileAndExtractVideoFormat(&file_, &capture_format_);
  current_byte_index_ = first_frame_byte_index_;
  DVLOG(1) << "Opened video file " << capture_format_.frame_size.ToString()
           << ", fps: " << capture_format_.frame_rate;

  frame_size_ = CalculateFrameSize();
  video_frame_.reset(new uint8[frame_size_]);

  capture_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&FileVideoCaptureDevice::OnCaptureTask,
                 base::Unretained(this)));
}

void FileVideoCaptureDevice::OnStopAndDeAllocate() {
  DCHECK_EQ(capture_thread_.message_loop(), base::MessageLoop::current());
  file_.Close();
  client_.reset();
  current_byte_index_ = 0;
  first_frame_byte_index_ = 0;
  frame_size_ = 0;
  video_frame_.reset();
}

void FileVideoCaptureDevice::OnCaptureTask() {
  DCHECK_EQ(capture_thread_.message_loop(), base::MessageLoop::current());
  if (!client_)
    return;
  int result = file_.Read(current_byte_index_,
                          reinterpret_cast<char*>(video_frame_.get()),
                          frame_size_);

  // If we passed EOF to base::File, it will return 0 read characters. In that
  // case, reset the pointer and read again.
  if (result != frame_size_) {
    CHECK_EQ(result, 0);
    current_byte_index_ = first_frame_byte_index_;
    CHECK_EQ(file_.Read(current_byte_index_,
                        reinterpret_cast<char*>(video_frame_.get()),
                        frame_size_),
             frame_size_);
  } else {
    current_byte_index_ += frame_size_ + kY4MSimpleFrameDelimiterSize;
  }

  // Give the captured frame to the client.
  client_->OnIncomingCapturedData(video_frame_.get(),
                                  frame_size_,
                                  capture_format_,
                                  0,
                                  base::TimeTicks::Now());
  // Reschedule next CaptureTask.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FileVideoCaptureDevice::OnCaptureTask,
                 base::Unretained(this)),
      base::TimeDelta::FromSeconds(1) / capture_format_.frame_rate);
}

}  // namespace media
