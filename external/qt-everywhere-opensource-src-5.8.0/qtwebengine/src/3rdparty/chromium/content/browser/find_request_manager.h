// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIND_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_FIND_REQUEST_MANAGER_H_

#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/stop_find_action.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace content {

class RenderFrameHost;
class WebContentsImpl;

// FindRequestManager manages all of the find-in-page requests/replies
// initiated/received through a WebContents. It coordinates searching across
// multiple (potentially out-of-process) frames, handles the aggregation of find
// results from each frame, and facilitates active match traversal. It is
// instantiated once per WebContents, and is owned by that WebContents.
class CONTENT_EXPORT FindRequestManager : public WebContentsObserver {
 public:
  explicit FindRequestManager(WebContentsImpl* web_contents);
  ~FindRequestManager() override;

  // Initiates a find operation for |search_text| with the options specified in
  // |options|. |request_id| uniquely identifies the find request.
  void Find(int request_id,
            const base::string16& search_text,
            const blink::WebFindOptions& options);

  // Stops the active find session and clears the general highlighting of the
  // matches. |action| determines whether the last active match (if any) will be
  // activated, cleared, or remain highlighted.
  void StopFinding(StopFindAction action);

  // Called when a reply is received from a frame with the results from a
  // find request.
  void OnFindReply(RenderFrameHost* rfh,
                   int request_id,
                   int number_of_matches,
                   const gfx::Rect& selection_rect,
                   int active_match_ordinal,
                   bool final_update);

  // Removes a frame from the set of frames being searched. This should be
  // called whenever a frame is discovered to no longer exist.
  void RemoveFrame(RenderFrameHost* rfh);

#if defined(OS_ANDROID)
  // Selects and zooms to the find result nearest to the point (x, y), defined
  // in find-in-page coordinates.
  void ActivateNearestFindResult(float x, float y);

  // Called when a reply is received from a frame in response to the
  // GetNearestFindResult IPC.
  void OnGetNearestFindResultReply(RenderFrameHost* rfh,
                                   int nearest_find_result_request_id,
                                   float distance);

  // Requests the rects of the current find matches from the renderer process.
  void RequestFindMatchRects(int current_version);

  // Called when a reply is received from a frame in response to a request for
  // find match rects.
  void OnFindMatchRectsReply(RenderFrameHost* rfh,
                             int version,
                             const std::vector<gfx::RectF>& rects,
                             const gfx::RectF& active_rect);
#endif

 private:
  // An invalid ID. This value is invalid for any render process ID, render
  // frame ID, find request ID, or find match rects version number.
  static const int kInvalidId;

  // The request data for a single find request.
  struct FindRequest {
    // The find request ID that uniquely identifies this find request.
    int id = kInvalidId;

    // The text that is being searched for in this find request.
    base::string16 search_text;

    // The set of find options in effect for this find request.
    blink::WebFindOptions options;

    FindRequest() = default;
    FindRequest(int id,
                const base::string16& search_text,
                const blink::WebFindOptions& options)
        : id(id), search_text(search_text), options(options) {}
  };

  // WebContentsObserver implementation.
  void RenderFrameDeleted(RenderFrameHost* rfh) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void FrameDeleted(RenderFrameHost* rfh) override;

  // Resets all of the per-session state for a new find-in-page session.
  void Reset(const FindRequest& initial_request);

  // Called internally as find requests come up in the queue.
  void FindInternal(const FindRequest& request);

  // Called when an informative response (a response with enough information to
  // be able to route subsequent find requests) comes in for the find request
  // with ID |request_id|. Advances the |find_request_queue_| if appropriate.
  void AdvanceQueue(int request_id);

  // Sends a find IPC containing the find request |request| to the RenderFrame
  // associated with |rfh|.
  void SendFindIPC(const FindRequest& request, RenderFrameHost* rfh);

  // Sends the find results (as they currently are) to the WebContents.
  void NotifyFindReply(int request_id, bool final_update) const;

  // Returns the initial frame in search order. This will be either the first
  // frame, if searching forward, or the last frame, if searching backward.
  RenderFrameHost* GetInitialFrame(bool forward) const;

  // Traverses the frame tree to find and return the next RenderFrameHost after
  // |from_rfh| in search order. |forward| indicates whether the frame tree
  // should be traversed forward (if true) or backward (if false). If
  // |matches_only| is set, then the frame tree will be traversed until the
  // first frame is found for which matches have been found. If |wrap| is set,
  // then the traversal can wrap around past the last frame to the first one (or
  // vice-versa, if |forward| == false). If no frame can be found under these
  // conditions, nullptr is returned.
  RenderFrameHost* Traverse(RenderFrameHost* from_rfh,
                            bool forward,
                            bool matches_only,
                            bool wrap) const;

  // Adds a frame to the set of frames that are being searched. The new frame
  // will automatically be searched when added, using the same options (stored
  // in |current_request_.options|).
  void AddFrame(RenderFrameHost* rfh);

  // Returns whether |rfh| is in the set of frames being searched in the current
  // find session.
  bool CheckFrame(RenderFrameHost* rfh) const;

  // Computes and updates |active_match_ordinal_| based on |active_frame_| and
  // |relative_active_match_ordinal_|.
  void UpdateActiveMatchOrdinal();

  // Called when all pending find replies have been received for the find
  // request with ID |request_id|. The final update was received from |rfh|.
  void FinalUpdate(int request_id, RenderFrameHost* rfh);

#if defined(OS_ANDROID)
  // Called when a nearest find result reply is no longer pending for a frame.
  void RemoveNearestFindResultPendingReply(RenderFrameHost* rfh);

  // Called when a find match rects reply is no longer pending for a frame.
  void RemoveFindMatchRectsPendingReply(RenderFrameHost* rfh);

  // State related to ActivateNearestFindResult requests.
  struct ActivateNearestFindResultState {
    // An ID to uniquely identify the current nearest find result request and
    // its replies.
    int current_request_id = kInvalidId;

    // The x value of the requested point, in find-in-page coordinates.
    float x = 0.0f;

    // The y value of the requested point, in find-in-page coordinates.
    float y = 0.0f;

    // The distance to the nearest result found so far.
    float nearest_distance = FLT_MAX;

    // The frame containing the nearest result found so far.
    RenderFrameHost* nearest_frame = nullptr;

    // Nearest find result replies are still pending for these frames.
    std::unordered_set<RenderFrameHost*> pending_replies;

    ActivateNearestFindResultState();
    ActivateNearestFindResultState(float x, float y);
    ~ActivateNearestFindResultState();

    static int GetNextID() {
      static int next_id = 0;
      return next_id++;
    }
  } activate_;

  // Data for find match rects in a single frame.
  struct FrameRects {
    // The rects contained in a single frame.
    std::vector<gfx::RectF> rects;

    // The version number for these rects, as reported by their containing
    // frame. This version is incremented independently in each frame.
    int version = kInvalidId;

    FrameRects();
    FrameRects(const std::vector<gfx::RectF>& rects, int version);
    ~FrameRects();
  };

  // State related to FindMatchRects requests.
  struct FindMatchRectsState {
    // The latest find match rects version known by the requester. This will be
    // compared to |known_version_| after polling frames for updates to their
    // match rects, in order to determine if the requester already has the
    // latest version of rects or not.
    int request_version = kInvalidId;

    // The current overall find match rects version known by
    // FindRequestManager. This version should be incremented whenever
    // |frame_rects| is updated.
    int known_version = 0;

    // A map from each frame to its find match rects.
    std::unordered_map<RenderFrameHost*, FrameRects> frame_rects;

    // The active find match rect.
    gfx::RectF active_rect;

    // Find match rects replies are still pending for these frames.
    std::unordered_set<RenderFrameHost*> pending_replies;

    FindMatchRectsState();
    ~FindMatchRectsState();
  } match_rects_;
#endif

  // The WebContents that owns this FindRequestManager.
  WebContentsImpl* const contents_;

  // The request ID of the initial find request in the current find-in-page
  // session, which uniquely identifies this session. Request IDs are included
  // in all find-related IPCs, which allows reply IPCs containing results from
  // previous sessions (with |request_id| < |current_session_id_|) to be easily
  // identified and ignored.
  int current_session_id_;

  // The current find request.
  FindRequest current_request_;

  // The set of frames that are still expected to reply to a pending find
  // request. Frames are removed from |pending_replies_| when their reply with
  // |final_update| set to true is received.
  std::unordered_set<RenderFrameHost*> pending_replies_;

  // Indicates whether an update to the active match ordinal is expected. Once
  // set, |pending_active_match_ordinal_| will not reset until an update to the
  // active match ordinal is received in response to the find request with ID
  // |current_request_.id| (the latest request).
  bool pending_active_match_ordinal_;

  // The number of matches found in each frame. There will necessarily be
  // entries in this map for every frame that is being (or has been) searched in
  // the current find session, and no other frames.
  std::unordered_map<RenderFrameHost*, int> matches_per_frame_;

  // The total number of matches found in the current find-in-page session. This
  // should always be equal to the sum of all the entries in
  // |matches_per_frame_|.
  int number_of_matches_;

  // The frame containing the active match, if one exists, or nullptr otherwise.
  RenderFrameHost* active_frame_;

  // The active match ordinal relative to the matches found in its own frame.
  int relative_active_match_ordinal_;

  // The overall active match ordinal for the current find-in-page session.
  int active_match_ordinal_;

  // The rectangle around the active match, in screen coordinates.
  gfx::Rect selection_rect_;

  // Find requests are queued here when previous requests need to be handled
  // before these ones can be properly routed.
  std::queue<FindRequest> find_request_queue_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIND_REQUEST_MANAGER_H_
