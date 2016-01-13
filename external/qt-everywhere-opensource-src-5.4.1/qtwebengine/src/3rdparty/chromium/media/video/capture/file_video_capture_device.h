// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_FILE_VIDEO_CAPTURE_DEVICE_H_
#define MEDIA_VIDEO_CAPTURE_FILE_VIDEO_CAPTURE_DEVICE_H_

#include <string>

#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "media/video/capture/video_capture_device.h"

namespace media {

// Implementation of a VideoCaptureDevice class that reads from a file. Used for
// testing the video capture pipeline when no real hardware is available. The
// only supported file format is YUV4MPEG2 (a.k.a. Y4M), a minimal container
// with a series of uncompressed video only frames, see the link
// http://wiki.multimedia.cx/index.php?title=YUV4MPEG2 for more information
// on the file format. Several restrictions and notes apply, see the
// implementation file.
// Example videos can be found in http://media.xiph.org/video/derf.
class MEDIA_EXPORT FileVideoCaptureDevice : public VideoCaptureDevice {
 public:
  static int64 ParseFileAndExtractVideoFormat(
      base::File* file,
      media::VideoCaptureFormat* video_format);
  static base::File OpenFileForRead(const base::FilePath& file_path);

  // Constructor of the class, with a fully qualified file path as input, which
  // represents the Y4M video file to stream repeatedly.
  explicit FileVideoCaptureDevice(const base::FilePath& file_path);

  // VideoCaptureDevice implementation, class methods.
  virtual ~FileVideoCaptureDevice();
  virtual void AllocateAndStart(
      const VideoCaptureParams& params,
      scoped_ptr<VideoCaptureDevice::Client> client) OVERRIDE;
  virtual void StopAndDeAllocate() OVERRIDE;

 private:
  // Returns size in bytes of an I420 frame, not including possible paddings,
  // defined by |capture_format_|.
  int CalculateFrameSize();

  // Called on the |capture_thread_|.
  void OnAllocateAndStart(const VideoCaptureParams& params,
                          scoped_ptr<Client> client);
  void OnStopAndDeAllocate();
  void OnCaptureTask();

  // |thread_checker_| is used to check that destructor, AllocateAndStart() and
  // StopAndDeAllocate() are called in the correct thread that owns the object.
  base::ThreadChecker thread_checker_;

  // |capture_thread_| is used for internal operations via posting tasks to it.
  // It is active between OnAllocateAndStart() and OnStopAndDeAllocate().
  base::Thread capture_thread_;
  // The following members belong to |capture_thread_|.
  scoped_ptr<VideoCaptureDevice::Client> client_;
  const base::FilePath file_path_;
  base::File file_;
  scoped_ptr<uint8[]> video_frame_;
  VideoCaptureFormat capture_format_;
  int frame_size_;
  int64 current_byte_index_;
  int64 first_frame_byte_index_;

  DISALLOW_COPY_AND_ASSIGN(FileVideoCaptureDevice);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_FILE_VIDEO_CAPTURE_DEVICE_H_
