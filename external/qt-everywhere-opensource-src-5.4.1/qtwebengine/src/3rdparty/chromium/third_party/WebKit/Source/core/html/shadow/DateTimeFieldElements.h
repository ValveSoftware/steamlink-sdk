/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef DateTimeFieldElements_h
#define DateTimeFieldElements_h

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "core/html/shadow/DateTimeNumericFieldElement.h"
#include "core/html/shadow/DateTimeSymbolicFieldElement.h"

namespace WebCore {

class DateTimeAMPMFieldElement FINAL : public DateTimeSymbolicFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeAMPMFieldElement);

public:
    static PassRefPtrWillBeRawPtr<DateTimeAMPMFieldElement> create(Document&, FieldOwner&, const Vector<String>&);

private:
    DateTimeAMPMFieldElement(Document&, FieldOwner&, const Vector<String>&);

    // DateTimeFieldElement functions.
    virtual void populateDateTimeFieldsState(DateTimeFieldsState&) OVERRIDE;
    virtual void setValueAsDate(const DateComponents&) OVERRIDE;
    virtual void setValueAsDateTimeFieldsState(const DateTimeFieldsState&) OVERRIDE;
};

class DateTimeDayFieldElement FINAL : public DateTimeNumericFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeDayFieldElement);

public:
    static PassRefPtrWillBeRawPtr<DateTimeDayFieldElement> create(Document&, FieldOwner&, const String& placeholder, const Range&);

private:
    DateTimeDayFieldElement(Document&, FieldOwner&, const String& placeholder, const Range&);

    // DateTimeFieldElement functions.
    virtual void populateDateTimeFieldsState(DateTimeFieldsState&) OVERRIDE;
    virtual void setValueAsDate(const DateComponents&) OVERRIDE;
    virtual void setValueAsDateTimeFieldsState(const DateTimeFieldsState&) OVERRIDE;
};

class DateTimeHourFieldElementBase : public DateTimeNumericFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeHourFieldElementBase);

protected:
    DateTimeHourFieldElementBase(Document&, FieldOwner&, const Range&, const Range& hardLimits, const Step&);
    void initialize();

private:
    // DateTimeFieldElement functions.
    virtual void setValueAsDate(const DateComponents&) OVERRIDE;
    virtual void setValueAsDateTimeFieldsState(const DateTimeFieldsState&) OVERRIDE;
};

class DateTimeHour11FieldElement FINAL : public DateTimeHourFieldElementBase {
    WTF_MAKE_NONCOPYABLE(DateTimeHour11FieldElement);

public:
    static PassRefPtrWillBeRawPtr<DateTimeHour11FieldElement> create(Document&, FieldOwner&, const Range&, const Step&);

private:
    DateTimeHour11FieldElement(Document&, FieldOwner&, const Range& hour23Range, const Step&);

    // DateTimeFieldElement functions.
    virtual void populateDateTimeFieldsState(DateTimeFieldsState&) OVERRIDE;
    virtual void setValueAsInteger(int, EventBehavior = DispatchNoEvent) OVERRIDE;
};

class DateTimeHour12FieldElement FINAL : public DateTimeHourFieldElementBase {
    WTF_MAKE_NONCOPYABLE(DateTimeHour12FieldElement);

public:
    static PassRefPtrWillBeRawPtr<DateTimeHour12FieldElement> create(Document&, FieldOwner&, const Range&, const Step&);

private:
    DateTimeHour12FieldElement(Document&, FieldOwner&, const Range& hour23Range, const Step&);

    // DateTimeFieldElement functions.
    virtual void populateDateTimeFieldsState(DateTimeFieldsState&) OVERRIDE;
    virtual void setValueAsInteger(int, EventBehavior = DispatchNoEvent) OVERRIDE;
};

class DateTimeHour23FieldElement FINAL : public DateTimeHourFieldElementBase {
    WTF_MAKE_NONCOPYABLE(DateTimeHour23FieldElement);

public:
    static PassRefPtrWillBeRawPtr<DateTimeHour23FieldElement> create(Document&, FieldOwner&, const Range&, const Step&);

private:
    DateTimeHour23FieldElement(Document&, FieldOwner&, const Range& hour23Range, const Step&);

    // DateTimeFieldElement functions.
    virtual void populateDateTimeFieldsState(DateTimeFieldsState&) OVERRIDE;
    virtual void setValueAsInteger(int, EventBehavior = DispatchNoEvent) OVERRIDE;
};

class DateTimeHour24FieldElement FINAL : public DateTimeHourFieldElementBase {
    WTF_MAKE_NONCOPYABLE(DateTimeHour24FieldElement);

public:
    static PassRefPtrWillBeRawPtr<DateTimeHour24FieldElement> create(Document&, FieldOwner&, const Range&, const Step&);

private:
    DateTimeHour24FieldElement(Document&, FieldOwner&, const Range& hour23Range, const Step&);

    // DateTimeFieldElement functions.
    virtual void populateDateTimeFieldsState(DateTimeFieldsState&) OVERRIDE;
    virtual void setValueAsInteger(int, EventBehavior = DispatchNoEvent) OVERRIDE;
};

class DateTimeMillisecondFieldElement FINAL : public DateTimeNumericFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeMillisecondFieldElement);

public:
    static PassRefPtrWillBeRawPtr<DateTimeMillisecondFieldElement> create(Document&, FieldOwner&, const Range&, const Step&);

private:
    DateTimeMillisecondFieldElement(Document&, FieldOwner&, const Range&, const Step&);

    // DateTimeFieldElement functions.
    virtual void populateDateTimeFieldsState(DateTimeFieldsState&) OVERRIDE;
    virtual void setValueAsDate(const DateComponents&) OVERRIDE;
    virtual void setValueAsDateTimeFieldsState(const DateTimeFieldsState&) OVERRIDE;
};

class DateTimeMinuteFieldElement FINAL : public DateTimeNumericFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeMinuteFieldElement);

public:
    static PassRefPtrWillBeRawPtr<DateTimeMinuteFieldElement> create(Document&, FieldOwner&, const Range&, const Step&);

private:
    DateTimeMinuteFieldElement(Document&, FieldOwner&, const Range&, const Step&);

    // DateTimeFieldElement functions.
    virtual void populateDateTimeFieldsState(DateTimeFieldsState&) OVERRIDE;
    virtual void setValueAsDate(const DateComponents&) OVERRIDE;
    virtual void setValueAsDateTimeFieldsState(const DateTimeFieldsState&) OVERRIDE;
};

class DateTimeMonthFieldElement FINAL : public DateTimeNumericFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeMonthFieldElement);

public:
    static PassRefPtrWillBeRawPtr<DateTimeMonthFieldElement> create(Document&, FieldOwner&, const String& placeholder, const Range&);

private:
    DateTimeMonthFieldElement(Document&, FieldOwner&, const String& placeholder, const Range&);

    // DateTimeFieldElement functions.
    virtual void populateDateTimeFieldsState(DateTimeFieldsState&) OVERRIDE;
    virtual void setValueAsDate(const DateComponents&) OVERRIDE;
    virtual void setValueAsDateTimeFieldsState(const DateTimeFieldsState&) OVERRIDE;
};

class DateTimeSecondFieldElement FINAL : public DateTimeNumericFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeSecondFieldElement);

public:
    static PassRefPtrWillBeRawPtr<DateTimeSecondFieldElement> create(Document&, FieldOwner&, const Range&, const Step&);

private:
    DateTimeSecondFieldElement(Document&, FieldOwner&, const Range&, const Step&);

    // DateTimeFieldElement functions.
    virtual void populateDateTimeFieldsState(DateTimeFieldsState&) OVERRIDE;
    virtual void setValueAsDate(const DateComponents&) OVERRIDE;
    virtual void setValueAsDateTimeFieldsState(const DateTimeFieldsState&) OVERRIDE;
};

class DateTimeSymbolicMonthFieldElement FINAL : public DateTimeSymbolicFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeSymbolicMonthFieldElement);

public:
    static PassRefPtrWillBeRawPtr<DateTimeSymbolicMonthFieldElement> create(Document&, FieldOwner&, const Vector<String>&, int minimum, int maximum);

private:
    DateTimeSymbolicMonthFieldElement(Document&, FieldOwner&, const Vector<String>&, int minimum, int maximum);

    // DateTimeFieldElement functions.
    virtual void populateDateTimeFieldsState(DateTimeFieldsState&) OVERRIDE;
    virtual void setValueAsDate(const DateComponents&) OVERRIDE;
    virtual void setValueAsDateTimeFieldsState(const DateTimeFieldsState&) OVERRIDE;
};

class DateTimeWeekFieldElement FINAL : public DateTimeNumericFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeWeekFieldElement);

public:
    static PassRefPtrWillBeRawPtr<DateTimeWeekFieldElement> create(Document&, FieldOwner&, const Range&);

private:
    DateTimeWeekFieldElement(Document&, FieldOwner&, const Range&);

    // DateTimeFieldElement functions.
    virtual void populateDateTimeFieldsState(DateTimeFieldsState&) OVERRIDE;
    virtual void setValueAsDate(const DateComponents&) OVERRIDE;
    virtual void setValueAsDateTimeFieldsState(const DateTimeFieldsState&) OVERRIDE;
};

class DateTimeYearFieldElement FINAL : public DateTimeNumericFieldElement {
    WTF_MAKE_NONCOPYABLE(DateTimeYearFieldElement);

public:
    struct Parameters {
        int minimumYear;
        int maximumYear;
        bool minIsSpecified;
        bool maxIsSpecified;
        String placeholder;

        Parameters()
            : minimumYear(-1)
            , maximumYear(-1)
            , minIsSpecified(false)
            , maxIsSpecified(false)
        {
        }
    };

    static PassRefPtrWillBeRawPtr<DateTimeYearFieldElement> create(Document&, FieldOwner&, const Parameters&);

private:
    DateTimeYearFieldElement(Document&, FieldOwner&, const Parameters&);

    // DateTimeFieldElement functions.
    virtual void populateDateTimeFieldsState(DateTimeFieldsState&) OVERRIDE;
    virtual void setValueAsDate(const DateComponents&) OVERRIDE;
    virtual void setValueAsDateTimeFieldsState(const DateTimeFieldsState&) OVERRIDE;

    // DateTimeNumericFieldElement functions.
    virtual int defaultValueForStepDown() const OVERRIDE;
    virtual int defaultValueForStepUp() const OVERRIDE;

    bool m_minIsSpecified;
    bool m_maxIsSpecified;
};

} // namespace WebCore

#endif
#endif
