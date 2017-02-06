// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/dbus_statistics.h"

#include <memory>
#include <set>

#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

namespace dbus {

namespace {

// Used to store dbus statistics sorted alphabetically by service, interface,
// then method (using std::string <).
struct Stat {
  Stat(const std::string& service,
       const std::string& interface,
       const std::string& method)
      : service(service),
        interface(interface),
        method(method),
        sent_method_calls(0),
        received_signals(0),
        sent_blocking_method_calls(0) {
  }
  std::string service;
  std::string interface;
  std::string method;
  int sent_method_calls;
  int received_signals;
  int sent_blocking_method_calls;

  bool Compare(const Stat& other) const {
    if (service != other.service)
      return service < other.service;
    if (interface != other.interface)
      return interface < other.interface;
    return method < other.method;
  }

  struct PtrCompare {
    bool operator()(Stat* lhs, Stat* rhs) const {
      DCHECK(lhs && rhs);
      return lhs->Compare(*rhs);
    }
  };
};

typedef std::set<Stat*, Stat::PtrCompare> StatSet;

//------------------------------------------------------------------------------
// DBusStatistics

// Simple class for gathering DBus usage statistics.
class DBusStatistics {
 public:
  DBusStatistics()
      : start_time_(base::Time::Now()),
        origin_thread_id_(base::PlatformThread::CurrentId()) {
  }

  ~DBusStatistics() {
    DCHECK_EQ(origin_thread_id_, base::PlatformThread::CurrentId());
    STLDeleteContainerPointers(stats_.begin(), stats_.end());
  }

  // Enum to specify which field in Stat to increment in AddStat
  enum StatType {
    TYPE_SENT_METHOD_CALLS,
    TYPE_RECEIVED_SIGNALS,
    TYPE_SENT_BLOCKING_METHOD_CALLS
  };

  // Add a call to |method| for |interface|. See also MethodCall in message.h.
  void AddStat(const std::string& service,
               const std::string& interface,
               const std::string& method,
               StatType type) {
    if (base::PlatformThread::CurrentId() != origin_thread_id_) {
      DVLOG(1) << "Ignoring DBusStatistics::AddStat call from thread: "
               << base::PlatformThread::CurrentId();
      return;
    }
    Stat* stat = GetStat(service, interface, method, true);
    DCHECK(stat);
    if (type == TYPE_SENT_METHOD_CALLS)
      ++stat->sent_method_calls;
    else if (type == TYPE_RECEIVED_SIGNALS)
      ++stat->received_signals;
    else if (type == TYPE_SENT_BLOCKING_METHOD_CALLS)
      ++stat->sent_blocking_method_calls;
    else
      NOTREACHED();
  }

  // Look up the Stat entry in |stats_|. If |add_stat| is true, add a new entry
  // if one does not already exist.
  Stat* GetStat(const std::string& service,
                const std::string& interface,
                const std::string& method,
                bool add_stat) {
    DCHECK_EQ(origin_thread_id_, base::PlatformThread::CurrentId());
    std::unique_ptr<Stat> stat(new Stat(service, interface, method));
    StatSet::iterator found = stats_.find(stat.get());
    if (found != stats_.end())
      return *found;
    if (!add_stat)
      return NULL;
    found = stats_.insert(stat.release()).first;
    return *found;
  }

  StatSet& stats() { return stats_; }
  base::Time start_time() { return start_time_; }

 private:
  StatSet stats_;
  base::Time start_time_;
  base::PlatformThreadId origin_thread_id_;

  DISALLOW_COPY_AND_ASSIGN(DBusStatistics);
};

DBusStatistics* g_dbus_statistics = NULL;

}  // namespace

//------------------------------------------------------------------------------

namespace statistics {

void Initialize() {
  if (g_dbus_statistics)
    delete g_dbus_statistics;  // reset statistics
  g_dbus_statistics = new DBusStatistics();
}

void Shutdown() {
  delete g_dbus_statistics;
  g_dbus_statistics = NULL;
}

void AddSentMethodCall(const std::string& service,
                       const std::string& interface,
                       const std::string& method) {
  if (!g_dbus_statistics)
    return;
  g_dbus_statistics->AddStat(
      service, interface, method, DBusStatistics::TYPE_SENT_METHOD_CALLS);
}

void AddReceivedSignal(const std::string& service,
                       const std::string& interface,
                       const std::string& method) {
  if (!g_dbus_statistics)
    return;
  g_dbus_statistics->AddStat(
      service, interface, method, DBusStatistics::TYPE_RECEIVED_SIGNALS);
}

void AddBlockingSentMethodCall(const std::string& service,
                               const std::string& interface,
                               const std::string& method) {
  if (!g_dbus_statistics)
    return;
  g_dbus_statistics->AddStat(
      service, interface, method,
      DBusStatistics::TYPE_SENT_BLOCKING_METHOD_CALLS);
}

// NOTE: If the output format is changed, be certain to change the test
// expectations as well.
std::string GetAsString(ShowInString show, FormatString format) {
  if (!g_dbus_statistics)
    return "DBusStatistics not initialized.";

  const StatSet& stats = g_dbus_statistics->stats();
  if (stats.empty())
    return "No DBus calls.";

  base::TimeDelta dtime = base::Time::Now() - g_dbus_statistics->start_time();
  int dminutes = dtime.InMinutes();
  dminutes = std::max(dminutes, 1);

  std::string result;
  int sent = 0, received = 0, sent_blocking = 0;
  // Stats are stored in order by service, then interface, then method.
  for (StatSet::const_iterator iter = stats.begin(); iter != stats.end(); ) {
    StatSet::const_iterator cur_iter = iter;
    StatSet::const_iterator next_iter = ++iter;
    const Stat* stat = *cur_iter;
    sent += stat->sent_method_calls;
    received += stat->received_signals;
    sent_blocking += stat->sent_blocking_method_calls;
    // If this is not the last stat, and if the next stat matches the current
    // stat, continue.
    if (next_iter != stats.end() &&
        (*next_iter)->service == stat->service &&
        (show < SHOW_INTERFACE || (*next_iter)->interface == stat->interface) &&
        (show < SHOW_METHOD || (*next_iter)->method == stat->method))
      continue;

    if (!sent && !received && !sent_blocking)
        continue;  // No stats collected for this line, skip it and continue.

    // Add a line to the result and clear the counts.
    std::string line;
    if (show == SHOW_SERVICE) {
      line += stat->service;
    } else {
      // The interface usually includes the service so don't show both.
      line += stat->interface;
      if (show >= SHOW_METHOD)
        line += "." + stat->method;
    }
    line += base::StringPrintf(":");
    if (sent_blocking) {
      line += base::StringPrintf(" Sent (BLOCKING):");
      if (format == FORMAT_TOTALS)
        line += base::StringPrintf(" %d", sent_blocking);
      else if (format == FORMAT_PER_MINUTE)
        line += base::StringPrintf(" %d/min", sent_blocking / dminutes);
      else if (format == FORMAT_ALL)
        line += base::StringPrintf(" %d (%d/min)",
                                   sent_blocking, sent_blocking / dminutes);
    }
    if (sent) {
      line += base::StringPrintf(" Sent:");
      if (format == FORMAT_TOTALS)
        line += base::StringPrintf(" %d", sent);
      else if (format == FORMAT_PER_MINUTE)
        line += base::StringPrintf(" %d/min", sent / dminutes);
      else if (format == FORMAT_ALL)
        line += base::StringPrintf(" %d (%d/min)", sent, sent / dminutes);
    }
    if (received) {
      line += base::StringPrintf(" Received:");
      if (format == FORMAT_TOTALS)
        line += base::StringPrintf(" %d", received);
      else if (format == FORMAT_PER_MINUTE)
        line += base::StringPrintf(" %d/min", received / dminutes);
      else if (format == FORMAT_ALL)
        line += base::StringPrintf(
            " %d (%d/min)", received, received / dminutes);
    }
    result += line + "\n";
    sent = 0;
    sent_blocking = 0;
    received = 0;
  }
  return result;
}

namespace testing {

bool GetCalls(const std::string& service,
              const std::string& interface,
              const std::string& method,
              int* sent,
              int* received,
              int* blocking) {
  if (!g_dbus_statistics)
    return false;
  Stat* stat = g_dbus_statistics->GetStat(service, interface, method, false);
  if (!stat)
    return false;
  *sent = stat->sent_method_calls;
  *received = stat->received_signals;
  *blocking = stat->sent_blocking_method_calls;
  return true;
}

}  // namespace testing

}  // namespace statistics
}  // namespace dbus
