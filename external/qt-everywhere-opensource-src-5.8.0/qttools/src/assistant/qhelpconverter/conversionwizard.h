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

#ifndef CONVERSIONWIZARD_H
#define CONVERSIONWIZARD_H

#include <QtWidgets/QWizard>
#include "adpreader.h"

QT_BEGIN_NAMESPACE

class InputPage;
class GeneralPage;
class FilterPage;
class IdentifierPage;
class PathPage;
class FilesPage;
class OutputPage;
class FinishPage;
class HelpWindow;

class ConversionWizard : public QWizard
{
    Q_OBJECT

public:
    ConversionWizard();
    void setAdpFileName(const QString &fileName);

private slots:
    void pageChanged(int id);
    void showHelp(bool toggle);
    void convert();

private:
    enum Pages {Input_Page, General_Page, Filter_Page,
        Identifier_Page, Path_Page, Files_Page, Output_Page,
        Finish_Page};
    void initializePage(int id);
    QStringList getUnreferencedFiles(const QStringList &files);
    bool eventFilter(QObject *obj, QEvent *e);

    AdpReader m_adpReader;
    InputPage *m_inputPage;
    GeneralPage *m_generalPage;
    FilterPage *m_filterPage;
    IdentifierPage *m_identifierPage;
    PathPage *m_pathPage;
    FilesPage *m_filesPage;
    OutputPage *m_outputPage;
    FinishPage *m_finishPage;
    QStringList m_files;
    HelpWindow *m_helpWindow;
};

QT_END_NAMESPACE

#endif
