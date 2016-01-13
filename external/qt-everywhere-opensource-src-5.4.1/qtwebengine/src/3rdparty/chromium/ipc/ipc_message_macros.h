// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defining IPC Messages
//
// Your IPC messages will be defined by macros inside of an XXX_messages.h
// header file.  Most of the time, the system can automatically generate all
// of messaging mechanism from these definitions, but sometimes some manual
// coding is required.  In these cases, you will also have an XXX_messages.cc
// implemation file as well.
//
// The senders of your messages will include your XXX_messages.h file to
// get the full set of definitions they need to send your messages.
//
// Each XXX_messages.h file must be registered with the IPC system.  This
// requires adding two things:
//   - An XXXMsgStart value to the IPCMessageStart enum in ipc_message_start.h
//   - An inclusion of XXX_messages.h file in a message generator .h file
//
// The XXXMsgStart value is an enumeration that ensures uniqueness for
// each different message file.  Later, you will use this inside your
// XXX_messages.h file before invoking message declaration macros:
//     #define IPC_MESSAGE_START XXXMsgStart
//       ( ... your macro invocations go here ... )
//
// Message Generator Files
//
// A message generator .h header file pulls in all other message-declaring
// headers for a given component.  It is included by a message generator
// .cc file, which is where all the generated code will wind up.  Typically,
// you will use an existing generator (e.g. common_message_generator.cc
// in /chrome/common), but there are circumstances where you may add a
// new one.
//
// In the rare cicrucmstances where you can't re-use an existing file,
// your YYY_message_generator.cc file for a component YYY would contain
// the following code:
//     // Get basic type definitions.
//     #define IPC_MESSAGE_IMPL
//     #include "path/to/YYY_message_generator.h"
//     // Generate constructors.
//     #include "ipc/struct_constructor_macros.h"
//     #include "path/to/YYY_message_generator.h"
//     // Generate destructors.
//     #include "ipc/struct_destructor_macros.h"
//     #include "path/to/YYY_message_generator.h"
//     // Generate param traits write methods.
//     #include "ipc/param_traits_write_macros.h"
//     namespace IPC {
//     #include "path/to/YYY_message_generator.h"
//     }  // namespace IPC
//     // Generate param traits read methods.
//     #include "ipc/param_traits_read_macros.h"
//     namespace IPC {
//     #include "path/to/YYY_message_generator.h"
//     }  // namespace IPC
//     // Generate param traits log methods.
//     #include "ipc/param_traits_log_macros.h"
//     namespace IPC {
//     #include "path/to/YYY_message_generator.h"
//     }  // namespace IPC
//
// In cases where manual generation is required, in your XXX_messages.cc
// file, put the following after all the includes for param types:
//     #define IPC_MESSAGE_IMPL
//     #include "XXX_messages.h"
//        (... implementation of traits not auto-generated ...)
//
// Multiple Inclusion
//
// The XXX_messages.h file will be multiply-included by the
// YYY_message_generator.cc file, so your XXX_messages file can't be
// guarded in the usual manner.  Ideally, there will be no need for any
// inclusion guard, since the XXX_messages.h file should consist soley
// of inclusions of other headers (which are self-guarding) and IPC
// macros (which are multiply evaluating).
//
// Note that #pragma once cannot be used here; doing so would mark the whole
// file as being singly-included.  Since your XXX_messages.h file is only
// partially-guarded, care must be taken to ensure that it is only included
// by other .cc files (and the YYY_message_generator.h file).  Including an
// XXX_messages.h file in some other .h file may result in duplicate
// declarations and a compilation failure.
//
// Type Declarations
//
// It is generally a bad idea to have type definitions in a XXX_messages.h
// file; most likely the typedef will then be used in the message, as opposed
// to the struct iself.  Later, an IPC message dispatcher wil need to call
// a function taking that type, and that function is declared in some other
// header.  Thus, in order to get the type definition, the other header
// would have to include the XXX_messages.h file, violating the rule above
// about not including XXX_messages.h file in other .h files.
//
// One approach here is to move these type definitions to another (guarded)
// .h file and include this second .h in your XXX_messages.h file.  This
// is still less than ideal, because the dispatched function would have to
// redeclare the typedef or include this second header.  This may be
// reasonable in a few cases.
//
// Failing all of the above, then you will want to bracket the smallest
// possible section of your XXX_messages.h file containing these types
// with an include guard macro.  Be aware that providing an incomplete
// class type declaration to avoid pulling in a long chain of headers is
// acceptable when your XXX_messages.h header is being included by the
// message sending caller's code, but not when the YYY_message_generator.c
// is building the messages. In addtion, due to the multiple inclusion
// restriction, these type ought to be guarded.  Follow a convention like:
//      #ifndef SOME_GUARD_MACRO
//      #define SOME_GUARD_MACRO
//      class some_class;        // One incomplete class declaration
//      class_some_other_class;  // Another incomplete class declaration
//      #endif  // SOME_GUARD_MACRO
//      #ifdef IPC_MESSAGE_IMPL
//      #include "path/to/some_class.h"        // Full class declaration
//      #include "path/to/some_other_class.h"  // Full class declaration
//      #endif  // IPC_MESSAGE_IMPL
//        (.. IPC macros using some_class and some_other_class ...)
//
// Macro Invocations
//
// You will use IPC message macro invocations for three things:
//   - New struct definitions for IPC
//   - Registering existing struct and enum definitions with IPC
//   - Defining the messages themselves
//
// New structs are defined with IPC_STRUCT_BEGIN(), IPC_STRUCT_MEMBER(),
// IPC_STRUCT_END() family of macros.  These cause the XXX_messages.h
// to proclaim equivalent struct declarations for use by callers, as well
// as later registering the type with the message generation.  Note that
// IPC_STRUCT_MEMBER() is only permitted inside matching calls to
// IPC_STRUCT_BEGIN() / IPC_STRUCT_END(). There is also an
// IPC_STRUCT_BEGIN_WITH_PARENT(), which behaves like IPC_STRUCT_BEGIN(),
// but also accomodates structs that inherit from other structs.
//
// Externally-defined structs are registered with IPC_STRUCT_TRAITS_BEGIN(),
// IPC_STRUCT_TRAITS_MEMBER(), and IPC_STRUCT_TRAITS_END() macros. These
// cause registration of the types with message generation only.
// There's also IPC_STRUCT_TRAITS_PARENT, which is used to register a parent
// class (whose own traits are already defined). Note that
// IPC_STRUCT_TRAITS_MEMBER() and IPC_STRUCT_TRAITS_PARENT are only permitted
// inside matching calls to IPC_STRUCT_TRAITS_BEGIN() /
// IPC_STRUCT_TRAITS_END().
//
// Enum types are registered with a single IPC_ENUM_TRAITS_VALIDATE() macro.
// There is no need to enumerate each value to the IPC mechanism. Instead,
// pass an expression in terms of the parameter |value| to provide
// range-checking.  For convenience, the IPC_ENUM_TRAITS() is provided which
// performs no checking, passing everything including out-of-range values.
// Its use is discouraged. The IPC_ENUM_TRAITS_MAX_VALUE() macro can be used
// for the typical case where the enum must be in the range 0..maxvalue
// inclusive. The IPC_ENUM_TRAITS_MIN_MAX_VALUE() macro can be used for the
// less typical case where the enum must be in the range minvalue..maxvalue
// inclusive.
//
// Do not place semicolons following these IPC_ macro invocations.  There
// is no reason to expect that their expansion corresponds one-to-one with
// C++ statements.
//
// Once the types have been declared / registered, message definitions follow.
// "Sync" messages are just synchronous calls, the Send() call doesn't return
// until a reply comes back.  To declare a sync message, use the IPC_SYNC_
// macros.  The numbers at the end show how many input/output parameters there
// are (i.e. 1_2 is 1 in, 2 out).  Input parameters are first, followed by
// output parameters.  The caller uses Send([route id, ], in1, &out1, &out2).
// The receiver's handler function will be
//     void OnSyncMessageName(const type1& in1, type2* out1, type3* out2)
//
// A caller can also send a synchronous message, while the receiver can respond
// at a later time.  This is transparent from the sender's side.  The receiver
// needs to use a different handler that takes in a IPC::Message* as the output
// type, stash the message, and when it has the data it can Send the message.
//
// Use the IPC_MESSAGE_HANDLER_DELAY_REPLY macro instead of IPC_MESSAGE_HANDLER
//     IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_SyncMessageName,
//                                     OnSyncMessageName)
//
// The handler function will look like:
//     void OnSyncMessageName(const type1& in1, IPC::Message* reply_msg);
//
// Receiver stashes the IPC::Message* pointer, and when it's ready, it does:
//     ViewHostMsg_SyncMessageName::WriteReplyParams(reply_msg, out1, out2);
//     Send(reply_msg);

// Files that want to export their ipc messages should do
//   #undef IPC_MESSAGE_EXPORT
//   #define IPC_MESSAGE_EXPORT VISIBILITY_MACRO
// after including this header, but before using any of the macros below.
// (This needs to be before the include guard.)
#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT

#ifndef IPC_IPC_MESSAGE_MACROS_H_
#define IPC_IPC_MESSAGE_MACROS_H_

#include "base/profiler/scoped_profile.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"

#if defined(IPC_MESSAGE_IMPL)
#include "ipc/ipc_message_utils_impl.h"
#endif

// Convenience macro for defining structs without inheritance. Should not need
// to be subsequently redefined.
#define IPC_STRUCT_BEGIN(struct_name) \
  IPC_STRUCT_BEGIN_WITH_PARENT(struct_name, IPC::NoParams)

// Macros for defining structs. Will be subsequently redefined.
#define IPC_STRUCT_BEGIN_WITH_PARENT(struct_name, parent) \
  struct struct_name; \
  IPC_STRUCT_TRAITS_BEGIN(struct_name) \
  IPC_STRUCT_TRAITS_END() \
  struct IPC_MESSAGE_EXPORT struct_name : parent { \
    struct_name(); \
    ~struct_name();
// Optional variadic parameters specify the default value for this struct
// member. They are passed through to the constructor for |type|.
#define IPC_STRUCT_MEMBER(type, name, ...) type name;
#define IPC_STRUCT_END() };

// Message macros collect specific numbers of arguments and funnel them into
// the common message generation macro.  These should never be redefined.
#define IPC_MESSAGE_CONTROL0(msg_class) \
  IPC_MESSAGE_DECL(EMPTY, CONTROL, msg_class, 0, 0, (), ())

#define IPC_MESSAGE_CONTROL1(msg_class, type1) \
  IPC_MESSAGE_DECL(ASYNC, CONTROL, msg_class, 1, 0, (type1), ())

#define IPC_MESSAGE_CONTROL2(msg_class, type1, type2) \
  IPC_MESSAGE_DECL(ASYNC, CONTROL, msg_class, 2, 0, (type1, type2), ())

#define IPC_MESSAGE_CONTROL3(msg_class, type1, type2, type3) \
  IPC_MESSAGE_DECL(ASYNC, CONTROL, msg_class, 3, 0, (type1, type2, type3), ())

#define IPC_MESSAGE_CONTROL4(msg_class, type1, type2, type3, type4) \
  IPC_MESSAGE_DECL(ASYNC, CONTROL, msg_class, 4, 0, (type1, type2, type3, type4), ())

#define IPC_MESSAGE_CONTROL5(msg_class, type1, type2, type3, type4, type5) \
  IPC_MESSAGE_DECL(ASYNC, CONTROL, msg_class, 5, 0, (type1, type2, type3, type4, type5), ())

#define IPC_MESSAGE_ROUTED0(msg_class) \
  IPC_MESSAGE_DECL(EMPTY, ROUTED, msg_class, 0, 0, (), ())

#define IPC_MESSAGE_ROUTED1(msg_class, type1) \
  IPC_MESSAGE_DECL(ASYNC, ROUTED, msg_class, 1, 0, (type1), ())

#define IPC_MESSAGE_ROUTED2(msg_class, type1, type2) \
  IPC_MESSAGE_DECL(ASYNC, ROUTED, msg_class, 2, 0, (type1, type2), ())

#define IPC_MESSAGE_ROUTED3(msg_class, type1, type2, type3) \
  IPC_MESSAGE_DECL(ASYNC, ROUTED, msg_class, 3, 0, (type1, type2, type3), ())

#define IPC_MESSAGE_ROUTED4(msg_class, type1, type2, type3, type4) \
  IPC_MESSAGE_DECL(ASYNC, ROUTED, msg_class, 4, 0, (type1, type2, type3, type4), ())

#define IPC_MESSAGE_ROUTED5(msg_class, type1, type2, type3, type4, type5) \
  IPC_MESSAGE_DECL(ASYNC, ROUTED, msg_class, 5, 0, (type1, type2, type3, type4, type5), ())

#define IPC_SYNC_MESSAGE_CONTROL0_0(msg_class) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 0, 0, (), ())

#define IPC_SYNC_MESSAGE_CONTROL0_1(msg_class, type1_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 0, 1, (), (type1_out))

#define IPC_SYNC_MESSAGE_CONTROL0_2(msg_class, type1_out, type2_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 0, 2, (), (type1_out, type2_out))

#define IPC_SYNC_MESSAGE_CONTROL0_3(msg_class, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 0, 3, (), (type1_out, type2_out, type3_out))

#define IPC_SYNC_MESSAGE_CONTROL0_4(msg_class, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 0, 4, (), (type1_out, type2_out, type3_out, type4_out))

#define IPC_SYNC_MESSAGE_CONTROL1_0(msg_class, type1_in) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 1, 0, (type1_in), ())

#define IPC_SYNC_MESSAGE_CONTROL1_1(msg_class, type1_in, type1_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 1, 1, (type1_in), (type1_out))

#define IPC_SYNC_MESSAGE_CONTROL1_2(msg_class, type1_in, type1_out, type2_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 1, 2, (type1_in), (type1_out, type2_out))

#define IPC_SYNC_MESSAGE_CONTROL1_3(msg_class, type1_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 1, 3, (type1_in), (type1_out, type2_out, type3_out))

#define IPC_SYNC_MESSAGE_CONTROL1_4(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 1, 4, (type1_in), (type1_out, type2_out, type3_out, type4_out))

#define IPC_SYNC_MESSAGE_CONTROL2_0(msg_class, type1_in, type2_in) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 2, 0, (type1_in, type2_in), ())

#define IPC_SYNC_MESSAGE_CONTROL2_1(msg_class, type1_in, type2_in, type1_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 2, 1, (type1_in, type2_in), (type1_out))

#define IPC_SYNC_MESSAGE_CONTROL2_2(msg_class, type1_in, type2_in, type1_out, type2_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 2, 2, (type1_in, type2_in), (type1_out, type2_out))

#define IPC_SYNC_MESSAGE_CONTROL2_3(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 2, 3, (type1_in, type2_in), (type1_out, type2_out, type3_out))

#define IPC_SYNC_MESSAGE_CONTROL2_4(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 2, 4, (type1_in, type2_in), (type1_out, type2_out, type3_out, type4_out))

#define IPC_SYNC_MESSAGE_CONTROL3_0(msg_class, type1_in, type2_in, type3_in) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 3, 0, (type1_in, type2_in, type3_in), ())

#define IPC_SYNC_MESSAGE_CONTROL3_1(msg_class, type1_in, type2_in, type3_in, type1_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 3, 1, (type1_in, type2_in, type3_in), (type1_out))

#define IPC_SYNC_MESSAGE_CONTROL3_2(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 3, 2, (type1_in, type2_in, type3_in), (type1_out, type2_out))

#define IPC_SYNC_MESSAGE_CONTROL3_3(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 3, 3, (type1_in, type2_in, type3_in), (type1_out, type2_out, type3_out))

#define IPC_SYNC_MESSAGE_CONTROL3_4(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 3, 4, (type1_in, type2_in, type3_in), (type1_out, type2_out, type3_out, type4_out))

#define IPC_SYNC_MESSAGE_CONTROL4_0(msg_class, type1_in, type2_in, type3_in, type4_in) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 4, 0, (type1_in, type2_in, type3_in, type4_in), ())

#define IPC_SYNC_MESSAGE_CONTROL4_1(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 4, 1, (type1_in, type2_in, type3_in, type4_in), (type1_out))

#define IPC_SYNC_MESSAGE_CONTROL4_2(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 4, 2, (type1_in, type2_in, type3_in, type4_in), (type1_out, type2_out))

#define IPC_SYNC_MESSAGE_CONTROL4_3(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 4, 3, (type1_in, type2_in, type3_in, type4_in), (type1_out, type2_out, type3_out))

#define IPC_SYNC_MESSAGE_CONTROL4_4(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 4, 4, (type1_in, type2_in, type3_in, type4_in), (type1_out, type2_out, type3_out, type4_out))

#define IPC_SYNC_MESSAGE_CONTROL5_0(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 5, 0, (type1_in, type2_in, type3_in, type4_in, type5_in), ())

#define IPC_SYNC_MESSAGE_CONTROL5_1(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 5, 1, (type1_in, type2_in, type3_in, type4_in, type5_in), (type1_out))

#define IPC_SYNC_MESSAGE_CONTROL5_2(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 5, 2, (type1_in, type2_in, type3_in, type4_in, type5_in), (type1_out, type2_out))

#define IPC_SYNC_MESSAGE_CONTROL5_3(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_DECL(SYNC, CONTROL, msg_class, 5, 3, (type1_in, type2_in, type3_in, type4_in, type5_in), (type1_out, type2_out, type3_out))

#define IPC_SYNC_MESSAGE_ROUTED0_0(msg_class) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 0, 0, (), ())

#define IPC_SYNC_MESSAGE_ROUTED0_1(msg_class, type1_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 0, 1, (), (type1_out))

#define IPC_SYNC_MESSAGE_ROUTED0_2(msg_class, type1_out, type2_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 0, 2, (), (type1_out, type2_out))

#define IPC_SYNC_MESSAGE_ROUTED0_3(msg_class, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 0, 3, (), (type1_out, type2_out, type3_out))

#define IPC_SYNC_MESSAGE_ROUTED0_4(msg_class, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 0, 4, (), (type1_out, type2_out, type3_out, type4_out))

#define IPC_SYNC_MESSAGE_ROUTED1_0(msg_class, type1_in) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 1, 0, (type1_in), ())

#define IPC_SYNC_MESSAGE_ROUTED1_1(msg_class, type1_in, type1_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 1, 1, (type1_in), (type1_out))

#define IPC_SYNC_MESSAGE_ROUTED1_2(msg_class, type1_in, type1_out, type2_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 1, 2, (type1_in), (type1_out, type2_out))

#define IPC_SYNC_MESSAGE_ROUTED1_3(msg_class, type1_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 1, 3, (type1_in), (type1_out, type2_out, type3_out))

#define IPC_SYNC_MESSAGE_ROUTED1_4(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 1, 4, (type1_in), (type1_out, type2_out, type3_out, type4_out))

#define IPC_SYNC_MESSAGE_ROUTED2_0(msg_class, type1_in, type2_in) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 2, 0, (type1_in, type2_in), ())

#define IPC_SYNC_MESSAGE_ROUTED2_1(msg_class, type1_in, type2_in, type1_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 2, 1, (type1_in, type2_in), (type1_out))

#define IPC_SYNC_MESSAGE_ROUTED2_2(msg_class, type1_in, type2_in, type1_out, type2_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 2, 2, (type1_in, type2_in), (type1_out, type2_out))

#define IPC_SYNC_MESSAGE_ROUTED2_3(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 2, 3, (type1_in, type2_in), (type1_out, type2_out, type3_out))

#define IPC_SYNC_MESSAGE_ROUTED2_4(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 2, 4, (type1_in, type2_in), (type1_out, type2_out, type3_out, type4_out))

#define IPC_SYNC_MESSAGE_ROUTED3_0(msg_class, type1_in, type2_in, type3_in) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 3, 0, (type1_in, type2_in, type3_in), ())

#define IPC_SYNC_MESSAGE_ROUTED3_1(msg_class, type1_in, type2_in, type3_in, type1_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 3, 1, (type1_in, type2_in, type3_in), (type1_out))

#define IPC_SYNC_MESSAGE_ROUTED3_2(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 3, 2, (type1_in, type2_in, type3_in), (type1_out, type2_out))

#define IPC_SYNC_MESSAGE_ROUTED3_3(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 3, 3, (type1_in, type2_in, type3_in), (type1_out, type2_out, type3_out))

#define IPC_SYNC_MESSAGE_ROUTED3_4(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 3, 4, (type1_in, type2_in, type3_in), (type1_out, type2_out, type3_out, type4_out))

#define IPC_SYNC_MESSAGE_ROUTED4_0(msg_class, type1_in, type2_in, type3_in, type4_in) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 4, 0, (type1_in, type2_in, type3_in, type4_in), ())

#define IPC_SYNC_MESSAGE_ROUTED4_1(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 4, 1, (type1_in, type2_in, type3_in, type4_in), (type1_out))

#define IPC_SYNC_MESSAGE_ROUTED4_2(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 4, 2, (type1_in, type2_in, type3_in, type4_in), (type1_out, type2_out))

#define IPC_SYNC_MESSAGE_ROUTED4_3(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 4, 3, (type1_in, type2_in, type3_in, type4_in), (type1_out, type2_out, type3_out))

#define IPC_SYNC_MESSAGE_ROUTED4_4(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out, type4_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 4, 4, (type1_in, type2_in, type3_in, type4_in), (type1_out, type2_out, type3_out, type4_out))

#define IPC_SYNC_MESSAGE_ROUTED5_0(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 5, 0, (type1_in, type2_in, type3_in, type4_in, type5_in), ())

#define IPC_SYNC_MESSAGE_ROUTED5_1(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 5, 1, (type1_in, type2_in, type3_in, type4_in, type5_in), (type1_out))

#define IPC_SYNC_MESSAGE_ROUTED5_2(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 5, 2, (type1_in, type2_in, type3_in, type4_in, type5_in), (type1_out, type2_out))

#define IPC_SYNC_MESSAGE_ROUTED5_3(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out) \
  IPC_MESSAGE_DECL(SYNC, ROUTED, msg_class, 5, 3, (type1_in, type2_in, type3_in, type4_in, type5_in), (type1_out, type2_out, type3_out))

// The following macros define the common set of methods provided by ASYNC
// message classes.
// This macro is for all the async IPCs that don't pass an extra parameter using
// IPC_BEGIN_MESSAGE_MAP_WITH_PARAM.
#define IPC_ASYNC_MESSAGE_METHODS_GENERIC                                     \
  template<class T, class S, class P, class Method>                           \
  static bool Dispatch(const Message* msg, T* obj, S* sender, P* parameter,   \
                       Method func) {                                         \
    Schema::Param p;                                                          \
    if (Read(msg, &p)) {                                                      \
      DispatchToMethod(obj, func, p);                                         \
      return true;                                                            \
    }                                                                         \
    return false;                                                             \
  }

// The following macros are for for async IPCs which have a dispatcher with an
// extra parameter specified using IPC_BEGIN_MESSAGE_MAP_WITH_PARAM.
#define IPC_ASYNC_MESSAGE_METHODS_1                                           \
  IPC_ASYNC_MESSAGE_METHODS_GENERIC                                           \
  template<class T, class S, class P, typename TA>                            \
  static bool Dispatch(const Message* msg, T* obj, S* sender, P* parameter,   \
                       void (T::*func)(P*, TA)) {                             \
    Schema::Param p;                                                          \
    if (Read(msg, &p)) {                                                      \
      (obj->*func)(parameter, p.a);                                           \
      return true;                                                            \
    }                                                                         \
    return false;                                                             \
  }
#define IPC_ASYNC_MESSAGE_METHODS_2                                           \
  IPC_ASYNC_MESSAGE_METHODS_GENERIC                                           \
  template<class T, class S, class P, typename TA, typename TB>               \
  static bool Dispatch(const Message* msg, T* obj, S* sender, P* parameter,   \
                       void (T::*func)(P*, TA, TB)) {                         \
    Schema::Param p;                                                          \
    if (Read(msg, &p)) {                                                      \
      (obj->*func)(parameter, p.a, p.b);                                      \
      return true;                                                            \
    }                                                                         \
    return false;                                                             \
  }
#define IPC_ASYNC_MESSAGE_METHODS_3                                           \
  IPC_ASYNC_MESSAGE_METHODS_GENERIC                                           \
  template<class T, class S, class P, typename TA, typename TB, typename TC>  \
  static bool Dispatch(const Message* msg, T* obj, S* sender, P* parameter,   \
                       void (T::*func)(P*, TA, TB, TC)) {                     \
    Schema::Param p;                                                          \
    if (Read(msg, &p)) {                                                      \
      (obj->*func)(parameter, p.a, p.b, p.c);                                 \
      return true;                                                            \
    }                                                                         \
    return false;                                                             \
  }
#define IPC_ASYNC_MESSAGE_METHODS_4                                           \
  IPC_ASYNC_MESSAGE_METHODS_GENERIC                                           \
  template<class T, class S, class P, typename TA, typename TB, typename TC,  \
           typename TD>                                                       \
  static bool Dispatch(const Message* msg, T* obj, S* sender, P* parameter,   \
                       void (T::*func)(P*, TA, TB, TC, TD)) {                 \
    Schema::Param p;                                                          \
    if (Read(msg, &p)) {                                                      \
      (obj->*func)(parameter, p.a, p.b, p.c, p.d);                            \
      return true;                                                            \
    }                                                                         \
    return false;                                                             \
  }
#define IPC_ASYNC_MESSAGE_METHODS_5                                           \
  IPC_ASYNC_MESSAGE_METHODS_GENERIC                                           \
  template<class T, class S, class P, typename TA, typename TB, typename TC,  \
           typename TD, typename TE>                                          \
  static bool Dispatch(const Message* msg, T* obj, S* sender, P* parameter,   \
                       void (T::*func)(P*, TA, TB, TC, TD, TE)) {             \
    Schema::Param p;                                                          \
    if (Read(msg, &p)) {                                                      \
      (obj->*func)(parameter, p.a, p.b, p.c, p.d, p.e);                       \
      return true;                                                            \
    }                                                                         \
    return false;                                                             \
  }

// The following macros define the common set of methods provided by SYNC
// message classes.
#define IPC_SYNC_MESSAGE_METHODS_GENERIC                                      \
  template<class T, class S, class P, class Method>                           \
  static bool Dispatch(const Message* msg, T* obj, S* sender, P* parameter,   \
                       Method func) {                                         \
    Schema::SendParam send_params;                                            \
    bool ok = ReadSendParam(msg, &send_params);                               \
    return Schema::DispatchWithSendParams(ok, send_params, msg, obj, sender,  \
                                          func);                              \
  }                                                                           \
  template<class T, class P, class Method>                                    \
  static bool DispatchDelayReply(const Message* msg, T* obj, P* parameter,    \
                                 Method func) {                               \
    Schema::SendParam send_params;                                            \
    bool ok = ReadSendParam(msg, &send_params);                               \
    return Schema::DispatchDelayReplyWithSendParams(ok, send_params, msg,     \
                                                    obj, func);               \
  }
#define IPC_SYNC_MESSAGE_METHODS_0 \
  IPC_SYNC_MESSAGE_METHODS_GENERIC
#define IPC_SYNC_MESSAGE_METHODS_1 \
  IPC_SYNC_MESSAGE_METHODS_GENERIC \
  template<typename TA> \
  static void WriteReplyParams(Message* reply, TA a) { \
    Schema::WriteReplyParams(reply, a); \
  }
#define IPC_SYNC_MESSAGE_METHODS_2 \
  IPC_SYNC_MESSAGE_METHODS_GENERIC \
  template<typename TA, typename TB> \
  static void WriteReplyParams(Message* reply, TA a, TB b) { \
    Schema::WriteReplyParams(reply, a, b); \
  }
#define IPC_SYNC_MESSAGE_METHODS_3 \
  IPC_SYNC_MESSAGE_METHODS_GENERIC \
  template<typename TA, typename TB, typename TC> \
  static void WriteReplyParams(Message* reply, TA a, TB b, TC c) { \
    Schema::WriteReplyParams(reply, a, b, c); \
  }
#define IPC_SYNC_MESSAGE_METHODS_4 \
  IPC_SYNC_MESSAGE_METHODS_GENERIC \
  template<typename TA, typename TB, typename TC, typename TD> \
  static void WriteReplyParams(Message* reply, TA a, TB b, TC c, TD d) { \
    Schema::WriteReplyParams(reply, a, b, c, d); \
  }
#define IPC_SYNC_MESSAGE_METHODS_5 \
  IPC_SYNC_MESSAGE_METHODS_GENERIC \
  template<typename TA, typename TB, typename TC, typename TD, typename TE> \
  static void WriteReplyParams(Message* reply, TA a, TB b, TC c, TD d, TE e) { \
    Schema::WriteReplyParams(reply, a, b, c, d, e); \
  }

// Common message macro which dispatches into one of the 6 (sync x kind)
// routines.  There is a way that these 6 cases can be lumped together,
// but the  macros get very complicated in that case.
// Note: intended be redefined to generate other information.
#define IPC_MESSAGE_DECL(sync, kind, msg_class,                               \
                         in_cnt, out_cnt, in_list, out_list)                  \
  IPC_##sync##_##kind##_DECL(msg_class, in_cnt, out_cnt, in_list, out_list)   \
  IPC_MESSAGE_EXTRA(sync, kind, msg_class, in_cnt, out_cnt, in_list, out_list)

#define IPC_EMPTY_CONTROL_DECL(msg_class, in_cnt, out_cnt, in_list, out_list) \
  class IPC_MESSAGE_EXPORT msg_class : public IPC::Message {                  \
   public:                                                                    \
    typedef IPC::Message Schema;                                              \
    enum { ID = IPC_MESSAGE_ID() };                                           \
    msg_class() : IPC::Message(MSG_ROUTING_CONTROL, ID, PRIORITY_NORMAL) {}   \
    static void Log(std::string* name, const Message* msg, std::string* l);   \
  };

#define IPC_EMPTY_ROUTED_DECL(msg_class, in_cnt, out_cnt, in_list, out_list)  \
  class IPC_MESSAGE_EXPORT msg_class : public IPC::Message {                  \
   public:                                                                    \
    typedef IPC::Message Schema;                                              \
    enum { ID = IPC_MESSAGE_ID() };                                           \
    msg_class(int32 routing_id)                                               \
        : IPC::Message(routing_id, ID, PRIORITY_NORMAL) {}                    \
    static void Log(std::string* name, const Message* msg, std::string* l);   \
  };

#define IPC_ASYNC_CONTROL_DECL(msg_class, in_cnt, out_cnt, in_list, out_list) \
  class IPC_MESSAGE_EXPORT msg_class : public IPC::Message {                  \
   public:                                                                    \
    typedef IPC::MessageSchema<IPC_TUPLE_IN_##in_cnt in_list> Schema;         \
    typedef Schema::Param Param;                                              \
    enum { ID = IPC_MESSAGE_ID() };                                           \
    msg_class(IPC_TYPE_IN_##in_cnt in_list);                                  \
    virtual ~msg_class();                                                     \
    static bool Read(const Message* msg, Schema::Param* p);                   \
    static void Log(std::string* name, const Message* msg, std::string* l);   \
    IPC_ASYNC_MESSAGE_METHODS_##in_cnt                                        \
  };

#define IPC_ASYNC_ROUTED_DECL(msg_class, in_cnt, out_cnt, in_list, out_list)  \
  class IPC_MESSAGE_EXPORT msg_class : public IPC::Message {                  \
   public:                                                                    \
    typedef IPC::MessageSchema<IPC_TUPLE_IN_##in_cnt in_list> Schema;         \
    typedef Schema::Param Param;                                              \
    enum { ID = IPC_MESSAGE_ID() };                                           \
    msg_class(int32 routing_id IPC_COMMA_##in_cnt                             \
              IPC_TYPE_IN_##in_cnt in_list);                                  \
    virtual ~msg_class();                                                     \
    static bool Read(const Message* msg, Schema::Param* p);                   \
    static void Log(std::string* name, const Message* msg, std::string* l);   \
    IPC_ASYNC_MESSAGE_METHODS_##in_cnt                                        \
  };

#define IPC_SYNC_CONTROL_DECL(msg_class, in_cnt, out_cnt, in_list, out_list)  \
  class IPC_MESSAGE_EXPORT msg_class : public IPC::SyncMessage {              \
   public:                                                                    \
    typedef IPC::SyncMessageSchema<IPC_TUPLE_IN_##in_cnt in_list,             \
                                   IPC_TUPLE_OUT_##out_cnt out_list> Schema;  \
    typedef Schema::ReplyParam ReplyParam;                                    \
    typedef Schema::SendParam SendParam;                                      \
    enum { ID = IPC_MESSAGE_ID() };                                           \
    msg_class(IPC_TYPE_IN_##in_cnt in_list                                    \
              IPC_COMMA_AND_##in_cnt(IPC_COMMA_##out_cnt)                     \
              IPC_TYPE_OUT_##out_cnt out_list);                               \
    virtual ~msg_class();                                                     \
    static bool ReadSendParam(const Message* msg, Schema::SendParam* p);      \
    static bool ReadReplyParam(                                               \
        const Message* msg,                                                   \
        TupleTypes<ReplyParam>::ValueTuple* p);                               \
    static void Log(std::string* name, const Message* msg, std::string* l);   \
    IPC_SYNC_MESSAGE_METHODS_##out_cnt                                        \
  };

#define IPC_SYNC_ROUTED_DECL(msg_class, in_cnt, out_cnt, in_list, out_list)   \
  class IPC_MESSAGE_EXPORT msg_class : public IPC::SyncMessage {              \
   public:                                                                    \
    typedef IPC::SyncMessageSchema<IPC_TUPLE_IN_##in_cnt in_list,             \
                                   IPC_TUPLE_OUT_##out_cnt out_list> Schema;  \
    typedef Schema::ReplyParam ReplyParam;                                    \
    typedef Schema::SendParam SendParam;                                      \
    enum { ID = IPC_MESSAGE_ID() };                                           \
    msg_class(int32 routing_id                                                \
              IPC_COMMA_OR_##in_cnt(IPC_COMMA_##out_cnt)                      \
              IPC_TYPE_IN_##in_cnt in_list                                    \
              IPC_COMMA_AND_##in_cnt(IPC_COMMA_##out_cnt)                     \
              IPC_TYPE_OUT_##out_cnt out_list);                               \
    virtual ~msg_class();                                                     \
    static bool ReadSendParam(const Message* msg, Schema::SendParam* p);      \
    static bool ReadReplyParam(                                               \
        const Message* msg,                                                   \
        TupleTypes<ReplyParam>::ValueTuple* p);                               \
    static void Log(std::string* name, const Message* msg, std::string* l);   \
    IPC_SYNC_MESSAGE_METHODS_##out_cnt                                        \
  };

#if defined(IPC_MESSAGE_IMPL)

// "Implementation" inclusion produces constructors, destructors, and
// logging functions, except for the no-arg special cases, where the
// implementation occurs in the declaration, and there is no special
// logging function.
#define IPC_MESSAGE_EXTRA(sync, kind, msg_class,                              \
                          in_cnt, out_cnt, in_list, out_list)                 \
  IPC_##sync##_##kind##_IMPL(msg_class, in_cnt, out_cnt, in_list, out_list)   \
  IPC_##sync##_MESSAGE_LOG(msg_class)

#define IPC_EMPTY_CONTROL_IMPL(msg_class, in_cnt, out_cnt, in_list, out_list)
#define IPC_EMPTY_ROUTED_IMPL(msg_class, in_cnt, out_cnt, in_list, out_list)

#define IPC_ASYNC_CONTROL_IMPL(msg_class, in_cnt, out_cnt, in_list, out_list) \
  msg_class::msg_class(IPC_TYPE_IN_##in_cnt in_list) :                        \
      IPC::Message(MSG_ROUTING_CONTROL, ID, PRIORITY_NORMAL) {                \
        Schema::Write(this, IPC_NAME_IN_##in_cnt in_list);                    \
      }                                                                       \
  msg_class::~msg_class() {}                                                  \
  bool msg_class::Read(const Message* msg, Schema::Param* p) {                \
    return Schema::Read(msg, p);                                              \
  }

#define IPC_ASYNC_ROUTED_IMPL(msg_class, in_cnt, out_cnt, in_list, out_list)  \
  msg_class::msg_class(int32 routing_id IPC_COMMA_##in_cnt                    \
                       IPC_TYPE_IN_##in_cnt in_list) :                        \
      IPC::Message(routing_id, ID, PRIORITY_NORMAL) {                         \
        Schema::Write(this, IPC_NAME_IN_##in_cnt in_list);                    \
      }                                                                       \
  msg_class::~msg_class() {}                                                  \
  bool msg_class::Read(const Message* msg, Schema::Param* p) {                \
    return Schema::Read(msg, p);                                              \
  }

#define IPC_SYNC_CONTROL_IMPL(msg_class, in_cnt, out_cnt, in_list, out_list)  \
  msg_class::msg_class(IPC_TYPE_IN_##in_cnt in_list                           \
                       IPC_COMMA_AND_##in_cnt(IPC_COMMA_##out_cnt)            \
                       IPC_TYPE_OUT_##out_cnt out_list) :                     \
      IPC::SyncMessage(MSG_ROUTING_CONTROL, ID, PRIORITY_NORMAL,              \
                       new IPC::ParamDeserializer<Schema::ReplyParam>(        \
                           IPC_NAME_OUT_##out_cnt out_list)) {                \
        Schema::Write(this, IPC_NAME_IN_##in_cnt in_list);                    \
      }                                                                       \
  msg_class::~msg_class() {}                                                  \
  bool msg_class::ReadSendParam(const Message* msg, Schema::SendParam* p) {   \
    return Schema::ReadSendParam(msg, p);                                     \
  }                                                                           \
  bool msg_class::ReadReplyParam(const Message* msg,                          \
                                 TupleTypes<ReplyParam>::ValueTuple* p) {     \
    return Schema::ReadReplyParam(msg, p);                                    \
  }

#define IPC_SYNC_ROUTED_IMPL(msg_class, in_cnt, out_cnt, in_list, out_list)   \
  msg_class::msg_class(int32 routing_id                                       \
                       IPC_COMMA_OR_##in_cnt(IPC_COMMA_##out_cnt)             \
                       IPC_TYPE_IN_##in_cnt in_list                           \
                       IPC_COMMA_AND_##in_cnt(IPC_COMMA_##out_cnt)            \
                       IPC_TYPE_OUT_##out_cnt out_list) :                     \
      IPC::SyncMessage(routing_id, ID, PRIORITY_NORMAL,                       \
                       new IPC::ParamDeserializer<Schema::ReplyParam>(        \
                           IPC_NAME_OUT_##out_cnt out_list)) {                \
        Schema::Write(this, IPC_NAME_IN_##in_cnt in_list);                    \
      }                                                                       \
  msg_class::~msg_class() {}                                                  \
  bool msg_class::ReadSendParam(const Message* msg, Schema::SendParam* p) {   \
    return Schema::ReadSendParam(msg, p);                                     \
  }                                                                           \
  bool msg_class::ReadReplyParam(const Message* msg,                          \
                                 TupleTypes<ReplyParam>::ValueTuple* p) {     \
    return Schema::ReadReplyParam(msg, p);                                    \
  }

#define IPC_EMPTY_MESSAGE_LOG(msg_class)                                \
  void msg_class::Log(std::string* name,                                \
                      const Message* msg,                               \
                      std::string* l) {                                 \
    if (name)                                                           \
      *name = #msg_class;                                               \
  }

#define IPC_ASYNC_MESSAGE_LOG(msg_class)                                \
  void msg_class::Log(std::string* name,                                \
                      const Message* msg,                               \
                      std::string* l) {                                 \
    if (name)                                                           \
      *name = #msg_class;                                               \
    if (!msg || !l)                                                     \
      return;                                                           \
    Schema::Param p;                                                    \
    if (Schema::Read(msg, &p))                                          \
      IPC::LogParam(p, l);                                              \
  }

#define IPC_SYNC_MESSAGE_LOG(msg_class)                                 \
  void msg_class::Log(std::string* name,                                \
                      const Message* msg,                               \
                      std::string* l) {                                 \
    if (name)                                                           \
      *name = #msg_class;                                               \
    if (!msg || !l)                                                     \
      return;                                                           \
    if (msg->is_sync()) {                                               \
      TupleTypes<Schema::SendParam>::ValueTuple p;                      \
      if (Schema::ReadSendParam(msg, &p))                               \
        IPC::LogParam(p, l);                                            \
      AddOutputParamsToLog(msg, l);                                     \
    } else {                                                            \
      TupleTypes<Schema::ReplyParam>::ValueTuple p;                     \
      if (Schema::ReadReplyParam(msg, &p))                              \
        IPC::LogParam(p, l);                                            \
    }                                                                   \
  }

#elif defined(IPC_MESSAGE_MACROS_LOG_ENABLED)

#ifndef IPC_LOG_TABLE_ADD_ENTRY
#error You need to define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger)
#endif

// "Log table" inclusion produces extra logging registration code.
#define IPC_MESSAGE_EXTRA(sync, kind, msg_class,                        \
                          in_cnt, out_cnt, in_list, out_list)           \
  class LoggerRegisterHelper##msg_class {                               \
 public:                                                                \
    LoggerRegisterHelper##msg_class() {                                 \
      const uint32 msg_id = static_cast<uint32>(msg_class::ID);         \
      IPC_LOG_TABLE_ADD_ENTRY(msg_id, msg_class::Log);                  \
    }                                                                   \
  };                                                                    \
  LoggerRegisterHelper##msg_class g_LoggerRegisterHelper##msg_class;

#else

// Normal inclusion produces nothing extra.
#define IPC_MESSAGE_EXTRA(sync, kind, msg_class,                \
                          in_cnt, out_cnt, in_list, out_list)

#endif // defined(IPC_MESSAGE_IMPL)

// Handle variable sized argument lists.  These are usually invoked by token
// pasting against the argument counts.
#define IPC_TYPE_IN_0()
#define IPC_TYPE_IN_1(t1)                   const t1& arg1
#define IPC_TYPE_IN_2(t1, t2)               const t1& arg1, const t2& arg2
#define IPC_TYPE_IN_3(t1, t2, t3)           const t1& arg1, const t2& arg2, const t3& arg3
#define IPC_TYPE_IN_4(t1, t2, t3, t4)       const t1& arg1, const t2& arg2, const t3& arg3, const t4& arg4
#define IPC_TYPE_IN_5(t1, t2, t3, t4, t5)   const t1& arg1, const t2& arg2, const t3& arg3, const t4& arg4, const t5& arg5

#define IPC_TYPE_OUT_0()
#define IPC_TYPE_OUT_1(t1)                  t1* arg6
#define IPC_TYPE_OUT_2(t1, t2)              t1* arg6, t2* arg7
#define IPC_TYPE_OUT_3(t1, t2, t3)          t1* arg6, t2* arg7, t3* arg8
#define IPC_TYPE_OUT_4(t1, t2, t3, t4)      t1* arg6, t2* arg7, t3* arg8, t4* arg9

#define IPC_TUPLE_IN_0()                    Tuple0
#define IPC_TUPLE_IN_1(t1)                  Tuple1<t1>
#define IPC_TUPLE_IN_2(t1, t2)              Tuple2<t1, t2>
#define IPC_TUPLE_IN_3(t1, t2, t3)          Tuple3<t1, t2, t3>
#define IPC_TUPLE_IN_4(t1, t2, t3, t4)      Tuple4<t1, t2, t3, t4>
#define IPC_TUPLE_IN_5(t1, t2, t3, t4, t5)  Tuple5<t1, t2, t3, t4, t5>

#define IPC_TUPLE_OUT_0()                   Tuple0
#define IPC_TUPLE_OUT_1(t1)                 Tuple1<t1&>
#define IPC_TUPLE_OUT_2(t1, t2)             Tuple2<t1&, t2&>
#define IPC_TUPLE_OUT_3(t1, t2, t3)         Tuple3<t1&, t2&, t3&>
#define IPC_TUPLE_OUT_4(t1, t2, t3, t4)     Tuple4<t1&, t2&, t3&, t4&>

#define IPC_NAME_IN_0()                     MakeTuple()
#define IPC_NAME_IN_1(t1)                   MakeRefTuple(arg1)
#define IPC_NAME_IN_2(t1, t2)               MakeRefTuple(arg1, arg2)
#define IPC_NAME_IN_3(t1, t2, t3)           MakeRefTuple(arg1, arg2, arg3)
#define IPC_NAME_IN_4(t1, t2, t3, t4)       MakeRefTuple(arg1, arg2, arg3, arg4)
#define IPC_NAME_IN_5(t1, t2, t3, t4, t5)   MakeRefTuple(arg1, arg2, arg3, arg4, arg5)

#define IPC_NAME_OUT_0()                    MakeTuple()
#define IPC_NAME_OUT_1(t1)                  MakeRefTuple(*arg6)
#define IPC_NAME_OUT_2(t1, t2)              MakeRefTuple(*arg6, *arg7)
#define IPC_NAME_OUT_3(t1, t2, t3)          MakeRefTuple(*arg6, *arg7, *arg8)
#define IPC_NAME_OUT_4(t1, t2, t3, t4)      MakeRefTuple(*arg6, *arg7, *arg8, *arg9)

// There are places where the syntax requires a comma if there are input args,
// if there are input args and output args, or if there are input args or
// output args.  These macros allow generation of the comma as needed; invoke
// by token pasting against the argument counts.
#define IPC_COMMA_0
#define IPC_COMMA_1 ,
#define IPC_COMMA_2 ,
#define IPC_COMMA_3 ,
#define IPC_COMMA_4 ,
#define IPC_COMMA_5 ,

#define IPC_COMMA_AND_0(x)
#define IPC_COMMA_AND_1(x) x
#define IPC_COMMA_AND_2(x) x
#define IPC_COMMA_AND_3(x) x
#define IPC_COMMA_AND_4(x) x
#define IPC_COMMA_AND_5(x) x

#define IPC_COMMA_OR_0(x) x
#define IPC_COMMA_OR_1(x) ,
#define IPC_COMMA_OR_2(x) ,
#define IPC_COMMA_OR_3(x) ,
#define IPC_COMMA_OR_4(x) ,
#define IPC_COMMA_OR_5(x) ,

// Message IDs
// Note: we currently use __LINE__ to give unique IDs to messages within
// a file.  They're globally unique since each file defines its own
// IPC_MESSAGE_START.
#define IPC_MESSAGE_ID() ((IPC_MESSAGE_START << 16) + __LINE__)
#define IPC_MESSAGE_ID_CLASS(id) ((id) >> 16)
#define IPC_MESSAGE_ID_LINE(id) ((id) & 0xffff)

// Message crackers and handlers. Usage:
//
//   bool MyClass::OnMessageReceived(const IPC::Message& msg) {
//     bool handled = true;
//     IPC_BEGIN_MESSAGE_MAP(MyClass, msg)
//       IPC_MESSAGE_HANDLER(MsgClassOne, OnMsgClassOne)
//       ...more handlers here ...
//       IPC_MESSAGE_HANDLER(MsgClassTen, OnMsgClassTen)
//       IPC_MESSAGE_UNHANDLED(handled = false)
//     IPC_END_MESSAGE_MAP()
//     return handled;
//   }


#define IPC_BEGIN_MESSAGE_MAP(class_name, msg) \
  { \
    typedef class_name _IpcMessageHandlerClass; \
    void* param__ = NULL; \
    const IPC::Message& ipc_message__ = msg; \
    switch (ipc_message__.type()) {

// gcc gives the following error now when using decltype so type typeof there:
//   error: identifier 'decltype' will become a keyword in C++0x [-Werror=c++0x-compat]
#if defined(OS_WIN)
#define IPC_DECLTYPE decltype
#else
#define IPC_DECLTYPE typeof
#endif

#define IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(class_name, msg, param)               \
  {                                                                            \
    typedef class_name _IpcMessageHandlerClass;                                \
    IPC_DECLTYPE(param) param__ = param;                                       \
    const IPC::Message& ipc_message__ = msg;                                   \
    switch (ipc_message__.type()) {

#define IPC_MESSAGE_FORWARD(msg_class, obj, member_func)                       \
    case msg_class::ID: {                                                      \
        TRACK_RUN_IN_IPC_HANDLER(member_func);                                 \
        if (!msg_class::Dispatch(&ipc_message__, obj, this, param__,           \
                                 &member_func))                                \
          ipc_message__.set_dispatch_error();                                  \
      }                                                                        \
      break;

#define IPC_MESSAGE_HANDLER(msg_class, member_func) \
  IPC_MESSAGE_FORWARD(msg_class, this, _IpcMessageHandlerClass::member_func)

#define IPC_MESSAGE_FORWARD_DELAY_REPLY(msg_class, obj, member_func)           \
    case msg_class::ID: {                                                      \
        TRACK_RUN_IN_IPC_HANDLER(member_func);                                 \
        if (!msg_class::DispatchDelayReply(&ipc_message__, obj, param__,       \
                                           &member_func))                      \
          ipc_message__.set_dispatch_error();                                  \
      }                                                                        \
      break;

#define IPC_MESSAGE_HANDLER_DELAY_REPLY(msg_class, member_func)                \
    IPC_MESSAGE_FORWARD_DELAY_REPLY(msg_class, this,                           \
                                    _IpcMessageHandlerClass::member_func)

// TODO(jar): fix chrome frame to always supply |code| argument.
#define IPC_MESSAGE_HANDLER_GENERIC(msg_class, code)                           \
    case msg_class::ID: {                                                      \
        /* TRACK_RUN_IN_IPC_HANDLER(code);  TODO(jar) */                       \
        code;                                                                  \
      }                                                                        \
      break;

#define IPC_REPLY_HANDLER(func)                                                \
    case IPC_REPLY_ID: {                                                       \
        TRACK_RUN_IN_IPC_HANDLER(func);                                        \
        func(ipc_message__);                                                   \
      }                                                                        \
      break;


#define IPC_MESSAGE_UNHANDLED(code)                                            \
    default: {                                                                 \
        code;                                                                  \
      }                                                                        \
      break;

#define IPC_MESSAGE_UNHANDLED_ERROR() \
  IPC_MESSAGE_UNHANDLED(NOTREACHED() << \
                              "Invalid message with type = " << \
                              ipc_message__.type())

#define IPC_END_MESSAGE_MAP() \
  } \
}

// This corresponds to an enum value from IPCMessageStart.
#define IPC_MESSAGE_CLASS(message) \
  IPC_MESSAGE_ID_CLASS(message.type())

#endif  // IPC_IPC_MESSAGE_MACROS_H_

// The following #ifdef cannot be removed.  Although the code is semantically
// equivalent without the #ifdef, VS2013 contains a bug where it is
// over-aggressive in optimizing out #includes.  Putting the #ifdef is a
// workaround for this bug.  See http://goo.gl/eGt2Fb for more details.
// This can be removed once VS2013 is fixed.
#ifdef IPC_MESSAGE_START
// Clean up IPC_MESSAGE_START in this unguarded section so that the
// XXX_messages.h files need not do so themselves.  This makes the
// XXX_messages.h files easier to write.
#undef IPC_MESSAGE_START
#endif
