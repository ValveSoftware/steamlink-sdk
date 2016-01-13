// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_
#define CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_

#include <map>

#include "base/files/file.h"
#include "base/memory/singleton.h"
#include "base/process/process.h"
#include "ipc/ipc_platform_file.h"

namespace base {
class FilePath;
}

namespace content {

class WebContents;

class MHTMLGenerationManager {
 public:
  static MHTMLGenerationManager* GetInstance();

  typedef base::Callback<void(int64 /* size of the file */)>
      GenerateMHTMLCallback;

  // Instructs the render view to generate a MHTML representation of the current
  // page for |web_contents|.
  void SaveMHTML(WebContents* web_contents,
                 const base::FilePath& file,
                 const GenerateMHTMLCallback& callback);

  // Instructs the render view to generate a MHTML representation of the current
  // page for |web_contents|.
  void StreamMHTML(WebContents* web_contents,
                   base::File file,
                   const GenerateMHTMLCallback& callback);

  // Notification from the renderer that the MHTML generation finished.
  // |mhtml_data_size| contains the size in bytes of the generated MHTML data,
  // or -1 in case of failure.
  void MHTMLGenerated(int job_id, int64 mhtml_data_size);

 private:
  friend struct DefaultSingletonTraits<MHTMLGenerationManager>;
  class Job;

  MHTMLGenerationManager();
  virtual ~MHTMLGenerationManager();

  // Called on the file thread to create |file|.
  void CreateFile(int job_id,
                  const base::FilePath& file,
                  base::ProcessHandle renderer_process);

  // Called on the UI thread when the file that should hold the MHTML data has
  // been created.  This receives a handle to that file for the browser process
  // and one for the renderer process.
  void FileAvailable(int job_id,
                     base::File browser_file,
                     IPC::PlatformFileForTransit renderer_file);

  // Called on the file thread to close the file the MHTML was saved to.
  void CloseFile(base::File file);

  // Called on the UI thread when a job has been processed (successfully or
  // not).  Closes the file and removes the job from the job map.
  // |mhtml_data_size| is -1 if the MHTML generation failed.
  void JobFinished(int job_id, int64 mhtml_data_size);

  // Creates an register a new job.
  int NewJob(WebContents* web_contents, const GenerateMHTMLCallback& callback);

  // Called when the render process connected to a job exits.
  void RenderProcessExited(Job* job);

  typedef std::map<int, Job*> IDToJobMap;
  IDToJobMap id_to_job_;

  DISALLOW_COPY_AND_ASSIGN(MHTMLGenerationManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_
