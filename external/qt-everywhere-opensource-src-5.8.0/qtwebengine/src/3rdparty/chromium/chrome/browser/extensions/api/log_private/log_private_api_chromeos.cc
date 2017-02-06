// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/log_private/log_private_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/api/log_private/filter_handler.h"
#include "chrome/browser/extensions/api/log_private/log_parser.h"
#include "chrome/browser/extensions/api/log_private/syslog_parser.h"
#include "chrome/browser/feedback/system_logs/scrubbed_system_logs_fetcher.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/log_private.h"
#include "chrome/common/logging_chrome.h"
#include "components/net_log/chrome_net_log.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/granted_file_entry.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system_logs/debug_log_writer.h"
#endif

using content::BrowserThread;

namespace events {
const char kOnCapturedEvents[] = "logPrivate.onCapturedEvents";
}  // namespace events

namespace extensions {
namespace {

const char kAppLogsSubdir[] = "apps";
const char kLogDumpsSubdir[] = "log_dumps";
const char kLogFileNameBase[] = "net-internals";
const int kNetLogEventDelayMilliseconds = 100;

// Gets sequenced task runner for file specific calls within this API.
scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner() {
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  return pool->GetSequencedTaskRunnerWithShutdownBehavior(
      pool->GetNamedSequenceToken(FileResource::kSequenceToken),
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);
}

// Checks if we are running on sequenced task runner thread.
bool IsRunningOnSequenceThread() {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  return pool->IsRunningSequenceOnCurrentThread(
      pool->GetNamedSequenceToken(FileResource::kSequenceToken));
}

std::unique_ptr<LogParser> CreateLogParser(const std::string& log_type) {
  if (log_type == "syslog")
    return std::unique_ptr<LogParser>(new SyslogParser());
  // TODO(shinfan): Add more parser here

  NOTREACHED() << "Invalid log type: " << log_type;
  return std::unique_ptr<LogParser>();
}

void CollectLogInfo(FilterHandler* filter_handler,
                    system_logs::SystemLogsResponse* logs,
                    std::vector<api::log_private::LogEntry>* output) {
  for (system_logs::SystemLogsResponse::const_iterator
      request_it = logs->begin(); request_it != logs->end(); ++request_it) {
    if (!filter_handler->IsValidSource(request_it->first)) {
      continue;
    }
    std::unique_ptr<LogParser> parser(CreateLogParser(request_it->first));
    if (parser) {
      parser->Parse(request_it->second, output, filter_handler);
    }
  }
}

// Returns directory location of app-specific logs that are initiated with
// logPrivate.startEventRecorder() calls - /home/chronos/user/log/apps
base::FilePath GetAppLogDirectory() {
  return logging::GetSessionLogDir(*base::CommandLine::ForCurrentProcess())
      .Append(kAppLogsSubdir);
}

// Returns directory location where logs dumps initiated with chrome.dumpLogs
// will be stored - /home/chronos/<user_profile_dir>/Downloads/log_dumps
base::FilePath GetLogDumpDirectory(content::BrowserContext* context) {
  const DownloadPrefs* const prefs = DownloadPrefs::FromBrowserContext(context);
  return prefs->DownloadPath().Append(kLogDumpsSubdir);
}

// Removes direcotry content of |logs_dumps| and |app_logs_dir| (only for the
// primary profile).
void CleanUpLeftoverLogs(bool is_primary_profile,
                         const base::FilePath& app_logs_dir,
                         const base::FilePath& logs_dumps) {
  LOG(WARNING) << "Deleting " << app_logs_dir.value();
  LOG(WARNING) << "Deleting " << logs_dumps.value();

  DCHECK(IsRunningOnSequenceThread());
  base::DeleteFile(logs_dumps, true);

  // App-specific logs are stored in /home/chronos/user/log/apps directory that
  // is shared between all profiles in multi-profile case. We should not
  // nuke it for non-primary profiles.
  if (!is_primary_profile)
    return;

  base::DeleteFile(app_logs_dir, true);
}

}  // namespace

const char FileResource::kSequenceToken[] = "log_api_files";

FileResource::FileResource(const std::string& owner_extension_id,
                           const base::FilePath& path)
    : ApiResource(owner_extension_id), path_(path) {
}

FileResource::~FileResource() {
  base::DeleteFile(path_, true);
}

bool FileResource::IsPersistent() const {
  return false;
}

// static
LogPrivateAPI* LogPrivateAPI::Get(content::BrowserContext* context) {
  LogPrivateAPI* api = GetFactoryInstance()->Get(context);
  api->Initialize();
  return api;
}

LogPrivateAPI::LogPrivateAPI(content::BrowserContext* context)
    : browser_context_(context),
      logging_net_internals_(false),
      event_sink_(api::log_private::EVENT_SINK_CAPTURE),
      extension_registry_observer_(this),
      log_file_resources_(context),
      initialized_(false) {
}

LogPrivateAPI::~LogPrivateAPI() {
}

void LogPrivateAPI::StartNetInternalsWatch(
    const std::string& extension_id,
    api::log_private::EventSink event_sink,
    const base::Closure& closure) {
  net_internal_watches_.insert(extension_id);

  // Nuke any leftover app-specific or dumped log files from previous sessions.
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&LogPrivateAPI::MaybeStartNetInternalLogging,
                 base::Unretained(this),
                 extension_id,
                 g_browser_process->io_thread(),
                 event_sink),
      closure);
}

void LogPrivateAPI::StopNetInternalsWatch(const std::string& extension_id,
                                          const base::Closure& closure) {
  net_internal_watches_.erase(extension_id);
  MaybeStopNetInternalLogging(closure);
}

void LogPrivateAPI::StopAllWatches(const std::string& extension_id,
                                   const base::Closure& closure) {
  StopNetInternalsWatch(extension_id, closure);
}

void LogPrivateAPI::RegisterTempFile(const std::string& owner_extension_id,
                                     const base::FilePath& file_path) {
  if (!IsRunningOnSequenceThread()) {
    GetSequencedTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&LogPrivateAPI::RegisterTempFile,
                   base::Unretained(this),
                   owner_extension_id,
                   file_path));
    return;
  }

  log_file_resources_.Add(new FileResource(owner_extension_id, file_path));
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<LogPrivateAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<LogPrivateAPI>*
LogPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void LogPrivateAPI::OnAddEntry(const net::NetLog::Entry& entry) {
  // We could receive events on whatever thread they happen to be generated,
  // since we are only interested in network events, we should ignore any
  // other thread than BrowserThread::IO.
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    return;
  }

  if (!pending_entries_.get()) {
    pending_entries_.reset(new base::ListValue());
    BrowserThread::PostDelayedTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&LogPrivateAPI::PostPendingEntries, base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(kNetLogEventDelayMilliseconds));
  }
  pending_entries_->Append(entry.ToValue());
}

void LogPrivateAPI::PostPendingEntries() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&LogPrivateAPI:: AddEntriesOnUI,
                 base::Unretained(this),
                 base::Passed(&pending_entries_)));
}

void LogPrivateAPI::AddEntriesOnUI(std::unique_ptr<base::ListValue> value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (std::set<std::string>::iterator ix = net_internal_watches_.begin();
       ix != net_internal_watches_.end(); ++ix) {
    // Create the event's arguments value.
    std::unique_ptr<base::ListValue> event_args(new base::ListValue());
    event_args->Append(value->DeepCopy());
    std::unique_ptr<Event> event(
        new Event(::extensions::events::LOG_PRIVATE_ON_CAPTURED_EVENTS,
                  ::events::kOnCapturedEvents, std::move(event_args)));
    EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(*ix, std::move(event));
  }
}

void LogPrivateAPI::CreateTempNetLogFile(const std::string& owner_extension_id,
                                         base::ScopedFILE* file) {
  DCHECK(IsRunningOnSequenceThread());

  // Create app-specific subdirectory in session logs folder.
  base::FilePath app_log_dir = GetAppLogDirectory().Append(owner_extension_id);
  if (!base::DirectoryExists(app_log_dir)) {
    if (!base::CreateDirectory(app_log_dir)) {
      LOG(ERROR) << "Could not create dir " << app_log_dir.value();
      return;
    }
  }

  base::FilePath file_path = app_log_dir.Append(kLogFileNameBase);
  file_path = logging::GenerateTimestampedName(file_path, base::Time::Now());
  FILE* file_ptr = fopen(file_path.value().c_str(), "w");
  if (file_ptr == nullptr) {
    LOG(ERROR) << "Could not open " << file_path.value();
    return;
  }

  RegisterTempFile(owner_extension_id, file_path);
  return file->reset(file_ptr);
}

void LogPrivateAPI::StartObservingNetEvents(
    IOThread* io_thread,
    base::ScopedFILE* file) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!file->get())
    return;

  write_to_file_observer_.reset(new net::WriteToFileNetLogObserver());
  write_to_file_observer_->set_capture_mode(
      net::NetLogCaptureMode::IncludeCookiesAndCredentials());
  write_to_file_observer_->StartObserving(io_thread->net_log(),
                                          std::move(*file), nullptr, nullptr);
}

void LogPrivateAPI::MaybeStartNetInternalLogging(
    const std::string& caller_extension_id,
    IOThread* io_thread,
    api::log_private::EventSink event_sink) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!logging_net_internals_) {
    logging_net_internals_ = true;
    event_sink_ = event_sink;
    switch (event_sink_) {
      case api::log_private::EVENT_SINK_CAPTURE: {
        io_thread->net_log()->DeprecatedAddObserver(
            this, net::NetLogCaptureMode::IncludeCookiesAndCredentials());
        break;
      }
      case api::log_private::EVENT_SINK_FILE: {
        base::ScopedFILE* file = new base::ScopedFILE();
        // Initialize a FILE on the blocking pool and start observing event
        // on IO thread.
        GetSequencedTaskRunner()->PostTaskAndReply(
            FROM_HERE,
            base::Bind(&LogPrivateAPI::CreateTempNetLogFile,
                       base::Unretained(this),
                       caller_extension_id,
                       file),
            base::Bind(&LogPrivateAPI::StartObservingNetEvents,
                       base::Unretained(this),
                       io_thread,
                       base::Owned(file)));
        break;
      }
      case api::log_private::EVENT_SINK_NONE: {
        NOTREACHED();
        break;
      }
    }
  }
}

void LogPrivateAPI::MaybeStopNetInternalLogging(const base::Closure& closure) {
  if (net_internal_watches_.empty()) {
    if (closure.is_null()) {
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(&LogPrivateAPI::StopNetInternalLogging,
                     base::Unretained(this)));
    } else {
      BrowserThread::PostTaskAndReply(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(&LogPrivateAPI::StopNetInternalLogging,
                     base::Unretained(this)),
          closure);
    }
  }
}

void LogPrivateAPI::StopNetInternalLogging() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (net_log() && logging_net_internals_) {
    logging_net_internals_ = false;
    switch (event_sink_) {
      case api::log_private::EVENT_SINK_CAPTURE:
        net_log()->DeprecatedRemoveObserver(this);
        break;
      case api::log_private::EVENT_SINK_FILE:
        write_to_file_observer_->StopObserving(nullptr);
        write_to_file_observer_.reset();
        break;
      case api::log_private::EVENT_SINK_NONE:
        NOTREACHED();
        break;
    }
  }
}

void LogPrivateAPI::Initialize() {
  if (initialized_)
    return;

  // Clean up temp files and folders from the previous sessions.
  initialized_ = true;
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
  GetSequencedTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&CleanUpLeftoverLogs,
                 Profile::FromBrowserContext(browser_context_) ==
                     ProfileManager::GetPrimaryUserProfile(),
                 GetAppLogDirectory(),
                 GetLogDumpDirectory(browser_context_)));
}

void LogPrivateAPI::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  StopNetInternalsWatch(extension->id(), base::Closure());
}

LogPrivateGetHistoricalFunction::LogPrivateGetHistoricalFunction() {
}

LogPrivateGetHistoricalFunction::~LogPrivateGetHistoricalFunction() {
}

bool LogPrivateGetHistoricalFunction::RunAsync() {
  // Get parameters
  std::unique_ptr<api::log_private::GetHistorical::Params> params(
      api::log_private::GetHistorical::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  filter_handler_.reset(new FilterHandler(params->filter));

  system_logs::SystemLogsFetcherBase* fetcher;
  if ((params->filter).scrub) {
    fetcher = new system_logs::ScrubbedSystemLogsFetcher();
  } else {
    fetcher = new system_logs::AboutSystemLogsFetcher();
  }
  fetcher->Fetch(
      base::Bind(&LogPrivateGetHistoricalFunction::OnSystemLogsLoaded, this));

  return true;
}

void LogPrivateGetHistoricalFunction::OnSystemLogsLoaded(
    std::unique_ptr<system_logs::SystemLogsResponse> sys_info) {
  // Prepare result
  api::log_private::Result result;
  CollectLogInfo(filter_handler_.get(), sys_info.get(), &result.data);
  api::log_private::Filter::Populate(
      *((filter_handler_->GetFilter())->ToValue()), &result.filter);
  SetResult(result.ToValue());
  SendResponse(true);
}

LogPrivateStartEventRecorderFunction::LogPrivateStartEventRecorderFunction() {
}

LogPrivateStartEventRecorderFunction::~LogPrivateStartEventRecorderFunction() {
}

bool LogPrivateStartEventRecorderFunction::RunAsync() {
  std::unique_ptr<api::log_private::StartEventRecorder::Params> params(
      api::log_private::StartEventRecorder::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  switch (params->event_type) {
    case api::log_private::EVENT_TYPE_NETWORK:
      LogPrivateAPI::Get(Profile::FromBrowserContext(browser_context()))
          ->StartNetInternalsWatch(
              extension_id(),
              params->sink,
              base::Bind(
                  &LogPrivateStartEventRecorderFunction::OnEventRecorderStarted,
                  this));
      break;
    case api::log_private::EVENT_TYPE_NONE:
      NOTREACHED();
      return false;
  }

  return true;
}

void LogPrivateStartEventRecorderFunction::OnEventRecorderStarted() {
  SendResponse(true);
}

LogPrivateStopEventRecorderFunction::LogPrivateStopEventRecorderFunction() {
}

LogPrivateStopEventRecorderFunction::~LogPrivateStopEventRecorderFunction() {
}

bool LogPrivateStopEventRecorderFunction::RunAsync() {
  std::unique_ptr<api::log_private::StopEventRecorder::Params> params(
      api::log_private::StopEventRecorder::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  switch (params->event_type) {
    case api::log_private::EVENT_TYPE_NETWORK:
      LogPrivateAPI::Get(Profile::FromBrowserContext(browser_context()))
          ->StopNetInternalsWatch(
              extension_id(),
              base::Bind(
                  &LogPrivateStopEventRecorderFunction::OnEventRecorderStopped,
                  this));
      break;
    case api::log_private::EVENT_TYPE_NONE:
      NOTREACHED();
      return false;
  }
  return true;
}

void LogPrivateStopEventRecorderFunction::OnEventRecorderStopped() {
  SendResponse(true);
}

LogPrivateDumpLogsFunction::LogPrivateDumpLogsFunction() {
}

LogPrivateDumpLogsFunction::~LogPrivateDumpLogsFunction() {
}

bool LogPrivateDumpLogsFunction::RunAsync() {
  LogPrivateAPI::Get(Profile::FromBrowserContext(browser_context()))
      ->StopAllWatches(
          extension_id(),
          base::Bind(&LogPrivateDumpLogsFunction::OnStopAllWatches, this));
  return true;
}

void LogPrivateDumpLogsFunction::OnStopAllWatches() {
  chromeos::DebugLogWriter::StoreCombinedLogs(
      GetLogDumpDirectory(browser_context()).Append(extension_id()),
      FileResource::kSequenceToken,
      base::Bind(&LogPrivateDumpLogsFunction::OnStoreLogsCompleted, this));
}

void LogPrivateDumpLogsFunction::OnStoreLogsCompleted(
    const base::FilePath& log_path,
    bool succeeded) {
  if (succeeded) {
    LogPrivateAPI::Get(Profile::FromBrowserContext(browser_context()))
        ->RegisterTempFile(extension_id(), log_path);
  }

  std::unique_ptr<base::DictionaryValue> response(new base::DictionaryValue());
  extensions::GrantedFileEntry file_entry =
      extensions::app_file_handler_util::CreateFileEntry(
          Profile::FromBrowserContext(browser_context()),
          extension(),
          render_frame_host()->GetProcess()->GetID(),
          log_path,
          false);

  base::DictionaryValue* entry = new base::DictionaryValue();
  entry->SetString("fileSystemId", file_entry.filesystem_id);
  entry->SetString("baseName", file_entry.registered_name);
  entry->SetString("id", file_entry.id);
  entry->SetBoolean("isDirectory", false);
  base::ListValue* entry_list = new base::ListValue();
  entry_list->Append(entry);
  response->Set("entries", entry_list);
  response->SetBoolean("multiple", false);
  SetResult(std::move(response));
  SendResponse(succeeded);
}

}  // namespace extensions
