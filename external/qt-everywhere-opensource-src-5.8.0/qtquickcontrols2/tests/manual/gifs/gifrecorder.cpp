/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include "gifrecorder.h"

#include <QLoggingCategory>
#include <QQmlComponent>
#include <QQuickItem>
#include <QtTest>

/*!
    QProcess wrapper around byzanz-record (sudo apt-get install byzanz).

    \note The following programs must be installed if \c setHighQuality(true)
    is called:

    \li \e avconv (sudo apt-get install libav-tools)
    \li \e convert (sudo apt-get install imagemagick)
    \li \e gifsicle (sudo apt-get install gifsicle)

    It is recommended to set the \c Qt::FramelessWindowHint flag on the view
    (this code has not been tested under other usage):

    view.setFlags(view.flags() | Qt::FramelessWindowHint);
*/

Q_LOGGING_CATEGORY(lcGifRecorder, "qt.gifrecorder")

namespace {
    static const char *byzanzProcessName = "byzanz-record";
}

GifRecorder::GifRecorder() :
    QObject(nullptr),
    mWindow(nullptr),
    mHighQuality(false),
    mRecordingDuration(0),
    mRecordCursor(false),
    mByzanzProcessFinished(false)
{
    if (lcGifRecorder().isDebugEnabled()) {
        // Ensures output from the process goes directly into the console.
        mByzanzProcess.setProcessChannelMode(QProcess::ForwardedChannels);
    }

    connect(&mByzanzProcess, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(onByzanzError()));
    connect(&mByzanzProcess, SIGNAL(finished(int)), this, SLOT(onByzanzFinished()));
}

void GifRecorder::setRecordingDuration(int duration)
{
    QVERIFY2(duration >= 1, qPrintable(QString::fromLatin1("Recording duration %1 must be larger than 1 second").arg(duration)));
    QVERIFY2(duration < 20, qPrintable(QString::fromLatin1("Recording duration %1 must be less than 20 seconds").arg(duration)));

    mRecordingDuration = duration;
}

void GifRecorder::setRecordCursor(bool recordCursor)
{
    mRecordCursor = recordCursor;
}

void GifRecorder::setDataDirPath(const QString &path)
{
    QVERIFY2(!path.isEmpty(), "Data directory path cannot be empty");
    mDataDirPath = path;
}

void GifRecorder::setOutputDir(const QDir &dir)
{
    QVERIFY2(dir.exists(), "Output directory must exist");
    mOutputDir = dir;
}

void GifRecorder::setOutputFileBaseName(const QString &fileBaseName)
{
    mOutputFileBaseName = fileBaseName;
}

void GifRecorder::setQmlFileName(const QString &fileName)
{
    QVERIFY2(!fileName.isEmpty(), "QML file name cannot be empty");
    mQmlInputFileName = fileName;
}

void GifRecorder::setView(QQuickWindow *view)
{
    this->mWindow = view;
}

/*!
    If \a highQuality is \c true, records as .flv (lossless) and then converts
    to .gif in order to retain more color information, at the expense of a
    larger file size. Otherwise, records directly to .gif using a limited
    amount of colors, resulting in a smaller file size.

    Set this to \c true if any of the items have transparency, for example.

    The default value is \c false.
*/
void GifRecorder::setHighQuality(bool highQuality)
{
    mHighQuality = highQuality;
}

QQuickWindow *GifRecorder::window() const
{
    return mWindow;
}

namespace {
    void startProcess(QProcess &process, const QString &processName, const QString &args)
    {
        qCDebug(lcGifRecorder) << "Starting" << processName << "with the following arguments:" << args;
        const QString command = processName + QLatin1Char(' ') + args;
        process.start(command);
        if (!process.waitForStarted(1000)) {
            QString message = QString::fromLatin1("Could not launch %1 with the following arguments: %2\nError:\n%3");
            message = message.arg(processName).arg(args).arg(process.errorString());
            QFAIL(qPrintable(message));
        } else {
            qCDebug(lcGifRecorder) << "Successfully started" << processName;
        }
    }

    void waitForProcessToFinish(QProcess &process, const QString &processName, int waitDuration)
    {
        if (!process.waitForFinished(waitDuration) || process.exitCode() != 0) {
            QString message = QString::fromLatin1("%1 failed to finish (exit code %2): %3");
            message = message.arg(processName).arg(process.exitCode()).arg(process.errorString());
            QFAIL(qPrintable(message));
        } else {
            qCDebug(lcGifRecorder) << processName << "finished";
        }
    }
}

void GifRecorder::start()
{
    QDir gifQmlDir(mDataDirPath);
    QVERIFY(gifQmlDir.entryList().contains(mQmlInputFileName));

    const QString qmlPath = gifQmlDir.absoluteFilePath(mQmlInputFileName);
    mEngine.load(QUrl::fromLocalFile(qmlPath));
    mWindow = qobject_cast<QQuickWindow*>(mEngine.rootObjects().first());
    QVERIFY2(mWindow, "Top level item must be a window");

    mWindow->setFlags(mWindow->flags() | Qt::FramelessWindowHint);

    mWindow->show();
    mWindow->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(mWindow, 500));
    QVERIFY(QTest::qWaitForWindowExposed(mWindow, 500));
    // For some reason, whatever is behind the window is sometimes
    // in the recording, so add this delay to be extra sure that it isn't.
    QTest::qWait(200);

    if (mOutputFileBaseName.isEmpty()) {
        mOutputFileBaseName = mOutputDir.absoluteFilePath(mQmlInputFileName);
        mOutputFileBaseName.replace(".qml", "");
    }

    mByzanzOutputFileName = mOutputDir.absoluteFilePath(mOutputFileBaseName);
    if (mHighQuality) {
        mByzanzOutputFileName.append(QLatin1String(".flv"));
        mGifFileName = mByzanzOutputFileName;
        mGifFileName.replace(QLatin1String(".flv"), QLatin1String(".gif"));
    } else {
        mByzanzOutputFileName.append(QLatin1String(".gif"));
    }

    const QPoint globalWindowPos = mWindow->mapToGlobal(QPoint(0, 0));
    QString args = QLatin1String("-d %1 -v %2 -x %3 -y %4 -w %5 -h %6 %7");
    args = args.arg(QString::number(mRecordingDuration))
        .arg(mRecordCursor ? QStringLiteral("-c") : QString())
        .arg(QString::number(globalWindowPos.x()))
        .arg(QString::number(globalWindowPos.y()))
        .arg(QString::number(mWindow->width()))
        .arg(QString::number(mWindow->height()))
        .arg(mByzanzOutputFileName);


    // https://bugs.launchpad.net/ubuntu/+source/byzanz/+bug/1483581
    // It seems that byzanz-record will cut a recording short if there are no
    // screen repaints, no matter what format it outputs. This can be tested
    // manually from the command line by recording any section of the screen
    // without moving the mouse and then running avprobe on the resulting .flv.
    // Our workaround is to force view updates.
    connect(&mEventTimer, SIGNAL(timeout()), mWindow, SLOT(update()));
    mEventTimer.start(100);

    startProcess(mByzanzProcess, byzanzProcessName, args);
}

void GifRecorder::waitForFinish()
{
    // Give it an extra couple of seconds on top of its recording duration.
    const int recordingDurationMs = mRecordingDuration * 1000;
    const int waitDuration = recordingDurationMs + 2000;
    QTRY_VERIFY_WITH_TIMEOUT(mByzanzProcessFinished, waitDuration);

    mEventTimer.stop();

    if (!QFileInfo::exists(mByzanzOutputFileName)) {
        const QString message = QString::fromLatin1(
            "The process said it finished successfully, but %1 was not generated.").arg(mByzanzOutputFileName);
        QFAIL(qPrintable(message));
    }

    if (mHighQuality) {
        // Indicate the end of recording and the beginning of conversion.
        QQmlComponent busyComponent(&mEngine);
        busyComponent.setData("import QtQuick 2.6; import QtQuick.Controls 2.1; Rectangle { anchors.fill: parent; " \
            "BusyIndicator { width: 32; height: 32; anchors.centerIn: parent } }", QUrl());
        QCOMPARE(busyComponent.status(), QQmlComponent::Ready);
        QQuickItem *busyRect = qobject_cast<QQuickItem*>(busyComponent.create());
        QVERIFY(busyRect);
        busyRect->setParentItem(mWindow->contentItem());
        QSignalSpy spy(mWindow, SIGNAL(frameSwapped()));
        QVERIFY(spy.wait());

        QProcess avconvProcess;
        QProcess convertProcess;
        avconvProcess.setStandardOutputProcess(&convertProcess);

        const QString avconvProcessName = QStringLiteral("avconv");
        const QString avconvArgs = QString::fromLatin1("-i %1 -r 20 -f image2pipe -vcodec ppm -").arg(mByzanzOutputFileName);
        startProcess(avconvProcess, avconvProcessName, avconvArgs);

        const QString convertProcessName = QStringLiteral("convert");
        const QString convertArgs = QString::fromLatin1("-delay 5 -loop 0 - %1").arg(mGifFileName);
        startProcess(convertProcess, convertProcessName, convertArgs);

        waitForProcessToFinish(avconvProcess, avconvProcessName, waitDuration);
        waitForProcessToFinish(convertProcess, convertProcessName, waitDuration);

        const QString gifsicleProcessName = QStringLiteral("gifsicle");
        const QString verbose = lcGifRecorder().isDebugEnabled() ? QStringLiteral("-V") : QString();

        // --colors 256 stops the warning about local color tables being used, and results in smaller files,
        // but it seems to affect the duration of the GIF (checked with exiftool), so we don't use it.
        // For example, the slider GIF has the following attributes with and without the option:
        //                With                    Without
        // Frame Count    57                      61
        // Duration       2.85 seconds            3.05 seconds
        // File size      11 kB                   13 kB
        const QString gifsicleArgs = QString::fromLatin1("%1 -b -O %2").arg(verbose).arg(mGifFileName);
        QProcess gifsicleProcess;
        if (lcGifRecorder().isDebugEnabled())
            gifsicleProcess.setProcessChannelMode(QProcess::ForwardedChannels);
        startProcess(gifsicleProcess, gifsicleProcessName, gifsicleArgs);
        waitForProcessToFinish(gifsicleProcess, gifsicleProcessName, waitDuration);

        if (QFile::exists(mByzanzOutputFileName)) {
            QVERIFY(QFile::remove(mByzanzOutputFileName));
        }
    }
}

void GifRecorder::onByzanzError()
{
    const QString message = QString::fromLatin1("%1 failed to finish: %2");
    QFAIL(qPrintable(message.arg(byzanzProcessName).arg(mByzanzProcess.errorString())));
}

void GifRecorder::onByzanzFinished()
{
    qCDebug(lcGifRecorder) << byzanzProcessName << "finished";
    mByzanzProcessFinished = true;
}
