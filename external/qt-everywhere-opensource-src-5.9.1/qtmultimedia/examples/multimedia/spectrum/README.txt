Spectrum analyser demo app
==========================

Introduction
------------

This application is a demo which uses the Qt Multimedia APIs to capture and play back PCM audio.  While either recording or playback is ongoing, the application performs real-time level and frequency spectrum analysis, displaying the results in its main window.


Acknowledgments
---------------

The application uses the FFTReal v2.00 library by Laurent de Soras to perform frequency analysis of the audio signal. For further information, see the project home page:
    http://ldesoras.free.fr/prod.html


Quick start
-----------

Play generated tone
1. Select 'Play generated tone' from the mode menu
2. Ensure that the 'Frequency sweep' box is checked
3. Press 'OK'
4. Press the play button
You should hear a rising tone, and see progressively higher frequencies indicated by the spectrograph.

Record and playback
1. Select 'Record and play back audio' from the mode menu
2. Press the record button, and speak into the microphone
3. Wait until the buffer is full (shown as a full blue bar in the top widget)
4. Press play, and wait until playback of the buffer is complete

Play file
1. Select 'Play file' from the mode menu
2. Select a WAV file
3. Press the play button
You should hear the first few seconds of the file being played.  The waveform, spectrograph and level meter should be updated as the file is played.


Things to play with
-------------------

Try repeating the 'Play generated tone' sequence using different window functions.  These can be selected from the  settings dialog - launch it by pressing the spanner icon.  The window function is applied to the audio signal before performing the frequency analysis; different windows should have a visible effect on the resulting frequency spectrum.

Try clicking on one of the spectrograph bars while the tone is being played.  The frequency range for that bar will be displayed at the top of the application window.


Troubleshooting
---------------

If either recording or playback do not work, you may need to select a different input / output audio device.  This can be done in the settings dialog - launch it by pressing the spanner icon.

If that doesn't work, there may be a problem either in the application or in Qt.  Report a bug in the usual way.


Application interface
---------------------

The main window of the application contains the following widgets, starting at the top:

Message box
This shows various messages during execution, such as the current audio format.

Progress bar / waveform display
- While recording or playback is ongoing, the audio waveform is displayed, sliding from right to left.
- Superimposed on the waveform, the amount of data currently in the buffer is showed as a blue bar.  When recording, this blue bar fills up from left to right; when playing, the bar gets consumed from left to right.
- A green window shows which part of the buffer has most recently been analysed.  This window should be close to the 'leading edge' of recording or playback, i.e. the most recently recorded / played data, although it will lag slightly depending on the performance of the machine on which the application is running.

Frequency spectrograph (on the left)
The spectrograph shows 10 bars, each representing a frequency range.  The frequency range of each bar is displayed in the message box when the bar is clicked.  The height of the bar shows the maximum amplitude of freqencies within its range.

Level meter (on the right)
The current peak audio level is shown as a pink bar; the current RMS level is shown as a red bar.  The 'high water mark' during a recent period is shown as a thin red line.

Button panel
- The mode menu allows switching between the three operation modes - 'Play generated tone', 'Record and play back' and 'Play file'.
- The record button starts or resumes audio capture from the current input device.
- The pause button suspends either capture or recording.
- The play button starts or resumes audio playback to the current output device.
- The settings button launches the settings dialog.


Hacking
-------

If you want to hack the application, here are some pointers to get started.

The spectrum.pri file contains several macros which you can enable by uncommenting:
- LOG_FOO      Enable logging from class Foo via qDebug()
- DUMP_FOO     Dump data from class Foo to the file system
               e.g. DUMP_SPECTRUMANALYSER writes files containing the raw FFT input and output.
               Be aware that this can generate a *lot* of data and may slow the app down considerably.
- DISABLE_FOO  Disable specified functionality

If you don't like the combination of the waveform and progress bar in a single widget, separate them by commenting out SUPERIMPOSE_PROGRESS_ON_WAVEFORM.

The spectrum.h file defines a number of parameters which can be played with.  These control things such as the number of audio samples analysed per FFT calculation, the range and number of bands displayed by the spectrograph, and so on.

The part of the application which interacts with Qt Multimedia is in the Engine class.

Some ideas for enhancements to the app are listed in TODO.txt.  Feel free to start work on any of them :)


