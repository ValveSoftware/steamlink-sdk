/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Speech module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

package org.qtproject.qt5.android.speech;

import android.content.ContentResolver;
import android.content.Context;
import android.provider.Settings;
import android.provider.Settings.SettingNotFoundException;
import android.speech.tts.TextToSpeech;
import android.speech.tts.TextToSpeech.Engine;
import android.speech.tts.TextToSpeech.OnInitListener;
import android.speech.tts.UtteranceProgressListener;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import java.lang.Float;
import java.util.HashMap;

public class QtTextToSpeech
{
    // Native callback functions
    native public void notifyError(long id);
    native public void notifyReady(long id);
    native public void notifySpeaking(long id);

    private TextToSpeech mTts;
    private final long mId;
    private float mPitch = 1.0f;
    private float mRate = 1.0f;
    private float mVolume = 1.0f;

    // OnInitListener
    private final OnInitListener mTtsChangeListener = new OnInitListener() {
        @Override
        public void onInit(int status) {
            Log.w("QtTextToSpeech", "tts initialized");
            if (status == TextToSpeech.SUCCESS) {
                notifyReady(mId);
            } else {
                notifyError(mId);
            }
        }
    };

    // UtteranceProgressListener
    private final UtteranceProgressListener mTtsUtteranceProgressListener = new UtteranceProgressListener() {
        @Override
        public void onDone(String utteranceId) {
            Log.w("UtteranceProgressListener", "onDone");
            if (utteranceId.equals("UtteranceId")) {
                notifyReady(mId);
            }
        }

        @Override
        public void onError(String utteranceId) {
            Log.w("UtteranceProgressListener", "onError");
            if (utteranceId.equals("UtteranceId")) {
                notifyReady(mId);
            }
        }

        @Override
        public void onStart(String utteranceId) {
            Log.w("UtteranceProgressListener", "onStart");
            if (utteranceId.equals("UtteranceId")) {
                notifySpeaking(mId);
            }
         }
     };

    public static QtTextToSpeech open(final Context context, final long id)
    {
        return new QtTextToSpeech(context, id);
    }

    QtTextToSpeech(final Context context, final long id) {
        mId = id;
        mTts = new TextToSpeech(context, mTtsChangeListener);
        mTts.setOnUtteranceProgressListener(mTtsUtteranceProgressListener);

        // Read pitch from settings
        ContentResolver resolver = context.getContentResolver();
        try {
            float pitch = Settings.Secure.getFloat(resolver, android.provider.Settings.Secure.TTS_DEFAULT_PITCH);
            mPitch = pitch / 100.0f;
        } catch (SettingNotFoundException e) {
            mPitch = 1.0f;
        }

        // Read rate from settings
        try {
            float rate = Settings.Secure.getFloat(resolver, android.provider.Settings.Secure.TTS_DEFAULT_RATE);
            mRate = rate / 100.0f;
        } catch (SettingNotFoundException e) {
            mRate = 1.0f;
        }
    }

    public void say(String text)
    {
        Log.w("QtTextToSpeech", text);

        int result = -1;

        HashMap<String, String> map = new HashMap<String, String>();
        map.put(TextToSpeech.Engine.KEY_PARAM_UTTERANCE_ID, "UtteranceId");
        map.put(TextToSpeech.Engine.KEY_PARAM_VOLUME, Float.toString(mVolume));
        result = mTts.speak(text, TextToSpeech.QUEUE_FLUSH, map);

        Log.w("QtTextToSpeech", "RESULT: " + Integer.toString(result));
    }

    public void stop()
    {
        Log.w("QtTextToSpeech", "STOP");
        mTts.stop();
    }

    public float pitch()
    {
        return mPitch;
    }

    public int setPitch(float pitch)
    {
        if (Float.compare(pitch, mPitch) == 0)
            return TextToSpeech.ERROR;

        int success = mTts.setPitch(pitch);
        if (success == TextToSpeech.SUCCESS)
            mPitch = pitch;

        return success;
    }

    public int setRate(float rate)
    {
        if (Float.compare(rate, mRate) == 0)
            return TextToSpeech.ERROR;

        int success = mTts.setSpeechRate(rate);
        if (success == TextToSpeech.SUCCESS)
            mRate = rate;

        return success;
    }

    public void shutdown()
    {
        mTts.shutdown();
    }

    public float volume()
    {
        return mVolume;
    }

    public int setVolume(float volume)
    {
        if (Float.compare(volume, mVolume) == 0)
            return TextToSpeech.ERROR;

        mVolume = volume;
        return TextToSpeech.SUCCESS;
    }

}
