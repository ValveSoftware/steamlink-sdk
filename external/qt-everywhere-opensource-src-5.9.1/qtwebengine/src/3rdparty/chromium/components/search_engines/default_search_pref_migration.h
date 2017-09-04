// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_PREF_MIGRATION_H_
#define COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_PREF_MIGRATION_H_

class PrefService;

// Migrates a DSE value stored in separate String/List/..Value preferences by
// M35 or earlier to the new single DictionaryValue used in M36.
// Operates immediately if |pref_service| is fully initialized. Otherwise, waits
// for the PrefService to load using an observer.
void ConfigureDefaultSearchPrefMigrationToDictionaryValue(
    PrefService* pref_service);

#endif  // COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_PREF_MIGRATION_H_
