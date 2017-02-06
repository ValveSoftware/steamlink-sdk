/* 
 * Copyright 2016 The Chromium Authors.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef STYLUS_UNSTABLE_V1_SERVER_PROTOCOL_H
#define STYLUS_UNSTABLE_V1_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-util.h"

struct wl_client;
struct wl_resource;

struct zwp_stylus_v1;
struct zwp_pointer_stylus_v1;

extern const struct wl_interface zwp_stylus_v1_interface;
extern const struct wl_interface zwp_pointer_stylus_v1_interface;

/**
 * zwp_stylus_v1 - extends wl_pointer with events for on-screen stylus
 * @get_pointer_stylus: get stylus interface for pointer
 *
 * Allows a wl_pointer to represent an on-screen stylus. The client can
 * interpret the on-screen stylus like any other mouse device, and use this
 * protocol to obtain detail information about the type of stylus, as well
 * as the force and tilt of the tool.
 *
 * These events are to be fired by the server within the same frame as
 * other wl_pointer events.
 *
 * Warning! The protocol described in this file is experimental and
 * backward incompatible changes may be made. Backward compatible changes
 * may be added together with the corresponding uinterface version bump.
 * Backward incompatible changes are done by bumping the version number in
 * the protocol and uinterface names and resetting the interface version.
 * Once the protocol is to be declared stable, the 'z' prefix and the
 * version number in the protocol and interface names are removed and the
 * interface version number is reset.
 */
struct zwp_stylus_v1_interface {
	/**
	 * get_pointer_stylus - get stylus interface for pointer
	 * @id: (none)
	 * @pointer: (none)
	 *
	 * Create pointer_stylus object. See zwp_pointer_stylus_v1
	 * interface for details.
	 */
	void (*get_pointer_stylus)(struct wl_client *client,
				   struct wl_resource *resource,
				   uint32_t id,
				   struct wl_resource *pointer);
};

#ifndef ZWP_POINTER_STYLUS_V1_TOOL_TYPE_ENUM
#define ZWP_POINTER_STYLUS_V1_TOOL_TYPE_ENUM
/**
 * zwp_pointer_stylus_v1_tool_type - tool type of device.
 * @ZWP_POINTER_STYLUS_V1_TOOL_TYPE_MOUSE: Mouse or touchpad, not a
 *	stylus.
 * @ZWP_POINTER_STYLUS_V1_TOOL_TYPE_PEN: Pen
 * @ZWP_POINTER_STYLUS_V1_TOOL_TYPE_TOUCH: Touch
 * @ZWP_POINTER_STYLUS_V1_TOOL_TYPE_ERASER: Eraser
 *
 * 
 */
enum zwp_pointer_stylus_v1_tool_type {
	ZWP_POINTER_STYLUS_V1_TOOL_TYPE_MOUSE = 0,
	ZWP_POINTER_STYLUS_V1_TOOL_TYPE_PEN = 1,
	ZWP_POINTER_STYLUS_V1_TOOL_TYPE_TOUCH = 2,
	ZWP_POINTER_STYLUS_V1_TOOL_TYPE_ERASER = 3,
};
#endif /* ZWP_POINTER_STYLUS_V1_TOOL_TYPE_ENUM */

/**
 * zwp_pointer_stylus_v1 - stylus extension for pointer
 * @destroy: destroy stylus object
 *
 * The zwp_pointer_stylus_v1 interface extends the wl_pointer interface
 * with events to describe details about a stylus acting as a pointer.
 */
struct zwp_pointer_stylus_v1_interface {
	/**
	 * destroy - destroy stylus object
	 *
	 * 
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
};

#define ZWP_POINTER_STYLUS_V1_TOOL_CHANGE	0
#define ZWP_POINTER_STYLUS_V1_FORCE	1
#define ZWP_POINTER_STYLUS_V1_TILT	2

static inline void
zwp_pointer_stylus_v1_send_tool_change(struct wl_resource *resource_, uint32_t type)
{
	wl_resource_post_event(resource_, ZWP_POINTER_STYLUS_V1_TOOL_CHANGE, type);
}

static inline void
zwp_pointer_stylus_v1_send_force(struct wl_resource *resource_, uint32_t time, wl_fixed_t force)
{
	wl_resource_post_event(resource_, ZWP_POINTER_STYLUS_V1_FORCE, time, force);
}

static inline void
zwp_pointer_stylus_v1_send_tilt(struct wl_resource *resource_, uint32_t time, wl_fixed_t tilt_x, wl_fixed_t tilt_y)
{
	wl_resource_post_event(resource_, ZWP_POINTER_STYLUS_V1_TILT, time, tilt_x, tilt_y);
}

#ifdef  __cplusplus
}
#endif

#endif
