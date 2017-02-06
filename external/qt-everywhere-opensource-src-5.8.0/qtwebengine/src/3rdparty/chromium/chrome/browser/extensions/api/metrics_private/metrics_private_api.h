// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_METRICS_PRIVATE_METRICS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_METRICS_PRIVATE_METRICS_PRIVATE_API_H_

#include <stddef.h>

#include <string>

#include "base/metrics/histogram.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class MetricsPrivateGetIsCrashReportingEnabledFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.getIsCrashReportingEnabled",
                             METRICSPRIVATE_GETISCRASHRECORDINGENABLED)

 protected:
  ~MetricsPrivateGetIsCrashReportingEnabledFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class MetricsPrivateGetFieldTrialFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.getFieldTrial",
                             METRICSPRIVATE_GETFIELDTRIAL)

 protected:
  ~MetricsPrivateGetFieldTrialFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class MetricsPrivateGetVariationParamsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.getVariationParams",
                             METRICSPRIVATE_GETVARIATIONPARAMS)

 protected:
  ~MetricsPrivateGetVariationParamsFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class MetricsPrivateRecordUserActionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordUserAction",
                             METRICSPRIVATE_RECORDUSERACTION)

 protected:
  ~MetricsPrivateRecordUserActionFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class MetricsHistogramHelperFunction : public SyncExtensionFunction {
 protected:
  ~MetricsHistogramHelperFunction() override {}
  virtual bool RecordValue(const std::string& name,
                           base::HistogramType type,
                           int min, int max, size_t buckets,
                           int sample);
};

class MetricsPrivateRecordValueFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordValue",
                             METRICSPRIVATE_RECORDVALUE)

 protected:
  ~MetricsPrivateRecordValueFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class MetricsPrivateRecordSparseValueFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordSparseValue",
                             METRICSPRIVATE_RECORDSPARSEVALUE)

 protected:
  ~MetricsPrivateRecordSparseValueFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class MetricsPrivateRecordPercentageFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordPercentage",
                             METRICSPRIVATE_RECORDPERCENTAGE)

 protected:
  ~MetricsPrivateRecordPercentageFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class MetricsPrivateRecordCountFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordCount",
                             METRICSPRIVATE_RECORDCOUNT)

 protected:
  ~MetricsPrivateRecordCountFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class MetricsPrivateRecordSmallCountFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordSmallCount",
                             METRICSPRIVATE_RECORDSMALLCOUNT)

 protected:
  ~MetricsPrivateRecordSmallCountFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class MetricsPrivateRecordMediumCountFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordMediumCount",
                             METRICSPRIVATE_RECORDMEDIUMCOUNT)

 protected:
  ~MetricsPrivateRecordMediumCountFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class MetricsPrivateRecordTimeFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordTime",
                             METRICSPRIVATE_RECORDTIME)

 protected:
  ~MetricsPrivateRecordTimeFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class MetricsPrivateRecordMediumTimeFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordMediumTime",
                             METRICSPRIVATE_RECORDMEDIUMTIME)

 protected:
  ~MetricsPrivateRecordMediumTimeFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class MetricsPrivateRecordLongTimeFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordLongTime",
                             METRICSPRIVATE_RECORDLONGTIME)

 protected:
  ~MetricsPrivateRecordLongTimeFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_METRICS_PRIVATE_METRICS_PRIVATE_API_H_
