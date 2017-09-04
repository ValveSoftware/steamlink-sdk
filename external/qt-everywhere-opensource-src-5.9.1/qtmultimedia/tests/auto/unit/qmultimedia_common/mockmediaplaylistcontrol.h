/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MOCKMEDIAPLAYLISTCONTROL_H
#define MOCKMEDIAPLAYLISTCONTROL_H

#include <private/qmediaplaylistcontrol_p.h>
#include <private/qmediaplaylistnavigator_p.h>
#include <private/qmedianetworkplaylistprovider_p.h>

#include "mockreadonlyplaylistprovider.h"

class MockMediaPlaylistControl : public QMediaPlaylistControl
{
public:
    MockMediaPlaylistControl(bool readonly = false, QObject *parent = 0)
        : QMediaPlaylistControl(parent)
        , m_navigator(0)
        , m_playlist(0)
        , m_ownsProvider(false)
        , m_readOnly(readonly)
    {
        reset();
    }

    ~MockMediaPlaylistControl()
    {
    }

    void reset()
    {
        delete m_navigator;
        if (m_ownsProvider)
            delete m_playlist;

        if (m_readOnly)
            m_playlist = new MockReadOnlyPlaylistProvider(this);
        else
            m_playlist = new QMediaNetworkPlaylistProvider(this);

        m_ownsProvider = true;
        m_navigator = new QMediaPlaylistNavigator(m_playlist, this);
    }

    void setReadOnly(bool ro)
    {
        if (m_readOnly == ro)
            return;

        m_readOnly = ro;
        reset();
    }

    QMediaPlaylistProvider* playlistProvider() const { return m_playlist; }
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

        if (m_ownsProvider)
            delete m_playlist;
        m_playlist = newProvider;
        m_ownsProvider = false;

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
    QMediaPlaylistProvider *m_playlist;
    bool m_ownsProvider;
    bool m_readOnly;
};

#endif // MOCKMEDIAPLAYLISTCONTROL_H
