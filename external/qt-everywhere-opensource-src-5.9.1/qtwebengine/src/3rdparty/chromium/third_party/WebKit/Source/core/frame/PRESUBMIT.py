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
            use_counter_cpp_file = f
            break
    else:
        return []

    largest_found_bucket = 0
    expected_max_bucket = 0

    # Looking for a line like "case CSSPropertyGrid: return 453;"
    bucket_finder = input_api.re.compile(
        r'case CSSProperty\w*?:\s+?return (\d+);',
        input_api.re.MULTILINE)
    # Looking for a line like "constexpr int kMaximumCSSSampleId = 452;"
    expected_max_finder = input_api.re.compile(
        r'constexpr int kMaximumCSSSampleId = (\d+);')
    joined_contents = '\n'.join(use_counter_cpp_file.NewContents())

    expected_max_match = expected_max_finder.search(joined_contents)
    if expected_max_match:
        expected_max_bucket = int(expected_max_match.group(1))

    for bucket_match in bucket_finder.finditer(joined_contents):
        bucket = int(bucket_match.group(1))
        largest_found_bucket = max(largest_found_bucket, bucket)

    if largest_found_bucket != expected_max_bucket:
        if input_api.is_committing:
            message_type = output_api.PresubmitError
        else:
            message_type = output_api.PresubmitPromptWarning

        return [message_type(
            'Largest found CSSProperty bucket Id (%d) does not match '
            'maximumCSSSampleId (%d)' % (
                largest_found_bucket, expected_max_bucket),
            items=[use_counter_cpp_file.LocalPath()])]

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
