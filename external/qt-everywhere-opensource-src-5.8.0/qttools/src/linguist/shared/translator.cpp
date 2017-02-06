/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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

#include "translator.h"

#include "simtexth.h"

#include <iostream>

#include <stdio.h>
#ifdef Q_OS_WIN
// required for _setmode, to avoid _O_TEXT streams...
#  include <io.h> // for _setmode
#  include <fcntl.h> // for _O_BINARY
#endif

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>

#include <private/qlocale_p.h>
#include <private/qtranslator_p.h>

QT_BEGIN_NAMESPACE

Translator::Translator() :
    m_locationsType(AbsoluteLocations),
    m_indexOk(true)
{
}

void Translator::registerFileFormat(const FileFormat &format)
{
    //qDebug() << "Translator: Registering format " << format.extension;
    QList<Translator::FileFormat> &formats = registeredFileFormats();
    for (int i = 0; i < formats.size(); ++i)
        if (format.fileType == formats[i].fileType && format.priority < formats[i].priority) {
            formats.insert(i, format);
            return;
        }
    formats.append(format);
}

QList<Translator::FileFormat> &Translator::registeredFileFormats()
{
    static QList<Translator::FileFormat> theFormats;
    return theFormats;
}

void Translator::addIndex(int idx, const TranslatorMessage &msg) const
{
    if (msg.sourceText().isEmpty() && msg.id().isEmpty()) {
        m_ctxCmtIdx[msg.context()] = idx;
    } else {
        m_msgIdx[TMMKey(msg)] = idx;
        if (!msg.id().isEmpty())
            m_idMsgIdx[msg.id()] = idx;
    }
}

void Translator::delIndex(int idx) const
{
    const TranslatorMessage &msg = m_messages.at(idx);
    if (msg.sourceText().isEmpty() && msg.id().isEmpty()) {
        m_ctxCmtIdx.remove(msg.context());
    } else {
        m_msgIdx.remove(TMMKey(msg));
        if (!msg.id().isEmpty())
            m_idMsgIdx.remove(msg.id());
    }
}

void Translator::ensureIndexed() const
{
    if (!m_indexOk) {
        m_indexOk = true;
        m_ctxCmtIdx.clear();
        m_idMsgIdx.clear();
        m_msgIdx.clear();
        for (int i = 0; i < m_messages.count(); i++)
            addIndex(i, m_messages.at(i));
    }
}

void Translator::replaceSorted(const TranslatorMessage &msg)
{
    int index = find(msg);
    if (index == -1) {
        appendSorted(msg);
    } else {
        delIndex(index);
        m_messages[index] = msg;
        addIndex(index, msg);
    }
}

static QString elidedId(const QString &id, int len)
{
    return id.length() <= len ? id : id.left(len - 5) + QLatin1String("[...]");
}

static QString makeMsgId(const TranslatorMessage &msg)
{
    QString id = msg.context() + QLatin1String("//") + elidedId(msg.sourceText(), 100);
    if (!msg.comment().isEmpty())
        id += QLatin1String("//") + elidedId(msg.comment(), 30);
    return id;
}

void Translator::extend(const TranslatorMessage &msg, ConversionData &cd)
{
    int index = find(msg);
    if (index == -1) {
        append(msg);
    } else {
        TranslatorMessage &emsg = m_messages[index];
        if (emsg.sourceText().isEmpty()) {
            delIndex(index);
            emsg.setSourceText(msg.sourceText());
            addIndex(index, msg);
        } else if (!msg.sourceText().isEmpty() && emsg.sourceText() != msg.sourceText()) {
            cd.appendError(QString::fromLatin1("Contradicting source strings for message with id '%1'.")
                           .arg(emsg.id()));
            return;
        }
        if (emsg.extras().isEmpty()) {
            emsg.setExtras(msg.extras());
        } else if (!msg.extras().isEmpty() && emsg.extras() != msg.extras()) {
            cd.appendError(QString::fromLatin1("Contradicting meta data for for %1.")
                           .arg(!emsg.id().isEmpty()
                                ? QString::fromLatin1("message with id '%1'").arg(emsg.id())
                                : QString::fromLatin1("message '%1'").arg(makeMsgId(msg))));
            return;
        }
        emsg.addReferenceUniq(msg.fileName(), msg.lineNumber());
        if (!msg.extraComment().isEmpty()) {
            QString cmt = emsg.extraComment();
            if (!cmt.isEmpty()) {
                QStringList cmts = cmt.split(QLatin1String("\n----------\n"));
                if (!cmts.contains(msg.extraComment())) {
                    cmts.append(msg.extraComment());
                    cmt = cmts.join(QLatin1String("\n----------\n"));
                }
            } else {
                cmt = msg.extraComment();
            }
            emsg.setExtraComment(cmt);
        }
    }
}

void Translator::insert(int idx, const TranslatorMessage &msg)
{
    if (m_indexOk) {
        if (idx == m_messages.count())
            addIndex(idx, msg);
        else
            m_indexOk = false;
    }
    m_messages.insert(idx, msg);
}

void Translator::append(const TranslatorMessage &msg)
{
    insert(m_messages.count(), msg);
}

void Translator::appendSorted(const TranslatorMessage &msg)
{
    int msgLine = msg.lineNumber();
    if (msgLine < 0) {
        append(msg);
        return;
    }

    int bestIdx = 0; // Best insertion point found so far
    int bestScore = 0; // Its category: 0 = no hit, 1 = pre or post, 2 = middle
    int bestSize = 0; // The length of the region. Longer is better within one category.

    // The insertion point to use should this region turn out to be the best one so far
    int thisIdx = 0;
    int thisScore = 0;
    int thisSize = 0;
    // Working vars
    int prevLine = 0;
    int curIdx = 0;
    foreach (const TranslatorMessage &mit, m_messages) {
        bool sameFile = mit.fileName() == msg.fileName() && mit.context() == msg.context();
        int curLine;
        if (sameFile && (curLine = mit.lineNumber()) >= prevLine) {
            if (msgLine >= prevLine && msgLine < curLine) {
                thisIdx = curIdx;
                thisScore = thisSize ? 2 : 1;
            }
            ++thisSize;
            prevLine = curLine;
        } else {
            if (thisSize) {
                if (!thisScore) {
                    thisIdx = curIdx;
                    thisScore = 1;
                }
                if (thisScore > bestScore || (thisScore == bestScore && thisSize > bestSize)) {
                    bestIdx = thisIdx;
                    bestScore = thisScore;
                    bestSize = thisSize;
                }
                thisScore = 0;
                thisSize = sameFile ? 1 : 0;
                prevLine = 0;
            }
        }
        ++curIdx;
    }
    if (thisSize && !thisScore) {
        thisIdx = curIdx;
        thisScore = 1;
    }
    if (thisScore > bestScore || (thisScore == bestScore && thisSize > bestSize))
        insert(thisIdx, msg);
    else if (bestScore)
        insert(bestIdx, msg);
    else
        append(msg);
}

static QString guessFormat(const QString &filename, const QString &format)
{
    if (format != QLatin1String("auto"))
        return format;

    foreach (const Translator::FileFormat &fmt, Translator::registeredFileFormats()) {
        if (filename.endsWith(QLatin1Char('.') + fmt.extension, Qt::CaseInsensitive))
            return fmt.extension;
    }

    // the default format.
    // FIXME: change to something more widely distributed later.
    return QLatin1String("ts");
}

bool Translator::load(const QString &filename, ConversionData &cd, const QString &format)
{
    cd.m_sourceDir = QFileInfo(filename).absoluteDir();
    cd.m_sourceFileName = filename;

    QFile file;
    if (filename.isEmpty() || filename == QLatin1String("-")) {
#ifdef Q_OS_WIN
        // QFile is broken for text files
        ::_setmode(0, _O_BINARY);
#endif
        if (!file.open(stdin, QIODevice::ReadOnly)) {
            cd.appendError(QString::fromLatin1("Cannot open stdin!? (%1)")
                .arg(file.errorString()));
            return false;
        }
    } else {
        file.setFileName(filename);
        if (!file.open(QIODevice::ReadOnly)) {
            cd.appendError(QString::fromLatin1("Cannot open %1: %2")
                .arg(filename, file.errorString()));
            return false;
        }
    }

    QString fmt = guessFormat(filename, format);

    foreach (const FileFormat &format, registeredFileFormats()) {
        if (fmt == format.extension) {
            if (format.loader)
                return (*format.loader)(*this, file, cd);
            cd.appendError(QString(QLatin1String("No loader for format %1 found"))
                .arg(fmt));
            return false;
        }
    }

    cd.appendError(QString(QLatin1String("Unknown format %1 for file %2"))
        .arg(format, filename));
    return false;
}


bool Translator::save(const QString &filename, ConversionData &cd, const QString &format) const
{
    QFile file;
    if (filename.isEmpty() || filename == QLatin1String("-")) {
#ifdef Q_OS_WIN
        // QFile is broken for text files
        ::_setmode(1, _O_BINARY);
#endif
        if (!file.open(stdout, QIODevice::WriteOnly)) {
            cd.appendError(QString::fromLatin1("Cannot open stdout!? (%1)")
                .arg(file.errorString()));
            return false;
        }
    } else {
        file.setFileName(filename);
        if (!file.open(QIODevice::WriteOnly)) {
            cd.appendError(QString::fromLatin1("Cannot create %1: %2")
                .arg(filename, file.errorString()));
            return false;
        }
    }

    QString fmt = guessFormat(filename, format);
    cd.m_targetDir = QFileInfo(filename).absoluteDir();

    foreach (const FileFormat &format, registeredFileFormats()) {
        if (fmt == format.extension) {
            if (format.saver)
                return (*format.saver)(*this, file, cd);
            cd.appendError(QString(QLatin1String("Cannot save %1 files")).arg(fmt));
            return false;
        }
    }

    cd.appendError(QString(QLatin1String("Unknown format %1 for file %2"))
        .arg(format).arg(filename));
    return false;
}

QString Translator::makeLanguageCode(QLocale::Language language, QLocale::Country country)
{
    QString result = QLocalePrivate::languageToCode(language);
    if (language != QLocale::C && country != QLocale::AnyCountry) {
        result.append(QLatin1Char('_'));
        result.append(QLocalePrivate::countryToCode(country));
    }
    return result;
}

void Translator::languageAndCountry(const QString &languageCode,
    QLocale::Language *lang, QLocale::Country *country)
{
    QLocale::Script script;
    QLocalePrivate::getLangAndCountry(languageCode, *lang, script, *country);
}

int Translator::find(const TranslatorMessage &msg) const
{
    ensureIndexed();
    if (msg.id().isEmpty())
        return m_msgIdx.value(TMMKey(msg), -1);
    int i = m_idMsgIdx.value(msg.id(), -1);
    if (i >= 0)
        return i;
    i = m_msgIdx.value(TMMKey(msg), -1);
    // If both have an id, then find only by id.
    return i >= 0 && m_messages.at(i).id().isEmpty() ? i : -1;
}

int Translator::find(const QString &context,
    const QString &comment, const TranslatorMessage::References &refs) const
{
    if (!refs.isEmpty()) {
        for (TMM::ConstIterator it = m_messages.constBegin(); it != m_messages.constEnd(); ++it) {
            if (it->context() == context && it->comment() == comment)
                foreach (const TranslatorMessage::Reference &itref, it->allReferences())
                    foreach (const TranslatorMessage::Reference &ref, refs)
                        if (itref == ref)
                            return it - m_messages.constBegin();
        }
    }
    return -1;
}

int Translator::find(const QString &context) const
{
    ensureIndexed();
    return m_ctxCmtIdx.value(context, -1);
}

void Translator::stripObsoleteMessages()
{
    for (TMM::Iterator it = m_messages.begin(); it != m_messages.end(); )
        if (it->type() == TranslatorMessage::Obsolete || it->type() == TranslatorMessage::Vanished)
            it = m_messages.erase(it);
        else
            ++it;
    m_indexOk = false;
}

void Translator::stripFinishedMessages()
{
    for (TMM::Iterator it = m_messages.begin(); it != m_messages.end(); )
        if (it->type() == TranslatorMessage::Finished)
            it = m_messages.erase(it);
        else
            ++it;
    m_indexOk = false;
}

void Translator::stripEmptyContexts()
{
    for (TMM::Iterator it = m_messages.begin(); it != m_messages.end();)
        if (it->sourceText() == QLatin1String(ContextComment))
            it = m_messages.erase(it);
        else
            ++it;
    m_indexOk = false;
}

void Translator::stripNonPluralForms()
{
    for (TMM::Iterator it = m_messages.begin(); it != m_messages.end(); )
        if (!it->isPlural())
            it = m_messages.erase(it);
        else
            ++it;
    m_indexOk = false;
}

void Translator::stripIdenticalSourceTranslations()
{
    for (TMM::Iterator it = m_messages.begin(); it != m_messages.end(); ) {
        // we need to have just one translation, and it be equal to the source
        if (it->translations().count() == 1 && it->translation() == it->sourceText())
            it = m_messages.erase(it);
        else
            ++it;
    }
    m_indexOk = false;
}

void Translator::dropTranslations()
{
    for (TMM::Iterator it = m_messages.begin(); it != m_messages.end(); ++it) {
        if (it->type() == TranslatorMessage::Finished)
            it->setType(TranslatorMessage::Unfinished);
        it->setTranslation(QString());
    }
}

void Translator::dropUiLines()
{
    QString uiXt = QLatin1String(".ui");
    QString juiXt = QLatin1String(".jui");
    for (TMM::Iterator it = m_messages.begin(); it != m_messages.end(); ++it) {
        QHash<QString, int> have;
        QList<TranslatorMessage::Reference> refs;
        foreach (const TranslatorMessage::Reference &itref, it->allReferences()) {
            const QString &fn = itref.fileName();
            if (fn.endsWith(uiXt) || fn.endsWith(juiXt)) {
                if (++have[fn] == 1)
                    refs.append(TranslatorMessage::Reference(fn, -1));
            } else {
                refs.append(itref);
            }
        }
        it->setReferences(refs);
    }
}

struct TranslatorMessageIdPtr {
    explicit TranslatorMessageIdPtr(const TranslatorMessage &tm)
    {
        ptr = &tm;
    }

    inline const TranslatorMessage *operator->() const
    {
        return ptr;
    }

    const TranslatorMessage *ptr;
};

Q_DECLARE_TYPEINFO(TranslatorMessageIdPtr, Q_MOVABLE_TYPE);

inline int qHash(TranslatorMessageIdPtr tmp)
{
    return qHash(tmp->id());
}

inline bool operator==(TranslatorMessageIdPtr tmp1, TranslatorMessageIdPtr tmp2)
{
    return tmp1->id() == tmp2->id();
}

struct TranslatorMessageContentPtr {
    explicit TranslatorMessageContentPtr(const TranslatorMessage &tm)
    {
        ptr = &tm;
    }

    inline const TranslatorMessage *operator->() const
    {
        return ptr;
    }

    const TranslatorMessage *ptr;
};

Q_DECLARE_TYPEINFO(TranslatorMessageContentPtr, Q_MOVABLE_TYPE);

inline int qHash(TranslatorMessageContentPtr tmp)
{
    int hash = qHash(tmp->context()) ^ qHash(tmp->sourceText());
    if (!tmp->sourceText().isEmpty())
        // Special treatment for context comments (empty source).
        hash ^= qHash(tmp->comment());
    return hash;
}

inline bool operator==(TranslatorMessageContentPtr tmp1, TranslatorMessageContentPtr tmp2)
{
    if (tmp1->context() != tmp2->context() || tmp1->sourceText() != tmp2->sourceText())
        return false;
    // Special treatment for context comments (empty source).
    if (tmp1->sourceText().isEmpty())
        return true;
    return tmp1->comment() == tmp2->comment();
}

Translator::Duplicates Translator::resolveDuplicates()
{
    Duplicates dups;
    QHash<TranslatorMessageIdPtr, int> idRefs;
    QHash<TranslatorMessageContentPtr, int> contentRefs;
    for (int i = 0; i < m_messages.count();) {
        const TranslatorMessage &msg = m_messages.at(i);
        TranslatorMessage *omsg;
        int oi;
        QSet<int> *pDup;
        if (!msg.id().isEmpty()) {
            QHash<TranslatorMessageIdPtr, int>::ConstIterator it =
                    idRefs.constFind(TranslatorMessageIdPtr(msg));
            if (it != idRefs.constEnd()) {
                oi = *it;
                omsg = &m_messages[oi];
                pDup = &dups.byId;
                goto gotDupe;
            }
        }
        {
            QHash<TranslatorMessageContentPtr, int>::ConstIterator it =
                    contentRefs.constFind(TranslatorMessageContentPtr(msg));
            if (it != contentRefs.constEnd()) {
                oi = *it;
                omsg = &m_messages[oi];
                if (msg.id().isEmpty() || omsg->id().isEmpty()) {
                    if (!msg.id().isEmpty() && omsg->id().isEmpty()) {
                        omsg->setId(msg.id());
                        idRefs[TranslatorMessageIdPtr(*omsg)] = oi;
                    }
                    pDup = &dups.byContents;
                    goto gotDupe;
                }
                // This is really a content dupe, but with two distinct IDs.
            }
        }
        if (!msg.id().isEmpty())
            idRefs[TranslatorMessageIdPtr(msg)] = i;
        contentRefs[TranslatorMessageContentPtr(msg)] = i;
        ++i;
        continue;
      gotDupe:
        pDup->insert(oi);
        if (!omsg->isTranslated() && msg.isTranslated())
            omsg->setTranslations(msg.translations());
        m_indexOk = false;
        m_messages.removeAt(i);
    }
    return dups;
}

void Translator::reportDuplicates(const Duplicates &dupes,
                                  const QString &fileName, bool verbose)
{
    if (!dupes.byId.isEmpty() || !dupes.byContents.isEmpty()) {
        std::cerr << "Warning: dropping duplicate messages in '" << qPrintable(fileName);
        if (!verbose) {
            std::cerr << "'\n(try -verbose for more info).\n";
        } else {
            std::cerr << "':\n";
            foreach (int i, dupes.byId)
                std::cerr << "\n* ID: " << qPrintable(message(i).id()) << std::endl;
            foreach (int j, dupes.byContents) {
                const TranslatorMessage &msg = message(j);
                std::cerr << "\n* Context: " << qPrintable(msg.context())
                          << "\n* Source: " << qPrintable(msg.sourceText()) << std::endl;
                if (!msg.comment().isEmpty())
                    std::cerr << "* Comment: " << qPrintable(msg.comment()) << std::endl;
            }
            std::cerr << std::endl;
        }
    }
}

// Used by lupdate to be able to search using absolute paths during merging
void Translator::makeFileNamesAbsolute(const QDir &originalPath)
{
    for (TMM::iterator it = m_messages.begin(); it != m_messages.end(); ++it) {
        TranslatorMessage &msg = *it;
        TranslatorMessage::References refs = msg.allReferences();
        msg.setReferences(TranslatorMessage::References());
        foreach (const TranslatorMessage::Reference &ref, refs) {
            QString fileName = ref.fileName();
            QFileInfo fi (fileName);
            if (fi.isRelative())
                fileName = originalPath.absoluteFilePath(fileName);
            msg.addReference(fileName, ref.lineNumber());
        }
    }
}

QList<TranslatorMessage> Translator::messages() const
{
    return m_messages;
}

QStringList Translator::normalizedTranslations(const TranslatorMessage &msg, int numPlurals)
{
    QStringList translations = msg.translations();
    int numTranslations = msg.isPlural() ? numPlurals : 1;

    // make sure that the stringlist always have the size of the
    // language's current numerus, or 1 if its not plural
    if (translations.count() > numTranslations) {
        for (int i = translations.count(); i > numTranslations; --i)
            translations.removeLast();
    } else if (translations.count() < numTranslations) {
        for (int i = translations.count(); i < numTranslations; ++i)
            translations.append(QString());
    }
    return translations;
}

void Translator::normalizeTranslations(ConversionData &cd)
{
    bool truncated = false;
    QLocale::Language l;
    QLocale::Country c;
    languageAndCountry(languageCode(), &l, &c);
    int numPlurals = 1;
    if (l != QLocale::C) {
        QStringList forms;
        if (getNumerusInfo(l, c, 0, &forms, 0))
            numPlurals = forms.count(); // includes singular
    }
    for (int i = 0; i < m_messages.count(); ++i) {
        const TranslatorMessage &msg = m_messages.at(i);
        QStringList tlns = msg.translations();
        int ccnt = msg.isPlural() ? numPlurals : 1;
        if (tlns.count() != ccnt) {
            while (tlns.count() < ccnt)
                tlns.append(QString());
            while (tlns.count() > ccnt) {
                tlns.removeLast();
                truncated = true;
            }
            m_messages[i].setTranslations(tlns);
        }
    }
    if (truncated)
        cd.appendError(QLatin1String(
            "Removed plural forms as the target language has less "
            "forms.\nIf this sounds wrong, possibly the target language is "
            "not set or recognized."));
}

QString Translator::guessLanguageCodeFromFileName(const QString &filename)
{
    QString str = filename;
    foreach (const FileFormat &format, registeredFileFormats()) {
        if (str.endsWith(format.extension)) {
            str = str.left(str.size() - format.extension.size() - 1);
            break;
        }
    }
    static QRegExp re(QLatin1String("[\\._]"));
    while (true) {
        QLocale locale(str);
        //qDebug() << "LANGUAGE FROM " << str << "LANG: " << locale.language();
        if (locale.language() != QLocale::C) {
            //qDebug() << "FOUND " << locale.name();
            return locale.name();
        }
        int pos = str.indexOf(re);
        if (pos == -1)
            break;
        str = str.mid(pos + 1);
    }
    //qDebug() << "LANGUAGE GUESSING UNSUCCESSFUL";
    return QString();
}

bool Translator::hasExtra(const QString &key) const
{
    return m_extra.contains(key);
}

QString Translator::extra(const QString &key) const
{
    return m_extra[key];
}

void Translator::setExtra(const QString &key, const QString &value)
{
    m_extra[key] = value;
}

void Translator::dump() const
{
    for (int i = 0; i != messageCount(); ++i)
        message(i).dump();
}

QT_END_NAMESPACE
