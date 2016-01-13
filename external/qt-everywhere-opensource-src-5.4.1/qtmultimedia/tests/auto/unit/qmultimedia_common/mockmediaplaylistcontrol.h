/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef MOCKMEDIAPLAYLISTCONTROL_H
#define MOCKMEDIAPLAYLISTCONTROL_H

#include <private/qmediaplaylistcontrol_p.h>
#include <private/qmediaplaylistnavigator_p.h>

#include "mockreadonlyplaylistprovider.h"

// Hmm, read only.
class MockMediaPlaylistControl : public QMediaPlaylistControl
{
    Q_OBJECT
public:
    MockMediaPlaylistControl(QObject *parent) : QMediaPlaylistControl(parent)
    {
        m_navigator = new QMediaPlaylistNavigator(new MockReadOnlyPlaylistProvider(this), this);
    }

    ~MockMediaPlaylistControl()
    {
    }

    QMediaPlaylistProvider* playlistProvider() const { return m_navigator->playlist(); }
    bool setPlaylistProvider(QMediaPlaylistProvider *newProvider)
    {
        bool bMediaContentChanged = false;
        int i = 0;
        for (; i < playlistProvider()->mediaCount(); i++) {
            if (playlistProvider()->media(i).canonicalUrl().toString() != newProvider->media(i).canonicalUrl().toString()) {
                bMediaContentChanged = true;
                break;
            }
        }

        if (playlistProvider()->mediaCount() != newProvider->mediaCount() || bMediaContentChanged ) {
            emit playlistProviderChanged();
            emit currentMediaChanged(newProvider->media(i));
        }

        m_navigator->setPlaylist(newProvider);
        return true;
    }

    int currentIndex() const { return m_navigator->currentIndex(); }
    void setCurrentIndex(int position)
    {
        if (position != currentIndex())
            emit currentIndexChanged(position);
        m_navigator->jump(position);
    }

    int nextIndex(int steps) const { return m_navigator->nextIndex(steps); }
    int previousIndex(int steps) const { return m_navigator->previousIndex(steps); }

    void next() { m_navigator->next(); }
    void previous() { m_navigator->previous(); }

    QMediaPlaylist::PlaybackMode playbackMode() const { return m_navigator->playbackMode(); }
    void setPlaybackMode(QMediaPlaylist::PlaybackMode mode)
    {
        if (playbackMode() != mode)
            emit playbackModeChanged(mode);

        m_navigator->setPlaybackMode(mode);
    }

private:
    QMediaPlaylistNavigator *m_navigator;
};

#endif // MOCKMEDIAPLAYLISTCONTROL_H
