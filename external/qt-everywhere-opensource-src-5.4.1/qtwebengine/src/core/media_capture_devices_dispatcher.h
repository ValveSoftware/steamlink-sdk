/****************************************************************************
**
** Copyright (c) 2012 The Chromium Authors. All rights reserved.
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MEDIA_CAPTURE_DEVICES_DISPATCHER_H
#define MEDIA_CAPTURE_DEVICES_DISPATCHER_H

#include <deque>
#include <list>
#include <map>
#include <QtCore/qcompilerdetection.h>

#include "web_contents_adapter_client.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/media_stream_request.h"

class DesktopStreamsRegistry;

// This singleton is used to receive updates about media events from the content
// layer. Based on Chrome's implementation.
class MediaCaptureDevicesDispatcher : public content::MediaObserver,
                                      public content::NotificationObserver {
 public:

  static MediaCaptureDevicesDispatcher *GetInstance();

  void processMediaAccessRequest(WebContentsAdapterClient *, content::WebContents *, const content::MediaStreamRequest &, const content::MediaResponseCallback &);

  // Called back from our WebContentsAdapter to grant the requested permission.
  void handleMediaAccessPermissionResponse(content::WebContents *, const QUrl &securityOrigin, WebContentsAdapterClient::MediaRequestFlags);

  void getDefaultDevices(const std::string &audioDeviceId, const std::string &videoDeviceId, bool audio, bool video, content::MediaStreamDevices *);

  // Overridden from content::MediaObserver:
  virtual void OnAudioCaptureDevicesChanged() Q_DECL_OVERRIDE { }
  virtual void OnVideoCaptureDevicesChanged() Q_DECL_OVERRIDE { }
  virtual void OnMediaRequestStateChanged(int renderProcessId, int renderViewId, int pageRequestId, const GURL &securityOrigin, const content::MediaStreamDevice &device
                                          , content::MediaRequestState state) Q_DECL_OVERRIDE;

  virtual void OnCreatingAudioStream(int /*render_process_id*/, int /*render_frame_id*/) Q_DECL_OVERRIDE { }
  virtual void OnAudioStreamPlaying(int /*render_process_id*/, int /*render_frame_id*/, int /*stream_id*/, const ReadPowerAndClipCallback& /*power_read_callback*/) Q_DECL_OVERRIDE { }
  virtual void OnAudioStreamStopped(int /*render_process_id*/, int /*render_frame_id*/, int /*stream_id*/) Q_DECL_OVERRIDE { }

  DesktopStreamsRegistry *getDesktopStreamsRegistry();

 private:
  friend struct DefaultSingletonTraits<MediaCaptureDevicesDispatcher>;

  struct PendingAccessRequest {
    PendingAccessRequest(const content::MediaStreamRequest &request,
                         const content::MediaResponseCallback &callback);
    ~PendingAccessRequest();

    content::MediaStreamRequest request;
    content::MediaResponseCallback callback;
  };
  typedef std::deque<PendingAccessRequest> RequestsQueue;
  typedef std::map<content::WebContents *, RequestsQueue> RequestsQueues;

  MediaCaptureDevicesDispatcher();
  virtual ~MediaCaptureDevicesDispatcher();

  // content::NotificationObserver implementation.
  virtual void Observe(int type, const content::NotificationSource &source, const content::NotificationDetails &details) Q_DECL_OVERRIDE;

  // Helpers for ProcessMediaAccessRequest().
  void processDesktopCaptureAccessRequest(content::WebContents *, const content::MediaStreamRequest &, const content::MediaResponseCallback &);
  void processScreenCaptureAccessRequest(content::WebContents *,const content::MediaStreamRequest &, const content::MediaResponseCallback &);
  void handleScreenCaptureAccessRequest(content::WebContents *, bool userAccepted, const base::string16 &/*unused callback_input*/);
  void enqueueMediaAccessRequest(content::WebContents *, const content::MediaStreamRequest &, const content::MediaResponseCallback &);
  void ProcessQueuedAccessRequest(content::WebContents *);

  // Called by the MediaObserver() functions, executed on UI thread.
  void updateMediaRequestStateOnUIThread(int renderProcessId, int renderViewId, int pageRequestId, const content::MediaStreamDevice &
                                         , content::MediaRequestState);

  RequestsQueues m_pendingRequests;

  scoped_ptr<DesktopStreamsRegistry> m_desktopStreamsRegistry;

  content::NotificationRegistrar m_notificationsRegistrar;

  // Tracks MEDIA_DESKTOP_VIDEO_CAPTURE sessions which reach the
  // MEDIA_REQUEST_STATE_DONE state.  Sessions are remove when
  // MEDIA_REQUEST_STATE_CLOSING is encountered.
  struct DesktopCaptureSession {
    int render_process_id;
    int render_view_id;
    int page_request_id;
  };
  typedef std::list<DesktopCaptureSession> DesktopCaptureSessions;
  DesktopCaptureSessions m_desktopCaptureSessions;

  DISALLOW_COPY_AND_ASSIGN(MediaCaptureDevicesDispatcher);
};

#endif  // MEDIA_CAPTURE_DEVICES_DISPATCHER_H
