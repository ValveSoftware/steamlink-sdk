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

#ifndef STYLUS_UNSTABLE_V1_CLIENT_PROTOCOL_H
#define STYLUS_UNSTABLE_V1_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct zwp_stylus_v1;
struct zwp_pointer_stylus_v1;

extern const struct wl_interface zwp_stylus_v1_interface;
extern const struct wl_interface zwp_pointer_stylus_v1_interface;

#define ZWP_STYLUS_V1_GET_POINTER_STYLUS	0

static inline void
zwp_stylus_v1_set_user_data(struct zwp_stylus_v1 *zwp_stylus_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_stylus_v1, user_data);
}

static inline void *
zwp_stylus_v1_get_user_data(struct zwp_stylus_v1 *zwp_stylus_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_stylus_v1);
}

static inline void
zwp_stylus_v1_destroy(struct zwp_stylus_v1 *zwp_stylus_v1)
{
	wl_proxy_destroy((struct wl_proxy *) zwp_stylus_v1);
}

static inline struct zwp_pointer_stylus_v1 *
zwp_stylus_v1_get_pointer_stylus(struct zwp_stylus_v1 *zwp_stylus_v1, struct wl_pointer *pointer)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) zwp_stylus_v1,
			 ZWP_STYLUS_V1_GET_POINTER_STYLUS, &zwp_pointer_stylus_v1_interface, NULL, pointer);

	return (struct zwp_pointer_stylus_v1 *) id;
}

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
 * @tool_change: pointing device tool type changed
 * @force: force change event
 * @tilt: force change event
 *
 * The zwp_pointer_stylus_v1 interface extends the wl_pointer interface
 * with events to describe details about a stylus acting as a pointer.
 */
struct zwp_pointer_stylus_v1_listener {
	/**
	 * tool_change - pointing device tool type changed
	 * @type: new device type
	 *
	 * Notification that the user is using a new tool type. There can
	 * only be one tool in use at a time. If the pointer enters a
	 * client surface, with a tool type other than mouse, this event
	 * will also be generated.
	 *
	 * If this event is not received, the client has to assume a mouse
	 * is in use. The remaining events of this protocol are only being
	 * generated after this event has been fired with a tool type other
	 * than mouse.
	 */
	void (*tool_change)(void *data,
			    struct zwp_pointer_stylus_v1 *zwp_pointer_stylus_v1,
			    uint32_t type);
	/**
	 * force - force change event
	 * @time: timestamp with millisecond granularity
	 * @force: new value of force
	 *
	 * Notification of a change in physical force on the surface of
	 * the screen.
	 *
	 * If the pointer enters a client surface, with a tool type other
	 * than mouse, this event will also be generated.
	 *
	 * The force is calibrated and normalized to the 0 to 1 range.
	 */
	void (*force)(void *data,
		      struct zwp_pointer_stylus_v1 *zwp_pointer_stylus_v1,
		      uint32_t time,
		      wl_fixed_t force);
	/**
	 * tilt - force change event
	 * @time: timestamp with millisecond granularity
	 * @tilt_x: tilt in x direction
	 * @tilt_y: tilt in y direction
	 *
	 * Notification of a change in tilt of the pointing tool.
	 *
	 * If the pointer enters a client surface, with a tool type other
	 * than mouse, this event will also be generated.
	 *
	 * Measured from surface normal as plane angle in degrees, values
	 * lie in [-90,90]. A positive x is to the right and a positive y
	 * is towards the user.
	 */
	void (*tilt)(void *data,
		     struct zwp_pointer_stylus_v1 *zwp_pointer_stylus_v1,
		     uint32_t time,
		     wl_fixed_t tilt_x,
		     wl_fixed_t tilt_y);
};

static inline int
zwp_pointer_stylus_v1_add_listener(struct zwp_pointer_stylus_v1 *zwp_pointer_stylus_v1,
				   const struct zwp_pointer_stylus_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) zwp_pointer_stylus_v1,
				     (void (**)(void)) listener, data);
}

#define ZWP_POINTER_STYLUS_V1_DESTROY	0

static inline void
zwp_pointer_stylus_v1_set_user_data(struct zwp_pointer_stylus_v1 *zwp_pointer_stylus_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_pointer_stylus_v1, user_data);
}

static inline void *
zwp_pointer_stylus_v1_get_user_data(struct zwp_pointer_stylus_v1 *zwp_pointer_stylus_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_pointer_stylus_v1);
}

static inline void
zwp_pointer_stylus_v1_destroy(struct zwp_pointer_stylus_v1 *zwp_pointer_stylus_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_pointer_stylus_v1,
			 ZWP_POINTER_STYLUS_V1_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) zwp_pointer_stylus_v1);
}

#ifdef  __cplusplus
}
#endif

#endif
