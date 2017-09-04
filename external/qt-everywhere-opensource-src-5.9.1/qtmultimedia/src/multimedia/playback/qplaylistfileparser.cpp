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

#include "qplaylistfileparser_p.h"
#include <qfileinfo.h>
#include <QtCore/QDebug>
#include <QtCore/qiodevice.h>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include "qmediaplayer.h"
#include "qmediaobject_p.h"
#include "qmediametadata.h"
#include "qmediacontent.h"
#include "qmediaresource.h"

QT_BEGIN_NAMESPACE

namespace {

class ParserBase
{
public:
    explicit ParserBase(QPlaylistFileParser *parent)
        : m_parent(parent)
        , m_aborted(false)
    {
        Q_ASSERT(m_parent);
    }

    bool parseLine(int lineIndex, const QString& line, const QUrl& root)
    {
        if (m_aborted)
            return false;

        const bool ok = parseLineImpl(lineIndex, line, root);
        return ok && !m_aborted;
    }

    virtual void abort() { m_aborted = true; }
    virtual ~ParserBase() { }

protected:
    virtual bool parseLineImpl(int lineIndex, const QString& line, const QUrl& root) = 0;

    static QUrl expandToFullPath(const QUrl &root, const QString &line)
    {
        // On Linux, backslashes are not converted to forward slashes :/
        if (line.startsWith(QLatin1String("//")) || line.startsWith(QLatin1String("\\\\"))) {
            // Network share paths are not resolved
            return QUrl::fromLocalFile(line);
        }

        QUrl url(line);
        if (url.scheme().isEmpty()) {
            // Resolve it relative to root
            if (root.isLocalFile())
                return QUrl::fromUserInput(line, root.adjusted(QUrl::RemoveFilename).toLocalFile(), QUrl::AssumeLocalFile);
            else
                return root.resolved(url);
        } else if (url.scheme().length() == 1) {
            // Assume it's a drive letter for a Windows path
            url = QUrl::fromLocalFile(line);
        }

        return url;
    }

    void newItemFound(const QVariant& content) { Q_EMIT m_parent->newItem(content); }

private:
    QPlaylistFileParser *m_parent;
    bool m_aborted;
};

class M3UParser : public ParserBase
{
public:
    explicit M3UParser(QPlaylistFileParser *q)
        : ParserBase(q)
        , m_extendedFormat(false)
    {
    }

    /*
     *
    Extended M3U directives

    #EXTM3U - header - must be first line of file
    #EXTINF - extra info - length (seconds), title
    #EXTINF - extra info - length (seconds), artist '-' title

    Example

    #EXTM3U
    #EXTINF:123, Sample artist - Sample title
    C:\Documents and Settings\I\My Music\Sample.mp3
    #EXTINF:321,Example Artist - Example title
    C:\Documents and Settings\I\My Music\Greatest Hits\Example.ogg

     */
    bool parseLineImpl(int lineIndex, const QString& line, const QUrl& root) override
    {
        if (line[0] == '#' ) {
            if (m_extendedFormat) {
                if (line.startsWith(QLatin1String("#EXTINF:"))) {
                    m_extraInfo.clear();
                    int artistStart = line.indexOf(QLatin1String(","), 8);
                    bool ok = false;
                    int length = line.midRef(8, artistStart < 8 ? -1 : artistStart - 8).trimmed().toInt(&ok);
                    if (ok && length > 0) {
                        //convert from second to milisecond
                        m_extraInfo[QMediaMetaData::Duration] = QVariant(length * 1000);
                    }
                    if (artistStart > 0) {
                        int titleStart = getSplitIndex(line, artistStart);
                        if (titleStart > artistStart) {
                            m_extraInfo[QMediaMetaData::Author] = line.midRef(artistStart + 1,
                                                             titleStart - artistStart - 1).trimmed().toString().
                                                             replace(QLatin1String("--"), QLatin1String("-"));
                            m_extraInfo[QMediaMetaData::Title] = line.midRef(titleStart + 1).trimmed().toString().
                                                   replace(QLatin1String("--"), QLatin1String("-"));
                        } else {
                            m_extraInfo[QMediaMetaData::Title] = line.midRef(artistStart + 1).trimmed().toString().
                                                   replace(QLatin1String("--"), QLatin1String("-"));
                        }
                    }
                }
            } else if (lineIndex == 0 && line.startsWith(QLatin1String("#EXTM3U"))) {
                m_extendedFormat = true;
            }
        } else {
            m_extraInfo[QLatin1String("url")] = expandToFullPath(root, line);
            newItemFound(QVariant(m_extraInfo));
            m_extraInfo.clear();
        }

        return true;
    }

    int getSplitIndex(const QString& line, int startPos)
    {
        if (startPos < 0)
            startPos = 0;
        const QChar* buf = line.data();
        for (int i = startPos; i < line.length(); ++i) {
            if (buf[i] == '-') {
                if (i == line.length() - 1)
                    return i;
                ++i;
                if (buf[i] != '-')
                    return i - 1;
            }
        }
        return -1;
    }

private:
    QVariantMap     m_extraInfo;
    bool            m_extendedFormat;
};

class PLSParser : public ParserBase
{
public:
    explicit PLSParser(QPlaylistFileParser *q)
        : ParserBase(q)
    {
    }

/*
 *
The format is essentially that of an INI file structured as follows:

Header

    * [playlist] : This tag indicates that it is a Playlist File

Track Entry
Assuming track entry #X

    * FileX : Variable defining location of stream.
    * TitleX : Defines track title.
    * LengthX : Length in seconds of track. Value of -1 indicates indefinite.

Footer

    * NumberOfEntries : This variable indicates the number of tracks.
    * Version : Playlist version. Currently only a value of 2 is valid.

[playlist]

File1=Alternative\everclear - SMFTA.mp3

Title1=Everclear - So Much For The Afterglow

Length1=233

File2=http://www.site.com:8000/listen.pls

Title2=My Cool Stream

Length5=-1

NumberOfEntries=2

Version=2
*/
    bool parseLineImpl(int, const QString &line, const QUrl &root) override
    {
        // We ignore everything but 'File' entries, since that's the only thing we care about.
        if (!line.startsWith(QLatin1String("File")))
            return true;

        QString value = getValue(line);
        if (value.isEmpty())
            return true;

        newItemFound(expandToFullPath(root, value));

        return true;
    }

    QString getValue(const QString& line) {
        int start = line.indexOf('=');
        if (start < 0)
            return QString();
        return line.midRef(start + 1).trimmed().toString();
    }
};
}

/////////////////////////////////////////////////////////////////////////////////////////////////

class QPlaylistFileParserPrivate
{
    Q_DECLARE_PUBLIC(QPlaylistFileParser)
public:
    QPlaylistFileParserPrivate(QPlaylistFileParser *q)
        : q_ptr(q)
        , m_stream(0)
        , m_type(QPlaylistFileParser::UNKNOWN)
        , m_scanIndex(0)
        , m_lineIndex(-1)
        , m_utf8(false)
        , m_aborted(false)
    {
    }

    void handleData();
    void handleParserFinished();
    void abort();
    void reset();

    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> m_source;
    QScopedPointer<ParserBase> m_currentParser;
    QByteArray      m_buffer;
    QUrl            m_root;
    QNetworkAccessManager m_mgr;
    QString m_mimeType;
    QPlaylistFileParser *q_ptr;
    QIODevice *m_stream;
    QPlaylistFileParser::FileType m_type;
    struct ParserJob
    {
        QIODevice *m_stream;
        QMediaResource m_resource;
        bool isValid() const { return m_stream || !m_resource.isNull(); }
        void reset() { m_stream = 0; m_resource = QMediaResource(); }
    } m_pendingJob;
    int m_scanIndex;
    int m_lineIndex;
    bool m_utf8;
    bool m_aborted;

private:
    bool processLine(int startIndex, int length);
};

#define LINE_LIMIT  4096
#define READ_LIMIT  64

bool QPlaylistFileParserPrivate::processLine(int startIndex, int length)
{
    Q_Q(QPlaylistFileParser);
    m_lineIndex++;

    if (!m_currentParser) {
        const QString urlString = m_root.toString();
        const QString &suffix = !urlString.isEmpty() ? QFileInfo(urlString).suffix() : urlString;
        const QString &mimeType = m_source->header(QNetworkRequest::ContentTypeHeader).toString();
        m_type = QPlaylistFileParser::findPlaylistType(suffix, !mimeType.isEmpty() ?  mimeType : m_mimeType, m_buffer.constData(), quint32(m_buffer.size()));

        switch (m_type) {
        case QPlaylistFileParser::UNKNOWN:
            emit q->error(QPlaylistFileParser::FormatError,
                          QPlaylistFileParser::tr("%1 playlist type is unknown").arg(m_root.toString()));
            q->abort();
            return false;
        case QPlaylistFileParser::M3U:
            m_currentParser.reset(new M3UParser(q));
            break;
        case QPlaylistFileParser::M3U8:
            m_currentParser.reset(new M3UParser(q));
            m_utf8 = true;
            break;
        case QPlaylistFileParser::PLS:
            m_currentParser.reset(new PLSParser(q));
            break;
        }

        Q_ASSERT(!m_currentParser.isNull());
    }

    QString line;

    if (m_utf8) {
        line = QString::fromUtf8(m_buffer.constData() + startIndex, length).trimmed();
    } else {
        line = QString::fromLatin1(m_buffer.constData() + startIndex, length).trimmed();
    }
    if (line.isEmpty())
        return true;

    Q_ASSERT(m_currentParser);
    return m_currentParser->parseLine(m_lineIndex, line, m_root);
}

void QPlaylistFileParserPrivate::handleData()
{
    Q_Q(QPlaylistFileParser);
    while (m_source->bytesAvailable() && !m_aborted) {
        int expectedBytes = qMin(READ_LIMIT, int(qMin(m_source->bytesAvailable(),
                                                      qint64(LINE_LIMIT - m_buffer.size()))));
        m_buffer.push_back(m_source->read(expectedBytes));
        int processedBytes = 0;
        while (m_scanIndex < m_buffer.length() && !m_aborted) {
            char s = m_buffer[m_scanIndex];
            if (s == '\r' || s == '\n') {
                int l = m_scanIndex - processedBytes;
                if (l > 0) {
                    if (!processLine(processedBytes, l))
                        break;
                }
                processedBytes = m_scanIndex + 1;
                if (!m_source) {
                    //some error happened, so exit parsing
                    return;
                }
            }
            m_scanIndex++;
        }

        if (m_aborted)
            break;

        if (m_buffer.length() - processedBytes >= LINE_LIMIT) {
            emit q->error(QPlaylistFileParser::FormatError, QPlaylistFileParser::tr("invalid line in playlist file"));
            q->abort();
            break;
        }

        if (m_source->isFinished() && !m_source->bytesAvailable()) {
            //last line
            processLine(processedBytes, -1);
            break;
        }

        Q_ASSERT(m_buffer.length() == m_scanIndex);
        if (processedBytes == 0)
            continue;

        int copyLength = m_buffer.length() - processedBytes;
        if (copyLength > 0) {
            Q_ASSERT(copyLength <= READ_LIMIT);
            m_buffer = m_buffer.right(copyLength);
        } else {
            m_buffer.clear();
        }
        m_scanIndex = 0;
    }

    handleParserFinished();
}

QPlaylistFileParser::QPlaylistFileParser(QObject *parent)
    : QObject(parent)
    , d_ptr(new QPlaylistFileParserPrivate(this))
{

}

QPlaylistFileParser::~QPlaylistFileParser()
{

}

QPlaylistFileParser::FileType QPlaylistFileParser::findByMimeType(const QString &mime)
{
    if (mime == QLatin1String("text/uri-list") || mime == QLatin1String("audio/x-mpegurl") || mime == QLatin1String("audio/mpegurl"))
        return QPlaylistFileParser::M3U;

    if (mime == QLatin1String("application/x-mpegURL") || mime == QLatin1String("application/vnd.apple.mpegurl"))
        return QPlaylistFileParser::M3U8;

    if (mime == QLatin1String("audio/x-scpls"))
        return QPlaylistFileParser::PLS;

    return QPlaylistFileParser::UNKNOWN;
}

QPlaylistFileParser::FileType QPlaylistFileParser::findBySuffixType(const QString &suffix)
{
    const QString &s = suffix.toLower();

    if (s == QLatin1String("m3u"))
        return QPlaylistFileParser::M3U;

    if (s == QLatin1String("m3u8"))
        return QPlaylistFileParser::M3U8;

    if (s == QLatin1String("pls"))
        return QPlaylistFileParser::PLS;

    return QPlaylistFileParser::UNKNOWN;
}

QPlaylistFileParser::FileType QPlaylistFileParser::findByDataHeader(const char *data, quint32 size)
{
    if (!data || size == 0)
        return QPlaylistFileParser::UNKNOWN;

    if (size >= 7 && strncmp(data, "#EXTM3U", 7) == 0)
        return QPlaylistFileParser::M3U;

    if (size >= 10 && strncmp(data, "[playlist]", 10) == 0)
        return QPlaylistFileParser::PLS;

    return QPlaylistFileParser::UNKNOWN;
}

QPlaylistFileParser::FileType QPlaylistFileParser::findPlaylistType(const QString& suffix,
                                                                    const QString& mime,
                                                                    const char *data,
                                                                    quint32 size)
{

    FileType dataHeaderType = findByDataHeader(data, size);
    if (dataHeaderType != UNKNOWN)
        return dataHeaderType;

    FileType mimeType = findByMimeType(mime);
    if (mimeType != UNKNOWN)
        return mimeType;

    FileType suffixType = findBySuffixType(suffix);
    if (suffixType != UNKNOWN)
        return suffixType;

    return UNKNOWN;
}

/*
 * Delegating
 */
void QPlaylistFileParser::start(const QMediaContent &media, QIODevice *stream)
{
    const QMediaResource &mediaResource = media.canonicalResource();
    const QString &mimeType = mediaResource.mimeType();

    if (stream) {
        start(stream, mediaResource.mimeType());
    } else {
        const QNetworkRequest &request = mediaResource.request();
        const QUrl &url = mediaResource.url();

        if (request.url().isValid())
            start(request, mimeType);
        else
            start(QNetworkRequest(url), mimeType);
    }
}

void QPlaylistFileParser::start(QIODevice *stream, const QString &mimeType)
{
    Q_D(QPlaylistFileParser);
    const bool validStream = stream ? (stream->isOpen() && stream->isReadable()) : false;

    if (!validStream) {
        Q_EMIT error(ResourceError, tr("Invalid stream"));
        return;
    }

    if (!d->m_currentParser.isNull()) {
        abort();
        d->m_pendingJob = { stream,  QMediaResource(QUrl(), mimeType) };
        return;
    }

    d->reset();
    d->m_mimeType = mimeType;
    d->m_stream = stream;
    connect(d->m_stream, SIGNAL(readyRead()), this, SLOT(_q_handleData()));
    d->handleData();
}

void QPlaylistFileParser::start(const QNetworkRequest& request, const QString &mimeType)
{
    Q_D(QPlaylistFileParser);
    const QUrl &url = request.url();

    if (url.isLocalFile() && !QFile::exists(url.toLocalFile())) {
        emit error(ResourceError, QString(tr("%1 does not exist")).arg(url.toString()));
        return;
    }

    if (!d->m_currentParser.isNull()) {
        abort();
        d->m_pendingJob = { Q_NULLPTR, QMediaResource(request, mimeType) };
        return;
    }

    d->reset();
    d->m_root = url;
    d->m_mimeType = mimeType;
    d->m_source.reset(d->m_mgr.get(request));
    connect(d->m_source.data(), SIGNAL(readyRead()), this, SLOT(handleData()));
    connect(d->m_source.data(), SIGNAL(finished()), this, SLOT(handleData()));
    connect(d->m_source.data(), SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(handleError()));

    d->handleData();
}

void QPlaylistFileParser::abort()
{
    Q_D(QPlaylistFileParser);
    d->abort();

    if (d->m_source)
        d->m_source->disconnect();

    if (d->m_stream)
        disconnect(d->m_stream, SIGNAL(readyRead()), this, SLOT(handleData()));
}

void QPlaylistFileParser::handleData()
{
    Q_D(QPlaylistFileParser);
    d->handleData();
}

void QPlaylistFileParserPrivate::handleParserFinished()
{
    Q_Q(QPlaylistFileParser);
    const bool isParserValid = !m_currentParser.isNull();
    if (!isParserValid && !m_aborted)
        emit q->error(QPlaylistFileParser::FormatNotSupportedError, QPlaylistFileParser::tr("Empty file provided"));

    if (isParserValid && !m_aborted) {
        m_currentParser.reset();
        emit q->finished();
    }

    if (!m_aborted)
        q->abort();

    if (!m_source.isNull())
        m_source.reset();

    if (m_pendingJob.isValid())
        q->start(m_pendingJob.m_resource, m_pendingJob.m_stream);
}

void QPlaylistFileParserPrivate::abort()
{
    m_aborted = true;
    if (!m_currentParser.isNull())
        m_currentParser->abort();
}

void QPlaylistFileParserPrivate::reset()
{
    Q_ASSERT(m_currentParser.isNull());
    Q_ASSERT(m_source.isNull());
    m_buffer.clear();
    m_root.clear();
    m_mimeType.clear();
    m_stream = 0;
    m_type = QPlaylistFileParser::UNKNOWN;
    m_scanIndex = 0;
    m_lineIndex = -1;
    m_utf8 = false;
    m_aborted = false;
    m_pendingJob.reset();
}

void QPlaylistFileParser::handleError()
{
    Q_D(QPlaylistFileParser);
    const QString &errorString = d->m_source->errorString();
    Q_EMIT error(QPlaylistFileParser::NetworkError, errorString);
    abort();
}

QT_END_NAMESPACE
