// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Intentionally no include guards because this file is meant to be included
// inside a macro to generate enum values.

WINDOW_OPEN_DISPOSITION(UNKNOWN, 0)
WINDOW_OPEN_DISPOSITION(SUPPRESS_OPEN, 1)
WINDOW_OPEN_DISPOSITION(CURRENT_TAB, 2)
// Indicates that only one tab with the url should exist in the same window.
WINDOW_OPEN_DISPOSITION(SINGLETON_TAB, 3)
WINDOW_OPEN_DISPOSITION(NEW_FOREGROUND_TAB, 4)
WINDOW_OPEN_DISPOSITION(NEW_BACKGROUND_TAB, 5)
WINDOW_OPEN_DISPOSITION(NEW_POPUP, 6)
WINDOW_OPEN_DISPOSITION(NEW_WINDOW, 7)
WINDOW_OPEN_DISPOSITION(SAVE_TO_DISK, 8)
WINDOW_OPEN_DISPOSITION(OFF_THE_RECORD, 9)
WINDOW_OPEN_DISPOSITION(IGNORE_ACTION, 10)
// Update when adding a new disposition.
WINDOW_OPEN_DISPOSITION(WINDOW_OPEN_DISPOSITION_LAST, 10)
