// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * This file contains typedefs <cr-network-list/> properties.
 */

var CrNetworkList = {};

/**
 * Generic managed property type. This should match any of the basic managed
 * types in chrome.networkingPrivate, e.g. networkingPrivate.ManagedBoolean.
 * @typedef {{
 *   customItemName: (!String|undefined),
 *   polymerIcon: (!String|undefined),
 *   customData: (!Object|undefined),
 * }}
 */
CrNetworkList.CustomItemState;
