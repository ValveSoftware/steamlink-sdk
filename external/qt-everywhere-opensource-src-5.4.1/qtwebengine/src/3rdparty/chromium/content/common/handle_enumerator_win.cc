// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/handle_enumerator_win.h"

#include <windows.h>
#include <map>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "sandbox/win/src/handle_table.h"

using base::ASCIIToUTF16;

namespace content {
namespace {

typedef std::map<const base::string16, HandleType> HandleTypeMap;

HandleTypeMap& MakeHandleTypeMap() {
  HandleTypeMap& handle_types = *(new HandleTypeMap());
  handle_types[sandbox::HandleTable::kTypeProcess] = ProcessHandle;
  handle_types[sandbox::HandleTable::kTypeThread] = ThreadHandle;
  handle_types[sandbox::HandleTable::kTypeFile] = FileHandle;
  handle_types[sandbox::HandleTable::kTypeDirectory] = DirectoryHandle;
  handle_types[sandbox::HandleTable::kTypeKey] = KeyHandle;
  handle_types[sandbox::HandleTable::kTypeWindowStation] = WindowStationHandle;
  handle_types[sandbox::HandleTable::kTypeDesktop] = DesktopHandle;
  handle_types[sandbox::HandleTable::kTypeService] = ServiceHandle;
  handle_types[sandbox::HandleTable::kTypeMutex] = MutexHandle;
  handle_types[sandbox::HandleTable::kTypeSemaphore] = SemaphoreHandle;
  handle_types[sandbox::HandleTable::kTypeEvent] = EventHandle;
  handle_types[sandbox::HandleTable::kTypeTimer] = TimerHandle;
  handle_types[sandbox::HandleTable::kTypeNamedPipe] = NamedPipeHandle;
  handle_types[sandbox::HandleTable::kTypeJobObject] = JobHandle;
  handle_types[sandbox::HandleTable::kTypeFileMap] = FileMapHandle;
  handle_types[sandbox::HandleTable::kTypeAlpcPort] = AlpcPortHandle;

  return handle_types;
}

}  // namespace

const size_t kMaxHandleNameLength = 1024;

void HandleEnumerator::EnumerateHandles() {
  sandbox::HandleTable handles;
  std::string process_type =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  base::string16 output = ASCIIToUTF16(process_type);
  output.append(ASCIIToUTF16(" process - Handles at shutdown:\n"));
  for (sandbox::HandleTable::Iterator sys_handle
      = handles.HandlesForProcess(::GetCurrentProcessId());
      sys_handle != handles.end(); ++sys_handle) {
    HandleType current_type = StringToHandleType(sys_handle->Type());
    if (!all_handles_ && (current_type != ProcessHandle &&
                          current_type != FileHandle &&
                          current_type != DirectoryHandle &&
                          current_type != KeyHandle &&
                          current_type != WindowStationHandle &&
                          current_type != DesktopHandle &&
                          current_type != ServiceHandle))
      continue;

    output += ASCIIToUTF16("[");
    output += sys_handle->Type();
    output += ASCIIToUTF16("] (");
    output += sys_handle->Name();
    output += ASCIIToUTF16(")\n");
    output += GetAccessString(current_type,
        sys_handle->handle_entry()->GrantedAccess);
  }
  DVLOG(0) << output;
}

HandleType StringToHandleType(const base::string16& type) {
  static HandleTypeMap handle_types = MakeHandleTypeMap();
  HandleTypeMap::iterator result = handle_types.find(type);
  return result != handle_types.end() ? result->second : OtherHandle;
}

base::string16 GetAccessString(HandleType handle_type,
                                           ACCESS_MASK access) {
  base::string16 output;
  if (access & GENERIC_READ)
    output.append(ASCIIToUTF16("\tGENERIC_READ\n"));
  if (access & GENERIC_WRITE)
    output.append(ASCIIToUTF16("\tGENERIC_WRITE\n"));
  if (access & GENERIC_EXECUTE)
    output.append(ASCIIToUTF16("\tGENERIC_EXECUTE\n"));
  if (access & GENERIC_ALL)
    output.append(ASCIIToUTF16("\tGENERIC_ALL\n"));
  if (access & DELETE)
    output.append(ASCIIToUTF16("\tDELETE\n"));
  if (access & READ_CONTROL)
    output.append(ASCIIToUTF16("\tREAD_CONTROL\n"));
  if (access & WRITE_DAC)
    output.append(ASCIIToUTF16("\tWRITE_DAC\n"));
  if (access & WRITE_OWNER)
    output.append(ASCIIToUTF16("\tWRITE_OWNER\n"));
  if (access & SYNCHRONIZE)
    output.append(ASCIIToUTF16("\tSYNCHRONIZE\n"));

  switch (handle_type) {
    case ProcessHandle:
      if (access & PROCESS_CREATE_PROCESS)
        output.append(ASCIIToUTF16("\tPROCESS_CREATE_PROCESS\n"));
      if (access & PROCESS_CREATE_THREAD)
        output.append(ASCIIToUTF16("\tPROCESS_CREATE_THREAD\n"));
      if (access & PROCESS_DUP_HANDLE)
        output.append(ASCIIToUTF16("\tPROCESS_DUP_HANDLE\n"));
      if (access & PROCESS_QUERY_INFORMATION)
        output.append(ASCIIToUTF16("\tPROCESS_QUERY_INFORMATION\n"));
      if (access & PROCESS_QUERY_LIMITED_INFORMATION)
        output.append(ASCIIToUTF16("\tPROCESS_QUERY_LIMITED_INFORMATION\n"));
      if (access & PROCESS_SET_INFORMATION)
        output.append(ASCIIToUTF16("\tPROCESS_SET_INFORMATION\n"));
      if (access & PROCESS_SET_QUOTA)
        output.append(ASCIIToUTF16("\tPROCESS_SET_QUOTA\n"));
      if (access & PROCESS_SUSPEND_RESUME)
        output.append(ASCIIToUTF16("\tPROCESS_SUSPEND_RESUME\n"));
      if (access & PROCESS_TERMINATE)
        output.append(ASCIIToUTF16("\tPROCESS_TERMINATE\n"));
      if (access & PROCESS_VM_OPERATION)
        output.append(ASCIIToUTF16("\tPROCESS_VM_OPERATION\n"));
      if (access & PROCESS_VM_READ)
        output.append(ASCIIToUTF16("\tPROCESS_VM_READ\n"));
      if (access & PROCESS_VM_WRITE)
        output.append(ASCIIToUTF16("\tPROCESS_VM_WRITE\n"));
      break;
    case ThreadHandle:
      if (access & THREAD_DIRECT_IMPERSONATION)
        output.append(ASCIIToUTF16("\tTHREAD_DIRECT_IMPERSONATION\n"));
      if (access & THREAD_GET_CONTEXT)
        output.append(ASCIIToUTF16("\tTHREAD_GET_CONTEXT\n"));
      if (access & THREAD_IMPERSONATE)
        output.append(ASCIIToUTF16("\tTHREAD_IMPERSONATE\n"));
      if (access & THREAD_QUERY_INFORMATION )
        output.append(ASCIIToUTF16("\tTHREAD_QUERY_INFORMATION\n"));
      if (access & THREAD_QUERY_LIMITED_INFORMATION)
        output.append(ASCIIToUTF16("\tTHREAD_QUERY_LIMITED_INFORMATION\n"));
      if (access & THREAD_SET_CONTEXT)
        output.append(ASCIIToUTF16("\tTHREAD_SET_CONTEXT\n"));
      if (access & THREAD_SET_INFORMATION)
        output.append(ASCIIToUTF16("\tTHREAD_SET_INFORMATION\n"));
      if (access & THREAD_SET_LIMITED_INFORMATION)
        output.append(ASCIIToUTF16("\tTHREAD_SET_LIMITED_INFORMATION\n"));
      if (access & THREAD_SET_THREAD_TOKEN)
        output.append(ASCIIToUTF16("\tTHREAD_SET_THREAD_TOKEN\n"));
      if (access & THREAD_SUSPEND_RESUME)
        output.append(ASCIIToUTF16("\tTHREAD_SUSPEND_RESUME\n"));
      if (access & THREAD_TERMINATE)
        output.append(ASCIIToUTF16("\tTHREAD_TERMINATE\n"));
      break;
    case FileHandle:
      if (access & FILE_APPEND_DATA)
        output.append(ASCIIToUTF16("\tFILE_APPEND_DATA\n"));
      if (access & FILE_EXECUTE)
        output.append(ASCIIToUTF16("\tFILE_EXECUTE\n"));
      if (access & FILE_READ_ATTRIBUTES)
        output.append(ASCIIToUTF16("\tFILE_READ_ATTRIBUTES\n"));
      if (access & FILE_READ_DATA)
        output.append(ASCIIToUTF16("\tFILE_READ_DATA\n"));
      if (access & FILE_READ_EA)
        output.append(ASCIIToUTF16("\tFILE_READ_EA\n"));
      if (access & FILE_WRITE_ATTRIBUTES)
        output.append(ASCIIToUTF16("\tFILE_WRITE_ATTRIBUTES\n"));
      if (access & FILE_WRITE_DATA)
        output.append(ASCIIToUTF16("\tFILE_WRITE_DATA\n"));
      if (access & FILE_WRITE_EA)
        output.append(ASCIIToUTF16("\tFILE_WRITE_EA\n"));
      break;
    case DirectoryHandle:
      if (access & FILE_ADD_FILE)
        output.append(ASCIIToUTF16("\tFILE_ADD_FILE\n"));
      if (access & FILE_ADD_SUBDIRECTORY)
        output.append(ASCIIToUTF16("\tFILE_ADD_SUBDIRECTORY\n"));
      if (access & FILE_APPEND_DATA)
        output.append(ASCIIToUTF16("\tFILE_APPEND_DATA\n"));
      if (access & FILE_DELETE_CHILD)
        output.append(ASCIIToUTF16("\tFILE_DELETE_CHILD\n"));
      if (access & FILE_LIST_DIRECTORY)
        output.append(ASCIIToUTF16("\tFILE_LIST_DIRECTORY\n"));
      if (access & FILE_READ_DATA)
        output.append(ASCIIToUTF16("\tFILE_READ_DATA\n"));
      if (access & FILE_TRAVERSE)
        output.append(ASCIIToUTF16("\tFILE_TRAVERSE\n"));
      if (access & FILE_WRITE_DATA)
        output.append(ASCIIToUTF16("\tFILE_WRITE_DATA\n"));
      break;
    case KeyHandle:
      if (access & KEY_CREATE_LINK)
        output.append(ASCIIToUTF16("\tKEY_CREATE_LINK\n"));
      if (access & KEY_CREATE_SUB_KEY)
        output.append(ASCIIToUTF16("\tKEY_CREATE_SUB_KEY\n"));
      if (access & KEY_ENUMERATE_SUB_KEYS)
        output.append(ASCIIToUTF16("\tKEY_ENUMERATE_SUB_KEYS\n"));
      if (access & KEY_EXECUTE)
        output.append(ASCIIToUTF16("\tKEY_EXECUTE\n"));
      if (access & KEY_NOTIFY)
        output.append(ASCIIToUTF16("\tKEY_NOTIFY\n"));
      if (access & KEY_QUERY_VALUE)
        output.append(ASCIIToUTF16("\tKEY_QUERY_VALUE\n"));
      if (access & KEY_READ)
        output.append(ASCIIToUTF16("\tKEY_READ\n"));
      if (access & KEY_SET_VALUE)
        output.append(ASCIIToUTF16("\tKEY_SET_VALUE\n"));
      if (access & KEY_WOW64_32KEY)
        output.append(ASCIIToUTF16("\tKEY_WOW64_32KEY\n"));
      if (access & KEY_WOW64_64KEY)
        output.append(ASCIIToUTF16("\tKEY_WOW64_64KEY\n"));
      break;
    case WindowStationHandle:
      if (access & WINSTA_ACCESSCLIPBOARD)
        output.append(ASCIIToUTF16("\tWINSTA_ACCESSCLIPBOARD\n"));
      if (access & WINSTA_ACCESSGLOBALATOMS)
        output.append(ASCIIToUTF16("\tWINSTA_ACCESSGLOBALATOMS\n"));
      if (access & WINSTA_CREATEDESKTOP)
        output.append(ASCIIToUTF16("\tWINSTA_CREATEDESKTOP\n"));
      if (access & WINSTA_ENUMDESKTOPS)
        output.append(ASCIIToUTF16("\tWINSTA_ENUMDESKTOPS\n"));
      if (access & WINSTA_ENUMERATE)
        output.append(ASCIIToUTF16("\tWINSTA_ENUMERATE\n"));
      if (access & WINSTA_EXITWINDOWS)
        output.append(ASCIIToUTF16("\tWINSTA_EXITWINDOWS\n"));
      if (access & WINSTA_READATTRIBUTES)
        output.append(ASCIIToUTF16("\tWINSTA_READATTRIBUTES\n"));
      if (access & WINSTA_READSCREEN)
        output.append(ASCIIToUTF16("\tWINSTA_READSCREEN\n"));
      if (access & WINSTA_WRITEATTRIBUTES)
        output.append(ASCIIToUTF16("\tWINSTA_WRITEATTRIBUTES\n"));
      break;
    case DesktopHandle:
      if (access & DESKTOP_CREATEMENU)
        output.append(ASCIIToUTF16("\tDESKTOP_CREATEMENU\n"));
      if (access & DESKTOP_CREATEWINDOW)
        output.append(ASCIIToUTF16("\tDESKTOP_CREATEWINDOW\n"));
      if (access & DESKTOP_ENUMERATE)
        output.append(ASCIIToUTF16("\tDESKTOP_ENUMERATE\n"));
      if (access & DESKTOP_HOOKCONTROL)
        output.append(ASCIIToUTF16("\tDESKTOP_HOOKCONTROL\n"));
      if (access & DESKTOP_JOURNALPLAYBACK)
        output.append(ASCIIToUTF16("\tDESKTOP_JOURNALPLAYBACK\n"));
      if (access & DESKTOP_JOURNALRECORD)
        output.append(ASCIIToUTF16("\tDESKTOP_JOURNALRECORD\n"));
      if (access & DESKTOP_READOBJECTS)
        output.append(ASCIIToUTF16("\tDESKTOP_READOBJECTS\n"));
      if (access & DESKTOP_SWITCHDESKTOP)
        output.append(ASCIIToUTF16("\tDESKTOP_SWITCHDESKTOP\n"));
      if (access & DESKTOP_WRITEOBJECTS)
        output.append(ASCIIToUTF16("\tDESKTOP_WRITEOBJECTS\n"));
      break;
    case ServiceHandle:
      if (access & SC_MANAGER_CREATE_SERVICE)
        output.append(ASCIIToUTF16("\tSC_MANAGER_CREATE_SERVICE\n"));
      if (access & SC_MANAGER_CONNECT)
        output.append(ASCIIToUTF16("\tSC_MANAGER_CONNECT\n"));
      if (access & SC_MANAGER_ENUMERATE_SERVICE )
        output.append(ASCIIToUTF16("\tSC_MANAGER_ENUMERATE_SERVICE\n"));
      if (access & SC_MANAGER_LOCK)
        output.append(ASCIIToUTF16("\tSC_MANAGER_LOCK\n"));
      if (access & SC_MANAGER_MODIFY_BOOT_CONFIG )
        output.append(ASCIIToUTF16("\tSC_MANAGER_MODIFY_BOOT_CONFIG\n"));
      if (access & SC_MANAGER_QUERY_LOCK_STATUS )
        output.append(ASCIIToUTF16("\tSC_MANAGER_QUERY_LOCK_STATUS\n"));
      break;
    case EventHandle:
      if (access & EVENT_MODIFY_STATE)
        output.append(ASCIIToUTF16("\tEVENT_MODIFY_STATE\n"));
      break;
    case MutexHandle:
      if (access & MUTEX_MODIFY_STATE)
        output.append(ASCIIToUTF16("\tMUTEX_MODIFY_STATE\n"));
      break;
    case SemaphoreHandle:
      if (access & SEMAPHORE_MODIFY_STATE)
        output.append(ASCIIToUTF16("\tSEMAPHORE_MODIFY_STATE\n"));
      break;
    case TimerHandle:
      if (access & TIMER_MODIFY_STATE)
        output.append(ASCIIToUTF16("\tTIMER_MODIFY_STATE\n"));
      if (access & TIMER_QUERY_STATE)
        output.append(ASCIIToUTF16("\tTIMER_QUERY_STATE\n"));
      break;
    case NamedPipeHandle:
      if (access & PIPE_ACCESS_INBOUND)
        output.append(ASCIIToUTF16("\tPIPE_ACCESS_INBOUND\n"));
      if (access & PIPE_ACCESS_OUTBOUND)
        output.append(ASCIIToUTF16("\tPIPE_ACCESS_OUTBOUND\n"));
      break;
    case JobHandle:
      if (access & JOB_OBJECT_ASSIGN_PROCESS)
        output.append(ASCIIToUTF16("\tJOB_OBJECT_ASSIGN_PROCESS\n"));
      if (access & JOB_OBJECT_QUERY)
        output.append(ASCIIToUTF16("\tJOB_OBJECT_QUERY\n"));
      if (access & JOB_OBJECT_SET_ATTRIBUTES)
        output.append(ASCIIToUTF16("\tJOB_OBJECT_SET_ATTRIBUTES\n"));
      if (access & JOB_OBJECT_SET_SECURITY_ATTRIBUTES)
        output.append(ASCIIToUTF16("\tJOB_OBJECT_SET_SECURITY_ATTRIBUTES\n"));
      if (access & JOB_OBJECT_TERMINATE)
        output.append(ASCIIToUTF16("\tJOB_OBJECT_TERMINATE\n"));
      break;
    case FileMapHandle:
      if (access & FILE_MAP_EXECUTE)
        output.append(ASCIIToUTF16("\tFILE_MAP_EXECUTE\n"));
      if (access & FILE_MAP_READ)
        output.append(ASCIIToUTF16("\tFILE_MAP_READ\n"));
      if (access & FILE_MAP_WRITE)
        output.append(ASCIIToUTF16("\tFILE_MAP_WRITE\n"));
      break;
  }
  return output;
}

}  // namespace content
