/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qmlprofilerdata.h"

#include <QStringList>
#include <QUrl>
#include <QHash>
#include <QFile>
#include <QXmlStreamReader>

#include <limits>

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
                      const QQmlEventLocation &_location, const QString &_details,
                      QQmlProfilerDefinitions::Message _message,
                      QQmlProfilerDefinitions::RangeType _rangeType)
        : displayName(_displayName), eventHashStr(_eventHashStr), location(_location),
          details(_details), message(_message), rangeType(_rangeType), detailType(_detailType) {}
    QString displayName;
    QString eventHashStr;
    QQmlEventLocation location;
    QString details;
    QQmlProfilerDefinitions::Message message;
    QQmlProfilerDefinitions::RangeType rangeType;
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
        int inputType;
        qint64 numericData1;
    };
    union {
        int animationCount;
        int inputA;
        qint64 numericData2;
    };
    union {
        int threadId;
        int inputB;
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

/////////////////////////////////////////////////////////////////
class QmlProfilerDataPrivate
{
public:
    QmlProfilerDataPrivate(QmlProfilerData *qq){ Q_UNUSED(qq); }

    // data storage
    QHash<QString, QmlRangeEventData *> eventDescriptions;
    QVector<QmlRangeEventStartInstance> startInstanceList;

    qint64 traceStartTime;
    qint64 traceEndTime;

    // internal state while collecting events
    qint64 qmlMeasuredTime;
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
    qDeleteAll(d->eventDescriptions);
    d->eventDescriptions.clear();
    d->startInstanceList.clear();

    d->traceEndTime = std::numeric_limits<qint64>::min();
    d->traceStartTime = std::numeric_limits<qint64>::max();
    d->qmlMeasuredTime = 0;

    setState(Empty);
}

QString QmlProfilerData::getHashStringForQmlEvent(const QQmlEventLocation &location, int eventType)
{
    return QString(QStringLiteral("%1:%2:%3:%4")).arg(
                location.filename,
                QString::number(location.line),
                QString::number(location.column),
                QString::number(eventType));
}

QString QmlProfilerData::qmlRangeTypeAsString(QQmlProfilerDefinitions::RangeType type)
{
    if (type * sizeof(char *) < sizeof(RANGE_TYPE_STRINGS))
        return QLatin1String(RANGE_TYPE_STRINGS[type]);
    else
        return QString::number(type);
}

QString QmlProfilerData::qmlMessageAsString(QQmlProfilerDefinitions::Message type)
{
    if (type * sizeof(char *) < sizeof(MESSAGE_STRINGS))
        return QLatin1String(MESSAGE_STRINGS[type]);
    else
        return QString::number(type);
}

void QmlProfilerData::setTraceStartTime(qint64 time)
{
    if (time < d->traceStartTime)
        d->traceStartTime = time;
}

void QmlProfilerData::setTraceEndTime(qint64 time)
{
    if (time > d->traceEndTime)
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

void QmlProfilerData::addQmlEvent(QQmlProfilerDefinitions::RangeType type,
                                  QQmlProfilerDefinitions::BindingType bindingType,
                                  qint64 startTime,
                                  qint64 duration,
                                  const QStringList &data,
                                  const QQmlEventLocation &location)
{
    setState(AcquiringData);

    QString details;
    // generate details string
    if (!data.isEmpty()) {
        details = data.join(QLatin1Char(' ')).replace(
                    QLatin1Char('\n'), QLatin1Char(' ')).simplified();
        QRegExp rewrite(QStringLiteral("\\(function \\$(\\w+)\\(\\) \\{ (return |)(.+) \\}\\)"));
        bool match = rewrite.exactMatch(details);
        if (match) {
            details = rewrite.cap(1) +QLatin1String(": ") + rewrite.cap(3);
        }
        if (details.startsWith(QLatin1String("file://")))
            details = details.mid(details.lastIndexOf(QLatin1Char('/')) + 1);
    }

    QQmlEventLocation eventLocation = location;
    QString displayName, eventHashStr;
    // generate hash
    if (eventLocation.filename.isEmpty()) {
        displayName = tr("<bytecode>");
        eventHashStr = getHashStringForQmlEvent(eventLocation, type);
    } else {
        const QString filePath = QUrl(eventLocation.filename).path();
        displayName = filePath.midRef(
                    filePath.lastIndexOf(QLatin1Char('/')) + 1) +
                    QLatin1Char(':') + QString::number(eventLocation.line);
        eventHashStr = getHashStringForQmlEvent(eventLocation, type);
    }

    QmlRangeEventData *newEvent;
    if (d->eventDescriptions.contains(eventHashStr)) {
        newEvent = d->eventDescriptions[eventHashStr];
    } else {
        newEvent = new QmlRangeEventData(displayName, bindingType, eventHashStr, location, details,
                                         QQmlProfilerDefinitions::MaximumMessage, type);
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
        newEvent = new QmlRangeEventData(displayName, QQmlProfilerDefinitions::AnimationFrame,
                                         eventHashStr,
                                         QQmlEventLocation(), details,
                                         QQmlProfilerDefinitions::Event,
                                         QQmlProfilerDefinitions::MaximumRangeType);
        d->eventDescriptions.insert(eventHashStr, newEvent);
    }

    QmlRangeEventStartInstance rangeEventStartInstance(time, -1, framerate, animationcount,
                                                       threadId, newEvent);

    d->startInstanceList.append(rangeEventStartInstance);
}

void QmlProfilerData::addSceneGraphFrameEvent(QQmlProfilerDefinitions::SceneGraphFrameType type,
                                              qint64 time, qint64 numericData1, qint64 numericData2,
                                              qint64 numericData3, qint64 numericData4,
                                              qint64 numericData5)
{
    setState(AcquiringData);

    QString eventHashStr = QString::fromLatin1("SceneGraph:%1").arg(type);
    QmlRangeEventData *newEvent;
    if (d->eventDescriptions.contains(eventHashStr)) {
        newEvent = d->eventDescriptions[eventHashStr];
    } else {
        newEvent = new QmlRangeEventData(QStringLiteral("<SceneGraph>"), type, eventHashStr,
                                         QQmlEventLocation(), QString(),
                                         QQmlProfilerDefinitions::SceneGraphFrame,
                                         QQmlProfilerDefinitions::MaximumRangeType);
        d->eventDescriptions.insert(eventHashStr, newEvent);
    }

    QmlRangeEventStartInstance rangeEventStartInstance(time, numericData1, numericData2,
                                                       numericData3, numericData4, numericData5,
                                                       newEvent);
    d->startInstanceList.append(rangeEventStartInstance);
}

void QmlProfilerData::addPixmapCacheEvent(QQmlProfilerDefinitions::PixmapEventType type,
                                          qint64 time, const QString &location,
                                          int numericData1, int numericData2)
{
    setState(AcquiringData);

    QString filePath = QUrl(location).path();

    const QString eventHashStr = filePath.midRef(filePath.lastIndexOf(QLatin1Char('/')) + 1)
            + QLatin1Char(':') + QString::number(type);
    QmlRangeEventData *newEvent;
    if (d->eventDescriptions.contains(eventHashStr)) {
        newEvent = d->eventDescriptions[eventHashStr];
    } else {
        newEvent = new QmlRangeEventData(eventHashStr, type, eventHashStr,
                                         QQmlEventLocation(location, -1, -1), QString(),
                                         QQmlProfilerDefinitions::PixmapCacheEvent,
                                         QQmlProfilerDefinitions::MaximumRangeType);
        d->eventDescriptions.insert(eventHashStr, newEvent);
    }

    QmlRangeEventStartInstance rangeEventStartInstance(time, numericData1, numericData2, 0, 0, 0,
                                                       newEvent);
    d->startInstanceList.append(rangeEventStartInstance);
}

void QmlProfilerData::addMemoryEvent(QQmlProfilerDefinitions::MemoryType type, qint64 time,
                                     qint64 size)
{
    setState(AcquiringData);
    QString eventHashStr = QString::fromLatin1("MemoryAllocation:%1").arg(type);
    QmlRangeEventData *newEvent;
    if (d->eventDescriptions.contains(eventHashStr)) {
        newEvent = d->eventDescriptions[eventHashStr];
    } else {
        newEvent = new QmlRangeEventData(eventHashStr, type, eventHashStr, QQmlEventLocation(),
                                         QString(), QQmlProfilerDefinitions::MemoryAllocation,
                                         QQmlProfilerDefinitions::MaximumRangeType);
        d->eventDescriptions.insert(eventHashStr, newEvent);
    }
    QmlRangeEventStartInstance rangeEventStartInstance(time, size, 0, 0, 0, 0, newEvent);
    d->startInstanceList.append(rangeEventStartInstance);
}

void QmlProfilerData::addInputEvent(QQmlProfilerDefinitions::InputEventType type, qint64 time,
                                    int a, int b)
{
    setState(AcquiringData);

    QQmlProfilerDefinitions::EventType eventType;
    switch (type) {
    case QQmlProfilerDefinitions::InputKeyPress:
    case QQmlProfilerDefinitions::InputKeyRelease:
    case QQmlProfilerDefinitions::InputKeyUnknown:
        eventType = QQmlProfilerDefinitions::Key;
        break;
    default:
        eventType = QQmlProfilerDefinitions::Mouse;
        break;
    }

    QString eventHashStr = QString::fromLatin1("Input:%1").arg(eventType);

    QmlRangeEventData *newEvent;
    if (d->eventDescriptions.contains(eventHashStr)) {
        newEvent = d->eventDescriptions[eventHashStr];
    } else {
        newEvent = new QmlRangeEventData(QString(), eventType, eventHashStr, QQmlEventLocation(),
                                         QString(), QQmlProfilerDefinitions::Event,
                                         QQmlProfilerDefinitions::MaximumRangeType);
        d->eventDescriptions.insert(eventHashStr, newEvent);
    }

    d->startInstanceList.append(QmlRangeEventStartInstance(time, -1, type, a, b, newEvent));
}

void QmlProfilerData::computeQmlTime()
{
    // compute levels
    QHash<int, qint64> endtimesPerLevel;
    int minimumLevel = 1;
    int level = minimumLevel;

    for (int i = 0; i < d->startInstanceList.count(); i++) {
        qint64 st = d->startInstanceList[i].startTime;

        if (d->startInstanceList[i].data->rangeType == QQmlProfilerDefinitions::Painting) {
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
            --itTo;
            itFrom = itTo - 1;
        }

        // if we're at the end of the list
        if (itFrom == d->startInstanceList.begin())
            break;

        // find block length
        while ( itFrom != d->startInstanceList.begin()
                && itTo->startTime <= itFrom->startTime )
            --itFrom;

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
    return d->startInstanceList.isEmpty();
}

bool QmlProfilerData::save(const QString &filename)
{
    if (isEmpty()) {
        emit error(tr("No data to save"));
        return false;
    }

    QFile file;
    if (!filename.isEmpty()) {
        file.setFileName(filename);
        if (!file.open(QIODevice::WriteOnly)) {
            emit error(tr("Could not open %1 for writing").arg(filename));
            return false;
        }
    } else {
        if (!file.open(stdout, QIODevice::WriteOnly)) {
            emit error(tr("Could not open stdout for writing"));
            return false;
        }
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

    const auto eventDescriptionsKeys = d->eventDescriptions.keys();
    for (auto it = d->eventDescriptions.cbegin(), end = d->eventDescriptions.cend();
         it != end; ++it) {
        const QmlRangeEventData *eventData = it.value();
        stream.writeStartElement(QStringLiteral("event"));
        stream.writeAttribute(QStringLiteral("index"), QString::number(
                                  eventDescriptionsKeys.indexOf(eventData->eventHashStr)));
        if (!eventData->displayName.isEmpty())
            stream.writeTextElement(QStringLiteral("displayname"), eventData->displayName);
        if (eventData->rangeType != QQmlProfilerDefinitions::MaximumRangeType)
            stream.writeTextElement(QStringLiteral("type"),
                                    qmlRangeTypeAsString(eventData->rangeType));
        else
            stream.writeTextElement(QStringLiteral("type"),
                                    qmlMessageAsString(eventData->message));
        if (!eventData->location.filename.isEmpty())
            stream.writeTextElement(QStringLiteral("filename"), eventData->location.filename);
        if (eventData->location.line >= 0)
            stream.writeTextElement(QStringLiteral("line"),
                                    QString::number(eventData->location.line));
        if (eventData->location.column >= 0)
            stream.writeTextElement(QStringLiteral("column"),
                                    QString::number(eventData->location.column));
        if (!eventData->details.isEmpty())
            stream.writeTextElement(QStringLiteral("details"), eventData->details);
        if (eventData->rangeType == QQmlProfilerDefinitions::Binding)
            stream.writeTextElement(QStringLiteral("bindingType"),
                                    QString::number((int)eventData->detailType));
        else if (eventData->message == QQmlProfilerDefinitions::Event) {
            switch (eventData->detailType) {
            case QQmlProfilerDefinitions::AnimationFrame:
                stream.writeTextElement(QStringLiteral("animationFrame"),
                                        QString::number((int)eventData->detailType));
                break;
            case QQmlProfilerDefinitions::Key:
                stream.writeTextElement(QStringLiteral("keyEvent"),
                                        QString::number((int)eventData->detailType));
                break;
            case QQmlProfilerDefinitions::Mouse:
                stream.writeTextElement(QStringLiteral("mouseEvent"),
                                        QString::number((int)eventData->detailType));
                break;
            }
        } else if (eventData->message == QQmlProfilerDefinitions::PixmapCacheEvent)
            stream.writeTextElement(QStringLiteral("cacheEventType"),
                                    QString::number((int)eventData->detailType));
        else if (eventData->message == QQmlProfilerDefinitions::SceneGraphFrame)
            stream.writeTextElement(QStringLiteral("sgEventType"),
                                    QString::number((int)eventData->detailType));
        else if (eventData->message == QQmlProfilerDefinitions::MemoryAllocation)
            stream.writeTextElement(QStringLiteral("memoryEventType"),
                                    QString::number((int)eventData->detailType));
        stream.writeEndElement();
    }
    stream.writeEndElement(); // eventData

    stream.writeStartElement(QStringLiteral("profilerDataModel"));
    for (const QmlRangeEventStartInstance &event : qAsConst(d->startInstanceList)) {
        stream.writeStartElement(QStringLiteral("range"));
        stream.writeAttribute(QStringLiteral("startTime"), QString::number(event.startTime));
        if (event.duration >= 0)
            stream.writeAttribute(QStringLiteral("duration"),
                                  QString::number(event.duration));
        stream.writeAttribute(QStringLiteral("eventIndex"), QString::number(
                                  eventDescriptionsKeys.indexOf(event.data->eventHashStr)));
        if (event.data->message == QQmlProfilerDefinitions::Event) {
            if (event.data->detailType == QQmlProfilerDefinitions::AnimationFrame) {
                // special: animation frame
                stream.writeAttribute(QStringLiteral("framerate"), QString::number(event.frameRate));
                stream.writeAttribute(QStringLiteral("animationcount"),
                                      QString::number(event.animationCount));
                stream.writeAttribute(QStringLiteral("thread"), QString::number(event.threadId));
            } else if (event.data->detailType == QQmlProfilerDefinitions::Key ||
                       event.data->detailType == QQmlProfilerDefinitions::Mouse) {
                // numerical value here, to keep the format a bit more compact
                stream.writeAttribute(QStringLiteral("type"),
                                      QString::number(event.inputType));
                stream.writeAttribute(QStringLiteral("data1"),
                                      QString::number(event.inputA));
                stream.writeAttribute(QStringLiteral("data2"),
                                      QString::number(event.inputB));
            }
        } else if (event.data->message == QQmlProfilerDefinitions::PixmapCacheEvent) {
            // special: pixmap cache event
            if (event.data->detailType == QQmlProfilerDefinitions::PixmapSizeKnown) {
                stream.writeAttribute(QStringLiteral("width"),
                                      QString::number(event.numericData1));
                stream.writeAttribute(QStringLiteral("height"),
                                      QString::number(event.numericData2));
            } else if (event.data->detailType ==
                       QQmlProfilerDefinitions::PixmapReferenceCountChanged ||
                    event.data->detailType ==
                       QQmlProfilerDefinitions::PixmapCacheCountChanged) {
                stream.writeAttribute(QStringLiteral("refCount"),
                                      QString::number(event.numericData1));
            }
        } else if (event.data->message == QQmlProfilerDefinitions::SceneGraphFrame) {
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
        } else if (event.data->message == QQmlProfilerDefinitions::MemoryAllocation) {
            stream.writeAttribute(QStringLiteral("amount"), QString::number(event.numericData1));
        }
        stream.writeEndElement();
    }
    stream.writeEndElement(); // profilerDataModel

    stream.writeEndElement(); // trace
    stream.writeEndDocument();

    file.close();
    return true;
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

