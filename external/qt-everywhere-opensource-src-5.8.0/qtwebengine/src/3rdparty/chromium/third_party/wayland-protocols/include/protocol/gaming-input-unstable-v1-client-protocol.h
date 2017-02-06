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

#ifndef GAMING_INPUT_UNSTABLE_V1_CLIENT_PROTOCOL_H
#define GAMING_INPUT_UNSTABLE_V1_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct zwp_gaming_input_v1;
struct zwp_gamepad_v1;

extern const struct wl_interface zwp_gaming_input_v1_interface;
extern const struct wl_interface zwp_gamepad_v1_interface;

#define ZWP_GAMING_INPUT_V1_GET_GAMEPAD	0

static inline void
zwp_gaming_input_v1_set_user_data(struct zwp_gaming_input_v1 *zwp_gaming_input_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_gaming_input_v1, user_data);
}

static inline void *
zwp_gaming_input_v1_get_user_data(struct zwp_gaming_input_v1 *zwp_gaming_input_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_gaming_input_v1);
}

static inline void
zwp_gaming_input_v1_destroy(struct zwp_gaming_input_v1 *zwp_gaming_input_v1)
{
	wl_proxy_destroy((struct wl_proxy *) zwp_gaming_input_v1);
}

static inline struct zwp_gamepad_v1 *
zwp_gaming_input_v1_get_gamepad(struct zwp_gaming_input_v1 *zwp_gaming_input_v1, struct wl_seat *seat)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) zwp_gaming_input_v1,
			 ZWP_GAMING_INPUT_V1_GET_GAMEPAD, &zwp_gamepad_v1_interface, NULL, seat);

	return (struct zwp_gamepad_v1 *) id;
}

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
 * @state_change: state change event
 * @axis: axis change event
 * @button: Gamepad button changed
 * @frame: Notifies end of a series of gamepad changes.
 *
 * The zwp_gamepad_v1 interface represents one or more gamepad input
 * devices, which are reported as a normalized 'Standard Gamepad' as it is
 * specified by the W3C Gamepad API at:
 * https://w3c.github.io/gamepad/#remapping
 */
struct zwp_gamepad_v1_listener {
	/**
	 * state_change - state change event
	 * @state: new state
	 *
	 * Notification that this seat's connection state has changed.
	 */
	void (*state_change)(void *data,
			     struct zwp_gamepad_v1 *zwp_gamepad_v1,
			     uint32_t state);
	/**
	 * axis - axis change event
	 * @time: timestamp with millisecond granularity
	 * @axis: axis that produced this event
	 * @value: new value of axis
	 *
	 * Notification of axis change.
	 *
	 * The axis id specifies which axis has changed as defined by the
	 * W3C 'Standard Gamepad'.
	 *
	 * The value is calibrated and normalized to the -1 to 1 range.
	 */
	void (*axis)(void *data,
		     struct zwp_gamepad_v1 *zwp_gamepad_v1,
		     uint32_t time,
		     uint32_t axis,
		     wl_fixed_t value);
	/**
	 * button - Gamepad button changed
	 * @time: timestamp with millisecond granularity
	 * @button: id of button
	 * @state: digital state of the button
	 * @analog: analog value of the button
	 *
	 * Notification of button change.
	 *
	 * The button id specifies which button has changed as defined by
	 * the W3C 'Standard Gamepad'.
	 *
	 * A button can have a digital and an analog value. The analog
	 * value is normalized to a 0 to 1 range. If a button does not
	 * provide an analog value, it will be derived from the digital
	 * state.
	 */
	void (*button)(void *data,
		       struct zwp_gamepad_v1 *zwp_gamepad_v1,
		       uint32_t time,
		       uint32_t button,
		       uint32_t state,
		       wl_fixed_t analog);
	/**
	 * frame - Notifies end of a series of gamepad changes.
	 * @time: timestamp with millisecond granularity
	 *
	 * Indicates the end of a set of events that logically belong
	 * together. A client is expected to accumulate the data in all
	 * events within the frame before proceeding.
	 */
	void (*frame)(void *data,
		      struct zwp_gamepad_v1 *zwp_gamepad_v1,
		      uint32_t time);
};

static inline int
zwp_gamepad_v1_add_listener(struct zwp_gamepad_v1 *zwp_gamepad_v1,
			    const struct zwp_gamepad_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) zwp_gamepad_v1,
				     (void (**)(void)) listener, data);
}

#define ZWP_GAMEPAD_V1_DESTROY	0

static inline void
zwp_gamepad_v1_set_user_data(struct zwp_gamepad_v1 *zwp_gamepad_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_gamepad_v1, user_data);
}

static inline void *
zwp_gamepad_v1_get_user_data(struct zwp_gamepad_v1 *zwp_gamepad_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_gamepad_v1);
}

static inline void
zwp_gamepad_v1_destroy(struct zwp_gamepad_v1 *zwp_gamepad_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_gamepad_v1,
			 ZWP_GAMEPAD_V1_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) zwp_gamepad_v1);
}

#ifdef  __cplusplus
}
#endif

#endif
