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

#ifndef GAMING_INPUT_UNSTABLE_V1_SERVER_PROTOCOL_H
#define GAMING_INPUT_UNSTABLE_V1_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-util.h"

struct wl_client;
struct wl_resource;

struct zwp_gaming_input_v1;
struct zwp_gamepad_v1;

extern const struct wl_interface zwp_gaming_input_v1_interface;
extern const struct wl_interface zwp_gamepad_v1_interface;

/**
 * zwp_gaming_input_v1 - extends wl_seat with gaming input devices
 * @get_gamepad: get gamepad device assigned to seat
 *
 * A global interface to provide gaming input devices for a given seat.
 *
 * Currently only gamepad devices are supported.
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
struct zwp_gaming_input_v1_interface {
	/**
	 * get_gamepad - get gamepad device assigned to seat
	 * @id: (none)
	 * @seat: (none)
	 *
	 * Create gamepad object. See zwp_gamepad_v1 interface for
	 * details.
	 */
	void (*get_gamepad)(struct wl_client *client,
			    struct wl_resource *resource,
			    uint32_t id,
			    struct wl_resource *seat);
};

#ifndef ZWP_GAMEPAD_V1_GAMEPAD_STATE_ENUM
#define ZWP_GAMEPAD_V1_GAMEPAD_STATE_ENUM
/**
 * zwp_gamepad_v1_gamepad_state - connection state
 * @ZWP_GAMEPAD_V1_GAMEPAD_STATE_OFF: no gamepads are connected or on.
 * @ZWP_GAMEPAD_V1_GAMEPAD_STATE_ON: at least one gamepad is connected.
 *
 * 
 */
enum zwp_gamepad_v1_gamepad_state {
	ZWP_GAMEPAD_V1_GAMEPAD_STATE_OFF = 0,
	ZWP_GAMEPAD_V1_GAMEPAD_STATE_ON = 1,
};
#endif /* ZWP_GAMEPAD_V1_GAMEPAD_STATE_ENUM */

#ifndef ZWP_GAMEPAD_V1_BUTTON_STATE_ENUM
#define ZWP_GAMEPAD_V1_BUTTON_STATE_ENUM
/**
 * zwp_gamepad_v1_button_state - physical button state
 * @ZWP_GAMEPAD_V1_BUTTON_STATE_RELEASED: the button is not pressed
 * @ZWP_GAMEPAD_V1_BUTTON_STATE_PRESSED: the button is pressed
 *
 * Describes the physical state of a button that produced the button
 * event.
 */
enum zwp_gamepad_v1_button_state {
	ZWP_GAMEPAD_V1_BUTTON_STATE_RELEASED = 0,
	ZWP_GAMEPAD_V1_BUTTON_STATE_PRESSED = 1,
};
#endif /* ZWP_GAMEPAD_V1_BUTTON_STATE_ENUM */

/**
 * zwp_gamepad_v1 - gamepad input device
 * @destroy: destroy gamepad object
 *
 * The zwp_gamepad_v1 interface represents one or more gamepad input
 * devices, which are reported as a normalized 'Standard Gamepad' as it is
 * specified by the W3C Gamepad API at:
 * https://w3c.github.io/gamepad/#remapping
 */
struct zwp_gamepad_v1_interface {
	/**
	 * destroy - destroy gamepad object
	 *
	 * 
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
};

#define ZWP_GAMEPAD_V1_STATE_CHANGE	0
#define ZWP_GAMEPAD_V1_AXIS	1
#define ZWP_GAMEPAD_V1_BUTTON	2
#define ZWP_GAMEPAD_V1_FRAME	3

static inline void
zwp_gamepad_v1_send_state_change(struct wl_resource *resource_, uint32_t state)
{
	wl_resource_post_event(resource_, ZWP_GAMEPAD_V1_STATE_CHANGE, state);
}

static inline void
zwp_gamepad_v1_send_axis(struct wl_resource *resource_, uint32_t time, uint32_t axis, wl_fixed_t value)
{
	wl_resource_post_event(resource_, ZWP_GAMEPAD_V1_AXIS, time, axis, value);
}

static inline void
zwp_gamepad_v1_send_button(struct wl_resource *resource_, uint32_t time, uint32_t button, uint32_t state, wl_fixed_t analog)
{
	wl_resource_post_event(resource_, ZWP_GAMEPAD_V1_BUTTON, time, button, state, analog);
}

static inline void
zwp_gamepad_v1_send_frame(struct wl_resource *resource_, uint32_t time)
{
	wl_resource_post_event(resource_, ZWP_GAMEPAD_V1_FRAME, time);
}

#ifdef  __cplusplus
}
#endif

#endif
