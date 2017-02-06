// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"

#include <stddef.h>

#include <memory>
#include <stack>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/service_process/service_process_control.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_proxy_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/service_messages.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend.h"

using content::BrowserThread;

CloudPrintProxyService::CloudPrintProxyService(Profile* profile)
    : profile_(profile),
      weak_factory_(this) {
}

CloudPrintProxyService::~CloudPrintProxyService() {
}

void CloudPrintProxyService::Initialize() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                            ServiceProcessControl::SERVICE_EVENT_INITIALIZE,
                            ServiceProcessControl::SERVICE_EVENT_MAX);
  if (profile_->GetPrefs()->HasPrefPath(prefs::kCloudPrintEmail) &&
      (!profile_->GetPrefs()->GetString(prefs::kCloudPrintEmail).empty() ||
       !profile_->GetPrefs()->GetBoolean(prefs::kCloudPrintProxyEnabled))) {
    // If the cloud print proxy is enabled, or the policy preventing it from
    // being enabled is set, establish a channel with the service process and
    // update the status. This will check the policy when the status is sent
    // back.
    UMA_HISTOGRAM_ENUMERATION(
        "CloudPrint.ServiceEvents",
        ServiceProcessControl::SERVICE_EVENT_ENABLED_ON_LAUNCH,
        ServiceProcessControl::SERVICE_EVENT_MAX);
    RefreshStatusFromService();
  }

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kCloudPrintProxyEnabled,
      base::Bind(
          base::IgnoreResult(
              &CloudPrintProxyService::ApplyCloudPrintConnectorPolicy),
          base::Unretained(this)));
}

void CloudPrintProxyService::RefreshStatusFromService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InvokeServiceTask(
      base::Bind(&CloudPrintProxyService::RefreshCloudPrintProxyStatus,
                 weak_factory_.GetWeakPtr()));
}

void CloudPrintProxyService::EnableForUserWithRobot(
    const std::string& robot_auth_code,
    const std::string& robot_email,
    const std::string& user_email,
    const base::DictionaryValue& user_preferences) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                            ServiceProcessControl::SERVICE_EVENT_ENABLE,
                            ServiceProcessControl::SERVICE_EVENT_MAX);
  if (profile_->GetPrefs()->GetBoolean(prefs::kCloudPrintProxyEnabled)) {
    InvokeServiceTask(
        base::Bind(&CloudPrintProxyService::EnableCloudPrintProxyWithRobot,
                   weak_factory_.GetWeakPtr(), robot_auth_code, robot_email,
                   user_email, base::Owned(user_preferences.DeepCopy())));
  }
}

void CloudPrintProxyService::DisableForUser() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.ServiceEvents",
                            ServiceProcessControl::SERVICE_EVENT_DISABLE,
                            ServiceProcessControl::SERVICE_EVENT_MAX);
  InvokeServiceTask(
      base::Bind(&CloudPrintProxyService::DisableCloudPrintProxy,
                 weak_factory_.GetWeakPtr()));
}

bool CloudPrintProxyService::ApplyCloudPrintConnectorPolicy() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!profile_->GetPrefs()->GetBoolean(prefs::kCloudPrintProxyEnabled)) {
    std::string email =
        profile_->GetPrefs()->GetString(prefs::kCloudPrintEmail);
    if (!email.empty()) {
      UMA_HISTOGRAM_ENUMERATION(
          "CloudPrint.ServiceEvents",
          ServiceProcessControl::SERVICE_EVENT_DISABLE_BY_POLICY,
          ServiceProcessControl::SERVICE_EVENT_MAX);
      DisableForUser();
      profile_->GetPrefs()->SetString(prefs::kCloudPrintEmail, std::string());
      return false;
    }
  }
  return true;
}

void CloudPrintProxyService::GetPrinters(const PrintersCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!profile_->GetPrefs()->GetBoolean(prefs::kCloudPrintProxyEnabled))
    return;

  base::FilePath list_path(
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kCloudPrintSetupProxy));
  if (!list_path.empty()) {
    std::string printers_json;
    base::ReadFileToString(list_path, &printers_json);
    std::unique_ptr<base::Value> value = base::JSONReader::Read(printers_json);
    base::ListValue* list = NULL;
    std::vector<std::string> printers;
    if (value && value->GetAsList(&list) && list) {
      for (size_t i = 0; i < list->GetSize(); ++i) {
        std::string printer;
        if (list->GetString(i, &printer))
          printers.push_back(printer);
      }
    }
    UMA_HISTOGRAM_COUNTS_10000("CloudPrint.AvailablePrintersList",
                               printers.size());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, printers));
  } else {
    InvokeServiceTask(
        base::Bind(&CloudPrintProxyService::GetCloudPrintProxyPrinters,
                   weak_factory_.GetWeakPtr(),
                   callback));
  }
}

void CloudPrintProxyService::GetCloudPrintProxyPrinters(
    const PrintersCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceProcessControl* process_control = GetServiceProcessControl();
  DCHECK(process_control->IsConnected());
  process_control->GetPrinters(callback);
}

void CloudPrintProxyService::RefreshCloudPrintProxyStatus() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceProcessControl* process_control = GetServiceProcessControl();
  DCHECK(process_control->IsConnected());
  ServiceProcessControl::CloudPrintProxyInfoCallback callback = base::Bind(
      &CloudPrintProxyService::ProxyInfoCallback, base::Unretained(this));
  process_control->GetCloudPrintProxyInfo(callback);
}

void CloudPrintProxyService::EnableCloudPrintProxyWithRobot(
    const std::string& robot_auth_code,
    const std::string& robot_email,
    const std::string& user_email,
    const base::DictionaryValue* user_preferences) {
  ServiceProcessControl* process_control = GetServiceProcessControl();
  DCHECK(process_control->IsConnected());
  process_control->Send(
      new ServiceMsg_EnableCloudPrintProxyWithRobot(
          robot_auth_code, robot_email, user_email, *user_preferences));
  // Assume the IPC worked.
  profile_->GetPrefs()->SetString(prefs::kCloudPrintEmail, user_email);
}

void CloudPrintProxyService::DisableCloudPrintProxy() {
  ServiceProcessControl* process_control = GetServiceProcessControl();
  DCHECK(process_control->IsConnected());
  process_control->Send(new ServiceMsg_DisableCloudPrintProxy);
  // Assume the IPC worked.
  profile_->GetPrefs()->SetString(prefs::kCloudPrintEmail, std::string());
}

void CloudPrintProxyService::ProxyInfoCallback(
    const cloud_print::CloudPrintProxyInfo& proxy_info) {
  proxy_id_ = proxy_info.proxy_id;
  profile_->GetPrefs()->SetString(
      prefs::kCloudPrintEmail,
      proxy_info.enabled ? proxy_info.email : std::string());
  ApplyCloudPrintConnectorPolicy();
}

bool CloudPrintProxyService::InvokeServiceTask(const base::Closure& task) {
  GetServiceProcessControl()->Launch(task, base::Closure());
  return true;
}

ServiceProcessControl* CloudPrintProxyService::GetServiceProcessControl() {
  return ServiceProcessControl::GetInstance();
}
