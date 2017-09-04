/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include <QtCore/qdebug.h>
#include <QtCore/qurl.h>
#include <QtCore/qdir.h>
#include <qaudiodeviceinfo.h>

#include "qmediarecorder.h"

#include "audiocapturesession.h"
#include "audiocaptureprobecontrol.h"

QT_BEGIN_NAMESPACE

void FileProbeProxy::startProbes(const QAudioFormat &format)
{
    m_format = format;
}

void FileProbeProxy::stopProbes()
{
    m_format = QAudioFormat();
}

void FileProbeProxy::addProbe(AudioCaptureProbeControl *probe)
{
    QMutexLocker locker(&m_probeMutex);

    if (m_probes.contains(probe))
        return;

    m_probes.append(probe);
}

void FileProbeProxy::removeProbe(AudioCaptureProbeControl *probe)
{
    QMutexLocker locker(&m_probeMutex);
    m_probes.removeOne(probe);
}

qint64 FileProbeProxy::writeData(const char *data, qint64 len)
{
    if (m_format.isValid()) {
        QMutexLocker locker(&m_probeMutex);

        for (AudioCaptureProbeControl* probe : qAsConst(m_probes))
            probe->bufferProbed(data, len, m_format);
    }

    return QFile::writeData(data, len);
}

AudioCaptureSession::AudioCaptureSession(QObject *parent)
    : QObject(parent)
    , m_state(QMediaRecorder::StoppedState)
    , m_status(QMediaRecorder::UnloadedStatus)
    , m_audioInput(0)
    , m_deviceInfo(QAudioDeviceInfo::defaultInputDevice())
    , m_wavFile(true)
    , m_volume(1.0)
    , m_muted(false)
{
    m_format = m_deviceInfo.preferredFormat();
}

AudioCaptureSession::~AudioCaptureSession()
{
    setState(QMediaRecorder::StoppedState);
}

QAudioFormat AudioCaptureSession::format() const
{
    return m_format;
}

void AudioCaptureSession::setFormat(const QAudioFormat &format)
{
    m_format = format;
}

void AudioCaptureSession::setContainerFormat(const QString &formatMimeType)
{
    m_wavFile = (formatMimeType.isEmpty()
                 || QString::compare(formatMimeType, QLatin1String("audio/x-wav")) == 0);
}

QString AudioCaptureSession::containerFormat() const
{
    if (m_wavFile)
        return QStringLiteral("audio/x-wav");

    return QStringLiteral("audio/x-raw");
}

QUrl AudioCaptureSession::outputLocation() const
{
    return m_actualOutputLocation;
}

bool AudioCaptureSession::setOutputLocation(const QUrl& location)
{
    if (m_requestedOutputLocation == location)
        return false;

    m_actualOutputLocation = QUrl();
    m_requestedOutputLocation = location;

    if (m_requestedOutputLocation.isEmpty())
        return true;

    if (m_requestedOutputLocation.isValid() && (m_requestedOutputLocation.isLocalFile()
                                       || m_requestedOutputLocation.isRelative())) {
        emit actualLocationChanged(m_requestedOutputLocation);
        return true;
    }

    m_requestedOutputLocation = QUrl();
    return false;
}

qint64 AudioCaptureSession::position() const
{
    if (m_audioInput)
        return m_audioInput->processedUSecs() / 1000;
    return 0;
}

void AudioCaptureSession::setState(QMediaRecorder::State state)
{
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(m_state);

    switch (m_state) {
    case QMediaRecorder::StoppedState:
        stop();
        break;
    case QMediaRecorder::PausedState:
        pause();
        break;
    case QMediaRecorder::RecordingState:
        record();
        break;
    }
}

QMediaRecorder::State AudioCaptureSession::state() const
{
    return m_state;
}

void AudioCaptureSession::setStatus(QMediaRecorder::Status status)
{
    if (m_status == status)
        return;

    m_status = status;
    emit statusChanged(m_status);
}

QMediaRecorder::Status AudioCaptureSession::status() const
{
    return m_status;
}

QDir AudioCaptureSession::defaultDir() const
{
    QStringList dirCandidates;

    dirCandidates << QDir::home().filePath("Documents");
    dirCandidates << QDir::home().filePath("My Documents");
    dirCandidates << QDir::homePath();
    dirCandidates << QDir::currentPath();
    dirCandidates << QDir::tempPath();

    for (const QString &path : qAsConst(dirCandidates)) {
        QDir dir(path);
        if (dir.exists() && QFileInfo(path).isWritable())
            return dir;
    }

    return QDir();
}

QString AudioCaptureSession::generateFileName(const QString &requestedName,
                                              const QString &extension) const
{
    if (requestedName.isEmpty())
        return generateFileName(defaultDir(), extension);

    QString path = requestedName;

    if (QFileInfo(path).isRelative())
        path = defaultDir().absoluteFilePath(path);

    if (QFileInfo(path).isDir())
        return generateFileName(QDir(path), extension);

    if (!path.endsWith(extension))
        path.append(QString(".%1").arg(extension));

    return path;
}

QString AudioCaptureSession::generateFileName(const QDir &dir,
                                              const QString &ext) const
{
    int lastClip = 0;
    const auto list = dir.entryList(QStringList() << QString("clip_*.%1").arg(ext));
    for (const QString &fileName : list) {
        int imgNumber = fileName.midRef(5, fileName.size()-6-ext.length()).toInt();
        lastClip = qMax(lastClip, imgNumber);
    }

    QString name = QString("clip_%1.%2").arg(lastClip+1,
                                     4, //fieldWidth
                                     10,
                                     QLatin1Char('0')).arg(ext);

    return dir.absoluteFilePath(name);
}

void AudioCaptureSession::record()
{
    if (m_status == QMediaRecorder::PausedStatus) {
        m_audioInput->resume();
    } else {
        if (m_deviceInfo.isNull()) {
            emit error(QMediaRecorder::ResourceError,
                       QStringLiteral("No input device available."));
            m_state = QMediaRecorder::StoppedState;
            emit stateChanged(m_state);
            setStatus(QMediaRecorder::UnavailableStatus);
            return;
        }

        setStatus(QMediaRecorder::LoadingStatus);

        m_format = m_deviceInfo.nearestFormat(m_format);
        m_audioInput = new QAudioInput(m_deviceInfo, m_format);
        connect(m_audioInput, SIGNAL(stateChanged(QAudio::State)),
                this, SLOT(audioInputStateChanged(QAudio::State)));
        connect(m_audioInput, SIGNAL(notify()),
                this, SLOT(notify()));


        QString filePath = generateFileName(
                    m_requestedOutputLocation.isLocalFile() ? m_requestedOutputLocation.toLocalFile()
                                                   : m_requestedOutputLocation.toString(),
                    m_wavFile ? QLatin1String("wav")
                              : QLatin1String("raw"));

        m_actualOutputLocation = QUrl::fromLocalFile(filePath);
        if (m_actualOutputLocation != m_requestedOutputLocation)
            emit actualLocationChanged(m_actualOutputLocation);

        file.setFileName(filePath);

        setStatus(QMediaRecorder::LoadedStatus);
        setStatus(QMediaRecorder::StartingStatus);

        if (file.open(QIODevice::WriteOnly)) {
            if (m_wavFile) {
                memset(&header,0,sizeof(CombinedHeader));
                memcpy(header.riff.descriptor.id,"RIFF",4);
                header.riff.descriptor.size = 0xFFFFFFFF; // This should be updated on stop(), filesize-8
                memcpy(header.riff.type,"WAVE",4);
                memcpy(header.wave.descriptor.id,"fmt ",4);
                header.wave.descriptor.size = 16;
                header.wave.audioFormat = 1; // for PCM data
                header.wave.numChannels = m_format.channelCount();
                header.wave.sampleRate = m_format.sampleRate();
                header.wave.byteRate = m_format.sampleRate()*m_format.channelCount()*m_format.sampleSize()/8;
                header.wave.blockAlign = m_format.channelCount()*m_format.sampleSize()/8;
                header.wave.bitsPerSample = m_format.sampleSize();
                memcpy(header.data.descriptor.id,"data",4);
                header.data.descriptor.size = 0xFFFFFFFF; // This should be updated on stop(),samples*channels*sampleSize/8
                file.write((char*)&header,sizeof(CombinedHeader));
            }

            setVolumeHelper(m_muted ? 0 : m_volume);

            file.startProbes(m_format);
            m_audioInput->start(qobject_cast<QIODevice*>(&file));
        } else {
            delete m_audioInput;
            m_audioInput = 0;
            emit error(QMediaRecorder::ResourceError,
                       QStringLiteral("Can't open output location"));
            m_state = QMediaRecorder::StoppedState;
            emit stateChanged(m_state);
            setStatus(QMediaRecorder::UnloadedStatus);
        }
    }
}

void AudioCaptureSession::pause()
{
    m_audioInput->suspend();
}

void AudioCaptureSession::stop()
{
    if(m_audioInput) {
        m_audioInput->stop();
        file.stopProbes();
        file.close();
        if (m_wavFile) {
            qint32 fileSize = file.size();
            file.open(QIODevice::ReadWrite | QIODevice::Unbuffered);
            file.read((char*)&header,sizeof(CombinedHeader));
            header.riff.descriptor.size = fileSize - 8; // The RIFF chunk size is the file size minus
                                                        // the first two RIFF fields (8 bytes)
            header.data.descriptor.size = fileSize - 44; // dataSize = fileSize - headerSize (44 bytes)
            file.seek(0);
            file.write((char*)&header,sizeof(CombinedHeader));
            file.close();
        }
        delete m_audioInput;
        m_audioInput = 0;
        setStatus(QMediaRecorder::UnloadedStatus);
    }
}

void AudioCaptureSession::addProbe(AudioCaptureProbeControl *probe)
{
    file.addProbe(probe);
}

void AudioCaptureSession::removeProbe(AudioCaptureProbeControl *probe)
{
    file.removeProbe(probe);
}

void AudioCaptureSession::audioInputStateChanged(QAudio::State state)
{
    switch(state) {
    case QAudio::ActiveState:
        setStatus(QMediaRecorder::RecordingStatus);
        break;
    case QAudio::SuspendedState:
        setStatus(QMediaRecorder::PausedStatus);
        break;
    case QAudio::StoppedState:
        setStatus(QMediaRecorder::FinalizingStatus);
        break;
    default:
        break;
    }
}

void AudioCaptureSession::notify()
{
    emit positionChanged(position());
}

void AudioCaptureSession::setCaptureDevice(const QString &deviceName)
{
    m_captureDevice = deviceName;

    QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    for (int i = 0; i < devices.size(); ++i) {
        QAudioDeviceInfo info = devices.at(i);
        if (m_captureDevice == info.deviceName()){
            m_deviceInfo = info;
            return;
        }
    }
    m_deviceInfo = QAudioDeviceInfo::defaultInputDevice();
}

qreal AudioCaptureSession::volume() const
{
    return m_volume;
}

bool AudioCaptureSession::isMuted() const
{
    return m_muted;
}

void AudioCaptureSession::setVolume(qreal v)
{
    qreal boundedVolume = qBound(qreal(0), v, qreal(1));

    if (m_volume == boundedVolume)
        return;

    m_volume = boundedVolume;

    if (!m_muted)
        setVolumeHelper(m_volume);

    emit volumeChanged(m_volume);
}

void AudioCaptureSession::setMuted(bool muted)
{
    if (m_muted == muted)
        return;

    m_muted = muted;

    setVolumeHelper(m_muted ? 0 : m_volume);

    emit mutedChanged(m_muted);
}

void AudioCaptureSession::setVolumeHelper(qreal volume)
{
    if (!m_audioInput)
        return;

    m_audioInput->setVolume(volume);
}



QT_END_NAMESPACE
