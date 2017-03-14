#pragma once

#ifdef __cplusplus
extern "C" {
#endif

	void winrt_timer_init(void);
	void winrt_timer_do(void);
	void winrt_timer_close(void);

#ifdef __cplusplus
};
#endif

#define TIME_INTERVAL		50
#define TIME_TIMEOUT		100
