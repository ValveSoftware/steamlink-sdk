// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/formatter.h"

#include <vector>

#include "base/logging.h"
#include "grit/ui_strings.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "ui/base/l10n/l10n_util_plurals.h"

namespace ui {

UI_BASE_EXPORT bool formatter_force_fallback = false;

static const size_t kNumberPluralities = 6;
struct Pluralities {
  int ids[kNumberPluralities];
  const char* fallback_one;
  const char* fallback_other;
};

static const Pluralities IDS_ELAPSED_SHORT_SEC = {
  { IDS_TIME_ELAPSED_SECS_DEFAULT, IDS_TIME_ELAPSED_SECS_SINGULAR,
    IDS_TIME_ELAPSED_SECS_ZERO, IDS_TIME_ELAPSED_SECS_TWO,
    IDS_TIME_ELAPSED_SECS_FEW, IDS_TIME_ELAPSED_SECS_MANY },
  "one{# sec ago}",
  " other{# secs ago}"
};
static const Pluralities IDS_ELAPSED_SHORT_MIN = {
  { IDS_TIME_ELAPSED_MINS_DEFAULT, IDS_TIME_ELAPSED_MINS_SINGULAR,
    IDS_TIME_ELAPSED_MINS_ZERO, IDS_TIME_ELAPSED_MINS_TWO,
    IDS_TIME_ELAPSED_MINS_FEW, IDS_TIME_ELAPSED_MINS_MANY },
  "one{# min ago}",
  " other{# mins ago}"
};
static const Pluralities IDS_ELAPSED_HOUR = {
  { IDS_TIME_ELAPSED_HOURS_DEFAULT, IDS_TIME_ELAPSED_HOURS_SINGULAR,
    IDS_TIME_ELAPSED_HOURS_ZERO, IDS_TIME_ELAPSED_HOURS_TWO,
    IDS_TIME_ELAPSED_HOURS_FEW, IDS_TIME_ELAPSED_HOURS_MANY },
  "one{# hour ago}",
  " other{# hours ago}"
};
static const Pluralities IDS_ELAPSED_DAY = {
  { IDS_TIME_ELAPSED_DAYS_DEFAULT, IDS_TIME_ELAPSED_DAYS_SINGULAR,
    IDS_TIME_ELAPSED_DAYS_ZERO, IDS_TIME_ELAPSED_DAYS_TWO,
    IDS_TIME_ELAPSED_DAYS_FEW, IDS_TIME_ELAPSED_DAYS_MANY },
  "one{# day ago}",
  " other{# days ago}"
};

static const Pluralities IDS_REMAINING_SHORT_SEC = {
  { IDS_TIME_REMAINING_SECS_DEFAULT, IDS_TIME_REMAINING_SECS_SINGULAR,
    IDS_TIME_REMAINING_SECS_ZERO, IDS_TIME_REMAINING_SECS_TWO,
    IDS_TIME_REMAINING_SECS_FEW, IDS_TIME_REMAINING_SECS_MANY },
  "one{# sec left}",
  " other{# secs left}"
};
static const Pluralities IDS_REMAINING_SHORT_MIN = {
  { IDS_TIME_REMAINING_MINS_DEFAULT, IDS_TIME_REMAINING_MINS_SINGULAR,
    IDS_TIME_REMAINING_MINS_ZERO, IDS_TIME_REMAINING_MINS_TWO,
    IDS_TIME_REMAINING_MINS_FEW, IDS_TIME_REMAINING_MINS_MANY },
  "one{# min left}",
  " other{# mins left}"
};

static const Pluralities IDS_REMAINING_LONG_SEC = {
  { IDS_TIME_REMAINING_LONG_SECS_DEFAULT, IDS_TIME_REMAINING_LONG_SECS_SINGULAR,
    IDS_TIME_REMAINING_LONG_SECS_ZERO, IDS_TIME_REMAINING_LONG_SECS_TWO,
    IDS_TIME_REMAINING_LONG_SECS_FEW, IDS_TIME_REMAINING_LONG_SECS_MANY },
  "one{# second left}",
  " other{# seconds left}"
};
static const Pluralities IDS_REMAINING_LONG_MIN = {
  { IDS_TIME_REMAINING_LONG_MINS_DEFAULT, IDS_TIME_REMAINING_LONG_MINS_SINGULAR,
    IDS_TIME_REMAINING_LONG_MINS_ZERO, IDS_TIME_REMAINING_LONG_MINS_TWO,
    IDS_TIME_REMAINING_LONG_MINS_FEW, IDS_TIME_REMAINING_LONG_MINS_MANY },
  "one{# minute left}",
  " other{# minutes left}"
};
static const Pluralities IDS_REMAINING_HOUR = {
  { IDS_TIME_REMAINING_HOURS_DEFAULT, IDS_TIME_REMAINING_HOURS_SINGULAR,
    IDS_TIME_REMAINING_HOURS_ZERO, IDS_TIME_REMAINING_HOURS_TWO,
    IDS_TIME_REMAINING_HOURS_FEW, IDS_TIME_REMAINING_HOURS_MANY },
  "one{# hour left}",
  " other{# hours left}"
};
static const Pluralities IDS_REMAINING_DAY = {
  { IDS_TIME_REMAINING_DAYS_DEFAULT, IDS_TIME_REMAINING_DAYS_SINGULAR,
    IDS_TIME_REMAINING_DAYS_ZERO, IDS_TIME_REMAINING_DAYS_TWO,
    IDS_TIME_REMAINING_DAYS_FEW, IDS_TIME_REMAINING_DAYS_MANY },
  "one{# day left}",
  " other{# days left}"
};

static const Pluralities IDS_DURATION_SHORT_SEC = {
  { IDS_TIME_SECS_DEFAULT, IDS_TIME_SECS_SINGULAR, IDS_TIME_SECS_ZERO,
    IDS_TIME_SECS_TWO, IDS_TIME_SECS_FEW, IDS_TIME_SECS_MANY },
  "one{# sec}",
  " other{# secs}"
};
static const Pluralities IDS_DURATION_SHORT_MIN = {
  { IDS_TIME_MINS_DEFAULT, IDS_TIME_MINS_SINGULAR, IDS_TIME_MINS_ZERO,
    IDS_TIME_MINS_TWO, IDS_TIME_MINS_FEW, IDS_TIME_MINS_MANY },
  "one{# min}",
  " other{# mins}"
};

static const Pluralities IDS_LONG_SEC = {
  { IDS_TIME_LONG_SECS_DEFAULT, IDS_TIME_LONG_SECS_SINGULAR,
    IDS_TIME_LONG_SECS_ZERO, IDS_TIME_LONG_SECS_TWO,
    IDS_TIME_LONG_SECS_FEW, IDS_TIME_LONG_SECS_MANY },
  "one{# second}",
  " other{# seconds}"
};
static const Pluralities IDS_LONG_MIN = {
  { IDS_TIME_LONG_MINS_DEFAULT, IDS_TIME_LONG_MINS_SINGULAR,
    IDS_TIME_LONG_MINS_ZERO, IDS_TIME_LONG_MINS_TWO,
    IDS_TIME_LONG_MINS_FEW, IDS_TIME_LONG_MINS_MANY },
  "one{# minute}",
  " other{# minutes}"
};
static const Pluralities IDS_DURATION_HOUR = {
  { IDS_TIME_HOURS_DEFAULT, IDS_TIME_HOURS_SINGULAR, IDS_TIME_HOURS_ZERO,
    IDS_TIME_HOURS_TWO, IDS_TIME_HOURS_FEW, IDS_TIME_HOURS_MANY },
  "one{# hour}",
  " other{# hours}"
};
static const Pluralities IDS_DURATION_DAY = {
  { IDS_TIME_DAYS_DEFAULT, IDS_TIME_DAYS_SINGULAR, IDS_TIME_DAYS_ZERO,
    IDS_TIME_DAYS_TWO, IDS_TIME_DAYS_FEW, IDS_TIME_DAYS_MANY },
  "one{# day}",
  " other{# days}"
};

static const Pluralities IDS_LONG_MIN_1ST = {
  { IDS_TIME_LONG_MINS_1ST_DEFAULT, IDS_TIME_LONG_MINS_1ST_SINGULAR,
    IDS_TIME_LONG_MINS_1ST_ZERO, IDS_TIME_LONG_MINS_1ST_TWO,
    IDS_TIME_LONG_MINS_1ST_FEW, IDS_TIME_LONG_MINS_1ST_MANY },
  "one{# minute }",
  " other{# minutes }"
};
static const Pluralities IDS_LONG_SEC_2ND = {
  { IDS_TIME_LONG_SECS_2ND_DEFAULT, IDS_TIME_LONG_SECS_2ND_SINGULAR,
    IDS_TIME_LONG_SECS_2ND_ZERO, IDS_TIME_LONG_SECS_2ND_TWO,
    IDS_TIME_LONG_SECS_2ND_FEW, IDS_TIME_LONG_SECS_2ND_MANY },
  "one{# second}",
  " other{# seconds}"
};
static const Pluralities IDS_DURATION_HOUR_1ST = {
  { IDS_TIME_HOURS_1ST_DEFAULT, IDS_TIME_HOURS_1ST_SINGULAR,
    IDS_TIME_HOURS_1ST_ZERO, IDS_TIME_HOURS_1ST_TWO,
    IDS_TIME_HOURS_1ST_FEW, IDS_TIME_HOURS_1ST_MANY },
  "one{# hour }",
  " other{# hours }"
};
static const Pluralities IDS_LONG_MIN_2ND = {
  { IDS_TIME_LONG_MINS_2ND_DEFAULT, IDS_TIME_LONG_MINS_2ND_SINGULAR,
    IDS_TIME_LONG_MINS_2ND_ZERO, IDS_TIME_LONG_MINS_2ND_TWO,
    IDS_TIME_LONG_MINS_2ND_FEW, IDS_TIME_LONG_MINS_2ND_MANY },
  "one{# minute}",
  " other{# minutes}"
};
static const Pluralities IDS_DURATION_DAY_1ST = {
  { IDS_TIME_DAYS_1ST_DEFAULT, IDS_TIME_DAYS_1ST_SINGULAR,
    IDS_TIME_DAYS_1ST_ZERO, IDS_TIME_DAYS_1ST_TWO,
    IDS_TIME_DAYS_1ST_FEW, IDS_TIME_DAYS_1ST_MANY },
  "one{# day }",
  " other{# days }"
};
static const Pluralities IDS_DURATION_HOUR_2ND = {
  { IDS_TIME_HOURS_2ND_DEFAULT, IDS_TIME_HOURS_2ND_SINGULAR,
    IDS_TIME_HOURS_2ND_ZERO, IDS_TIME_HOURS_2ND_TWO,
    IDS_TIME_HOURS_2ND_FEW, IDS_TIME_HOURS_2ND_MANY },
  "one{# hour}",
  " other{# hours}"
};

Formatter::Formatter(const Pluralities& sec_pluralities,
                     const Pluralities& min_pluralities,
                     const Pluralities& hour_pluralities,
                     const Pluralities& day_pluralities) {
  simple_format_[UNIT_SEC] = InitFormat(sec_pluralities);
  simple_format_[UNIT_MIN] = InitFormat(min_pluralities);
  simple_format_[UNIT_HOUR] = InitFormat(hour_pluralities);
  simple_format_[UNIT_DAY] = InitFormat(day_pluralities);
}

Formatter::Formatter(const Pluralities& sec_pluralities,
                     const Pluralities& min_pluralities,
                     const Pluralities& hour_pluralities,
                     const Pluralities& day_pluralities,
                     const Pluralities& min_sec_pluralities1,
                     const Pluralities& min_sec_pluralities2,
                     const Pluralities& hour_min_pluralities1,
                     const Pluralities& hour_min_pluralities2,
                     const Pluralities& day_hour_pluralities1,
                     const Pluralities& day_hour_pluralities2) {
  simple_format_[UNIT_SEC] = InitFormat(sec_pluralities);
  simple_format_[UNIT_MIN] = InitFormat(min_pluralities);
  simple_format_[UNIT_HOUR] = InitFormat(hour_pluralities);
  simple_format_[UNIT_DAY] = InitFormat(day_pluralities);
  detailed_format_[TWO_UNITS_MIN_SEC][0] = InitFormat(min_sec_pluralities1);
  detailed_format_[TWO_UNITS_MIN_SEC][1] = InitFormat(min_sec_pluralities2);
  detailed_format_[TWO_UNITS_HOUR_MIN][0] = InitFormat(hour_min_pluralities1);
  detailed_format_[TWO_UNITS_HOUR_MIN][1] = InitFormat(hour_min_pluralities2);
  detailed_format_[TWO_UNITS_DAY_HOUR][0] = InitFormat(day_hour_pluralities1);
  detailed_format_[TWO_UNITS_DAY_HOUR][1] = InitFormat(day_hour_pluralities2);
}

void Formatter::Format(Unit unit,
                       int value,
                       icu::UnicodeString& formatted_string) const {
  DCHECK(simple_format_[unit]);
  UErrorCode error = U_ZERO_ERROR;
  formatted_string = simple_format_[unit]->format(value, error);
  DCHECK(U_SUCCESS(error)) << "Error in icu::PluralFormat::format().";
  return;
}

void Formatter::Format(TwoUnits units,
                       int value_1,
                       int value_2,
                       icu::UnicodeString& formatted_string) const {
  DCHECK(detailed_format_[units][0])
      << "Detailed() not implemented for your (format, length) combination!";
  DCHECK(detailed_format_[units][1])
      << "Detailed() not implemented for your (format, length) combination!";
  UErrorCode error = U_ZERO_ERROR;
  formatted_string = detailed_format_[units][0]->format(value_1, error);
  DCHECK(U_SUCCESS(error));
  formatted_string += detailed_format_[units][1]->format(value_2, error);
  DCHECK(U_SUCCESS(error));
  return;
}

scoped_ptr<icu::PluralFormat> Formatter::CreateFallbackFormat(
    const icu::PluralRules& rules,
    const Pluralities& pluralities) const {
  icu::UnicodeString pattern;
  if (rules.isKeyword(UNICODE_STRING_SIMPLE("one")))
    pattern += icu::UnicodeString(pluralities.fallback_one);
  pattern += icu::UnicodeString(pluralities.fallback_other);

  UErrorCode error = U_ZERO_ERROR;
  scoped_ptr<icu::PluralFormat> format(
      new icu::PluralFormat(rules, pattern, error));
  DCHECK(U_SUCCESS(error));
  return format.Pass();
}

scoped_ptr<icu::PluralFormat> Formatter::InitFormat(
    const Pluralities& pluralities) {
  if (!formatter_force_fallback) {
    icu::UnicodeString pattern;
    std::vector<int> ids;
    for (size_t j = 0; j < kNumberPluralities; ++j)
      ids.push_back(pluralities.ids[j]);
    scoped_ptr<icu::PluralFormat> format = l10n_util::BuildPluralFormat(ids);
    if (format.get())
      return format.Pass();
  }

  scoped_ptr<icu::PluralRules> rules(l10n_util::BuildPluralRules());
  return CreateFallbackFormat(*rules, pluralities);
}

const Formatter* FormatterContainer::Get(TimeFormat::Format format,
                                         TimeFormat::Length length) const {
  DCHECK(formatter_[format][length])
      << "Combination of FORMAT_ELAPSED and LENGTH_LONG is not implemented!";
  return formatter_[format][length].get();
}

FormatterContainer::FormatterContainer() {
  Initialize();
}

FormatterContainer::~FormatterContainer() {
}

void FormatterContainer::Initialize() {
  formatter_[TimeFormat::FORMAT_ELAPSED][TimeFormat::LENGTH_SHORT].reset(
      new Formatter(IDS_ELAPSED_SHORT_SEC,
                    IDS_ELAPSED_SHORT_MIN,
                    IDS_ELAPSED_HOUR,
                    IDS_ELAPSED_DAY));
  formatter_[TimeFormat::FORMAT_ELAPSED][TimeFormat::LENGTH_LONG].reset();
  formatter_[TimeFormat::FORMAT_REMAINING][TimeFormat::LENGTH_SHORT].reset(
      new Formatter(IDS_REMAINING_SHORT_SEC,
                    IDS_REMAINING_SHORT_MIN,
                    IDS_REMAINING_HOUR,
                    IDS_REMAINING_DAY));
  formatter_[TimeFormat::FORMAT_REMAINING][TimeFormat::LENGTH_LONG].reset(
      new Formatter(IDS_REMAINING_LONG_SEC,
                    IDS_REMAINING_LONG_MIN,
                    IDS_REMAINING_HOUR,
                    IDS_REMAINING_DAY));
  formatter_[TimeFormat::FORMAT_DURATION][TimeFormat::LENGTH_SHORT].reset(
      new Formatter(IDS_DURATION_SHORT_SEC,
                    IDS_DURATION_SHORT_MIN,
                    IDS_DURATION_HOUR,
                    IDS_DURATION_DAY));
  formatter_[TimeFormat::FORMAT_DURATION][TimeFormat::LENGTH_LONG].reset(
      new Formatter(IDS_LONG_SEC,
                    IDS_LONG_MIN,
                    IDS_DURATION_HOUR,
                    IDS_DURATION_DAY,
                    IDS_LONG_MIN_1ST,
                    IDS_LONG_SEC_2ND,
                    IDS_DURATION_HOUR_1ST,
                    IDS_LONG_MIN_2ND,
                    IDS_DURATION_DAY_1ST,
                    IDS_DURATION_HOUR_2ND));
}

void FormatterContainer::Shutdown() {
  for (int format = 0; format < TimeFormat::FORMAT_COUNT; ++format) {
    for (int length = 0; length < TimeFormat::LENGTH_COUNT; ++length) {
      formatter_[format][length].reset();
    }
  }
}

}  // namespace ui
