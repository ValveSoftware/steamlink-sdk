// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mhtml_generation_manager.h"

#include <map>
#include <queue>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/bad_message.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/mhtml_generation_params.h"
#include "net/base/mime_util.h"

namespace content {

// The class and all of its members live on the UI thread.  Only static methods
// are executed on other threads.
class MHTMLGenerationManager::Job : public RenderProcessHostObserver {
 public:
  Job(int job_id,
      WebContents* web_contents,
      const MHTMLGenerationParams& params,
      const GenerateMHTMLCallback& callback);
  ~Job() override;

  int id() const { return job_id_; }
  void set_browser_file(base::File file) { browser_file_ = std::move(file); }

  const GenerateMHTMLCallback& callback() const { return callback_; }

  // Indicates whether we expect a message from the |sender| at this time.
  // We expect only one message per frame - therefore calling this method
  // will always clear |frame_tree_node_id_of_busy_frame_|.
  bool IsMessageFromFrameExpected(RenderFrameHostImpl* sender);

  // Handler for FrameHostMsg_SerializeAsMHTMLResponse (a notification from the
  // renderer that the MHTML generation for previous frame has finished).
  // Returns |true| upon success; |false| otherwise.
  bool OnSerializeAsMHTMLResponse(
      const std::set<std::string>& digests_of_uris_of_serialized_resources);

  // Sends IPC to the renderer, asking for MHTML generation of the next frame.
  //
  // Returns true if the message was sent successfully; false otherwise.
  bool SendToNextRenderFrame();

  // Indicates if more calls to SendToNextRenderFrame are needed.
  bool IsDone() const {
    bool waiting_for_response_from_renderer =
        frame_tree_node_id_of_busy_frame_ !=
        FrameTreeNode::kFrameTreeNodeInvalidId;
    bool no_more_requests_to_send = pending_frame_tree_node_ids_.empty();
    return !waiting_for_response_from_renderer && no_more_requests_to_send;
  }

  // Close the file on the file thread and respond back on the UI thread with
  // file size.
  void CloseFile(base::Callback<void(int64_t file_size)> callback);

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           base::TerminationStatus status,
                           int exit_code) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  void MarkAsFinished();

 private:
  static int64_t CloseFileOnFileThread(base::File file);
  void AddFrame(RenderFrameHost* render_frame_host);

  // Creates a new map with values (content ids) the same as in
  // |frame_tree_node_to_content_id_| map, but with the keys translated from
  // frame_tree_node_id into a |site_instance|-specific routing_id.
  std::map<int, std::string> CreateFrameRoutingIdToContentId(
      SiteInstance* site_instance);

  // Id used to map renderer responses to jobs.
  // See also MHTMLGenerationManager::id_to_job_ map.
  int job_id_;

  // User-configurable parameters. Includes the file location, binary encoding
  // choices, and whether to skip storing resources marked
  // Cache-Control: no-store.
  MHTMLGenerationParams params_;

  // The IDs of frames that still need to be processed.
  std::queue<int> pending_frame_tree_node_ids_;

  // Identifies a frame to which we've sent FrameMsg_SerializeAsMHTML but for
  // which we didn't yet process FrameHostMsg_SerializeAsMHTMLResponse via
  // OnSerializeAsMHTMLResponse.
  int frame_tree_node_id_of_busy_frame_;

  // The handle to the file the MHTML is saved to for the browser process.
  base::File browser_file_;

  // Map from frames to content ids (see WebFrameSerializer::generateMHTMLParts
  // for more details about what "content ids" are and how they are used).
  std::map<int, std::string> frame_tree_node_to_content_id_;

  // MIME multipart boundary to use in the MHTML doc.
  std::string mhtml_boundary_marker_;

  // Digests of URIs of already generated MHTML parts.
  std::set<std::string> digests_of_already_serialized_uris_;
  std::string salt_;

  // The callback to call once generation is complete.
  const GenerateMHTMLCallback callback_;

  // Whether the job is finished (set to true only for the short duration of
  // time between MHTMLGenerationManager::JobFinished is called and the job is
  // destroyed by MHTMLGenerationManager::OnFileClosed).
  bool is_finished_;

  // RAII helper for registering this Job as a RenderProcessHost observer.
  ScopedObserver<RenderProcessHost, MHTMLGenerationManager::Job>
      observed_renderer_process_host_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

MHTMLGenerationManager::Job::Job(int job_id,
                                 WebContents* web_contents,
                                 const MHTMLGenerationParams& params,
                                 const GenerateMHTMLCallback& callback)
    : job_id_(job_id),
      params_(params),
      frame_tree_node_id_of_busy_frame_(FrameTreeNode::kFrameTreeNodeInvalidId),
      mhtml_boundary_marker_(net::GenerateMimeMultipartBoundary()),
      salt_(base::GenerateGUID()),
      callback_(callback),
      is_finished_(false),
      observed_renderer_process_host_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents->ForEachFrame(base::Bind(
      &MHTMLGenerationManager::Job::AddFrame,
      base::Unretained(this)));  // Safe because ForEachFrame is synchronous.

  // Main frame needs to be processed first.
  DCHECK(!pending_frame_tree_node_ids_.empty());
  DCHECK(FrameTreeNode::GloballyFindByID(pending_frame_tree_node_ids_.front())
             ->parent() == nullptr);
}

MHTMLGenerationManager::Job::~Job() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

std::map<int, std::string>
MHTMLGenerationManager::Job::CreateFrameRoutingIdToContentId(
    SiteInstance* site_instance) {
  std::map<int, std::string> result;
  for (const auto& it : frame_tree_node_to_content_id_) {
    int ftn_id = it.first;
    const std::string& content_id = it.second;

    FrameTreeNode* ftn = FrameTreeNode::GloballyFindByID(ftn_id);
    if (!ftn)
      continue;

    int routing_id =
        ftn->render_manager()->GetRoutingIdForSiteInstance(site_instance);
    if (routing_id == MSG_ROUTING_NONE)
      continue;

    result[routing_id] = content_id;
  }
  return result;
}

bool MHTMLGenerationManager::Job::SendToNextRenderFrame() {
  DCHECK(browser_file_.IsValid());
  DCHECK(!pending_frame_tree_node_ids_.empty());

  FrameMsg_SerializeAsMHTML_Params ipc_params;
  ipc_params.job_id = job_id_;
  ipc_params.mhtml_boundary_marker = mhtml_boundary_marker_;
  ipc_params.mhtml_binary_encoding = params_.use_binary_encoding;
  ipc_params.mhtml_cache_control_policy = params_.cache_control_policy;

  int frame_tree_node_id = pending_frame_tree_node_ids_.front();
  pending_frame_tree_node_ids_.pop();
  ipc_params.is_last_frame = pending_frame_tree_node_ids_.empty();

  FrameTreeNode* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!ftn)  // The contents went away.
    return false;
  RenderFrameHost* rfh = ftn->current_frame_host();

  // Get notified if the target of the IPC message dies between responding.
  observed_renderer_process_host_.RemoveAll();
  observed_renderer_process_host_.Add(rfh->GetProcess());

  // Tell the renderer to skip (= deduplicate) already covered MHTML parts.
  ipc_params.salt = salt_;
  ipc_params.digests_of_uris_to_skip = digests_of_already_serialized_uris_;

  ipc_params.destination_file = IPC::GetPlatformFileForTransit(
      browser_file_.GetPlatformFile(), false);  // |close_source_handle|.
  ipc_params.frame_routing_id_to_content_id =
      CreateFrameRoutingIdToContentId(rfh->GetSiteInstance());

  // Send the IPC asking the renderer to serialize the frame.
  DCHECK_EQ(FrameTreeNode::kFrameTreeNodeInvalidId,
            frame_tree_node_id_of_busy_frame_);
  frame_tree_node_id_of_busy_frame_ = frame_tree_node_id;
  rfh->Send(new FrameMsg_SerializeAsMHTML(rfh->GetRoutingID(), ipc_params));
  return true;
}

void MHTMLGenerationManager::Job::RenderProcessExited(
    RenderProcessHost* host,
    base::TerminationStatus status,
    int exit_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  MHTMLGenerationManager::GetInstance()->RenderProcessExited(this);
}

void MHTMLGenerationManager::Job::MarkAsFinished() {
  DCHECK(!is_finished_);
  is_finished_ = true;

  // Stopping RenderProcessExited notifications is needed to avoid calling
  // JobFinished twice.  See also https://crbug.com/612098.
  observed_renderer_process_host_.RemoveAll();
}

void MHTMLGenerationManager::Job::AddFrame(RenderFrameHost* render_frame_host) {
  auto* rfhi = static_cast<RenderFrameHostImpl*>(render_frame_host);
  int frame_tree_node_id = rfhi->frame_tree_node()->frame_tree_node_id();
  pending_frame_tree_node_ids_.push(frame_tree_node_id);

  std::string guid = base::GenerateGUID();
  std::string content_id = base::StringPrintf("<frame-%d-%s@mhtml.blink>",
                                              frame_tree_node_id, guid.c_str());
  frame_tree_node_to_content_id_[frame_tree_node_id] = content_id;
}

void MHTMLGenerationManager::Job::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observed_renderer_process_host_.Remove(host);
}

void MHTMLGenerationManager::Job::CloseFile(
    base::Callback<void(int64_t)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!browser_file_.IsValid()) {
    callback.Run(-1);
    return;
  }

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MHTMLGenerationManager::Job::CloseFileOnFileThread,
                 base::Passed(std::move(browser_file_))),
      callback);
}

bool MHTMLGenerationManager::Job::IsMessageFromFrameExpected(
    RenderFrameHostImpl* sender) {
  int sender_id = sender->frame_tree_node()->frame_tree_node_id();
  if (sender_id != frame_tree_node_id_of_busy_frame_)
    return false;

  // We only expect one message per frame - let's make sure subsequent messages
  // from the same |sender| will be rejected.
  frame_tree_node_id_of_busy_frame_ = FrameTreeNode::kFrameTreeNodeInvalidId;

  return true;
}

bool MHTMLGenerationManager::Job::OnSerializeAsMHTMLResponse(
    const std::set<std::string>& digests_of_uris_of_serialized_resources) {
  // Renderer should be deduping resources with the same uris.
  DCHECK_EQ(0u, base::STLSetIntersection<std::set<std::string>>(
                    digests_of_already_serialized_uris_,
                    digests_of_uris_of_serialized_resources).size());
  digests_of_already_serialized_uris_.insert(
      digests_of_uris_of_serialized_resources.begin(),
      digests_of_uris_of_serialized_resources.end());

  if (pending_frame_tree_node_ids_.empty())
    return true;  // Report success - all frames have been processed.

  return SendToNextRenderFrame();
}

// static
int64_t MHTMLGenerationManager::Job::CloseFileOnFileThread(base::File file) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(file.IsValid());
  int64_t file_size = file.GetLength();
  file.Close();
  return file_size;
}

MHTMLGenerationManager* MHTMLGenerationManager::GetInstance() {
  return base::Singleton<MHTMLGenerationManager>::get();
}

MHTMLGenerationManager::MHTMLGenerationManager() : next_job_id_(0) {}

MHTMLGenerationManager::~MHTMLGenerationManager() {
  STLDeleteValues(&id_to_job_);
}

void MHTMLGenerationManager::SaveMHTML(WebContents* web_contents,
                                       const MHTMLGenerationParams& params,
                                       const GenerateMHTMLCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int job_id = NewJob(web_contents, params, callback);

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MHTMLGenerationManager::CreateFile, params.file_path),
      base::Bind(&MHTMLGenerationManager::OnFileAvailable,
                 base::Unretained(this),  // Safe b/c |this| is a singleton.
                 job_id));
}

void MHTMLGenerationManager::OnSerializeAsMHTMLResponse(
    RenderFrameHostImpl* sender,
    int job_id,
    bool mhtml_generation_in_renderer_succeeded,
    const std::set<std::string>& digests_of_uris_of_serialized_resources) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Job* job = FindJob(job_id);
  if (!job || !job->IsMessageFromFrameExpected(sender)) {
    NOTREACHED();
    ReceivedBadMessage(sender->GetProcess(),
                       bad_message::DWNLD_INVALID_SERIALIZE_AS_MHTML_RESPONSE);
    return;
  }

  if (!mhtml_generation_in_renderer_succeeded) {
    JobFinished(job, JobStatus::FAILURE);
    return;
  }

  if (!job->OnSerializeAsMHTMLResponse(
          digests_of_uris_of_serialized_resources)) {
    JobFinished(job, JobStatus::FAILURE);
    return;
  }

  if (job->IsDone())
    JobFinished(job, JobStatus::SUCCESS);
}

// static
base::File MHTMLGenerationManager::CreateFile(const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  // SECURITY NOTE: A file descriptor to the file created below will be passed
  // to multiple renderer processes which (in out-of-process iframes mode) can
  // act on behalf of separate web principals.  Therefore it is important to
  // only allow writing to the file and forbid reading from the file (as this
  // would allow reading content generated by other renderers / other web
  // principals).
  uint32_t file_flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE;

  base::File browser_file(file_path, file_flags);
  if (!browser_file.IsValid()) {
    LOG(ERROR) << "Failed to create file to save MHTML at: " <<
        file_path.value();
  }
  return browser_file;
}

void MHTMLGenerationManager::OnFileAvailable(int job_id,
                                             base::File browser_file) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Job* job = FindJob(job_id);
  DCHECK(job);

  if (!browser_file.IsValid()) {
    LOG(ERROR) << "Failed to create file";
    JobFinished(job, JobStatus::FAILURE);
    return;
  }

  job->set_browser_file(std::move(browser_file));

  if (!job->SendToNextRenderFrame()) {
    JobFinished(job, JobStatus::FAILURE);
  }
}

void MHTMLGenerationManager::JobFinished(Job* job, JobStatus job_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(job);

  job->MarkAsFinished();
  job->CloseFile(
      base::Bind(&MHTMLGenerationManager::OnFileClosed,
                 base::Unretained(this),  // Safe b/c |this| is a singleton.
                 job->id(), job_status));
}

void MHTMLGenerationManager::OnFileClosed(int job_id,
                                          JobStatus job_status,
                                          int64_t file_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Job* job = FindJob(job_id);
  job->callback().Run(job_status == JobStatus::SUCCESS ? file_size : -1);
  id_to_job_.erase(job_id);
  delete job;
}

int MHTMLGenerationManager::NewJob(WebContents* web_contents,
                                   const MHTMLGenerationParams& params,
                                   const GenerateMHTMLCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int job_id = next_job_id_++;
  id_to_job_[job_id] = new Job(job_id, web_contents, params, callback);
  return job_id;
}

MHTMLGenerationManager::Job* MHTMLGenerationManager::FindJob(int job_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  IDToJobMap::iterator iter = id_to_job_.find(job_id);
  if (iter == id_to_job_.end()) {
    NOTREACHED();
    return nullptr;
  }
  return iter->second;
}

void MHTMLGenerationManager::RenderProcessExited(Job* job) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(job);
  JobFinished(job, JobStatus::FAILURE);
}

}  // namespace content
