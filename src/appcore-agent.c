/*
 *  service-app-core
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jayoun Lee <airjany@samsung.com>, Sewook Park <sewook7.park@samsung.com>, Jaeho Lee <jaeho81.lee@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#define _GNU_SOURCE

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <glib.h>

#include <sysman.h>
#include "aul.h"
#include "appcore-agent.h"

#ifndef EXPORT_API
#  define EXPORT_API __attribute__ ((visibility("default")))
#endif

#define _warn_if(expr, fmt, arg...) do { \
		if (expr) { \
			_ERR(fmt, ##arg); \
		} \
	} while (0)

#define _ret_if(expr) do { \
		if (expr) { \
			return; \
		} \
	} while (0)

#define _retv_if(expr, val) do { \
		if (expr) { \
			return (val); \
		} \
	} while (0)

#define _retm_if(expr, fmt, arg...) do { \
		if (expr) { \
			_ERR(fmt, ##arg); \
			return; \
		} \
	} while (0)

#define _retvm_if(expr, val, fmt, arg...) do { \
		if (expr) { \
			_ERR(fmt, ##arg); \
			return (val); \
		} \
	} while (0)


static pid_t _pid;

/**
 * agent internal state
 */
enum agent_state {
	AGS_NONE,
	AGS_CREATED,
	AGS_RUNNING,
	AGS_STOPED,
	AGS_DYING,
};

enum agent_event {
	AGE_UNKNOWN,
	AGE_CREATE,
	AGE_TERMINATE,
	AGE_STOP,
	AGE_REQUEST,
	AGE_MAX
};

struct agent_priv {
	enum agent_state state;

	struct agentcore_ops *ops;
};

static struct agent_priv priv;

struct agent_ops {
	void *data;
	void (*cb_app)(int, void *, bundle *);
};

GMainLoop *mainloop;

extern int service_create_request(bundle *data, service_h *service);

static void __do_app(enum agent_event event, void *data, bundle * b)
{
	int r;
	struct agent_priv *svc = data;
	service_h service;

	if (event == AGE_TERMINATE) {
		svc->state = AGS_DYING;
		g_main_loop_quit(mainloop);
		return;
	}

	if (service_create_event(b, &service) != 0)
	{
		return;
	}

	_ret_if(svc->ops == NULL);

	switch (event) {
	case AGE_REQUEST:
		if (svc->ops->service)
			r = svc->ops->service(service, svc->ops->data);
		svc->state = AGS_RUNNING;
		break;
/*	case AGE_STOP:
		if(svc->state == AGS_RUNNING) {
			if (svc->ops->stop)
				r = svc->ops->stop(svc->ops->data);
			svc->state = AGS_STOPED;
		}
		break;
*/	default:
		/* do nothing */
		break;
	}
	service_destroy(service);
}


static struct agent_ops s_ops = {
	.data = &priv,
	.cb_app = __do_app,
};

/**
 * Appcore internal system event
 */
enum sys_event {
	SE_UNKNOWN,
	SE_LOWMEM,
	SE_LOWBAT,
	SE_LANGCHG,
	SE_REGIONCHG,
	SE_MAX
};

/**
 * Appcore system event operation
 */
struct sys_op {
	int (*func) (void *);
	void *data;
};

struct agent_appcore {
	int state;

	const struct agent_ops *ops;
	struct sys_op sops[SE_MAX];
};

static struct agent_appcore core;

static int __set_data(struct agent_priv *agent, struct agentcore_ops *ops)
{
	if (ops == NULL) {
		errno = EINVAL;
		return -1;
	}

	agent->ops = ops;

	_pid = getpid();

	return 0;
}

static int __agent_request(void *data, bundle * k)
{
	struct agent_appcore *ac = data;
	_retv_if(ac == NULL || ac->ops == NULL, -1);
	_retv_if(ac->ops->cb_app == NULL, 0);

	ac->ops->cb_app(AGE_REQUEST, ac->ops->data, k);

	return 0;
}

static int __agent_terminate(void *data)
{
	struct agent_appcore *ac = data;

	_retv_if(ac == NULL || ac->ops == NULL, -1);
	_retv_if(ac->ops->cb_app == NULL, 0);

	ac->ops->cb_app(AGE_TERMINATE, ac->ops->data, NULL);

	return 0;
}

static int __aul_handler(aul_type type, bundle *b, void *data)
{
	int ret;

	switch (type) {
	case AUL_START:
		ret = __agent_request(data, b);
		break;
	case AUL_RESUME:
		break;
/*	case AUL_STOP:
		ret = __service_stop(data);
		break;
*/
	case AUL_TERMINATE:
		ret = __agent_terminate(data);
		break;
	default:
		/* do nothing */
		break;
	}

	return 0;
}

EXPORT_API int appcore_agent_init(const struct agent_ops *ops,
			    int argc, char **argv)
{
	int r;

	if (core.state != 0) {
		errno = EALREADY;
		return -1;
	}

	if (ops == NULL || ops->cb_app == NULL) {
		errno = EINVAL;
		return -1;
	}

	r = aul_launch_init(__aul_handler, &core);
	if (r < 0) {
		goto err;
	}

	r = aul_launch_argv_handler(argc, argv);
	if (r < 0) {
		goto err;
	}

	core.ops = ops;
	core.state = 1;		/* TODO: use enum value */

	return 0;
 err:
	//__clear(&core);
	return -1;
}


static int __before_loop(struct agent_priv *agent, int argc, char **argv)
{
	int r;

	if (argc == NULL || argv == NULL) {
		errno = EINVAL;
		return -1;
	}

	g_type_init();

	r = appcore_agent_init(&s_ops, argc, argv);
	_retv_if(r == -1, -1);

	if (agent->ops && agent->ops->create) {
		r = agent->ops->create(agent->ops->data);
		if (r == -1) {
			//appcore_exit();
			errno = ECANCELED;
			return -1;
		}
	}
	agent->state = AGS_CREATED;
	sysman_inform_backgrd();

	return 0;
}

static void __after_loop(struct agent_priv *agent)
{
	if (agent->ops && agent->ops->terminate)
		agent->ops->terminate(agent->ops->data);
}


EXPORT_API int appcore_agent_terminate()
{
	priv.state = AGS_DYING;
	g_main_loop_quit(mainloop);
}


EXPORT_API int appcore_agent_main(int argc, char **argv,
				struct agentcore_ops *ops)
{
	int r;

	r = __set_data(&priv, ops);
	_retv_if(r == -1, -1);

	r = __before_loop(&priv, argc, argv);
	if (r == -1) {
		//__unset_data(&priv);
		return -1;
	}

	mainloop = g_main_loop_new(NULL, FALSE);

	g_main_loop_run(mainloop);


	__after_loop(&priv);

	//__unset_data(&priv);

	return 0;
}
