// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure compiler typedefs.
 */

/**
 * The payload of the certificate-action event that is emitted from this
 * component.
 * @typedef {{
 *   action: !CertificateAction,
 *   subnode: (null|CertificateSubnode|NewCertificateSubNode),
 *   certificateType: !CertificateType
 * }}
 */
var CertificateActionEventDetail;

/**
 * Enumeration of actions that require a popup menu to be shown to the user.
 * @enum {number}
 */
var CertificateAction = {
  DELETE: 0,
  EDIT: 1,
  EXPORT_PERSONAL: 2,
  IMPORT: 3,
};
