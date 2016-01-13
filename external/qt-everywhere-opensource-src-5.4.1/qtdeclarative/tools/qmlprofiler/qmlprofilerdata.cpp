/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qmlprofilerdata.h"

#include <QStringList>
#include <QUrl>
#include <QHash>
#include <QFile>
#include <QXmlStreamReader>

const char PROFILER_FILE_VERSION[] = "1.02";

static const char *RANGE_TYPE_STRINGS[] = {
    "Painting",
    "Compiling",
    "Creating",
    "Binding",
    "HandlingSignal",
    "Javascript"
};

Q_STATIC_ASSERT(sizeof(RANGE_TYPE_STRINGS) ==
                QQmlProfilerDefinitions::MaximumRangeType * sizeof(const char *));

static const char *MESSAGE_STRINGS[] = {
    "Event",
    "RangeStart",
    "RangeData",
    "RangeLocation",
    "RangeEnd",
    "Complete",
    "PixmapCache",
    "SceneGraph",
    "MemoryAllocation"
};

Q_STATIC_ASSERT(sizeof(MESSAGE_STRINGS) ==
                QQmlProfilerDefinitions::MaximumMessage * sizeof(const char *));

struct QmlRangeEventData {
    QmlRangeEventData() {} // never called
    QmlRangeEventData(const QString &_displayName, int _detailType, const QString &_eventHashStr,
                      const QmlEventLocation &_location, const QString &_details,
                      QQmlProfilerService::Message _message,
                      QQmlProfilerService::RangeType _rangeType)
        : displayName(_displayName), eventHashStr(_eventHashStr), location(_location),
          details(_details), message(_message), rangeType(_rangeType), detailType(_detailType) {}
    QString displayName;
    QString eventHashStr;
    QmlEventLocation location;
    QString details;
    QQmlProfilerService::Message message;
    QQmlProfilerService::RangeType rangeType;
    int detailType; // can be BindingType, PixmapCacheEventType or SceneGraphFrameType
};

struct QmlRangeEventStartInstance {
    QmlRangeEventStartInstance() {} // never called
    QmlRangeEventStartInstance(qint64 _startTime, qint64 _duration, int _frameRate,
                  int _animationCount, int _threadId, QmlRangeEventData *_data)
        : startTime(_startTime), duration(_duration), frameRate(_frameRate),
          animationCount(_animationCount), threadId(_threadId), numericData4(-1), numericData5(-1),
          data(_data)
        { }

    QmlRangeEventStartInstance(qint64 _startTime, qint64 _numericData1, qint64 _numericData2,
                               qint64 _numericData3, qint64 _numericData4, qint64 _numericData5,
                               QmlRangeEventData *_data)
        : startTime(_startTime), duration(-1), numericData1(_numericData1),
          numericData2(_numericData2), numericData3(_numericData3), numericData4(_numericData4),
          numericData5(_numericData5), data(_data)
        { }
    qint64 startTime;
    qint64 duration;
    union {
        int frameRate;
        qint64 numericData1;
    };
    union {
        int animationCount;
        qint64 numericData2;
    };
    union {
        int threadId;
        qint64 numericData3;
    };
    qint64 numericData4;
    qint64 numericData5;
    QmlRangeEventData *data;
};

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(QmlRangeEventData, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QmlRangeEventStartInstance, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

struct QV8EventInfo {
    QString displayName;
    QString eventHashStr;
    QString functionName;
    QString fileName;
    int line;
    qint64 totalTime;
    qint64 selfTime;

    QHash<QString, qint64> v8children;
};

/////////////////////////////////////////////////////////////////
class QmlProfilerDataPrivate
{
public:
    QmlProfilerDataPrivate(QmlProfilerData *qq){ Q_UNUSED(qq); }

    // data storage
    QHash<QString, QmlRangeEventData *> eventDescriptions;
    QVector<QmlRangeEventStartInstance> startInstanceList;
    QHash<QString, QV8EventInfo *> v8EventHash;

    qint64 traceStartTime;
    qint64 traceEndTime;

    // internal state while collecting events
    qint64 qmlMeasuredTime;
    qint64 v8MeasuredTime;
    QHash<int, QV8EventInfo *> v8parents;
    void clearV8RootEvent();
    QV8EventInfo v8RootEvent;

    QmlProfilerData::State state;
};

/////////////////////////////////////////////////////////////////
QmlProfilerData::QmlProfilerData(QObject *parent) :
    QObject(parent),d(new QmlProfilerDataPrivate(this))
{
    d->state = Empty;
    clear();
}

QmlProfilerData::~QmlProfilerData()
{
    clear();
    delete d;
}

void QmlProfilerData::clear()
{
    qDeleteAll(d->eventDescriptions.values());
    d->eventDescriptions.clear();
    d->startInstanceList.clear();

    qDeleteAll(d->v8EventHash.values());
    d->v8EventHash.clear();
    d->v8parents.clear();
    d->clearV8RootEvent();
    d->v8MeasuredTime = 0;

    d->traceEndTime = 0;
    d->traceStartTime = -1;
    d->qmlMeasuredTime = 0;

    setState(Empty);
}

QString QmlProfilerData::getHashStringForQmlEvent(const QmlEventLocation &location, int eventType)
{
    return QString(QStringLiteral("%1:%2:%3:%4")).arg(
                location.filename,
                QString::number(location.line),
                QString::number(location.column),
                QString::number(eventType));
}

QString QmlProfilerData::getHashStringForV8Event(const QString &displayName, const QString &function)
{
    return QString(QStringLiteral("%1:%2")).arg(displayName, function);
}

QString QmlProfilerData::qmlRangeTypeAsString(QQmlProfilerService::RangeType type)
{
    if (type * sizeof(QString) < sizeof(RANGE_TYPE_STRINGS))
        return QLatin1String(RANGE_TYPE_STRINGS[type]);
    else
        return QString::number(type);
}

QString QmlProfilerData::qmlMessageAsString(QQmlProfilerService::Message type)
{
    if (type * sizeof(QString) < sizeof(MESSAGE_STRINGS))
        return QLatin1String(MESSAGE_STRINGS[type]);
    else
        return QString::number(type);
}

void QmlProfilerData::setTraceStartTime(qint64 time)
{
    d->traceStartTime = time;
}

void QmlProfilerData::setTraceEndTime(qint64 time)
{
    d->traceEndTime = time;
}

qint64 QmlProfilerData::traceStartTime() const
{
    return d->traceStartTime;
}

qint64 QmlProfilerData::traceEndTime() const
{
    return d->traceEndTime;
}

void QmlProfilerData::addQmlEvent(QQmlProfilerService::RangeType type,
                                  QQmlProfilerService::BindingType bindingType,
                                  qint64 startTime,
                                  qint64 duration,
                                  const QStringList &data,
                                  const QmlEventLocation &location)
{
    setState(AcquiringData);

    QString details;
    // generate details string
    if (data.isEmpty())
        details = tr("Source code not available");
    else {
        details = data.join(QStringLiteral(" ")).replace(
                    QLatin1Char('\n'),QStringLiteral(" ")).simplified();
        QRegExp rewrite(QStringLiteral("\\(function \\$(\\w+)\\(\\) \\{ (return |)(.+) \\}\\)"));
        bool match = rewrite.exactMatch(details);
        if (match) {
            details = rewrite.cap(1) +QLatin1String(": ") + rewrite.cap(3);
        }
        if (details.startsWith(QLatin1String("file://")))
            details = details.mid(details.lastIndexOf(QLatin1Char('/')) + 1);
    }

    QmlEventLocation eventLocation = location;
    QString displayName, eventHashStr;
    // generate hash
    if (eventLocation.filename.isEmpty()) {
        displayName = tr("<bytecode>");
        eventHashStr = getHashStringForQmlEvent(eventLocation, type);
    } else {
        const QString filePath = QUrl(eventLocation.filename).path();
        displayName = filePath.mid(
                    filePath.lastIndexOf(QLatin1Char('/')) + 1) +
                    QLatin1Char(':') + QString::number(eventLocation.line);
        eventHashStr = getHashStringForQmlEvent(eventLocation, type);
    }

    QmlRangeEventData *newEvent;
    if (d->eventDescriptions.contains(eventHashStr)) {
        newEvent = d->eventDescriptions[eventHashStr];
    } else {
        newEvent = new QmlRangeEventData(displayName, bindingType, eventHashStr, location, details,
                                         QQmlProfilerService::MaximumMessage, type);
        d->eventDescriptions.insert(eventHashStr, newEvent);
    }

    QmlRangeEventStartInstance rangeEventStartInstance(startTime, duration, -1, -1, -1, newEvent);

    d->startInstanceList.append(rangeEventStartInstance);
}

void QmlProfilerData::addFrameEvent(qint64 time, int framerate, int animationcount, int threadId)
{
    setState(AcquiringData);

    QString details = tr("Animation Timer Update");
    QString displayName = tr("<Animation Update>");
    QString eventHashStr = displayName;

    QmlRangeEventData *newEvent;
    if (d->eventDescriptions.contains(eventHashStr)) {
        newEvent = d->eventDescriptions[eventHashStr];
    } else {
        newEvent = new QmlRangeEventData(displayName, QQmlProfilerService::AnimationFrame,
                                         eventHashStr,
                                         QmlEventLocation(), details,
                                         QQmlProfilerService::Event,
                                         QQmlProfilerService::MaximumRangeType);
        d->eventDescriptions.insert(eventHashStr, newEvent);
    }

    QmlRangeEventStartInstance rangeEventStartInstance(time, -1, framerate, animationcount,
                                                       threadId, newEvent);

    d->startInstanceList.append(rangeEventStartInstance);
}

void QmlProfilerData::addSceneGraphFrameEvent(QQmlProfilerDefinitions::SceneGraphFrameType type, qint64 time, qint64 numericData1, qint64 numericData2, qint64 numericData3, qint64 numericData4, qint64 numericData5)
{
    setState(AcquiringData);

    QString eventHashStr = QString::fromLatin1("SceneGraph:%1").arg(type);
    QmlRangeEventData *newEvent;
    if (d->eventDescriptions.contains(eventHashStr)) {
        newEvent = d->eventDescriptions[eventHashStr];
    } else {
        newEvent = new QmlRangeEventData(QStringLiteral("<SceneGraph>"), type, eventHashStr,
                                         QmlEventLocation(), QString(),
                                         QQmlProfilerService::SceneGraphFrame,
                                         QQmlProfilerService::MaximumRangeType);
        d->eventDescriptions.insert(eventHashStr, newEvent);
    }

    QmlRangeEventStartInstance rangeEventStartInstance(time, numericData1, numericData2,
                                                       numericData3, numericData4, numericData5,
                                                       newEvent);
    d->startInstanceList.append(rangeEventStartInstance);
}

void QmlProfilerData::addPixmapCacheEvent(QQmlProfilerDefinitions::PixmapEventType type,
                                          qint64 time, const QmlEventLocation &location,
                                          int width, int height, int refcount)
{
    setState(AcquiringData);

    QString filePath = QUrl(location.filename).path();

    QString eventHashStr = filePath.mid(filePath.lastIndexOf(QLatin1Char('/')) + 1) +
            QStringLiteral(":") + QString::number(type);
    QmlRangeEventData *newEvent;
    if (d->eventDescriptions.contains(eventHashStr)) {
        newEvent = d->eventDescriptions[eventHashStr];
    } else {
        newEvent = new QmlRangeEventData(eventHashStr, type, eventHashStr, location, QString(),
                                         QQmlProfilerService::PixmapCacheEvent,
                                         QQmlProfilerService::MaximumRangeType);
        d->eventDescriptions.insert(eventHashStr, newEvent);
    }

    QmlRangeEventStartInstance rangeEventStartInstance(time, width, height, refcount, 0, 0,
                                                       newEvent);
    d->startInstanceList.append(rangeEventStartInstance);
}

void QmlProfilerData::addMemoryEvent(QQmlProfilerService::MemoryType type, qint64 time,
                                     qint64 size)
{
    setState(AcquiringData);
    QString eventHashStr = QString::fromLatin1("MemoryAllocation:%1").arg(type);
    QmlRangeEventData *newEvent;
    if (d->eventDescriptions.contains(eventHashStr)) {
        newEvent = d->eventDescriptions[eventHashStr];
    } else {
        newEvent = new QmlRangeEventData(eventHashStr, type, eventHashStr, QmlEventLocation(),
                                         QString(), QQmlProfilerService::MemoryAllocation,
                                         QQmlProfilerService::MaximumRangeType);
        d->eventDescriptions.insert(eventHashStr, newEvent);
    }
    QmlRangeEventStartInstance rangeEventStartInstance(time, size, 0, 0, 0, 0, newEvent);
    d->startInstanceList.append(rangeEventStartInstance);
}

QString QmlProfilerData::rootEventName()
{
    return tr("<program>");
}

QString QmlProfilerData::rootEventDescription()
{
    return tr("Main Program");
}

void QmlProfilerDataPrivate::clearV8RootEvent()
{
    v8RootEvent.displayName = QmlProfilerData::rootEventName();
    v8RootEvent.eventHashStr = QmlProfilerData::rootEventName();
    v8RootEvent.functionName = QmlProfilerData::rootEventDescription();
    v8RootEvent.line = -1;
    v8RootEvent.totalTime = 0;
    v8RootEvent.selfTime = 0;
    v8RootEvent.v8children.clear();
}

void QmlProfilerData::addV8Event(int depth, const QString &function, const QString &filename,
                                 int lineNumber, double totalTime, double selfTime)
{
    QString displayName = filename.mid(filename.lastIndexOf(QLatin1Char('/')) + 1) +
            QLatin1Char(':') + QString::number(lineNumber);
    QString hashStr = getHashStringForV8Event(displayName, function);

    setState(AcquiringData);

    // time is given in milliseconds, but internally we store it in microseconds
    totalTime *= 1e6;
    selfTime *= 1e6;

    // accumulate information
    QV8EventInfo *eventData = d->v8EventHash[hashStr];
    if (!eventData) {
        eventData = new QV8EventInfo;
        eventData->displayName = displayName;
        eventData->eventHashStr = hashStr;
        eventData->fileName = filename;
        eventData->functionName = function;
        eventData->line = lineNumber;
        eventData->totalTime = totalTime;
        eventData->selfTime = selfTime;
        d->v8EventHash[hashStr] = eventData;
    } else {
        eventData->totalTime += totalTime;
        eventData->selfTime += selfTime;
    }
    d->v8parents[depth] = eventData;

    QV8EventInfo *parentEvent = 0;
    if (depth == 0) {
        parentEvent = &d->v8RootEvent;
        d->v8MeasuredTime += totalTime;
    }
    if (depth > 0 && d->v8parents.contains(depth-1)) {
        parentEvent = d->v8parents.value(depth-1);
    }

    if (parentEvent != 0) {
        if (!parentEvent->v8children.contains(eventData->eventHashStr)) {
            parentEvent->v8children[eventData->eventHashStr] = totalTime;
        } else {
            parentEvent->v8children[eventData->eventHashStr] += totalTime;
        }
    }
}

void QmlProfilerData::computeQmlTime()
{
    // compute levels
    QHash<int, qint64> endtimesPerLevel;
    int minimumLevel = 1;
    int level = minimumLevel;

    for (int i = 0; i < d->startInstanceList.count(); i++) {
        qint64 st = d->startInstanceList[i].startTime;

        if (d->startInstanceList[i].data->rangeType == QQmlProfilerService::Painting) {
            continue;
        }

        // general level
        if (endtimesPerLevel[level] > st) {
            level++;
        } else {
            while (level > minimumLevel && endtimesPerLevel[level-1] <= st)
                level--;
        }
        endtimesPerLevel[level] = st + d->startInstanceList[i].duration;

        if (level == minimumLevel) {
            d->qmlMeasuredTime += d->startInstanceList[i].duration;
        }
    }
}

bool compareStartTimes(const QmlRangeEventStartInstance &t1, const QmlRangeEventStartInstance &t2)
{
    return t1.startTime < t2.startTime;
}

void QmlProfilerData::sortStartTimes()
{
    if (d->startInstanceList.count() < 2)
        return;

    // assuming startTimes is partially sorted
    // identify blocks of events and sort them with quicksort
    QVector<QmlRangeEventStartInstance>::iterator itFrom = d->startInstanceList.end() - 2;
    QVector<QmlRangeEventStartInstance>::iterator itTo = d->startInstanceList.end() - 1;

    while (itFrom != d->startInstanceList.begin() && itTo != d->startInstanceList.begin()) {
        // find block to sort
        while ( itFrom != d->startInstanceList.begin()
                && itTo->startTime > itFrom->startTime ) {
            itTo--;
            itFrom = itTo - 1;
        }

        // if we're at the end of the list
        if (itFrom == d->startInstanceList.begin())
            break;

        // find block length
        while ( itFrom != d->startInstanceList.begin()
                && itTo->startTime <= itFrom->startTime )
            itFrom--;

        if (itTo->startTime <= itFrom->startTime)
            std::sort(itFrom, itTo + 1, compareStartTimes);
        else
            std::sort(itFrom + 1, itTo + 1, compareStartTimes);

        // move to next block
        itTo = itFrom;
        itFrom = itTo - 1;
    }
}

void QmlProfilerData::complete()
{
    setState(ProcessingData);
    sortStartTimes();
    computeQmlTime();
    setState(Done);
    emit dataReady();
}

bool QmlProfilerData::isEmpty() const
{
    return d->startInstanceList.isEmpty() && d->v8EventHash.isEmpty();
}

bool QmlProfilerData::save(const QString &filename)
{
    if (isEmpty()) {
        emit error(tr("No data to save"));
        return false;
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        emit error(tr("Could not open %1 for writing").arg(filename));
        return false;
    }

    QXmlStreamWriter stream(&file);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();

    stream.writeStartElement(QStringLiteral("trace"));
    stream.writeAttribute(QStringLiteral("version"), PROFILER_FILE_VERSION);

    stream.writeAttribute(QStringLiteral("traceStart"), QString::number(traceStartTime()));
    stream.writeAttribute(QStringLiteral("traceEnd"), QString::number(traceEndTime()));

    stream.writeStartElement(QStringLiteral("eventData"));
    stream.writeAttribute(QStringLiteral("totalTime"), QString::number(d->qmlMeasuredTime));

    foreach (const QmlRangeEventData *eventData, d->eventDescriptions.values()) {
        stream.writeStartElement(QStringLiteral("event"));
        stream.writeAttribute(QStringLiteral("index"), QString::number(d->eventDescriptions.keys().indexOf(eventData->eventHashStr)));
        stream.writeTextElement(QStringLiteral("displayname"), eventData->displayName);
        if (eventData->rangeType != QQmlProfilerService::MaximumRangeType)
            stream.writeTextElement(QStringLiteral("type"),
                                    qmlRangeTypeAsString(eventData->rangeType));
        else
            stream.writeTextElement(QStringLiteral("type"),
                                    qmlMessageAsString(eventData->message));
        if (!eventData->location.filename.isEmpty()) {
            stream.writeTextElement(QStringLiteral("filename"), eventData->location.filename);
            stream.writeTextElement(QStringLiteral("line"), QString::number(eventData->location.line));
            stream.writeTextElement(QStringLiteral("column"), QString::number(eventData->location.column));
        }
        stream.writeTextElement(QStringLiteral("details"), eventData->details);
        if (eventData->rangeType == QQmlProfilerService::Binding)
            stream.writeTextElement(QStringLiteral("bindingType"),
                                    QString::number((int)eventData->detailType));
        else if (eventData->message == QQmlProfilerService::Event &&
                 eventData->detailType == QQmlProfilerService::AnimationFrame)
            stream.writeTextElement(QStringLiteral("animationFrame"),
                                    QString::number((int)eventData->detailType));
        else if (eventData->message == QQmlProfilerService::PixmapCacheEvent)
            stream.writeTextElement(QStringLiteral("cacheEventType"),
                                    QString::number((int)eventData->detailType));
        else if (eventData->message == QQmlProfilerService::SceneGraphFrame)
            stream.writeTextElement(QStringLiteral("sgEventType"),
                                    QString::number((int)eventData->detailType));
        else if (eventData->message == QQmlProfilerService::MemoryAllocation)
            stream.writeTextElement(QStringLiteral("memoryEventType"),
                                    QString::number((int)eventData->detailType));
        stream.writeEndElement();
    }
    stream.writeEndElement(); // eventData

    stream.writeStartElement(QStringLiteral("profilerDataModel"));
    foreach (const QmlRangeEventStartInstance &event, d->startInstanceList) {
        stream.writeStartElement(QStringLiteral("range"));
        stream.writeAttribute(QStringLiteral("startTime"), QString::number(event.startTime));
        if (event.duration >= 0)
            stream.writeAttribute(QStringLiteral("duration"),
                                  QString::number(event.duration));
        stream.writeAttribute(QStringLiteral("eventIndex"), QString::number(d->eventDescriptions.keys().indexOf(event.data->eventHashStr)));
        if (event.data->message == QQmlProfilerService::Event &&
                event.data->detailType == QQmlProfilerService::AnimationFrame) {
            // special: animation frame
            stream.writeAttribute(QStringLiteral("framerate"), QString::number(event.frameRate));
            stream.writeAttribute(QStringLiteral("animationcount"), QString::number(event.animationCount));
            stream.writeAttribute(QStringLiteral("thread"), QString::number(event.threadId));
        } else if (event.data->message == QQmlProfilerService::PixmapCacheEvent) {
            // special: pixmap cache event
            if (event.data->detailType == QQmlProfilerService::PixmapSizeKnown) {
                stream.writeAttribute(QStringLiteral("width"),
                                      QString::number(event.numericData1));
                stream.writeAttribute(QStringLiteral("height"),
                                      QString::number(event.numericData2));
            } else if (event.data->detailType == QQmlProfilerService::PixmapReferenceCountChanged ||
                    event.data->detailType == QQmlProfilerService::PixmapCacheCountChanged) {
                stream.writeAttribute(QStringLiteral("refCount"),
                                      QString::number(event.numericData3));
            }
        } else if (event.data->message == QQmlProfilerService::SceneGraphFrame) {
            // special: scenegraph frame events
            if (event.numericData1 > 0)
                stream.writeAttribute(QStringLiteral("timing1"),
                                      QString::number(event.numericData1));
            if (event.numericData2 > 0)
                stream.writeAttribute(QStringLiteral("timing2"),
                                      QString::number(event.numericData2));
            if (event.numericData3 > 0)
                stream.writeAttribute(QStringLiteral("timing3"),
                                      QString::number(event.numericData3));
            if (event.numericData4 > 0)
                stream.writeAttribute(QStringLiteral("timing4"),
                                      QString::number(event.numericData4));
            if (event.numericData5 > 0)
                stream.writeAttribute(QStringLiteral("timing5"),
                                      QString::number(event.numericData5));
        } else if (event.data->message == QQmlProfilerService::MemoryAllocation) {
            stream.writeAttribute(QStringLiteral("amount"), QString::number(event.numericData1));
        }
        stream.writeEndElement();
    }
    stream.writeEndElement(); // profilerDataModel

    stream.writeStartElement(QStringLiteral("v8profile")); // v8 profiler output
    stream.writeAttribute(QStringLiteral("totalTime"), QString::number(d->v8MeasuredTime));
    foreach (QV8EventInfo *v8event, d->v8EventHash.values()) {
        stream.writeStartElement(QStringLiteral("event"));
        stream.writeAttribute(QStringLiteral("index"), QString::number(d->v8EventHash.keys().indexOf(v8event->eventHashStr)));
        stream.writeTextElement(QStringLiteral("displayname"), v8event->displayName);
        stream.writeTextElement(QStringLiteral("functionname"), v8event->functionName);
        if (!v8event->fileName.isEmpty()) {
            stream.writeTextElement(QStringLiteral("filename"), v8event->fileName);
            stream.writeTextElement(QStringLiteral("line"), QString::number(v8event->line));
        }
        stream.writeTextElement(QStringLiteral("totalTime"), QString::number(v8event->totalTime));
        stream.writeTextElement(QStringLiteral("selfTime"), QString::number(v8event->selfTime));
        if (!v8event->v8children.isEmpty()) {
            stream.writeStartElement(QStringLiteral("childrenEvents"));
            QStringList childrenIndexes;
            QStringList childrenTimes;
            foreach (const QString &childHash, v8event->v8children.keys()) {
                childrenIndexes << QString::number(v8EventIndex(childHash));
                childrenTimes << QString::number(v8event->v8children[childHash]);
            }

            stream.writeAttribute(QStringLiteral("list"), childrenIndexes.join(QString(", ")));
            stream.writeAttribute(QStringLiteral("childrenTimes"), childrenTimes.join(QString(", ")));
            stream.writeEndElement();
        }
        stream.writeEndElement();
    }
    stream.writeEndElement(); // v8 profiler output

    stream.writeEndElement(); // trace
    stream.writeEndDocument();

    file.close();
    return true;
}

int QmlProfilerData::v8EventIndex(const QString &hashStr)
{
    if (!d->v8EventHash.contains(hashStr)) {
        emit error("Trying to index nonexisting v8 event");
        return -1;
    }
    return d->v8EventHash.keys().indexOf( hashStr );
}

void QmlProfilerData::setState(QmlProfilerData::State state)
{
    // It's not an error, we are continuously calling "AcquiringData" for example
    if (d->state == state)
        return;

    switch (state) {
    case Empty:
        // if it's not empty, complain but go on
        if (!isEmpty())
            emit error("Invalid qmlprofiler state change (Empty)");
        break;
    case AcquiringData:
        // we're not supposed to receive new data while processing older data
        if (d->state == ProcessingData)
            emit error("Invalid qmlprofiler state change (AcquiringData)");
        break;
    case ProcessingData:
        if (d->state != AcquiringData)
            emit error("Invalid qmlprofiler state change (ProcessingData)");
        break;
    case Done:
        if (d->state != ProcessingData && d->state != Empty)
            emit error("Invalid qmlprofiler state change (Done)");
        break;
    default:
        emit error("Trying to set unknown state in events list");
        break;
    }

    d->state = state;
    emit stateChanged();

    // special: if we were done with an empty list, clean internal data and go back to empty
    if (d->state == Done && isEmpty()) {
        clear();
    }
    return;
}

