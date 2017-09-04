/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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
#include "tracer.h"

#include <QtCore/QFileInfo>
#include <QtCore/QStringBuilder>
#include <QtWidgets/QMessageBox>

#include "cmdlineparser.h"

QT_BEGIN_NAMESPACE

static const char helpMessage[] = QT_TRANSLATE_NOOP("CmdLineParser",
        "Usage: assistant [Options]\n\n"
        "-collectionFile file       Uses the specified collection\n"
        "                           file instead of the default one\n"
        "-showUrl url               Shows the document with the\n"
        "                           url.\n"
        "-enableRemoteControl       Enables Assistant to be\n"
        "                           remotely controlled.\n"
        "-show widget               Shows the specified dockwidget\n"
        "                           which can be \"contents\", \"index\",\n"
        "                           \"bookmarks\" or \"search\".\n"
        "-activate widget           Activates the specified dockwidget\n"
        "                           which can be \"contents\", \"index\",\n"
        "                           \"bookmarks\" or \"search\".\n"
        "-hide widget               Hides the specified dockwidget\n"
        "                           which can be \"contents\", \"index\"\n"
        "                           \"bookmarks\" or \"search\".\n"
        "-register helpFile         Registers the specified help file\n"
        "                           (.qch) in the given collection\n"
        "                           file.\n"
        "-unregister helpFile       Unregisters the specified help file\n"
        "                           (.qch) from the give collection\n"
        "                           file.\n"
        "-setCurrentFilter filter   Set the filter as the active filter.\n"
        "-remove-search-index       Removes the full text search index.\n"
        "-rebuild-search-index      Obsolete. Use -remove-search-index instead.\n"
        "                           Removes the full text search index.\n"
        "                           It will be rebuilt on next Assistant run.\n"
        "-quiet                     Does not display any error or\n"
        "                           status message.\n"
        "-help                      Displays this help.\n"
        );


CmdLineParser::CmdLineParser(const QStringList &arguments)
    : m_pos(0),
    m_enableRemoteControl(false),
    m_contents(Untouched),
    m_index(Untouched),
    m_bookmarks(Untouched),
    m_search(Untouched),
    m_register(None),
    m_removeSearchIndex(false),
    m_quiet(false)
{
    TRACE_OBJ
    for (int i = 1; i < arguments.count(); ++i) {
        const QString &arg = arguments.at(i);
        if (arg.toLower() == "-quiet")
            m_quiet = true;
        else
            m_arguments.append(arg);
    }
}

CmdLineParser::Result CmdLineParser::parse()
{
    TRACE_OBJ
    bool showHelp = false;

    while (m_error.isEmpty() && hasMoreArgs()) {
        const QString &arg = nextArg().toLower();
        if (arg == QLatin1String("-collectionfile"))
            handleCollectionFileOption();
        else if (arg == QLatin1String("-showurl"))
            handleShowUrlOption();
        else if (arg == QLatin1String("-enableremotecontrol"))
            m_enableRemoteControl = true;
        else if (arg == QLatin1String("-show"))
            handleShowOption();
        else if (arg == QLatin1String("-hide"))
            handleHideOption();
        else if (arg == QLatin1String("-activate"))
            handleActivateOption();
        else if (arg == QLatin1String("-register"))
            handleRegisterOption();
        else if (arg == QLatin1String("-unregister"))
            handleUnregisterOption();
        else if (arg == QLatin1String("-setcurrentfilter"))
            handleSetCurrentFilterOption();
        else if (arg == QLatin1String("-remove-search-index"))
            m_removeSearchIndex = true;
        else if (arg == QLatin1String("-rebuild-search-index"))
            m_removeSearchIndex = true;
        else if (arg == QLatin1String("-help"))
            showHelp = true;
        else
            m_error = tr("Unknown option: %1").arg(arg);
    }

    if (!m_error.isEmpty()) {
        showMessage(m_error + QLatin1String("\n\n\n") + tr(helpMessage), true);
        return Error;
    } else if (showHelp) {
        showMessage(tr(helpMessage), false);
        return Help;
    }
    return Ok;
}

bool CmdLineParser::hasMoreArgs() const
{
    TRACE_OBJ
    return m_pos < m_arguments.count();
}

const QString &CmdLineParser::nextArg()
{
    TRACE_OBJ
    Q_ASSERT(hasMoreArgs());
    return m_arguments.at(m_pos++);
}

void CmdLineParser::handleCollectionFileOption()
{
    TRACE_OBJ
    if (hasMoreArgs()) {
        const QString &fileName = nextArg();
        m_collectionFile = getFileName(fileName);
        if (m_collectionFile.isEmpty())
            m_error = tr("The collection file '%1' does not exist.").
                          arg(fileName);
    } else {
        m_error = tr("Missing collection file.");
    }
}

void CmdLineParser::handleShowUrlOption()
{
    TRACE_OBJ
    if (hasMoreArgs()) {
        const QString &urlString = nextArg();
        QUrl url(urlString);
        if (url.isValid()) {
            m_url = url;
        } else
            m_error = tr("Invalid URL '%1'.").arg(urlString);
    } else {
        m_error = tr("Missing URL.");
    }
}

void CmdLineParser::handleShowOption()
{
    TRACE_OBJ
    handleShowOrHideOrActivateOption(Show);
}

void CmdLineParser::handleHideOption()
{
    TRACE_OBJ
    handleShowOrHideOrActivateOption(Hide);
}

void CmdLineParser::handleActivateOption()
{
    TRACE_OBJ
    handleShowOrHideOrActivateOption(Activate);
}

void CmdLineParser::handleShowOrHideOrActivateOption(ShowState state)
{
    TRACE_OBJ
    if (hasMoreArgs()) {
        const QString &widget = nextArg().toLower();
        if (widget == QLatin1String("contents"))
            m_contents = state;
        else if (widget == QLatin1String("index"))
            m_index = state;
        else if (widget == QLatin1String("bookmarks"))
            m_bookmarks = state;
        else if (widget == QLatin1String("search"))
            m_search = state;
        else
            m_error = tr("Unknown widget: %1").arg(widget);
    } else {
        m_error = tr("Missing widget.");
    }
}

void CmdLineParser::handleRegisterOption()
{
    TRACE_OBJ
    handleRegisterOrUnregisterOption(Register);
}

void CmdLineParser::handleUnregisterOption()
{
    TRACE_OBJ
    handleRegisterOrUnregisterOption(Unregister);
}

void CmdLineParser::handleRegisterOrUnregisterOption(RegisterState state)
{
    TRACE_OBJ
    if (hasMoreArgs()) {
        const QString &fileName = nextArg();
        m_helpFile = getFileName(fileName);
        if (m_helpFile.isEmpty())
            m_error = tr("The Qt help file '%1' does not exist.").arg(fileName);
        else
            m_register = state;
    } else {
        m_error = tr("Missing help file.");
    }
}

void CmdLineParser::handleSetCurrentFilterOption()
{
    TRACE_OBJ
    if (hasMoreArgs())
        m_currentFilter = nextArg();
    else
        m_error = tr("Missing filter argument.");
}

QString CmdLineParser::getFileName(const QString &fileName)
{
    TRACE_OBJ
    QFileInfo fi(fileName);
    if (!fi.exists())
        return QString();
    return fi.absoluteFilePath();
}

void CmdLineParser::showMessage(const QString &msg, bool error)
{
    TRACE_OBJ
    if (m_quiet)
        return;
#ifdef Q_OS_WIN
    QString message = QLatin1String("<pre>") % msg % QLatin1String("</pre>");
    if (error)
        QMessageBox::critical(0, tr("Error"), message);
    else
        QMessageBox::information(0, tr("Notice"), message);
#else
    fprintf(error ? stderr : stdout, "%s\n", qPrintable(msg));
#endif
}

void CmdLineParser::setCollectionFile(const QString &file)
{
    TRACE_OBJ
    m_collectionFile = file;
}

QString CmdLineParser::collectionFile() const
{
    TRACE_OBJ
    return m_collectionFile;
}

bool CmdLineParser::collectionFileGiven() const
{
    TRACE_OBJ
    return m_arguments.contains(QLatin1String("-collectionfile"),
        Qt::CaseInsensitive);
}

QUrl CmdLineParser::url() const
{
    TRACE_OBJ
    return m_url;
}

bool CmdLineParser::enableRemoteControl() const
{
    TRACE_OBJ
    return m_enableRemoteControl;
}

CmdLineParser::ShowState CmdLineParser::contents() const
{
    TRACE_OBJ
    return m_contents;
}

CmdLineParser::ShowState CmdLineParser::index() const
{
    TRACE_OBJ
    return m_index;
}

CmdLineParser::ShowState CmdLineParser::bookmarks() const
{
    TRACE_OBJ
    return m_bookmarks;
}

CmdLineParser::ShowState CmdLineParser::search() const
{
    TRACE_OBJ
    return m_search;
}

QString CmdLineParser::currentFilter() const
{
    TRACE_OBJ
    return m_currentFilter;
}

bool CmdLineParser::removeSearchIndex() const
{
    TRACE_OBJ
    return m_removeSearchIndex;
}

CmdLineParser::RegisterState CmdLineParser::registerRequest() const
{
    TRACE_OBJ
    return m_register;
}

QString CmdLineParser::helpFile() const
{
    TRACE_OBJ
    return m_helpFile;
}

QT_END_NAMESPACE
