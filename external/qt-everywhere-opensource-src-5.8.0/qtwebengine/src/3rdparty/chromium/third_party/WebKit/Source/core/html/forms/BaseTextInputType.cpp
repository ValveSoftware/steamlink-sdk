/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2009 Michelangelo De Simone <micdesim@gmail.com>
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/html/forms/BaseTextInputType.h"

#include "bindings/core/v8/ScriptRegexp.h"
#include "core/HTMLNames.h"
#include "core/html/HTMLInputElement.h"
#include "core/inspector/ConsoleMessage.h"

namespace blink {

using namespace HTMLNames;

BaseTextInputType::BaseTextInputType(HTMLInputElement& element)
    : TextFieldInputType(element)
{
}

BaseTextInputType::~BaseTextInputType()
{
}

int BaseTextInputType::maxLength() const
{
    return element().maxLength();
}

int BaseTextInputType::minLength() const
{
    return element().minLength();
}

bool BaseTextInputType::tooLong(const String& value, HTMLTextFormControlElement::NeedsToCheckDirtyFlag check) const
{
    int max = element().maxLength();
    if (max < 0)
        return false;
    if (check == HTMLTextFormControlElement::CheckDirtyFlag) {
        // Return false for the default value or a value set by a script even if
        // it is longer than maxLength.
        if (!element().hasDirtyValue() || !element().lastChangeWasUserEdit())
            return false;
    }
    return value.length() > static_cast<unsigned>(max);
}

bool BaseTextInputType::tooShort(const String& value, HTMLTextFormControlElement::NeedsToCheckDirtyFlag check) const
{
    int min = element().minLength();
    if (min <= 0)
        return false;
    if (check == HTMLTextFormControlElement::CheckDirtyFlag) {
        // Return false for the default value or a value set by a script even if
        // it is shorter than minLength.
        if (!element().hasDirtyValue() || !element().lastChangeWasUserEdit())
            return false;
    }
    // An empty string is excluded from minlength check.
    unsigned len = value.length();
    return len > 0 && len < static_cast<unsigned>(min);
}

bool BaseTextInputType::patternMismatch(const String& value) const
{
    const AtomicString& rawPattern = element().fastGetAttribute(patternAttr);
    // Empty values can't be mismatched
    if (rawPattern.isNull() || value.isEmpty())
        return false;
    if (!m_regexp || m_patternForRegexp != rawPattern) {
        std::unique_ptr<ScriptRegexp> rawRegexp(new ScriptRegexp(rawPattern, TextCaseSensitive, MultilineDisabled, ScriptRegexp::UTF16));
        if (!rawRegexp->isValid()) {
            element().document().addConsoleMessage(ConsoleMessage::create(RenderingMessageSource, ErrorMessageLevel,
                String::format("Pattern attribute value %s is not a valid regular expression: %s",
                    rawPattern.utf8().data(),
                    rawRegexp->exceptionMessage().utf8().data())));
            m_regexp.reset(rawRegexp.release());
            m_patternForRegexp = rawPattern;
            return false;
        }
        String pattern = "^(?:" + rawPattern + ")$";
        m_regexp.reset(new ScriptRegexp(pattern, TextCaseSensitive, MultilineDisabled, ScriptRegexp::UTF16));
        m_patternForRegexp = rawPattern;
    }

    int matchLength = 0;
    int valueLength = value.length();
    int matchOffset = m_regexp->match(value, 0, &matchLength);
    bool mismatched = matchOffset != 0 || matchLength != valueLength;
    return mismatched;
}

bool BaseTextInputType::supportsPlaceholder() const
{
    return true;
}

bool BaseTextInputType::supportsSelectionAPI() const
{
    return true;
}

bool BaseTextInputType::supportsAutocapitalize() const
{
    return true;
}

} // namespace blink
