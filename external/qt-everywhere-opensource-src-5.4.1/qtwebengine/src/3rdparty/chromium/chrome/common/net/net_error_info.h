// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_NET_ERROR_INFO_H_
#define CHROME_COMMON_NET_NET_ERROR_INFO_H_

namespace chrome_common_net {

// Network error page events.  Used for UMA statistics.
enum NetworkErrorPageEvent {
  NETWORK_ERROR_PAGE_SHOWN,                      // Error pages shown.

  NETWORK_ERROR_PAGE_RELOAD_BUTTON_SHOWN,        // Reload buttons shown.
  NETWORK_ERROR_PAGE_RELOAD_BUTTON_CLICKED,      // Reload button clicked.
  NETWORK_ERROR_PAGE_RELOAD_BUTTON_ERROR,        // Reload button clicked
                                                 // -> error.

  NETWORK_ERROR_PAGE_LOAD_STALE_BUTTON_SHOWN,    // Load stale buttons shown.
  NETWORK_ERROR_PAGE_LOAD_STALE_BUTTON_CLICKED,  // Load stale button clicked.
  NETWORK_ERROR_PAGE_LOAD_STALE_BUTTON_ERROR,    // Load stale buttons -> error.

  NETWORK_ERROR_PAGE_MORE_BUTTON_CLICKED,        // More button clicked.

  NETWORK_ERROR_PAGE_BROWSER_INITIATED_RELOAD,   // Reload from browser.

  NETWORK_ERROR_PAGE_EVENT_MAX,
};

// The status of a DNS probe that the NetErrorTabHelper may or may not have
// started.
//
// The DNS_PROBE_FINISHED_* values are used in histograms, so:
// 1. FINISHED_UNKNOWN must remain the first FINISHED_* value.
// 2. FINISHED_* values must not be rearranged relative to FINISHED_UNKNOWN.
// 3. New FINISHED_* values must be inserted at the end.
// 4. New non-FINISHED_* values cannot be inserted.
enum DnsProbeStatus {
  // A DNS probe may be run for this error page.  (This status is only used on
  // the renderer side before it's received a status update from the browser.)
  DNS_PROBE_POSSIBLE,

  // A DNS probe will not be run for this error page.  (This happens if the
  // user has the "Use web service to resolve navigation errors" preference
  // turned off, or if probes are disabled by the field trial.)
  DNS_PROBE_NOT_RUN,

  // A DNS probe has been started for this error page.  The renderer should
  // expect to receive another IPC with one of the FINISHED statuses once the
  // probe has finished (as long as the error page is still loaded).
  DNS_PROBE_STARTED,

  // A DNS probe has finished with one of the following results:

  // The probe was inconclusive.
  DNS_PROBE_FINISHED_INCONCLUSIVE,

  // There's no internet connection.
  DNS_PROBE_FINISHED_NO_INTERNET,

  // The DNS configuration is wrong, or the servers are down or broken.
  DNS_PROBE_FINISHED_BAD_CONFIG,

  // The DNS servers are working fine, so the domain must not exist.
  DNS_PROBE_FINISHED_NXDOMAIN,

  DNS_PROBE_MAX
};

// Returns a string representing |status|.  It should be simply the name of
// the value as a string, but don't rely on that.  This is presented to the
// user as part of the DNS error page (as the error code, at the bottom),
// and is also used in some verbose log messages.
//
// |status| is an int because error codes are ints by the time they get to the
// localized error system, and we don't want to require the caller to cast back
// to a probe status.  The function will NOTREACHED() and return an empty
// string if given an int that does not match a value in DnsProbeStatus (or if
// it is DNS_PROBE_MAX, which is not a real status).
const char* DnsProbeStatusToString(int status);

// Returns true if |status| is one of the DNS_PROBE_FINISHED_* statuses.
bool DnsProbeStatusIsFinished(DnsProbeStatus status);

// Record specific error page events.
void RecordEvent(NetworkErrorPageEvent event);

// The error domain used to pass DNS probe statuses to the localized error
// code.
extern const char kDnsProbeErrorDomain[];

}  // namespace chrome_common_net

#endif  // CHROME_COMMON_NET_NET_ERROR_INFO_H_
