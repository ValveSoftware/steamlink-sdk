/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Mobility Components.
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

#ifndef MFPLAYERSERVICE_H
#define MFPLAYERSERVICE_H

#include <mfapi.h>
#include <mfidl.h>

#include "qmediaplayer.h"
#include "qmediaresource.h"
#include "qmediaservice.h"
#include "qmediatimerange.h"

QT_BEGIN_NAMESPACE
class QMediaContent;
QT_END_NAMESPACE

QT_USE_NAMESPACE

#if defined(HAVE_WIDGETS) && !defined(Q_WS_SIMULATOR)
class Evr9VideoWindowControl;
#endif
class MFAudioEndpointControl;
class MFVideoRendererControl;
class MFPlayerControl;
class MFMetaDataControl;
class MFPlayerSession;

class MFPlayerService : public QMediaService
{
    Q_OBJECT
public:
    MFPlayerService(QObject *parent = 0);
    ~MFPlayerService();

    QMediaControl* requestControl(const char *name);
    void releaseControl(QMediaControl *control);

    MFAudioEndpointControl* audioEndpointControl() const;
    MFVideoRendererControl* videoRendererControl() const;
#if defined(HAVE_WIDGETS) && !defined(Q_WS_SIMULATOR)
    Evr9VideoWindowControl* videoWindowControl() const;
#endif
    MFMetaDataControl* metaDataControl() const;

private:
    MFPlayerSession *m_session;
    MFVideoRendererControl *m_videoRendererControl;
    MFAudioEndpointControl *m_audioEndpointControl;
#if defined(HAVE_WIDGETS) && !defined(Q_WS_SIMULATOR)
    Evr9VideoWindowControl *m_videoWindowControl;
#endif
    MFPlayerControl        *m_player;
    MFMetaDataControl      *m_metaDataControl;
};

#endif
