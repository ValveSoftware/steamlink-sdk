/****************************************************************************
**
** Copyright (C) 2012 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "mmrenderermediaplayerservice.h"

#include "mmrenderermediaplayercontrol.h"
#include "mmrenderermetadatareadercontrol.h"
#include "mmrendererplayervideorenderercontrol.h"
#include "mmrendererutil.h"
#include "mmrenderervideowindowcontrol.h"

#ifdef Q_OS_BLACKBERRY
#include "bpsmediaplayercontrol.h"
typedef BpsMediaPlayerControl PlatformSpecificMediaPlayerControl;
#else
#include "ppsmediaplayercontrol.h"
typedef PpsMediaPlayerControl PlatformSpecificMediaPlayerControl;
#endif

QT_BEGIN_NAMESPACE

MmRendererMediaPlayerService::MmRendererMediaPlayerService(QObject *parent)
    : QMediaService(parent),
      m_videoRendererControl(0),
      m_videoWindowControl(0),
      m_mediaPlayerControl(0),
      m_metaDataReaderControl(0),
      m_appHasDrmPermission(false),
      m_appHasDrmPermissionChecked(false)
{
}

MmRendererMediaPlayerService::~MmRendererMediaPlayerService()
{
    // Someone should have called releaseControl(), but better be safe
    delete m_videoRendererControl;
    delete m_videoWindowControl;
    delete m_mediaPlayerControl;
    delete m_metaDataReaderControl;
}

QMediaControl *MmRendererMediaPlayerService::requestControl(const char *name)
{
    if (qstrcmp(name, QMediaPlayerControl_iid) == 0) {
        if (!m_mediaPlayerControl) {
            m_mediaPlayerControl = new PlatformSpecificMediaPlayerControl;
            updateControls();
        }
        return m_mediaPlayerControl;
    }
    else if (qstrcmp(name, QMetaDataReaderControl_iid) == 0) {
        if (!m_metaDataReaderControl) {
            m_metaDataReaderControl = new MmRendererMetaDataReaderControl();
            updateControls();
        }
        return m_metaDataReaderControl;
    }
    else if (qstrcmp(name, QVideoRendererControl_iid) == 0) {
        if (!m_appHasDrmPermissionChecked) {
            m_appHasDrmPermission = checkForDrmPermission();
            m_appHasDrmPermissionChecked = true;
        }

        if (m_appHasDrmPermission) {
            // When the application wants to play back DRM secured media, we can't use
            // the QVideoRendererControl, because we won't have access to the pixel data
            // in this case.
            return 0;
        }

        if (!m_videoRendererControl) {
            m_videoRendererControl = new MmRendererPlayerVideoRendererControl();
            updateControls();
        }
        return m_videoRendererControl;
    }
    else if (qstrcmp(name, QVideoWindowControl_iid) == 0) {
        if (!m_videoWindowControl) {
            m_videoWindowControl = new MmRendererVideoWindowControl();
            updateControls();
        }
        return m_videoWindowControl;
    }
    return 0;
}

void MmRendererMediaPlayerService::releaseControl(QMediaControl *control)
{
    if (control == m_videoRendererControl)
        m_videoRendererControl = 0;
    if (control == m_videoWindowControl)
        m_videoWindowControl = 0;
    if (control == m_mediaPlayerControl)
        m_mediaPlayerControl = 0;
    if (control == m_metaDataReaderControl)
        m_metaDataReaderControl = 0;
    delete control;
}

void MmRendererMediaPlayerService::updateControls()
{
    if (m_videoRendererControl && m_mediaPlayerControl)
        m_mediaPlayerControl->setVideoRendererControl(m_videoRendererControl);

    if (m_videoWindowControl && m_mediaPlayerControl)
        m_mediaPlayerControl->setVideoWindowControl(m_videoWindowControl);

    if (m_metaDataReaderControl && m_mediaPlayerControl)
        m_mediaPlayerControl->setMetaDataReaderControl(m_metaDataReaderControl);
}

QT_END_NAMESPACE
