/****************************************************************************
**
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

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media_capture_devices_dispatcher.h"

#include "javascript_dialog_manager_qt.h"
#include "type_conversion.h"
#include "web_contents_view_qt.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/desktop_streams_registry.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_capture_devices.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/media_stream_request.h"
#include "media/audio/audio_manager_base.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::MediaStreamDevices;

namespace {

const content::MediaStreamDevice *findDeviceWithId(const content::MediaStreamDevices &devices, const std::string &deviceId)
{
  content::MediaStreamDevices::const_iterator iter = devices.begin();
  for (; iter != devices.end(); ++iter) {
    if (iter->id == deviceId) {
      return &(*iter);
    }
  }
  return 0;
}

base::string16 getContentsUrl(content::WebContents *webContents)
{
  return base::UTF8ToUTF16(webContents->GetURL().GetOrigin().spec());
}

scoped_ptr<content::MediaStreamUI> getDevicesForDesktopCapture(content::MediaStreamDevices &devices, content::DesktopMediaID mediaId
                                                               , bool captureAudio, bool /*display_notification*/, base::string16 /*application_title*/)
{
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<content::MediaStreamUI> ui;

  // Add selected desktop source to the list.
  devices.push_back(content::MediaStreamDevice(
      content::MEDIA_DESKTOP_VIDEO_CAPTURE, mediaId.ToString(), "Screen"));
  if (captureAudio) {
    // Use the special loopback device ID for system audio capture.
    devices.push_back(content::MediaStreamDevice(
        content::MEDIA_LOOPBACK_AUDIO_CAPTURE,
        media::AudioManagerBase::kLoopbackInputDeviceId, "System Audio"));
  }

  return ui.Pass();
}

WebContentsAdapterClient::MediaRequestFlags mediaRequestFlagsForRequest(const content::MediaStreamRequest &request)
{
    WebContentsAdapterClient::MediaRequestFlags requestFlags;
    if (request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE)
        requestFlags |= WebContentsAdapterClient::MediaAudioCapture;
    if (request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE)
        requestFlags |= WebContentsAdapterClient::MediaVideoCapture;
    return requestFlags;
}

}  // namespace

MediaCaptureDevicesDispatcher::PendingAccessRequest::PendingAccessRequest(const content::MediaStreamRequest &request
                                                                          , const content::MediaResponseCallback &callback)
    : request(request)
    , callback(callback)
{
}

MediaCaptureDevicesDispatcher::PendingAccessRequest::~PendingAccessRequest()
{
}


void MediaCaptureDevicesDispatcher::handleMediaAccessPermissionResponse(content::WebContents *webContents, const QUrl &securityOrigin
                                                                        , WebContentsAdapterClient::MediaRequestFlags authorizationFlags)
{
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    content::MediaStreamDevices devices;
    std::map<content::WebContents*, RequestsQueue>::iterator it = m_pendingRequests.find(webContents);

    if (it == m_pendingRequests.end())
        // WebContents has been destroyed. Don't need to do anything.
        return;

    RequestsQueue &queue(it->second);
    if (queue.empty())
        return;

    content::MediaStreamRequest &request = queue.front().request;

    const QUrl requestSecurityOrigin(toQt(request.security_origin));
    bool securityOriginsMatch = (requestSecurityOrigin.host() == securityOrigin.host()
                                   && requestSecurityOrigin.scheme() == securityOrigin.scheme()
                                   && requestSecurityOrigin.port() == securityOrigin.port());
    if (!securityOriginsMatch)
        qWarning("Security origin mismatch for media access permission: %s requested and %s provided\n", qPrintable(requestSecurityOrigin.toString())
                 , qPrintable(securityOrigin.toString()));


    bool microphoneRequested =
        (request.audio_type && authorizationFlags & WebContentsAdapterClient::MediaAudioCapture);
    bool webcamRequested =
        (request.video_type && authorizationFlags & WebContentsAdapterClient::MediaVideoCapture);
    if (securityOriginsMatch && (microphoneRequested || webcamRequested)) {
        switch (request.request_type) {
        case content::MEDIA_OPEN_DEVICE:
            Q_UNREACHABLE(); // only speculative as this is for Pepper
            getDefaultDevices("", "", microphoneRequested, webcamRequested, &devices);
            break;
        case content::MEDIA_DEVICE_ACCESS:
        case content::MEDIA_GENERATE_STREAM:
        case content::MEDIA_ENUMERATE_DEVICES:
            getDefaultDevices(request.requested_audio_device_id, request.requested_video_device_id,
                        microphoneRequested, webcamRequested, &devices);
            break;
        }
    }

    content::MediaResponseCallback callback = queue.front().callback;
    queue.pop_front();

    if (!queue.empty()) {
        // Post a task to process next queued request. It has to be done
        // asynchronously to make sure that calling infobar is not destroyed until
        // after this function returns.
        BrowserThread::PostTask(
                    BrowserThread::UI, FROM_HERE, base::Bind(&MediaCaptureDevicesDispatcher::ProcessQueuedAccessRequest, base::Unretained(this), webContents));
    }

    callback.Run(devices, devices.empty() ? content::MEDIA_DEVICE_INVALID_STATE : content::MEDIA_DEVICE_OK, scoped_ptr<content::MediaStreamUI>());
}



MediaCaptureDevicesDispatcher *MediaCaptureDevicesDispatcher::GetInstance()
{
    return Singleton<MediaCaptureDevicesDispatcher>::get();
}

MediaCaptureDevicesDispatcher::MediaCaptureDevicesDispatcher()
{
  // MediaCaptureDevicesDispatcher is a singleton. It should be created on
  // UI thread. Otherwise, it will not receive
  // content::NOTIFICATION_WEB_CONTENTS_DESTROYED, and that will result in
  // possible use after free.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  m_notificationsRegistrar.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                               content::NotificationService::AllSources());
}

MediaCaptureDevicesDispatcher::~MediaCaptureDevicesDispatcher()
{
}

void MediaCaptureDevicesDispatcher::Observe(int type, const content::NotificationSource &source, const content::NotificationDetails &details)
{
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED) {
    content::WebContents *webContents = content::Source<content::WebContents>(source).ptr();
    m_pendingRequests.erase(webContents);
  }
}

void MediaCaptureDevicesDispatcher::processMediaAccessRequest(WebContentsAdapterClient *adapterClient, content::WebContents *webContents
                                                              , const content::MediaStreamRequest &request
                                                              , const content::MediaResponseCallback &callback)
{
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Let's not support tab capture for now.
  if (request.video_type == content::MEDIA_TAB_VIDEO_CAPTURE || request.audio_type == content::MEDIA_TAB_AUDIO_CAPTURE)
      return;

  if (request.video_type == content::MEDIA_DESKTOP_VIDEO_CAPTURE || request.audio_type == content::MEDIA_LOOPBACK_AUDIO_CAPTURE)
      // It's still unclear what to make of screen capture. We can rely on existing javascript dialog infrastructure
      // to experiment with this without exposing it through our API yet.
      processDesktopCaptureAccessRequest(webContents, request, callback);
  else {
      enqueueMediaAccessRequest(webContents, request, callback);
      // We might not require this approval for pepper requests.
      adapterClient->runMediaAccessPermissionRequest(toQt(request.security_origin), mediaRequestFlagsForRequest(request));
  }

}

void MediaCaptureDevicesDispatcher::processDesktopCaptureAccessRequest(content::WebContents *webContents, const content::MediaStreamRequest &request
                                                                       , const content::MediaResponseCallback &callback)
{
  content::MediaStreamDevices devices;
  scoped_ptr<content::MediaStreamUI> ui;

  if (request.video_type != content::MEDIA_DESKTOP_VIDEO_CAPTURE) {
    callback.Run(devices, content::MEDIA_DEVICE_INVALID_STATE, ui.Pass());
    return;
  }

  // If the device id wasn't specified then this is a screen capture request
  // (i.e. chooseDesktopMedia() API wasn't used to generate device id).
  if (request.requested_video_device_id.empty()) {
    processScreenCaptureAccessRequest(
        webContents, request, callback);
    return;
  }

  // The extension name that the stream is registered with.
  std::string originalExtensionName;
  // Resolve DesktopMediaID for the specified device id.
  content::DesktopMediaID mediaId =
      getDesktopStreamsRegistry()->RequestMediaForStreamId(
          request.requested_video_device_id, request.render_process_id,
          request.render_view_id, request.security_origin,
          &originalExtensionName);

  // Received invalid device id.
  if (mediaId.type == content::DesktopMediaID::TYPE_NONE) {
    callback.Run(devices, content::MEDIA_DEVICE_INVALID_STATE, ui.Pass());
    return;
  }

  // Audio is only supported for screen capture streams.
  bool capture_audio = (mediaId.type == content::DesktopMediaID::TYPE_SCREEN &&
       request.audio_type == content::MEDIA_LOOPBACK_AUDIO_CAPTURE);

  ui = getDevicesForDesktopCapture(
      devices, mediaId, capture_audio, true,
      getContentsUrl(webContents));

  callback.Run(devices, devices.empty() ? content::MEDIA_DEVICE_INVALID_STATE : content::MEDIA_DEVICE_OK, ui.Pass());
}

void MediaCaptureDevicesDispatcher::processScreenCaptureAccessRequest(content::WebContents *webContents, const content::MediaStreamRequest &request
                                                                      ,const content::MediaResponseCallback &callback)
{
  DCHECK_EQ(request.video_type, content::MEDIA_DESKTOP_VIDEO_CAPTURE);

  // FIXME: expose through the settings once we have them
  const bool screenCaptureEnabled = !qgetenv("QT_WEBENGINE_USE_EXPERIMENTAL_SCREEN_CAPTURE").isNull();

  const bool originIsSecure = request.security_origin.SchemeIsSecure();

  if (screenCaptureEnabled && originIsSecure) {

      enqueueMediaAccessRequest(webContents, request, callback);
      base::Callback<void(bool, const base::string16&)> dialogCallback = base::Bind(&MediaCaptureDevicesDispatcher::handleScreenCaptureAccessRequest,
                                                                                 base::Unretained(this), base::Unretained(webContents));

      QUrl securityOrigin(toQt(request.security_origin));
      QString message = QObject::tr("Do you want %1 to share your screen?").arg(securityOrigin.toString());
      QString title = QObject::tr("%1 Screen Sharing request").arg(securityOrigin.toString());
      JavaScriptDialogManagerQt::GetInstance()->runDialogForContents(webContents, WebContentsAdapterClient::InternalAuthorizationDialog, message
                                                                     , QString(), securityOrigin, dialogCallback, title);
  } else
      callback.Run(content::MediaStreamDevices(), content::MEDIA_DEVICE_INVALID_STATE, scoped_ptr<content::MediaStreamUI>());
}

void MediaCaptureDevicesDispatcher::handleScreenCaptureAccessRequest(content::WebContents *webContents, bool userAccepted, const base::string16 &)
{
    content::MediaStreamDevices devices;
    scoped_ptr<content::MediaStreamUI> ui;
    if (userAccepted) {
      content::DesktopMediaID screenId = content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN, 0);
      ui = getDevicesForDesktopCapture(devices, screenId,  false/*capture_audio*/, false/*display_notification*/, getContentsUrl(webContents));
    }
    std::map<content::WebContents*, RequestsQueue>::iterator it =
        m_pendingRequests.find(webContents);
    if (it == m_pendingRequests.end()) {
      // WebContents has been destroyed. Don't need to do anything.
      return;
    }

    RequestsQueue &queue(it->second);
    if (queue.empty())
      return;

    content::MediaResponseCallback callback = queue.front().callback;
    queue.pop_front();

    callback.Run(devices, devices.empty() ? content::MEDIA_DEVICE_INVALID_STATE : content::MEDIA_DEVICE_OK, ui.Pass());
}

void MediaCaptureDevicesDispatcher::enqueueMediaAccessRequest(content::WebContents *webContents, const content::MediaStreamRequest &request
                                                                     ,const content::MediaResponseCallback &callback)
{
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RequestsQueue &queue = m_pendingRequests[webContents];
  queue.push_back(PendingAccessRequest(request, callback));
}

void MediaCaptureDevicesDispatcher::ProcessQueuedAccessRequest(content::WebContents *webContents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::map<content::WebContents*, RequestsQueue>::iterator it =
      m_pendingRequests.find(webContents);

  if (it == m_pendingRequests.end() || it->second.empty())
    return;

  RequestsQueue &queue(it->second);
  if (queue.empty())
    return;

  content::MediaStreamRequest &request = queue.front().request;

  DCHECK(!it->second.empty());
  WebContentsAdapterClient *adapterClient = WebContentsViewQt::from(static_cast<content::WebContentsImpl*>(webContents)->GetView())->client();
  adapterClient->runMediaAccessPermissionRequest(toQt(request.security_origin), mediaRequestFlagsForRequest(request));
}

void MediaCaptureDevicesDispatcher::getDefaultDevices(const std::string &audioDeviceId, const std::string &videoDeviceId, bool audio, bool video
                                                      , content::MediaStreamDevices *devices)
{
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(audio || video);

  if (audio) {
    const content::MediaStreamDevices &audioDevices = content::MediaCaptureDevices::GetInstance()->GetAudioCaptureDevices();
    const content::MediaStreamDevice *device = findDeviceWithId(audioDevices, audioDeviceId);
    if (!device && !audioDevices.empty())
        device = &(*audioDevices.begin());
    if (device)
      devices->push_back(*device);
  }

  if (video) {
    const content::MediaStreamDevices &videoDevices = content::MediaCaptureDevices::GetInstance()->GetVideoCaptureDevices();
    const content::MediaStreamDevice *device = findDeviceWithId(videoDevices, videoDeviceId);
    if (!device && !videoDevices.empty())
      device = &(*videoDevices.begin());
    if (device)
      devices->push_back(*device);
  }
}

DesktopStreamsRegistry *MediaCaptureDevicesDispatcher::getDesktopStreamsRegistry()
{
  if (!m_desktopStreamsRegistry)
    m_desktopStreamsRegistry.reset(new DesktopStreamsRegistry());
  return m_desktopStreamsRegistry.get();
}

void MediaCaptureDevicesDispatcher::OnMediaRequestStateChanged(int renderProcessId, int renderViewId, int pageRequestId, const GURL& securityOrigin
                                                               , const content::MediaStreamDevice &device, content::MediaRequestState state)
{
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &MediaCaptureDevicesDispatcher::updateMediaRequestStateOnUIThread,
          base::Unretained(this), renderProcessId, renderViewId,
          pageRequestId, device, state));
}

void MediaCaptureDevicesDispatcher::updateMediaRequestStateOnUIThread(int renderProcessId, int renderViewId, int pageRequestId
                                                                      , const content::MediaStreamDevice &device, content::MediaRequestState state)
{
  // Track desktop capture sessions.  Tracking is necessary to avoid unbalanced
  // session counts since not all requests will reach MEDIA_REQUEST_STATE_DONE,
  // but they will all reach MEDIA_REQUEST_STATE_CLOSING.
  if (device.type == content::MEDIA_DESKTOP_VIDEO_CAPTURE) {
    if (state == content::MEDIA_REQUEST_STATE_DONE) {
      DesktopCaptureSession session = { renderProcessId, renderViewId,
                                        pageRequestId };
      m_desktopCaptureSessions.push_back(session);
    } else if (state == content::MEDIA_REQUEST_STATE_CLOSING) {
      for (DesktopCaptureSessions::iterator it =
               m_desktopCaptureSessions.begin();
           it != m_desktopCaptureSessions.end();
           ++it) {
        if (it->render_process_id == renderProcessId &&
            it->render_view_id == renderViewId &&
            it->page_request_id == pageRequestId) {
          m_desktopCaptureSessions.erase(it);
          break;
        }
      }
    }
  }

  // Cancel the request.
  if (state == content::MEDIA_REQUEST_STATE_CLOSING) {
    bool found = false;
    for (RequestsQueues::iterator rqs_it = m_pendingRequests.begin();
         rqs_it != m_pendingRequests.end(); ++rqs_it) {
      RequestsQueue &queue = rqs_it->second;
      for (RequestsQueue::iterator it = queue.begin();
           it != queue.end(); ++it) {
        if (it->request.render_process_id == renderProcessId &&
            it->request.render_view_id == renderViewId &&
            it->request.page_request_id == pageRequestId) {
          queue.erase(it);
          found = true;
          break;
        }
      }
      if (found)
        break;
    }
  }
}
