/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "t9writeinputmethod.h"
#include "inputengine.h"
#include "inputcontext.h"
#include "trace.h"
#include "t9writeworker.h"
#include "virtualkeyboarddebug.h"
#include "QDirIterator"
#ifdef QT_VIRTUALKEYBOARD_DEBUG
#include <QTime>
#endif

#include "decuma_hwr.h"
#include "decumaStatus.h"
#include "decumaSymbolCategories.h"
#include "decumaLanguages.h"
#include "xxt9wOem.h"

namespace QtVirtualKeyboard {

class T9WriteCaseFormatter
{
public:
    T9WriteCaseFormatter() :
        preferLowercase(false)
    {
    }

    void clear()
    {
        textCaseList.clear();
    }

    void ensureLength(int length, InputEngine::TextCase textCase)
    {
        if (length <= 0) {
            textCaseList.clear();
            return;
        }
        while (length < textCaseList.length())
            textCaseList.removeLast();
        while (length > textCaseList.length())
            textCaseList.append(textCase);
    }

    QString formatString(const QString &str) const
    {
        QString result;
        InputEngine::TextCase textCase = InputEngine::Lower;
        for (int i = 0; i < str.length(); ++i) {
            if (i < textCaseList.length())
                textCase = textCaseList.at(i);
            result.append(textCase == InputEngine::Upper ? str.at(i).toUpper() : (preferLowercase ? str.at(i).toLower() : str.at(i)));
        }
        return result;
    }

    bool preferLowercase;

private:
    QList<InputEngine::TextCase> textCaseList;
};

class T9WriteInputMethodPrivate : public AbstractInputMethodPrivate
{
    Q_DECLARE_PUBLIC(T9WriteInputMethod)
public:

    T9WriteInputMethodPrivate(T9WriteInputMethod *q_ptr) :
        AbstractInputMethodPrivate(),
        q_ptr(q_ptr),
        dictionaryLock(QMutex::Recursive),
        convertedDictionary(0),
        attachedDictionary(0),
        resultId(0),
        resultTimer(0),
        decumaSession(0),
        activeWordIndex(-1),
        arcAdditionStarted(false),
        ignoreUpdate(false),
        textCase(InputEngine::Lower)
    {
    }

    static void *decumaMalloc(size_t size, void *pPrivate)
    {
        Q_UNUSED(pPrivate)
        return malloc(size);
    }

    static void *decumaCalloc(size_t elements, size_t size, void *pPrivate)
    {
        Q_UNUSED(pPrivate)
        return calloc(elements, size);
    }

    static void decumaFree(void *ptr, void *pPrivate)
    {
        Q_UNUSED(pPrivate)
        free(ptr);
    }

    void initEngine()
    {
        VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethodPrivate::initEngine()";

        if (decumaSession)
            return;

        symbolCategories.clear();
        symbolCategories.append(DECUMA_CATEGORY_ANSI);
        languageCategories.clear();
        languageCategories.append(DECUMA_LANG_EN);

        memset(&sessionSettings, 0, sizeof(sessionSettings));

        QString latinDb = findLatinDb(":/databases/HWR_LatinCG/");
        hwrDbFile.setFileName(latinDb);
        if (!hwrDbFile.open(QIODevice::ReadOnly)) {
            qWarning() << "Could not open hwr database file" << latinDb;
            return;
        }

        sessionSettings.pStaticDB = (DECUMA_STATIC_DB_PTR)hwrDbFile.map(0, hwrDbFile.size(), QFile::NoOptions);
        if (!sessionSettings.pStaticDB) {
            hwrDbFile.close();
            qWarning() << "Could not map hwr database" << latinDb;
            return;
        }

        sessionSettings.recognitionMode = mcrMode;
        sessionSettings.bMinimizeAddArcPreProcessing = 1;
        sessionSettings.writingDirection = unknownWriting;
        sessionSettings.charSet.pSymbolCategories = symbolCategories.data();
        sessionSettings.charSet.nSymbolCategories = symbolCategories.size();
        sessionSettings.charSet.pLanguages = languageCategories.data();
        sessionSettings.charSet.nLanguages = languageCategories.size();

        session = QByteArray(decumaGetSessionSize(), 0);
        decumaSession = (DECUMA_SESSION *)(!session.isEmpty() ? session.data() : 0);

        DECUMA_STATUS status = decumaBeginSession(decumaSession, &sessionSettings, &memFuncs);
        Q_ASSERT(status == decumaNoError);
        if (status != decumaNoError) {
            qWarning() << "Could not initialize T9Write engine" << status;
        }

        worker.reset(new T9WriteWorker(decumaSession));
        worker->start();
    }

    void exitEngine()
    {
        VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethodPrivate::exitEngine()";

        worker.reset();

        if (sessionSettings.pStaticDB) {
            hwrDbFile.unmap((uchar *)sessionSettings.pStaticDB);
            hwrDbFile.close();
        }

        detachDictionary(&attachedDictionary);
        destroyConvertedDictionary(&convertedDictionary);

        if (decumaSession) {
            decumaEndSession(decumaSession);
            decumaSession = 0;
            session.clear();
        }

        memset(&sessionSettings, 0, sizeof(sessionSettings));
    }

    QString findLatinDb(const QString &dir)
    {
        QString latinDb;
        QDirIterator it(dir, QDirIterator::NoIteratorFlags);
        while (it.hasNext()) {
            QString fileEntry = it.next();

            if (!fileEntry.endsWith(QLatin1String(".bin")))
                continue;

            latinDb = fileEntry;
            break;
        }
        return latinDb;
    }

    QString findDictionary(const QString &dir, const QLocale &locale)
    {
        QStringList languageCountry = locale.name().split("_");
        if (languageCountry.length() != 2)
            return QString();

        QString dictionary;
        QDirIterator it(dir, QDirIterator::NoIteratorFlags);
        while (it.hasNext()) {
            QString fileEntry = it.next();

            if (!fileEntry.endsWith(QLatin1String(".ldb")))
                continue;

            if (!fileEntry.contains("_" + languageCountry[0].toUpper()))
                continue;

            dictionary = fileEntry;
            break;
        }

        return dictionary;
    }

    bool attachDictionary(void *dictionary)
    {
        VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethodPrivate::attachDictionary():" << dictionary;

        QMutexLocker dictionaryGuard(&dictionaryLock);
        Q_ASSERT(decumaSession != 0);
        Q_ASSERT(dictionary != 0);

        DECUMA_STATUS status = decumaAttachConvertedDictionary(decumaSession, dictionary);
        Q_ASSERT(status == decumaNoError);
        return status == decumaNoError;
    }

    void detachDictionary(void **dictionary)
    {
        QMutexLocker dictionaryGuard(&dictionaryLock);
        Q_ASSERT(decumaSession != 0);
        if (!dictionary || !*dictionary)
            return;

        VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethodPrivate::detachDictionary():" << *dictionary;

        DECUMA_STATUS status = decumaDetachDictionary(decumaSession, *dictionary);
        Q_UNUSED(status)
        Q_ASSERT(status == decumaNoError);
        *dictionary = 0;
    }

    void destroyConvertedDictionary(void **dictionary)
    {
        QMutexLocker dictionaryGuard(&dictionaryLock);
        Q_ASSERT(decumaSession != 0);
        if (!dictionary || !*dictionary)
            return;

        VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethodPrivate::destroyConvertedDictionary():" << *dictionary;

        DECUMA_STATUS status = decumaDestroyConvertedDictionary(dictionary, &memFuncs);
        Q_UNUSED(status)
        Q_ASSERT(status == decumaNoError);
        Q_ASSERT(*dictionary == 0);
    }

    bool setInputMode(const QLocale &locale, InputEngine::InputMode inputMode)
    {
        VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethodPrivate::setInputMode():" << locale << inputMode;

        Q_Q(T9WriteInputMethod);
        DECUMA_UINT32 language = mapToDecumaLanguage(locale);
        if (language == DECUMA_LANG_GSMDEFAULT) {
            qWarning() << "Handwriting input does not support the language" << locale.name();
            return false;
        }

        int isLanguageSupported = 0;
        decumaDatabaseIsLanguageSupported(sessionSettings.pStaticDB, language, &isLanguageSupported);
        if (!isLanguageSupported) {
            qWarning() << "Handwriting input does not support the language" << locale.name();
            return false;
        }

        finishRecognition();

        bool languageChanged = languageCategories.isEmpty() || !languageCategories.contains(language);
        if (languageChanged) {
            languageCategories.clear();
            languageCategories.append(language);
        }

        // Choose the symbol categories by input mode, script and input method hints
        bool leftToRightGestures = true;
        Qt::InputMethodHints inputMethodHints = q->inputContext()->inputMethodHints();
        symbolCategories.clear();
        switch (inputMode) {
        case InputEngine::Latin:
            if (inputMethodHints.testFlag(Qt::ImhEmailCharactersOnly)) {
                symbolCategories.append(DECUMA_CATEGORY_EMAIL);
            } else if (inputMethodHints.testFlag(Qt::ImhUrlCharactersOnly)) {
                symbolCategories.append(DECUMA_CATEGORY_URL);
            } else {
                bool includeDigits = true;
                bool includeBasicPunctuation = true;
                switch (locale.script()) {
                case QLocale::LatinScript:
                    if (language == DECUMA_LANG_EN)
                        symbolCategories.append(DECUMA_CATEGORY_ANSI);
                    else
                        symbolCategories.append(DECUMA_CATEGORY_ISO8859_1);
                    break;

                case QLocale::CyrillicScript:
                    symbolCategories.append(DECUMA_CATEGORY_CYRILLIC);
                    break;

                default:
                    qWarning() << "Handwriting input does not support the language" << locale.name();
                    return false;
                }

                if (includeDigits)
                    symbolCategories.append(DECUMA_CATEGORY_DIGIT);

                if (includeBasicPunctuation) {
                    symbolCategories.append(DECUMA_CATEGORY_BASIC_PUNCTUATIONS);
                    symbolCategories.append(DECUMA_CATEGORY_CONTRACTION_MARK);
                }

                if (language == DECUMA_LANG_ES)
                    symbolCategories.append(DECUMA_CATEGORY_SPANISH_PUNCTUATIONS);
            }
            break;

        case InputEngine::Numeric:
            symbolCategories.append(DECUMA_CATEGORY_DIGIT);
            if (!inputMethodHints.testFlag(Qt::ImhDigitsOnly))
                symbolCategories.append(DECUMA_CATEGORY_NUM_SUP);
            break;

        case InputEngine::Dialable:
            symbolCategories.append(DECUMA_CATEGORY_PHONE_NUMBER);
            break;

        default:
            return false;
        }

        if (leftToRightGestures) {
            symbolCategories.append(DECUMA_CATEGORY_BACKSPACE_STROKE);
            symbolCategories.append(DECUMA_CATEGORY_RETURN_STROKE);
            symbolCategories.append(DECUMA_CATEGORY_WHITESPACE_STROKE);
        }

        /*  The dictionary is loaded in the background thread. Once the loading is
            complete the dictionary will be attached to the current session. The
            attachment happens in the worker thread context, thus the direct
            connection for the signal handler and the mutex protecting the
            converted dictionary for concurrent access.

            The loading operation is blocking for the main thread only if the
            user starts handwriting input before the operation is complete.
        */
        {
            QMutexLocker dictionaryGuard(&dictionaryLock);

            // Select recognition mode
            // Note: MCR mode is preferred in all cases, since it eliminates the need
            //       for recognition timer, thus provides better user experience.
            sessionSettings.recognitionMode = inputMethodHints.testFlag(Qt::ImhHiddenText) ? scrMode : mcrMode;

            // Detach previous dictionary if the language is being changed
            // or the recognizer mode is single-character mode
            if ((languageChanged || inputMethodHints.testFlag(Qt::ImhNoPredictiveText) || sessionSettings.recognitionMode == scrMode) && attachedDictionary) {
                detachDictionary(&attachedDictionary);
            }

            // Check if a dictionary needs to be loaded
            if (languageChanged || !convertedDictionary) {
                destroyConvertedDictionary(&convertedDictionary);
                dictionaryFileName = findDictionary(":/databases/XT9_LDBs/", locale);
                if (!dictionaryFileName.isEmpty()) {
                    if (dictionaryTask.isNull() || dictionaryTask->fileUri != dictionaryFileName) {
                        dictionaryTask.reset(new T9WriteDictionaryTask(dictionaryFileName, memFuncs));
                        q->connect(dictionaryTask.data(), SIGNAL(completed(QString,void*)),
                                   SLOT(dictionaryLoadCompleted(QString,void*)), Qt::DirectConnection);
                        worker->addTask(dictionaryTask);
                    }
                }
            }

            // Attach existing dictionary, if necessary
            if (sessionSettings.recognitionMode == mcrMode && !inputMethodHints.testFlag(Qt::ImhNoPredictiveText) &&
                    convertedDictionary && !attachedDictionary) {
                attachDictionary(convertedDictionary);
                attachedDictionary = convertedDictionary;
            }
        }

        // Change session settings
        sessionSettings.charSet.pSymbolCategories = symbolCategories.data();
        sessionSettings.charSet.nSymbolCategories = symbolCategories.size();
        sessionSettings.charSet.pLanguages = languageCategories.data();
        sessionSettings.charSet.nLanguages = languageCategories.size();
        DECUMA_STATUS status = decumaChangeSessionSettings(decumaSession, &sessionSettings);
        Q_ASSERT(status == decumaNoError);

        caseFormatter.preferLowercase = inputMethodHints.testFlag(Qt::ImhPreferLowercase);

        return status == decumaNoError;
    }

    DECUMA_UINT32 mapToDecumaLanguage(const QLocale &locale)
    {
        static const QLocale::Language maxLanguage = QLocale::Vietnamese;
        static const DECUMA_UINT32 languageMap[maxLanguage + 1] = {
            DECUMA_LANG_GSMDEFAULT,         // AnyLanguage = 0
            DECUMA_LANG_GSMDEFAULT,         // C = 1
            DECUMA_LANG_GSMDEFAULT,         // Abkhazian = 2
            DECUMA_LANG_GSMDEFAULT,         // Oromo = 3
            DECUMA_LANG_GSMDEFAULT,         // Afar = 4
            DECUMA_LANG_AF,                 // Afrikaans = 5
            DECUMA_LANG_SQ,                 // Albanian = 6
            DECUMA_LANG_GSMDEFAULT,         // Amharic = 7
            DECUMA_LANG_AR,                 // Arabic = 8
            DECUMA_LANG_GSMDEFAULT,         // Armenian = 9
            DECUMA_LANG_GSMDEFAULT,         // Assamese = 10
            DECUMA_LANG_GSMDEFAULT,         // Aymara = 11
            DECUMA_LANG_AZ,                 // Azerbaijani = 12
            DECUMA_LANG_GSMDEFAULT,         // Bashkir = 13
            DECUMA_LANG_EU,                 // Basque = 14
            DECUMA_LANG_BN,                 // Bengali = 15
            DECUMA_LANG_GSMDEFAULT,         // Dzongkha = 16
            DECUMA_LANG_GSMDEFAULT,         // Bihari = 17
            DECUMA_LANG_GSMDEFAULT,         // Bislama = 18
            DECUMA_LANG_GSMDEFAULT,         // Breton = 19
            DECUMA_LANG_BG,                 // Bulgarian = 20
            DECUMA_LANG_GSMDEFAULT,         // Burmese = 21
            DECUMA_LANG_BE,                 // Belarusian = 22
            DECUMA_LANG_KM,                 // Khmer = 23
            DECUMA_LANG_CA,                 // Catalan = 24
            DECUMA_LANG_PRC,                // Chinese = 25
            DECUMA_LANG_GSMDEFAULT,         // Corsican = 26
            DECUMA_LANG_HR,                 // Croatian = 27
            DECUMA_LANG_CS,                 // Czech = 28
            DECUMA_LANG_DA,                 // Danish = 29
            DECUMA_LANG_NL,                 // Dutch = 30
            DECUMA_LANG_EN,                 // English = 31
            DECUMA_LANG_GSMDEFAULT,         // Esperanto = 32
            DECUMA_LANG_ET,                 // Estonian = 33
            DECUMA_LANG_GSMDEFAULT,         // Faroese = 34
            DECUMA_LANG_GSMDEFAULT,         // Fijian = 35
            DECUMA_LANG_FI,                 // Finnish = 36
            DECUMA_LANG_FR,                 // French = 37
            DECUMA_LANG_GSMDEFAULT,         // WesternFrisian = 38
            DECUMA_LANG_GSMDEFAULT,         // Gaelic = 39
            DECUMA_LANG_GL,                 // Galician = 40
            DECUMA_LANG_GSMDEFAULT,         // Georgian = 41
            DECUMA_LANG_DE,                 // German = 42
            DECUMA_LANG_EL,                 // Greek = 43
            DECUMA_LANG_GSMDEFAULT,         // Greenlandic = 44
            DECUMA_LANG_GSMDEFAULT,         // Guarani = 45
            DECUMA_LANG_GU,                 // Gujarati = 46
            DECUMA_LANG_HA,                 // Hausa = 47
            DECUMA_LANG_IW,                 // Hebrew = 48
            DECUMA_LANG_HI,                 // Hindi = 49
            DECUMA_LANG_HU,                 // Hungarian = 50
            DECUMA_LANG_IS,                 // Icelandic = 51
            DECUMA_LANG_IN,                 // Indonesian = 52
            DECUMA_LANG_GSMDEFAULT,         // Interlingua = 53
            DECUMA_LANG_GSMDEFAULT,         // Interlingue = 54
            DECUMA_LANG_GSMDEFAULT,         // Inuktitut = 55
            DECUMA_LANG_GSMDEFAULT,         // Inupiak = 56
            DECUMA_LANG_GSMDEFAULT,         // Irish = 57
            DECUMA_LANG_IT,                 // Italian = 58
            DECUMA_LANG_JP,                 // Japanese = 59
            DECUMA_LANG_GSMDEFAULT,         // Javanese = 60
            DECUMA_LANG_KN,                 // Kannada = 61
            DECUMA_LANG_GSMDEFAULT,         // Kashmiri = 62
            DECUMA_LANG_KK,                 // Kazakh = 63
            DECUMA_LANG_GSMDEFAULT,         // Kinyarwanda = 64
            DECUMA_LANG_KY,                 // Kirghiz = 65
            DECUMA_LANG_KO,                 // Korean = 66
            DECUMA_LANG_GSMDEFAULT,         // Kurdish = 67
            DECUMA_LANG_GSMDEFAULT,         // Rundi = 68
            DECUMA_LANG_GSMDEFAULT,         // Lao = 69
            DECUMA_LANG_GSMDEFAULT,         // Latin = 70
            DECUMA_LANG_LV,                 // Latvian = 71
            DECUMA_LANG_GSMDEFAULT,         // Lingala = 72
            DECUMA_LANG_LT,                 // Lithuanian = 73
            DECUMA_LANG_MK,                 // Macedonian = 74
            DECUMA_LANG_GSMDEFAULT,         // Malagasy = 75
            DECUMA_LANG_MS,                 // Malay = 76
            DECUMA_LANG_ML,                 // Malayalam = 77
            DECUMA_LANG_GSMDEFAULT,         // Maltese = 78
            DECUMA_LANG_GSMDEFAULT,         // Maori = 79
            DECUMA_LANG_MR,                 // Marathi = 80
            DECUMA_LANG_GSMDEFAULT,         // Marshallese = 81
            DECUMA_LANG_MN,                 // Mongolian = 82
            DECUMA_LANG_GSMDEFAULT,         // NauruLanguage = 83
            DECUMA_LANG_GSMDEFAULT,         // Nepali = 84
            DECUMA_LANG_NO,                 // NorwegianBokmal = 85
            DECUMA_LANG_GSMDEFAULT,         // Occitan = 86
            DECUMA_LANG_GSMDEFAULT,         // Oriya = 87
            DECUMA_LANG_GSMDEFAULT,         // Pashto = 88
            DECUMA_LANG_FA,                 // Persian = 89
            DECUMA_LANG_PL,                 // Polish = 90
            DECUMA_LANG_PT,                 // Portuguese = 91
            DECUMA_LANG_PA,                 // Punjabi = 92
            DECUMA_LANG_GSMDEFAULT,         // Quechua = 93
            DECUMA_LANG_GSMDEFAULT,         // Romansh = 94
            DECUMA_LANG_RO,                 // Romanian = 95
            DECUMA_LANG_RU,                 // Russian = 96
            DECUMA_LANG_GSMDEFAULT,         // Samoan = 97
            DECUMA_LANG_GSMDEFAULT,         // Sango = 98
            DECUMA_LANG_GSMDEFAULT,         // Sanskrit = 99
            DECUMA_LANG_SR,                 // Serbian = 100
            DECUMA_LANG_GSMDEFAULT,         // Ossetic = 101
            DECUMA_LANG_ST,                 // SouthernSotho = 102
            DECUMA_LANG_GSMDEFAULT,         // Tswana = 103
            DECUMA_LANG_GSMDEFAULT,         // Shona = 104
            DECUMA_LANG_GSMDEFAULT,         // Sindhi = 105
            DECUMA_LANG_SI,                 // Sinhala = 106
            DECUMA_LANG_GSMDEFAULT,         // Swati = 107
            DECUMA_LANG_SK,                 // Slovak = 108
            DECUMA_LANG_SL,                 // Slovenian = 109
            DECUMA_LANG_GSMDEFAULT,         // Somali = 110
            DECUMA_LANG_ES,                 // Spanish = 111
            DECUMA_LANG_GSMDEFAULT,         // Sundanese = 112
            DECUMA_LANG_SW,                 // Swahili = 113
            DECUMA_LANG_SV,                 // Swedish = 114
            DECUMA_LANG_GSMDEFAULT,         // Sardinian = 115
            DECUMA_LANG_TG,                 // Tajik = 116
            DECUMA_LANG_TA,                 // Tamil = 117
            DECUMA_LANG_GSMDEFAULT,         // Tatar = 118
            DECUMA_LANG_TE,                 // Telugu = 119
            DECUMA_LANG_TH,                 // Thai = 120
            DECUMA_LANG_GSMDEFAULT,         // Tibetan = 121
            DECUMA_LANG_GSMDEFAULT,         // Tigrinya = 122
            DECUMA_LANG_GSMDEFAULT,         // Tongan = 123
            DECUMA_LANG_GSMDEFAULT,         // Tsonga = 124
            DECUMA_LANG_TR,                 // Turkish = 125
            DECUMA_LANG_GSMDEFAULT,         // Turkmen = 126
            DECUMA_LANG_GSMDEFAULT,         // Tahitian = 127
            DECUMA_LANG_GSMDEFAULT,         // Uighur = 128
            DECUMA_LANG_UK,                 // Ukrainian = 129
            DECUMA_LANG_UR,                 // Urdu = 130
            DECUMA_LANG_UZ,                 // Uzbek = 131
            DECUMA_LANG_VI                  // Vietnamese = 132
        };

        if (locale.language() > maxLanguage)
            return DECUMA_LANG_GSMDEFAULT;

        DECUMA_UINT32 language = languageMap[locale.language()];

        return language;
    }

    Trace *traceBegin(int traceId, InputEngine::PatternRecognitionMode patternRecognitionMode,
                      const QVariantMap &traceCaptureDeviceInfo, const QVariantMap &traceScreenInfo)
    {
        Q_UNUSED(traceId)
        Q_UNUSED(patternRecognitionMode)
        Q_UNUSED(traceCaptureDeviceInfo)
        Q_UNUSED(traceScreenInfo)

        stopResultTimer();

        // Dictionary must be completed before the arc addition can begin
        if (dictionaryTask) {
            dictionaryTask->wait();
            dictionaryTask.reset();
        }

        // Cancel the current recognition task
        worker->removeAllTasks<T9WriteRecognitionResultsTask>();
        if (recognitionTask) {
            recognitionTask->cancelRecognition();
            recognitionTask->wait();
            worker->removeTask(recognitionTask);
            recognitionTask.reset();
        }

        const int dpi = traceCaptureDeviceInfo.value("dpi", 96).toInt();
        static const int INSTANT_GESTURE_WIDTH_THRESHOLD_MM = 25;
        static const int INSTANT_GESTURE_HEIGHT_THRESHOLD_MM = 25;
        instantGestureSettings.widthThreshold = INSTANT_GESTURE_WIDTH_THRESHOLD_MM / 25.4 * dpi;
        instantGestureSettings.heightThreshold = INSTANT_GESTURE_HEIGHT_THRESHOLD_MM / 25.4 * dpi;

        DECUMA_STATUS status;

        if (!arcAdditionStarted) {
            status = decumaBeginArcAddition(decumaSession);
            Q_ASSERT(status == decumaNoError);
            arcAdditionStarted = true;
        }

        DECUMA_UINT32 arcID = (DECUMA_UINT32)traceId;
        status = decumaStartNewArc(decumaSession, arcID);
        Q_ASSERT(status == decumaNoError);
        if (status != decumaNoError) {
            decumaEndArcAddition(decumaSession);
            arcAdditionStarted = false;
            return NULL;
        }

        Trace *trace = new Trace();
        traceList.append(trace);

        return trace;
    }

    void traceEnd(Trace *trace)
    {
        if (trace->isCanceled()) {
            decumaCancelArc(decumaSession, trace->traceId());
            traceList.removeOne(trace);
            delete trace;
        } else {
            addPointsToTraceGroup(trace);
        }
        if (!traceList.isEmpty()) {
            Q_ASSERT(arcAdditionStarted);
            if (countActiveTraces() == 0)
                restartRecognition();
        } else if (arcAdditionStarted) {
            decumaEndArcAddition(decumaSession);
            arcAdditionStarted = false;
        }
    }

    int countActiveTraces() const
    {
        int count = 0;
        for (Trace *trace : qAsConst(traceList)) {
            if (!trace->isFinal())
                count++;
        }
        return count;
    }

    void clearTraces()
    {
        qDeleteAll(traceList);
        traceList.clear();
    }

    void addPointsToTraceGroup(Trace *trace)
    {
        Q_ASSERT(decumaSession != 0);

        const QVariantList points = trace->points();
        DECUMA_UINT32 arcID = (DECUMA_UINT32)trace->traceId();
        DECUMA_STATUS status;

        for (const QVariant &p : points) {
            const QPoint pt(p.toPointF().toPoint());
            status = decumaAddPoint(decumaSession, (DECUMA_COORD)pt.x(),(DECUMA_COORD)pt.y(), arcID);
            Q_ASSERT(status == decumaNoError);
        }

        status = decumaCommitArc(decumaSession, arcID);
        Q_ASSERT(status == decumaNoError);
    }

    void noteSelected(int index)
    {
        if (wordCandidatesHwrResultIndex.isEmpty())
            return;

        VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethodPrivate::noteSelected():" << index;
        Q_ASSERT(index >= 0 && index < wordCandidatesHwrResultIndex.length());
        int resultIndex = wordCandidatesHwrResultIndex[index];
        DECUMA_STATUS status = decumaNoteSelectedCandidate(decumaSession, resultIndex);
        Q_UNUSED(status)
        Q_ASSERT(status == decumaNoError);
    }

    void restartRecognition()
    {
        VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethodPrivate::restartRecognition()";

        Q_Q(T9WriteInputMethod);

        QSharedPointer<T9WriteRecognitionResult> recognitionResult(new T9WriteRecognitionResult(++resultId, 9, 64));
        recognitionTask.reset(new T9WriteRecognitionTask(recognitionResult, instantGestureSettings,
                                                         attachedDictionary ? boostDictWords : noBoost,
                                                         stringStart));
        worker->addTask(recognitionTask);

        QSharedPointer<T9WriteRecognitionResultsTask> resultsTask(new T9WriteRecognitionResultsTask(recognitionResult));
        q->connect(resultsTask.data(), SIGNAL(resultsAvailable(const QVariantList &)), SLOT(resultsAvailable(const QVariantList &)));
        worker->addTask(resultsTask);

        resetResultTimer();
    }

    bool finishRecognition(bool emitSelectionListChanged = true)
    {
        VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethodPrivate::finishRecognition()";

        bool result = !traceList.isEmpty();

        Q_ASSERT(decumaSession != 0);

        stopResultTimer();

        clearTraces();

        worker->removeAllTasks<T9WriteRecognitionResultsTask>();
        if (recognitionTask) {
            recognitionTask->cancelRecognition();
            recognitionTask->wait();
            worker->removeTask(recognitionTask);
            recognitionTask.reset();
            result = true;
        }

        if (arcAdditionStarted) {
            decumaEndArcAddition(decumaSession);
            arcAdditionStarted = false;
        }

        if (!wordCandidates.isEmpty()) {
            wordCandidates.clear();
            wordCandidatesHwrResultIndex.clear();
            activeWordIndex = -1;
            if (emitSelectionListChanged) {
                Q_Q(T9WriteInputMethod);
                emit q->selectionListChanged(SelectionListModel::WordCandidateList);
                emit q->selectionListActiveItemChanged(SelectionListModel::WordCandidateList, activeWordIndex);
            }
            result = true;
        }

        stringStart.clear();
        scrResult.clear();
        caseFormatter.clear();

        return result;
    }

    bool select(int index = -1)
    {
        if (sessionSettings.recognitionMode == mcrMode && wordCandidates.isEmpty()) {
            finishRecognition();
            return false;
        }
        if (sessionSettings.recognitionMode == scrMode && scrResult.isEmpty()) {
            finishRecognition();
            return false;
        }

        VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethodPrivate::select():" << index;

        Q_Q(T9WriteInputMethod);
        if (sessionSettings.recognitionMode == mcrMode) {
            index = index >= 0 ? index : activeWordIndex;
            noteSelected(index);
            QString finalWord = wordCandidates.at(index);
            finishRecognition();
            q->inputContext()->commit(finalWord);
        } else if (sessionSettings.recognitionMode == scrMode) {
            QString finalWord = scrResult;
            finishRecognition();
            q->inputContext()->inputEngine()->virtualKeyClick((Qt::Key)finalWord.at(0).unicode(), finalWord, Qt::NoModifier);
        }

        return true;
    }

    void resetResultTimer()
    {
        VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethodPrivate::resetResultTimer()";
        Q_Q(T9WriteInputMethod);
        stopResultTimer();
        resultTimer = q->startTimer(500);
    }

    void stopResultTimer()
    {
        if (resultTimer) {
            VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethodPrivate::stopResultTimer()";
            Q_Q(T9WriteInputMethod);
            q->killTimer(resultTimer);
            resultTimer = 0;
        }
    }

    void resultsAvailable(const QVariantList &resultList)
    {
        if (!resultList.isEmpty()) {
            if (recognitionTask && recognitionTask->resultId() == resultList.first().toMap()["resultId"].toInt())
                processResult(resultList);
        }
    }

    void processResult(const QVariantList &resultList)
    {
        if (resultList.isEmpty())
            return;

        if (resultList.first().toMap()["resultId"] != resultId)
            return;

        VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethodPrivate::processResult()";

        Q_Q(T9WriteInputMethod);
        InputContext *ic = q->inputContext();
        if (!ic)
            return;

        QStringList newWordCandidates;
        QList<int> newWordCandidatesHwrResultIndex;
        QString resultString;
        QString gesture;
        QVariantList symbolStrokes;
        for (int i = 0; i < resultList.size(); i++) {
            QVariantMap result = resultList.at(i).toMap();
            QString resultChars = result["chars"].toString();
            if (i == 0)
                caseFormatter.ensureLength(resultChars.length(), textCase);
            if (!resultChars.isEmpty()) {
                resultChars = caseFormatter.formatString(resultChars);
                if (sessionSettings.recognitionMode == mcrMode) {
                    newWordCandidates.append(resultChars);
                    newWordCandidatesHwrResultIndex.append(i);
                }
            }
            if (i == 0) {
                resultString = resultChars;
                if (result.contains("gesture"))
                    gesture = result["gesture"].toString();
                if (sessionSettings.recognitionMode == mcrMode && result.contains("symbolStrokes"))
                    symbolStrokes = result["symbolStrokes"].toList();
                if (sessionSettings.recognitionMode == scrMode)
                    break;
            }
        }

        bool wordCandidatesChanged = wordCandidates != newWordCandidates;

        // Delete trace history
        if (sessionSettings.recognitionMode == mcrMode && !symbolStrokes.isEmpty()) {
            int activeTraces = symbolStrokes.at(symbolStrokes.count() - 1).toInt();
            if (symbolStrokes.count() > 1)
                activeTraces += symbolStrokes.at(symbolStrokes.count() - 2).toInt();
            while (activeTraces < traceList.count())
                delete traceList.takeFirst();
        }

        // Look for a gesture at the end of first result
        if (!gesture.isEmpty()) {

            DECUMA_UNICODE gestureSymbol = gesture.at(0).unicode();
            switch (gestureSymbol) {
            case '\b':
                ic->inputEngine()->virtualKeyClick(Qt::Key_Backspace, QString(), Qt::NoModifier);
                break;

            case '\r':
                ic->inputEngine()->virtualKeyClick(Qt::Key_Return, QLatin1String("\n"), Qt::NoModifier);
                break;

            case ' ':
                ic->inputEngine()->virtualKeyClick(Qt::Key_Space, QLatin1String(" "), Qt::NoModifier);
                break;

            default:
                finishRecognition();
                ic->commit(ic->preeditText());
                break;
            }

            return;
        }

        if (sessionSettings.recognitionMode == mcrMode) {
            ignoreUpdate = true;
            ic->setPreeditText(resultString);
            ignoreUpdate = false;
        } else if (sessionSettings.recognitionMode == scrMode) {
            if (resultTimer == 0 && !resultString.isEmpty())
                ic->inputEngine()->virtualKeyClick((Qt::Key)resultString.at(0).unicode(), resultString, Qt::NoModifier);
            else
                scrResult = resultString;
        }

        if (wordCandidatesChanged) {
            wordCandidates = newWordCandidates;
            wordCandidatesHwrResultIndex = newWordCandidatesHwrResultIndex;
            activeWordIndex = wordCandidates.isEmpty() ? -1 : 0;
            emit q->selectionListChanged(SelectionListModel::WordCandidateList);
            emit q->selectionListActiveItemChanged(SelectionListModel::WordCandidateList, activeWordIndex);
        }
    }

    bool isValidInputChar(const QChar &c) const
    {
        if (c.isLetterOrNumber())
            return true;
        if (isJoiner(c))
            return true;
        return false;
    }

    bool isJoiner(const QChar &c) const
    {
        if (c.isPunct() || c.isSymbol()) {
            Q_Q(const T9WriteInputMethod);
            InputContext *ic = q->inputContext();
            if (ic) {
                Qt::InputMethodHints inputMethodHints = ic->inputMethodHints();
                if (inputMethodHints.testFlag(Qt::ImhUrlCharactersOnly) || inputMethodHints.testFlag(Qt::ImhEmailCharactersOnly))
                    return QString(QStringLiteral(":/?#[]@!$&'()*+,;=-_.%")).contains(c);
            }
            ushort unicode = c.unicode();
            if (unicode == Qt::Key_Apostrophe || unicode == Qt::Key_Minus)
                return true;
        }
        return false;
    }

    T9WriteInputMethod *q_ptr;
    static const DECUMA_MEM_FUNCTIONS memFuncs;
    DECUMA_SESSION_SETTINGS sessionSettings;
    DECUMA_INSTANT_GESTURE_SETTINGS instantGestureSettings;
    QFile hwrDbFile;
    QVector<DECUMA_UINT32> languageCategories;
    QVector<DECUMA_UINT32> symbolCategories;
    QScopedPointer<T9WriteWorker> worker;
    QList<Trace *> traceList;
    QMutex dictionaryLock;
    QString dictionaryFileName;
    void *convertedDictionary;
    void *attachedDictionary;
    QSharedPointer<T9WriteDictionaryTask> dictionaryTask;
    QSharedPointer<T9WriteRecognitionTask> recognitionTask;
    int resultId;
    int resultTimer;
    QByteArray session;
    DECUMA_SESSION *decumaSession;
    QStringList wordCandidates;
    QList<int> wordCandidatesHwrResultIndex;
    QString stringStart;
    QString scrResult;
    int activeWordIndex;
    bool arcAdditionStarted;
    bool ignoreUpdate;
    InputEngine::TextCase textCase;
    T9WriteCaseFormatter caseFormatter;
};

const DECUMA_MEM_FUNCTIONS T9WriteInputMethodPrivate::memFuncs = {
    T9WriteInputMethodPrivate::decumaMalloc,
    T9WriteInputMethodPrivate::decumaCalloc,
    T9WriteInputMethodPrivate::decumaFree,
    NULL
};

/*!
    \class QtVirtualKeyboard::T9WriteInputMethod
    \internal
*/

T9WriteInputMethod::T9WriteInputMethod(QObject *parent) :
    AbstractInputMethod(*new T9WriteInputMethodPrivate(this), parent)
{
    Q_D(T9WriteInputMethod);
    d->initEngine();
}

T9WriteInputMethod::~T9WriteInputMethod()
{
    Q_D(T9WriteInputMethod);
    d->exitEngine();
}

QList<InputEngine::InputMode> T9WriteInputMethod::inputModes(const QString &locale)
{
    Q_UNUSED(locale)
    return QList<InputEngine::InputMode>()
            << InputEngine::Latin
            << InputEngine::Numeric
            << InputEngine::Dialable;
}

bool T9WriteInputMethod::setInputMode(const QString &locale, InputEngine::InputMode inputMode)
{
    Q_D(T9WriteInputMethod);
    d->select();
    return d->setInputMode(locale, inputMode);
}

bool T9WriteInputMethod::setTextCase(InputEngine::TextCase textCase)
{
    Q_D(T9WriteInputMethod);
    d->textCase = textCase;
    return true;
}

bool T9WriteInputMethod::keyEvent(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(text)
    Q_UNUSED(modifiers)
    Q_D(T9WriteInputMethod);
    switch (key) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Tab:
    case Qt::Key_Space:
        d->select();
        update();
        break;

    case Qt::Key_Backspace:
        {
            InputContext *ic = inputContext();
            QString preeditText = ic->preeditText();
            if (preeditText.length() > 1) {
                preeditText.chop(1);
                ic->setPreeditText(preeditText);
                d->caseFormatter.ensureLength(preeditText.length(), d->textCase);
                T9WriteCaseFormatter caseFormatter(d->caseFormatter);
                d->finishRecognition(false);
                d->caseFormatter = caseFormatter;
                d->stringStart = preeditText;
                d->wordCandidates.append(preeditText);
                d->activeWordIndex = 0;
                emit selectionListChanged(SelectionListModel::WordCandidateList);
                emit selectionListActiveItemChanged(SelectionListModel::WordCandidateList, d->activeWordIndex);
                return true;
            } else {
                bool result = !preeditText.isEmpty();
                if (result)
                    ic->clear();
                else
                    result = !d->scrResult.isEmpty();
                d->finishRecognition();
                return result;
            }
            break;
        }

    default:
        if (d->sessionSettings.recognitionMode == mcrMode && text.length() > 0) {
            InputContext *ic = inputContext();
            QString preeditText = ic->preeditText();
            QChar c = text.at(0);
            bool addToWord = d->isValidInputChar(c) && (!preeditText.isEmpty() || !d->isJoiner(c));
            if (addToWord) {
                preeditText.append(text);
                ic->setPreeditText(preeditText);
                d->caseFormatter.ensureLength(preeditText.length(), d->textCase);
                T9WriteCaseFormatter caseFormatter(d->caseFormatter);
                d->finishRecognition(false);
                d->caseFormatter = caseFormatter;
                d->stringStart = preeditText;
                d->wordCandidates.append(preeditText);
                d->activeWordIndex = 0;
                emit selectionListChanged(SelectionListModel::WordCandidateList);
                emit selectionListActiveItemChanged(SelectionListModel::WordCandidateList, d->activeWordIndex);
                return true;
            } else {
                ic->clear();
                d->finishRecognition();
            }
            break;
        } else if (d->sessionSettings.recognitionMode == scrMode) {
            d->finishRecognition();
        }
    }
    return false;
}

void T9WriteInputMethod::reset()
{
    Q_D(T9WriteInputMethod);
    d->finishRecognition();
    d->setInputMode(QLocale(inputContext()->locale()), inputEngine()->inputMode());
}

void T9WriteInputMethod::update()
{
    Q_D(T9WriteInputMethod);
    if (d->ignoreUpdate)
        return;
    d->select();
}

QList<SelectionListModel::Type> T9WriteInputMethod::selectionLists()
{
    return QList<SelectionListModel::Type>() << SelectionListModel::WordCandidateList;
}

int T9WriteInputMethod::selectionListItemCount(SelectionListModel::Type type)
{
    Q_UNUSED(type)
    Q_D(T9WriteInputMethod);
    return d->wordCandidates.count();
}

QVariant T9WriteInputMethod::selectionListData(SelectionListModel::Type type, int index, int role)
{
    QVariant result;
    Q_UNUSED(type)
    Q_D(T9WriteInputMethod);
    switch (role) {
    case SelectionListModel::DisplayRole:
        result = QVariant(d->wordCandidates.at(index));
        break;
    case SelectionListModel::WordCompletionLengthRole:
        result.setValue(0);
        break;
    default:
        result = AbstractInputMethod::selectionListData(type, index, role);
        break;
    }
    return result;
}

void T9WriteInputMethod::selectionListItemSelected(SelectionListModel::Type type, int index)
{
    Q_UNUSED(type)
    Q_D(T9WriteInputMethod);
    d->select(index);
}

QList<InputEngine::PatternRecognitionMode> T9WriteInputMethod::patternRecognitionModes() const
{
    return QList<InputEngine::PatternRecognitionMode>()
            << InputEngine::HandwritingRecoginition;
}

Trace *T9WriteInputMethod::traceBegin(int traceId, InputEngine::PatternRecognitionMode patternRecognitionMode,
                                      const QVariantMap &traceCaptureDeviceInfo, const QVariantMap &traceScreenInfo)
{
    Q_D(T9WriteInputMethod);
    return d->traceBegin(traceId, patternRecognitionMode, traceCaptureDeviceInfo, traceScreenInfo);
}

bool T9WriteInputMethod::traceEnd(Trace *trace)
{
    Q_D(T9WriteInputMethod);
    d->traceEnd(trace);
    return true;
}

bool T9WriteInputMethod::reselect(int cursorPosition, const InputEngine::ReselectFlags &reselectFlags)
{
    Q_D(T9WriteInputMethod);

    if (d->sessionSettings.recognitionMode == scrMode)
        return false;

    InputContext *ic = inputContext();
    if (!ic)
        return false;

    const QString surroundingText = ic->surroundingText();
    int replaceFrom = 0;

    if (reselectFlags.testFlag(InputEngine::WordBeforeCursor)) {
        for (int i = cursorPosition - 1; i >= 0; --i) {
            QChar c = surroundingText.at(i);
            if (!d->isValidInputChar(c))
                break;
            d->stringStart.insert(0, c);
            --replaceFrom;
        }

        while (replaceFrom < 0 && d->isJoiner(d->stringStart.at(0))) {
            d->stringStart.remove(0, 1);
            ++replaceFrom;
        }
    }

    if (reselectFlags.testFlag(InputEngine::WordAtCursor) && replaceFrom == 0) {
        d->stringStart.clear();
        return false;
    }

    if (reselectFlags.testFlag(InputEngine::WordAfterCursor)) {
        for (int i = cursorPosition; i < surroundingText.length(); ++i) {
            QChar c = surroundingText.at(i);
            if (!d->isValidInputChar(c))
                break;
            d->stringStart.append(c);
        }

        while (replaceFrom > -d->stringStart.length()) {
            int lastPos = d->stringStart.length() - 1;
            if (!d->isJoiner(d->stringStart.at(lastPos)))
                break;
            d->stringStart.remove(lastPos, 1);
        }
    }

    if (d->stringStart.isEmpty())
        return false;

    if (reselectFlags.testFlag(InputEngine::WordAtCursor) && replaceFrom == -d->stringStart.length()) {
        d->stringStart.clear();
        return false;
    }

    if (d->isJoiner(d->stringStart.at(0))) {
        d->stringStart.clear();
        return false;
    }

    if (d->isJoiner(d->stringStart.at(d->stringStart.length() - 1))) {
        d->stringStart.clear();
        return false;
    }

    ic->setPreeditText(d->stringStart, QList<QInputMethodEvent::Attribute>(), replaceFrom, d->stringStart.length());
    for (int i = 0; i < d->stringStart.length(); ++i)
        d->caseFormatter.ensureLength(i + 1, d->stringStart.at(i).isUpper() ? InputEngine::Upper : InputEngine::Lower);
    d->wordCandidates.append(d->stringStart);
    d->activeWordIndex = 0;
    emit selectionListChanged(SelectionListModel::WordCandidateList);
    emit selectionListActiveItemChanged(SelectionListModel::WordCandidateList, d->activeWordIndex);

    return true;
}

void T9WriteInputMethod::timerEvent(QTimerEvent *timerEvent)
{
    Q_D(T9WriteInputMethod);
    int timerId = timerEvent->timerId();
    VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethod::timerEvent():" << timerId;
    if (timerId == d->resultTimer) {
        if (d->sessionSettings.recognitionMode == mcrMode) {
            d->stopResultTimer();
            d->clearTraces();
        } else if (d->sessionSettings.recognitionMode == scrMode) {
            d->select();
        }
    }
}

void T9WriteInputMethod::dictionaryLoadCompleted(const QString &fileUri, void *dictionary)
{
    Q_D(T9WriteInputMethod);
    // Note: This method is called in worker thread context
    QMutexLocker dictionaryGuard(&d->dictionaryLock);

    VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethod::dictionaryLoadCompleted():" << fileUri << dictionary;

    if (!dictionary)
        return;

    InputContext *ic = inputContext();
    if (ic && fileUri == d->dictionaryFileName) {
        d->convertedDictionary = dictionary;
        if (d->sessionSettings.recognitionMode == mcrMode &&
                !ic->inputMethodHints().testFlag(Qt::ImhNoPredictiveText) &&
                !d->attachedDictionary) {
            d->attachDictionary(d->convertedDictionary);
            d->attachedDictionary = d->convertedDictionary;
        }
    } else {
        d->destroyConvertedDictionary(&dictionary);
    }
}

void T9WriteInputMethod::resultsAvailable(const QVariantList &resultList)
{
#ifdef QT_VIRTUALKEYBOARD_DEBUG
    {
        VIRTUALKEYBOARD_DEBUG() << "T9WriteInputMethod::resultsAvailable():";
        for (int i = 0; i < resultList.size(); i++) {
            QVariantMap result = resultList.at(i).toMap();
            QString resultPrint = QString("%1: ").arg(i + 1);
            QString resultChars = result.value("chars").toString();
            if (!resultChars.isEmpty())
                resultPrint.append(resultChars);
            if (result.contains("gesture")) {
                if (!resultChars.isEmpty())
                    resultPrint.append(", ");
                resultPrint.append("gesture = 0x");
                resultPrint.append(result["gesture"].toString().toUtf8().toHex());
            }
            VIRTUALKEYBOARD_DEBUG() << resultPrint.toUtf8().constData();
        }
    }
#endif
    Q_D(T9WriteInputMethod);
    d->resultsAvailable(resultList);
}

} // namespace QtVirtualKeyboard
