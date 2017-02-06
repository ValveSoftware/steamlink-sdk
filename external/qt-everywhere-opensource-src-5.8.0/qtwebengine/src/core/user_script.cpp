/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include "common/user_script_data.h"
#include "extensions/common/url_pattern.h"
#include "user_script.h"
#include "type_conversion.h"

namespace {

// Helper function to parse Greasemonkey headers
bool GetDeclarationValue(const base::StringPiece& line,
                         const base::StringPiece& prefix,
                         std::string* value) {
    base::StringPiece::size_type index = line.find(prefix);
    if (index == base::StringPiece::npos)
        return false;

    std::string temp(line.data() + index + prefix.length(),
                     line.length() - index - prefix.length());

    if (temp.empty() || !base::IsUnicodeWhitespace(temp[0]))
        return false;

    base::TrimWhitespaceASCII(temp, base::TRIM_ALL, value);
        return true;
}

}  // namespace

namespace QtWebEngineCore {

int UserScript::validUserScriptSchemes()
{
    return URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS | URLPattern::SCHEME_FILE;
}

ASSERT_ENUMS_MATCH(UserScript::AfterLoad, UserScriptData::AfterLoad)
ASSERT_ENUMS_MATCH(UserScript::DocumentLoadFinished, UserScriptData::DocumentLoadFinished)
ASSERT_ENUMS_MATCH(UserScript::DocumentElementCreation, UserScriptData::DocumentElementCreation)

UserScript::UserScript()
    : QSharedData()
{
}

UserScript::UserScript(const UserScript &other)
    : QSharedData(other)
{
    if (other.isNull())
        return;
    scriptData.reset(new UserScriptData(*other.scriptData));
    m_name = other.m_name;
}

UserScript::~UserScript()
{
}

UserScript &UserScript::operator=(const UserScript &other)
{
    if (other.isNull()) {
        scriptData.reset();
        m_name = QString();
        return *this;
    }
    scriptData.reset(new UserScriptData(*other.scriptData));
    m_name = other.m_name;
    return *this;
}

QString UserScript::name() const
{
    return m_name;
}

void UserScript::setName(const QString &name)
{
    m_name = name;
    initData();
    scriptData->url = GURL(QStringLiteral("userScript:%1").arg(name).toStdString());
}

QString UserScript::sourceCode() const
{
    if (isNull())
        return QString();
    return toQt(scriptData->source);
}

void UserScript::setSourceCode(const QString &source)
{
    initData();
    scriptData->source = source.toStdString();
    parseMetadataHeader();
}

UserScript::InjectionPoint UserScript::injectionPoint() const
{
    if (isNull())
        return UserScript::AfterLoad;
    return static_cast<UserScript::InjectionPoint>(scriptData->injectionPoint);
}

void UserScript::setInjectionPoint(UserScript::InjectionPoint p)
{
    initData();
    scriptData->injectionPoint = p;
}

uint UserScript::worldId() const
{
    if (isNull())
        return 1;
    return scriptData->worldId;
}

void UserScript::setWorldId(uint id)
{
    initData();
    scriptData->worldId = id;
}

bool UserScript::runsOnSubFrames() const
{
    if (isNull())
        return false;
    return scriptData->injectForSubframes;
}

void UserScript::setRunsOnSubFrames(bool on)
{
    initData();
    scriptData->injectForSubframes = on;
}

bool UserScript::operator==(const UserScript &other) const
{
    if (isNull() != other.isNull())
        return false;
    if (isNull()) // neither is valid
        return true;
    return worldId() == other.worldId()
            && runsOnSubFrames() == other.runsOnSubFrames()
            && injectionPoint() == other.injectionPoint()
            && name() == other.name() && sourceCode() == other.sourceCode();
}

void UserScript::initData()
{
    if (scriptData.isNull())
        scriptData.reset(new UserScriptData);
}

bool UserScript::isNull() const
{
    return scriptData.isNull();
}

UserScriptData &UserScript::data() const
{
    return *(scriptData.data());
}

void UserScript::parseMetadataHeader()
{
    // Logic taken from Chromium (extensions/browser/user_script_loader.cc)
    // http://wiki.greasespot.net/Metadata_block
    const std::string &script_text = scriptData->source;
    base::StringPiece line;
    size_t line_start = 0;
    size_t line_end = line_start;
    bool in_metadata = false;

    static const base::StringPiece kUserScriptBegin("// ==UserScript==");
    static const base::StringPiece kUserScriptEnd("// ==/UserScript==");
    static const base::StringPiece kNameDeclaration("// @name");
    static const base::StringPiece kIncludeDeclaration("// @include");
    static const base::StringPiece kExcludeDeclaration("// @exclude");
    static const base::StringPiece kMatchDeclaration("// @match");
    static const base::StringPiece kRunAtDeclaration("// @run-at");
    static const base::StringPiece kRunAtDocumentStartValue("document-start");
    static const base::StringPiece kRunAtDocumentEndValue("document-end");
    static const base::StringPiece kRunAtDocumentIdleValue("document-idle");
    // FIXME: Scripts don't run in subframes by default. If we would like to
    // support @noframes rule, we have to change the current default behavior.
    // static const base::StringPiece kNoFramesDeclaration("// @noframes");

    static URLPattern urlPatternParser(validUserScriptSchemes());

    while (line_start < script_text.length()) {
        line_end = script_text.find('\n', line_start);

        // Handle the case where there is no trailing newline in the file.
        if (line_end == std::string::npos)
            line_end = script_text.length() - 1;

        line.set(script_text.data() + line_start, line_end - line_start);

        if (!in_metadata) {
            if (line.starts_with(kUserScriptBegin))
                in_metadata = true;
        } else {
            if (line.starts_with(kUserScriptEnd))
                break;

            std::string value;
            if (GetDeclarationValue(line, kNameDeclaration, &value)) {
                setName(toQt(value));
            } else if (GetDeclarationValue(line, kIncludeDeclaration, &value)) {
                // We escape some characters that MatchPattern() considers special.
                base::ReplaceSubstringsAfterOffset(&value, 0, "\\", "\\\\");
                base::ReplaceSubstringsAfterOffset(&value, 0, "?", "\\?");
                scriptData->globs.push_back(value);
            } else if (GetDeclarationValue(line, kExcludeDeclaration, &value)) {
                base::ReplaceSubstringsAfterOffset(&value, 0, "\\", "\\\\");
                base::ReplaceSubstringsAfterOffset(&value, 0, "?", "\\?");
                scriptData->excludeGlobs.push_back(value);
            } else if (GetDeclarationValue(line, kMatchDeclaration, &value)) {
                if (URLPattern::PARSE_SUCCESS == urlPatternParser.Parse(value))
                    scriptData->urlPatterns.push_back(value);
            } else if (GetDeclarationValue(line, kRunAtDeclaration, &value)) {
                if (value == kRunAtDocumentStartValue)
                    scriptData->injectionPoint = DocumentElementCreation;
                else if (value == kRunAtDocumentEndValue)
                    scriptData->injectionPoint = DocumentLoadFinished;
                else if (value == kRunAtDocumentIdleValue)
                    scriptData->injectionPoint = AfterLoad;
            }
        }

        line_start = line_end + 1;
    }

    // If no patterns were specified, default to @include *. This is what
    // Greasemonkey does.
    if (scriptData->globs.empty() && scriptData->urlPatterns.empty())
        scriptData->globs.push_back("*");
}

} // namespace QtWebEngineCore
