# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Blink frame presubmit script

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""


def _RunUseCounterChecks(input_api, output_api):
    for f in input_api.AffectedFiles():
        if f.LocalPath().endswith('UseCounter.cpp'):
            useCounterCpp = f
            break
    else:
        return []

    largestFoundBucket = 0
    maximumBucket = 0
    # Looking for a line like "case CSSPropertyGrid: return 453;"
    bucketFinder = input_api.re.compile(r'.*CSSProperty.*return\s*([0-9]+).*')
    # Looking for a line like "static int maximumCSSSampleId() { return 452; }"
    maximumFinder = input_api.re.compile(
        r'static int maximumCSSSampleId\(\) { return ([0-9]+)')
    for line in useCounterCpp.NewContents():
        bucketMatch = bucketFinder.match(line)
        if bucketMatch:
            bucket = int(bucketMatch.group(1))
            largestFoundBucket = max(largestFoundBucket, bucket)
        else:
            maximumMatch = maximumFinder.match(line)
            if maximumMatch:
                maximumBucket = int(maximumMatch.group(1))

    if largestFoundBucket != maximumBucket:
        if input_api.is_committing:
            message_type = output_api.PresubmitError
        else:
            message_type = output_api.PresubmitPromptWarning

        return [message_type(
            'Largest found CSSProperty bucket Id (%d) does not match '
            'maximumCSSSampleId (%d)' %
                    (largestFoundBucket, maximumBucket),
            items=[useCounterCpp.LocalPath()])]

    return []


def _RunUmaHistogramChecks(input_api, output_api):
    import sys

    original_sys_path = sys.path
    try:
        sys.path = sys.path + [input_api.os_path.join(
            input_api.PresubmitLocalPath(), '..', '..', '..', '..', '..',
            'tools', 'metrics', 'histograms')]
        import update_histogram_enum
    finally:
        sys.path = original_sys_path

    source_path = ''
    for f in input_api.AffectedFiles():
        if f.LocalPath().endswith('UseCounter.h'):
            source_path = f.LocalPath()
            break
    else:
        return []

    START_MARKER = '^enum Feature {'
    END_MARKER = '^NumberOfFeatures'
    if update_histogram_enum.HistogramNeedsUpdate(
            histogram_enum_name='FeatureObserver',
            source_enum_path=source_path,
            start_marker=START_MARKER,
            end_marker=END_MARKER):
        return [output_api.PresubmitPromptWarning(
            'UseCounter::Feature has been updated and the UMA mapping needs to '
            'be regenerated. Please run update_use_counter_feature_enum.py in '
            'src/tools/metrics/histograms/ to update the mapping.',
            items=[source_path])]

    return []


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_RunUseCounterChecks(input_api, output_api))
    results.extend(_RunUmaHistogramChecks(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(_RunUseCounterChecks(input_api, output_api))
    results.extend(_RunUmaHistogramChecks(input_api, output_api))
    return results
