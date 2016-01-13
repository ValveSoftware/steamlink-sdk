/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2012-2014  BMW Car IT GmbH.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __CONNMAN_SESSION_H
#define __CONNMAN_SESSION_H

#include <connman/service.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CONNMAN_SESSION_POLICY_PRIORITY_LOW      -100
#define CONNMAN_SESSION_POLICY_PRIORITY_DEFAULT     0
#define CONNMAN_SESSION_POLICY_PRIORITY_HIGH      100

enum connman_session_roaming_policy {
	CONNMAN_SESSION_ROAMING_POLICY_UNKNOWN		= 0,
	CONNMAN_SESSION_ROAMING_POLICY_DEFAULT		= 1,
	CONNMAN_SESSION_ROAMING_POLICY_ALWAYS		= 2,
	CONNMAN_SESSION_ROAMING_POLICY_FORBIDDEN	= 3,
	CONNMAN_SESSION_ROAMING_POLICY_NATIONAL		= 4,
	CONNMAN_SESSION_ROAMING_POLICY_INTERNATIONAL	= 5,
};

enum connman_session_type {
	CONNMAN_SESSION_TYPE_UNKNOWN  = 0,
	CONNMAN_SESSION_TYPE_ANY      = 1,
	CONNMAN_SESSION_TYPE_LOCAL    = 2,
	CONNMAN_SESSION_TYPE_INTERNET = 3,
};

enum connman_session_id_type {
	CONNMAN_SESSION_ID_TYPE_UNKNOWN = 0,
	CONNMAN_SESSION_ID_TYPE_UID	= 1,
	CONNMAN_SESSION_ID_TYPE_GID	= 2,
	CONNMAN_SESSION_ID_TYPE_LSM	= 3,
};

struct connman_session;

struct connman_session_config {
	enum connman_session_id_type id_type;
	char *id;
	bool priority;
	enum connman_session_roaming_policy roaming_policy;
	enum connman_session_type type;
	bool ecall;
	GSList *allowed_bearers;
};

typedef int (* connman_session_config_func_t) (struct connman_session *session,
					struct connman_session_config *config,
					void *user_data, int err);

struct connman_session_policy {
	const char *name;
	int priority;
	bool (*autoconnect)(enum connman_service_connect_reason reason);
	int (*create)(struct connman_session *session,
			connman_session_config_func_t cb,
			void *user_data);
	void (*destroy)(struct connman_session *session);
	void (*session_changed)(struct connman_session *session, bool active,
				GSList *bearers);
	bool (*allowed)(struct connman_session *session,
			struct connman_service *service);
};

int connman_session_policy_register(struct connman_session_policy *config);
void connman_session_policy_unregister(struct connman_session_policy *config);

int connman_session_config_update(struct connman_session *session);
void connman_session_destroy(struct connman_session *session);

void connman_session_set_default_config(struct connman_session_config *config);
struct connman_session_config *connman_session_create_default_config(void);

enum connman_session_roaming_policy connman_session_parse_roaming_policy(const char *policy);
enum connman_session_type connman_session_parse_connection_type(const char *type);
int connman_session_parse_bearers(const char *token, GSList **list);

const char *connman_session_get_owner(struct connman_session *session);

int connman_session_connect(struct connman_service *service);
int connman_session_disconnect(struct connman_service *service);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_SESSION_H */
