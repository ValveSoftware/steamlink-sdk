/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtMultimedia of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

package org.qtproject.qt5.android.multimedia;

import java.io.IOException;
import java.lang.String;
import java.io.FileInputStream;

// API is level is < 9 unless marked otherwise.
import android.content.Context;
import android.media.MediaPlayer;
import android.net.Uri;
import android.util.Log;
import java.io.FileDescriptor;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.view.SurfaceHolder;

public class QtAndroidMediaPlayer
{
    // Native callback functions for MediaPlayer
    native public void onErrorNative(int what, int extra, long id);
    native public void onBufferingUpdateNative(int percent, long id);
    native public void onProgressUpdateNative(int progress, long id);
    native public void onDurationChangedNative(int duration, long id);
    native public void onInfoNative(int what, int extra, long id);
    native public void onVideoSizeChangedNative(int width, int height, long id);
    native public void onStateChangedNative(int state, long id);

    private MediaPlayer mMediaPlayer = null;
    private Uri mUri = null;
    private final long mID;
    private final Context mContext;
    private boolean mMuted = false;
    private int mVolume = 100;
    private static final String TAG = "Qt MediaPlayer";
    private SurfaceHolder mSurfaceHolder = null;

    private class State {
        final static int Uninitialized = 0x1 /* End */;
        final static int Idle = 0x2;
        final static int Preparing = 0x4;
        final static int Prepared = 0x8;
        final static int Initialized = 0x10;
        final static int Started = 0x20;
        final static int Stopped = 0x40;
        final static int Paused = 0x80;
        final static int PlaybackCompleted = 0x100;
        final static int Error = 0x200;
    }

    private volatile int mState = State.Uninitialized;

    /**
     * MediaPlayer OnErrorListener
     */
    private class MediaPlayerErrorListener
    implements MediaPlayer.OnErrorListener
    {
        @Override
        public boolean onError(final MediaPlayer mp,
                               final int what,
                               final int extra)
        {
            setState(State.Error);
            onErrorNative(what, extra, mID);
            return true;
        }

    }

    /**
     * MediaPlayer OnBufferingListener
     */
    private class MediaPlayerBufferingListener
    implements MediaPlayer.OnBufferingUpdateListener
    {
        private int mBufferPercent = -1;
        @Override
        public void onBufferingUpdate(final android.media.MediaPlayer mp,
                                      final int percent)
        {
            // Avoid updates when percent is unchanged.
            // E.g., we keep getting updates when percent == 100
            if (mBufferPercent == percent)
                return;

            onBufferingUpdateNative((mBufferPercent = percent), mID);
        }

    }

    /**
     * MediaPlayer OnCompletionListener
     */
    private class MediaPlayerCompletionListener
    implements MediaPlayer.OnCompletionListener
    {
        @Override
        public void onCompletion(final MediaPlayer mp)
        {
            setState(State.PlaybackCompleted);
        }

    }

    /**
     * MediaPlayer OnInfoListener
     */
    private class MediaPlayerInfoListener
    implements MediaPlayer.OnInfoListener
    {
        @Override
        public boolean onInfo(final MediaPlayer mp,
                              final int what,
                              final int extra)
        {
            onInfoNative(what, extra, mID);
            return true;
        }

    }

    /**
     * MediaPlayer OnPreparedListener
     */
    private class MediaPlayerPreparedListener
    implements MediaPlayer.OnPreparedListener
    {

        @Override
        public void onPrepared(final MediaPlayer mp)
        {
            setState(State.Prepared);
            onDurationChangedNative(getDuration(), mID);
        }

    }

    /**
     * MediaPlayer OnSeekCompleteListener
     */
    private class MediaPlayerSeekCompleteListener
    implements MediaPlayer.OnSeekCompleteListener
    {

        @Override
        public void onSeekComplete(final MediaPlayer mp)
        {
            onProgressUpdateNative(getCurrentPosition(), mID);
        }

    }

    /**
     * MediaPlayer OnVideoSizeChangedListener
     */
    private class MediaPlayerVideoSizeChangedListener
    implements MediaPlayer.OnVideoSizeChangedListener
    {

        @Override
        public void onVideoSizeChanged(final MediaPlayer mp,
                                       final int width,
                                       final int height)
        {
            onVideoSizeChangedNative(width, height, mID);
        }

    }

    public QtAndroidMediaPlayer(final Context context, final long id)
    {
        mID = id;
        mContext = context;
    }

    private void setState(int state)
    {
        if (mState == state)
            return;

        mState = state;

        onStateChangedNative(mState, mID);
    }


    private void init()
    {
        if (mMediaPlayer == null) {
            mMediaPlayer = new MediaPlayer();
            setState(State.Idle);
            // Make sure the new media player has the volume that was set on the QMediaPlayer
            setVolumeHelper(mMuted ? 0 : mVolume);
        }
    }

    public void start()
    {
        if ((mState & (State.Prepared
                       | State.Started
                       | State.Paused
                       | State.PlaybackCompleted)) == 0) {
            return;
        }

        try {
            mMediaPlayer.start();
            setState(State.Started);
        } catch (final IllegalStateException e) {
            Log.d(TAG, "" + e.getMessage());
        }
    }


    public void pause()
    {
        if ((mState & (State.Started | State.Paused | State.PlaybackCompleted)) == 0)
            return;

        try {
            mMediaPlayer.pause();
            setState(State.Paused);
        } catch (final IllegalStateException e) {
            Log.d(TAG, "" + e.getMessage());
        }
    }


    public void stop()
    {
        if ((mState & (State.Prepared
                       | State.Started
                       | State.Stopped
                       | State.Paused
                       | State.PlaybackCompleted)) == 0) {
            return;
        }

        try {
            mMediaPlayer.stop();
            setState(State.Stopped);
        } catch (final IllegalStateException e) {
            Log.d(TAG, "" + e.getMessage());
        }
    }


    public void seekTo(final int msec)
    {
        if ((mState & (State.Prepared
                       | State.Started
                       | State.Paused
                       | State.PlaybackCompleted)) == 0) {
            return;
        }

        try {
            mMediaPlayer.seekTo(msec);
        } catch (final IllegalStateException e) {
            Log.d(TAG, "" + e.getMessage());
        }
    }


    public boolean isPlaying()
    {
        boolean playing = false;
        if ((mState & (State.Idle
                       | State.Initialized
                       | State.Prepared
                       | State.Started
                       | State.Paused
                       | State.Stopped
                       | State.PlaybackCompleted)) == 0) {
            return playing;
        }

        try {
            playing = mMediaPlayer.isPlaying();
        } catch (final IllegalStateException e) {
            Log.d(TAG, "" + e.getMessage());
        }

        return playing;
    }

    public void prepareAsync()
    {
        if ((mState & (State.Initialized | State.Stopped)) == 0)
           return;

        try {
            mMediaPlayer.prepareAsync();
            setState(State.Preparing);
        } catch (final IllegalStateException e) {
            Log.d(TAG, "" + e.getMessage());
        }
    }

    public void setDataSource(final String path)
    {
        if ((mState & State.Uninitialized) != 0)
            init();

        if ((mState & State.Idle) == 0)
           return;

        mMediaPlayer.setOnBufferingUpdateListener(new MediaPlayerBufferingListener());
        mMediaPlayer.setOnCompletionListener(new MediaPlayerCompletionListener());
        mMediaPlayer.setOnInfoListener(new MediaPlayerInfoListener());
        mMediaPlayer.setOnSeekCompleteListener(new MediaPlayerSeekCompleteListener());
        mMediaPlayer.setOnVideoSizeChangedListener(new MediaPlayerVideoSizeChangedListener());
        mMediaPlayer.setOnErrorListener(new MediaPlayerErrorListener());
        mMediaPlayer.setOnPreparedListener(new MediaPlayerPreparedListener());

        if (mSurfaceHolder != null)
            mMediaPlayer.setDisplay(mSurfaceHolder);

        AssetFileDescriptor afd = null;
        FileInputStream fis = null;
        try {
            mUri = Uri.parse(path);
            final boolean inAssets = (mUri.getScheme().compareTo("assets") == 0);
            if (inAssets) {
                final String asset = mUri.getPath().substring(1 /* Remove first '/' */);
                final AssetManager am = mContext.getAssets();
                afd = am.openFd(asset);
                final long offset = afd.getStartOffset();
                final long length = afd.getLength();
                FileDescriptor fd = afd.getFileDescriptor();
                mMediaPlayer.setDataSource(fd, offset, length);
            } else if (mUri.getScheme().compareTo("file") == 0) {
                fis = new FileInputStream(mUri.getPath());
                FileDescriptor fd = fis.getFD();
                mMediaPlayer.setDataSource(fd);
            } else {
                mMediaPlayer.setDataSource(mContext, mUri);
            }
            setState(State.Initialized);
        } catch (final IOException e) {
            Log.d(TAG, "" + e.getMessage());
        } catch (final IllegalArgumentException e) {
            Log.d(TAG, "" + e.getMessage());
        } catch (final SecurityException e) {
            Log.d(TAG, "" + e.getMessage());
        } catch (final IllegalStateException e) {
            Log.d(TAG, "" + e.getMessage());
        } catch (final NullPointerException e) {
            Log.d(TAG, "" + e.getMessage());
        } finally {
            try {
               if (afd != null)
                   afd.close();
               if (fis != null)
                   fis.close();
            } catch (final IOException ioe) { /* Ignore... */ }

            if ((mState & State.Initialized) == 0) {
                setState(State.Error);
                onErrorNative(MediaPlayer.MEDIA_ERROR_UNKNOWN,
                              -1004 /*MEDIA_ERROR_IO*/,
                              mID);
                return;
            }
        }
    }


   public int getCurrentPosition()
   {
       int currentPosition = 0;
       if ((mState & (State.Idle
                      | State.Initialized
                      | State.Prepared
                      | State.Started
                      | State.Paused
                      | State.Stopped
                      | State.PlaybackCompleted)) == 0) {
           return currentPosition;
       }

       try {
           currentPosition = mMediaPlayer.getCurrentPosition();
       } catch (final IllegalStateException e) {
           Log.d(TAG, "" + e.getMessage());
       }

       return currentPosition;
   }


   public int getDuration()
   {
       int duration = 0;
       if ((mState & (State.Prepared
                      | State.Started
                      | State.Paused
                      | State.Stopped
                      | State.PlaybackCompleted)) == 0) {
           return duration;
       }

       try {
           duration = mMediaPlayer.getDuration();
       } catch (final IllegalStateException e) {
           Log.d(TAG, "" + e.getMessage());
       }

       return duration;
   }

   public void setVolume(int volume)
   {
       if (volume < 0)
           volume = 0;

       if (volume > 100)
           volume = 100;

       mVolume = volume;

       if (!mMuted)
           setVolumeHelper(mVolume);
   }

   private void setVolumeHelper(int volume)
   {
       if ((mState & (State.Idle
                      | State.Initialized
                      | State.Stopped
                      | State.Prepared
                      | State.Started
                      | State.Paused
                      | State.PlaybackCompleted)) == 0) {
           return;
       }

       try {
           float newVolume = (float)volume / 100;
           mMediaPlayer.setVolume(newVolume, newVolume);
       } catch (final IllegalStateException e) {
           Log.d(TAG, "" + e.getMessage());
       }
   }

   public SurfaceHolder display()
   {
       return mSurfaceHolder;
   }

   public void setDisplay(SurfaceHolder sh)
   {
       mSurfaceHolder = sh;

       if ((mState & State.Uninitialized) != 0)
           return;

       mMediaPlayer.setDisplay(mSurfaceHolder);
   }


   public int getVolume()
   {
       return mVolume;
   }

    public void mute(final boolean mute)
    {
        mMuted = mute;
        setVolumeHelper(mute ? 0 : mVolume);
    }

    public boolean isMuted()
    {
        return mMuted;
    }


    public void reset()
    {
        if ((mState & (State.Idle
                       | State.Initialized
                       | State.Prepared
                       | State.Started
                       | State.Paused
                       | State.Stopped
                       | State.PlaybackCompleted
                       | State.Error)) == 0) {
            return;
        }

        mMediaPlayer.reset();
        setState(State.Idle);
    }

    public void release()
    {
        if (mMediaPlayer != null) {
            mMediaPlayer.reset();
            mMediaPlayer.release();
            mMediaPlayer = null;
        }

        setState(State.Uninitialized);
    }
}
