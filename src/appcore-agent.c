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
#include <malloc.h>
#include <Ecore.h>
#include <linux/limits.h>
#include <dlfcn.h>
#include <glib.h>

#include <bundle.h>
#include <aul.h>
#include <appcore-common.h>
#include <app_control_internal.h>
#include <dlog.h>
#include <vconf.h>

#include "appcore-agent.h"

#ifdef _APPFW_FEATURE_BACKGROUND_MANAGEMENT
#include <gio/gio.h>

#define RESOURCED_FREEZER_PATH "/Org/Tizen/Resourced/Freezer"
#define RESOURCED_FREEZER_INTERFACE "org.tizen.resourced.frezer"
#define RESOURCED_FREEZER_SIGNAL "FreezerState"
#define APPFW_SUSPEND_HINT_PATH "/Org/Tizen/Appfw/SuspendHint"
#define APPFW_SUSPEND_HINT_INTERFACE "org.tizen.appfw.SuspendHint"
#define APPFW_SUSPEND_HINT_SIGNAL "SuspendHint"

#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "APPCORE_AGENT"
#define SQLITE_FLUSH_MAX	(1024 * 1024)

#define _ERR(fmt, arg...) LOGE(fmt, ##arg)
#define _INFO(fmt, arg...) LOGI(fmt, ##arg)
#define _DBG(fmt, arg...) LOGD(fmt, ##arg)

#ifndef EXPORT_API
#define EXPORT_API __attribute__ ((visibility("default")))
#endif

#ifndef _ERR
#define _ERR(fmt, arg...) LOGE(fmt, ##arg)
#endif

#ifndef _INFO
#define _INFO(...) LOGI(__VA_ARGS__)
#endif

#ifndef _DBG
#define _DBG(...) LOGD(__VA_ARGS__)
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

#define APPID_MAX 256
#define PATH_LOCALE "locale"

static pid_t _pid;

/**
 * Appcore internal system event
 */
enum sys_event {
	SE_UNKNOWN,
	SE_LOWMEM,
	SE_LOWBAT,
	SE_LANGCHG,
	SE_REGIONCHG,
	SE_SUSPENDED_STATE,
	SE_MAX
};

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


static enum appcore_agent_event to_ae[SE_MAX] = {
	APPCORE_AGENT_EVENT_UNKNOWN,		/* SE_UNKNOWN */
	APPCORE_AGENT_EVENT_LOW_MEMORY,		/* SE_LOWMEM */
	APPCORE_AGENT_EVENT_LOW_BATTERY,	/* SE_LOWBAT */
	APPCORE_AGENT_EVENT_LANG_CHANGE,	/* SE_LANGCHG */
	APPCORE_AGENT_EVENT_REGION_CHANGE,	/* SE_REGIONCHG */
	APPCORE_AGENT_EVENT_SUSPENDED_STATE_CHANGE, /* SE_SUSPENDED_STATE */
};

static int appcore_agent_event_initialized[SE_MAX] = {0};

enum cb_type {			/* callback */
	_CB_NONE,
	_CB_SYSNOTI,
	_CB_APPNOTI,
	_CB_VCONF,
};

enum appcore_agent_suspended_state {
	APPCORE_AGENT_SUSPENDED_STATE_WILL_ENTER_SUSPEND = 0,
	APPCORE_AGENT_SUSPENDED_STATE_DID_EXIT_FROM_SUSPEND
};

struct evt_ops {
	enum cb_type type;
	union {
		enum appcore_agent_event sys;
		enum agent_event app;
		const char *vkey;
	} key;

	int (*cb_pre) (void *);
	int (*cb) (void *);
	int (*cb_post) (void *);

	int (*vcb_pre) (void *, void *);
	int (*vcb) (void *, void *);
	int (*vcb_post) (void *, void *);
};

struct agent_priv {
	enum agent_state state;

	struct agent_appcore *app_core;
	struct agentcore_ops *ops;
};

static struct agent_priv priv;

struct agent_ops {
	void *data;
	void (*cb_app)(enum agent_event, void *, bundle *);
};

/**
 * Appcore system event operation
 */
struct sys_op {
	int (*func) (void *, void *);
	void *data;
};

struct agent_appcore {
	int state;
	unsigned int tid;
	bool suspended_state;
	bool allowed_bg;

	const struct agent_ops *ops;
	struct sys_op sops[SE_MAX];
};

static struct agent_appcore core;

static int __sys_lowmem_post(void *data, void *evt);
static int __sys_lowmem(void *data, void *evt);
static int __sys_lowbatt(void *data, void *evt);
static int __sys_langchg_pre(void *data, void *evt);
static int __sys_langchg(void *data, void *evt);
static int __sys_regionchg_pre(void *data, void *evt);
static int __sys_regionchg(void *data, void *evt);

static struct evt_ops evtops[] = {
	{
	 .type = _CB_VCONF,
	 .key.vkey = VCONFKEY_SYSMAN_LOW_MEMORY,
	 .vcb_post = __sys_lowmem_post,
	 .vcb = __sys_lowmem,
	 },
	{
	 .type = _CB_VCONF,
	 .key.vkey = VCONFKEY_SYSMAN_BATTERY_STATUS_LOW,
	 .vcb = __sys_lowbatt,
	 },
	{
	.type = _CB_VCONF,
	.key.vkey = VCONFKEY_LANGSET,
	.vcb_pre = __sys_langchg_pre,
	.vcb = __sys_langchg,
	},
	{
	.type = _CB_VCONF,
	.key.vkey = VCONFKEY_REGIONFORMAT,
	.vcb_pre = __sys_regionchg_pre,
	.vcb = __sys_regionchg,
	 },
	{
	.type = _CB_VCONF,
	.key.vkey = VCONFKEY_REGIONFORMAT_TIME1224,
	.vcb = __sys_regionchg,
	 },
};

#ifdef _APPFW_FEATURE_BACKGROUND_MANAGEMENT
static GDBusConnection *bus = NULL;
static guint __suspend_dbus_handler_initialized = 0;
#endif

extern int app_control_create_event(bundle *data, struct app_control_s **app_control);
static int __sys_do(struct agent_appcore *ac, void *event_info, enum sys_event event);

static int appcore_agent_flush_memory(void)
{
	int (*flush_fn) (int);

	if (!core.state) {
		_ERR("Appcore not initialized");
		return -1;
	}

	flush_fn = dlsym(RTLD_DEFAULT, "sqlite3_release_memory");
	if (flush_fn) {
		flush_fn(SQLITE_FLUSH_MAX);
	}

	malloc_trim(0);

	return 0;
}

static int _appcore_agent_request_to_suspend(int pid)
{
#ifdef _APPFW_FEATURE_BACKGROUND_MANAGEMENT
	GError *err = NULL;

	if (bus == NULL) {
		bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);
		if (bus == NULL) {
			_ERR("g_bus_get_sync() is failed: %s", err->message);
			g_error_free(err);
			return -1;
		}
	}

	if (g_dbus_connection_emit_signal(bus,
					NULL,
					APPFW_SUSPEND_HINT_PATH,
					APPFW_SUSPEND_HINT_INTERFACE,
					APPFW_SUSPEND_HINT_SIGNAL,
					g_variant_new("(i)", pid),
					&err) == FALSE) {
		_ERR("g_dbus_connection_emit_signal() is failed: %s",
					err->message);
		g_error_free(err);
		return -1;
	}

	if (g_dbus_connection_flush_sync(bus, NULL, &err) == FALSE) {
		_ERR("g_dbus_connection_flush_sync() is failed: %s",
					err->message);
		g_error_free(err);
		return -1;
	}

	_DBG("[__SUSPEND__] Send suspend hint, pid: %d", pid);
#endif
	return 0;
}

static void __prepare_to_suspend(void *data)
{
#ifdef _APPFW_FEATURE_BACKGROUND_MANAGEMENT
	int suspend = APPCORE_AGENT_SUSPENDED_STATE_WILL_ENTER_SUSPEND;
	struct agent_appcore *ac = data;

	if (ac && !ac->allowed_bg && !ac->suspended_state) {
		_DBG("[__SUSPEND__]");
		__sys_do(ac, &suspend, SE_SUSPENDED_STATE);
		_appcore_agent_request_to_suspend(getpid()); /* send dbus signal to resourced */
		ac->suspended_state = true;
	}
#endif
}

static void __exit_from_suspend(void *data)
{
#ifdef _APPFW_FEATURE_BACKGROUND_MANAGEMENT
	int suspend = APPCORE_AGENT_SUSPENDED_STATE_DID_EXIT_FROM_SUSPEND;
	struct agent_appcore *ac = data;

	if (ac && !ac->allowed_bg && ac->suspended_state) {
		_DBG("[__SUSPEND__]");
		__sys_do(ac, &suspend, SE_SUSPENDED_STATE);
		ac->suspended_state = false;
	}
#endif
}

static gboolean __flush_memory(gpointer data)
{
#ifdef _APPFW_FEATURE_BACKGROUND_MANAGEMENT
	struct agent_appcore *ac = (struct agent_appcore *)data;

	appcore_agent_flush_memory();

	if (!ac) {
		return FALSE;
	}
	ac->tid = 0;

	_DBG("[__SUSPEND__] flush case");
	__prepare_to_suspend(ac);
#endif
	return FALSE;
}

static void __add_suspend_timer(struct agent_appcore *ac)
{
#ifdef _APPFW_FEATURE_BACKGROUND_MANAGEMENT
	ac->tid = g_timeout_add_seconds(5, __flush_memory, ac);
#endif
}

static void __remove_suspend_timer(struct agent_appcore *ac)
{
#ifdef _APPFW_FEATURE_BACKGROUND_MANAGEMENT
	if (ac->tid > 0) {
		g_source_remove(ac->tid);
		ac->tid = 0;
	}
#endif
}

static void __exit_loop(void *data)
{
	ecore_main_loop_quit();
	__remove_suspend_timer(&core);
}

static void __do_app(enum agent_event event, void *data, bundle * b)
{
	struct agent_priv *svc = data;
	app_control_h app_control = NULL;

	_ret_if(svc == NULL);

	if (event == AGE_TERMINATE) {
		svc->state = AGS_DYING;
		ecore_main_loop_thread_safe_call_sync((Ecore_Data_Cb)__exit_loop, NULL);
		return;
	}

	_ret_if(svc->ops == NULL);

	if (app_control_create_event(b, &app_control) != 0)
		return;

	switch (event) {
	case AGE_REQUEST:
		if (svc->ops->app_control)
			svc->ops->app_control(app_control, svc->ops->data);
		svc->state = AGS_RUNNING;
		break;
/*	case AGE_STOP:
		if(svc->state == AGS_RUNNING) {
			if (svc->ops->stop)
				svc->ops->stop(svc->ops->data);
			svc->state = AGS_STOPED;
		}
		break;
*/	default:
		/* do nothing */
		break;
	}
	app_control_destroy(app_control);
}

static struct agent_ops s_ops = {
	.data = &priv,
	.cb_app = __do_app,
};

static int __set_data(struct agent_priv *agent, struct agentcore_ops *ops)
{
	if (ops == NULL) {
		errno = EINVAL;
		return -1;
	}

	agent->ops = ops;
	agent->app_core = NULL;

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

static int __sys_do_default(struct agent_appcore *ac, enum sys_event event)
{
	int r;

	switch (event) {
	case SE_LOWBAT:
		/*r = __def_lowbatt(ac);*/
		r = 0;
		break;
	default:
		r = 0;
		break;
	};

	return r;
}

static int __sys_do(struct agent_appcore *ac, void *event_info, enum sys_event event)
{
	struct sys_op *op;

	_retv_if(ac == NULL || event >= SE_MAX, -1);

	op = &ac->sops[event];

	if (op->func == NULL)
		return __sys_do_default(ac, event);

	return op->func(event_info, op->data);
}

static int __sys_lowmem_post(void *data, void *evt)
{
#if defined(MEMORY_FLUSH_ACTIVATE)
	struct agent_appcore *ac = data;
	ac->ops->cb_app(AE_LOWMEM_POST, ac->ops->data, NULL);
#else
	malloc_trim(0);
#endif
	return 0;
}

static int __sys_lowmem(void *data, void *evt)
{
	keynode_t *key = evt;
	int val;

	val = vconf_keynode_get_int(key);

	if (val >= VCONFKEY_SYSMAN_LOW_MEMORY_SOFT_WARNING)
		return __sys_do(data, (void *)&val, SE_LOWMEM);

	return 0;
}

static int __sys_lowbatt(void *data, void *evt)
{
	keynode_t *key = evt;
	int val;

	val = vconf_keynode_get_int(key);

	/* VCONFKEY_SYSMAN_BAT_CRITICAL_LOW or VCONFKEY_SYSMAN_POWER_OFF */
	if (val <= VCONFKEY_SYSMAN_BAT_CRITICAL_LOW)
		return __sys_do(data, (void *)&val, SE_LOWBAT);

	return 0;
}

static int __sys_langchg_pre(void *data, void *evt)
{
	keynode_t *key = evt;
	char *lang;
	char *r;

	lang = vconf_keynode_get_str(key);
	if (lang) {
		setenv("LANG", lang, 1);
		setenv("LC_MESSAGES", lang, 1);

		r = setlocale(LC_ALL, lang);
		if (r == NULL) {
			r = setlocale(LC_ALL, lang);
			if (r)
				_DBG("*****appcore-agent setlocale=%s\n", r);
		}
	}

	return 0;
}

static int __sys_langchg(void *data, void *evt)
{
	keynode_t *key = evt;
	char *val;

	val = vconf_keynode_get_str(key);

	return __sys_do(data, (void *)val, SE_LANGCHG);
}

static int __sys_regionchg_pre(void *data, void *evt)
{
	keynode_t *key = evt;
	char *region;
	char *r;

	region = vconf_keynode_get_str(key);
	if (region) {
		setenv("LC_CTYPE", region, 1);
		setenv("LC_NUMERIC", region, 1);
		setenv("LC_TIME", region, 1);
		setenv("LC_COLLATE", region, 1);
		setenv("LC_MONETARY", region, 1);
		setenv("LC_PAPER", region, 1);
		setenv("LC_NAME", region, 1);
		setenv("LC_ADDRESS", region, 1);
		setenv("LC_TELEPHONE", region, 1);
		setenv("LC_MEASUREMENT", region, 1);
		setenv("LC_IDENTIFICATION", region, 1);

		r = setlocale(LC_ALL, "");
		if (r != NULL)
			_DBG("*****appcore-agent setlocale=%s\n", r);
	}

	return 0;
}

static int __sys_regionchg(void *data, void *evt)
{
	keynode_t *key = evt;
	char *val = NULL;
	const char *name;

	name = vconf_keynode_get_name(key);
	if (!strcmp(name, VCONFKEY_REGIONFORMAT))
		val = vconf_keynode_get_str(key);

	return __sys_do(data, (void *)val, SE_REGIONCHG);
}

static void __vconf_do(struct evt_ops *eo, keynode_t * key, void *data)
{
	_ret_if(eo == NULL);

	if (eo->vcb_pre)
		eo->vcb_pre(data, key);

	if (eo->vcb)
		eo->vcb(data, key);

	if (eo->vcb_post)
		eo->vcb_post(data, key);
}

static void __vconf_cb(keynode_t *key, void *data)
{
	int i;
	const char *name;

	name = vconf_keynode_get_name(key);
	_ret_if(name == NULL);

	SECURE_LOGD("[APP %d] vconf changed: %s", _pid, name);

	for (i = 0; i < sizeof(evtops) / sizeof(evtops[0]); i++) {
		struct evt_ops *eo = &evtops[i];

		switch (eo->type) {
		case _CB_VCONF:
			if (!strcmp(name, eo->key.vkey))
				__vconf_do(eo, key, data);
			break;
		default:
			/* do nothing */
			break;
		}
	}
}

static int __add_vconf(struct agent_appcore *ac, enum sys_event se)
{
	int r;

	switch (se) {
	case SE_LOWMEM:
		r = vconf_notify_key_changed(VCONFKEY_SYSMAN_LOW_MEMORY, __vconf_cb, ac);
		break;
	case SE_LOWBAT:
		r = vconf_notify_key_changed(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, __vconf_cb, ac);
		break;
	case SE_LANGCHG:
		r = vconf_notify_key_changed(VCONFKEY_LANGSET, __vconf_cb, ac);
		break;
	case SE_REGIONCHG:
		r = vconf_notify_key_changed(VCONFKEY_REGIONFORMAT, __vconf_cb, ac);
		if (r < 0)
			break;

		r = vconf_notify_key_changed(VCONFKEY_REGIONFORMAT_TIME1224, __vconf_cb, ac);
		break;
	default:
		r = -1;
		break;
	}

	return r;
}

static int __del_vconf(enum sys_event se)
{
	int r;

	switch (se) {
	case SE_LOWMEM:
		r = vconf_ignore_key_changed(VCONFKEY_SYSMAN_LOW_MEMORY, __vconf_cb);
		break;
	case SE_LOWBAT:
		r = vconf_ignore_key_changed(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, __vconf_cb);
		break;
	case SE_LANGCHG:
		r = vconf_ignore_key_changed(VCONFKEY_LANGSET, __vconf_cb);
		break;
	case SE_REGIONCHG:
		r = vconf_ignore_key_changed(VCONFKEY_REGIONFORMAT, __vconf_cb);
		if (r < 0)
			break;

		r = vconf_ignore_key_changed(VCONFKEY_REGIONFORMAT_TIME1224, __vconf_cb);
		break;
	default:
		r = -1;
		break;
	}

	return r;
}

static int __del_vconf_list(void)
{
	int r;
	enum sys_event se;

	for (se = SE_LOWMEM; se < SE_MAX; se++) {
		if (appcore_agent_event_initialized[se]) {
			r = __del_vconf(se);
			if (r < 0)
				_ERR("Delete vconf callback failed");
			else
				appcore_agent_event_initialized[se] = 0;
		}
	}

	return 0;
}

static int __aul_handler(aul_type type, bundle *b, void *data)
{
	int ret = 0;
	char *bg = NULL;
	struct agent_appcore *ac = data;

	switch (type) {
	case AUL_START:
		bundle_get_str(b, AUL_K_ALLOWED_BG, &bg);
		if (bg && strncmp(bg, "ALLOWED_BG", strlen("ALLOWED_BG")) == 0) {
			_DBG("[__SUSPEND__] allowed background");
			ac->allowed_bg = true;
			__remove_suspend_timer(data);
		}
		ret = __agent_request(data, b);
		break;
	case AUL_RESUME:
		bundle_get_str(b, AUL_K_ALLOWED_BG, &bg);
		if (bg && strncmp(bg, "ALLOWED_BG", strlen("ALLOWED_BG")) == 0) {
			_DBG("[__SUSPEND__] allowed background");
			ac->allowed_bg = true;
			__remove_suspend_timer(data);
		}
		break;
/*	case AUL_STOP:
		__service_stop(data);
		break;
*/
	case AUL_TERMINATE:
	case AUL_TERMINATE_BGAPP:
		if (!ac->allowed_bg) {
			__remove_suspend_timer(data);
		}
		ret = __agent_terminate(data);
		break;
	case AUL_SUSPEND:
		if (!ac->allowed_bg) {
			_DBG("[__SUSPEND__] suspend");
			__add_suspend_timer(data);
		}
		break;
	case AUL_WAKE:
		if (!ac->allowed_bg) {
			_DBG("[__SUSPEND__] wake");
			__remove_suspend_timer(data);
			__exit_from_suspend(data);
		}
		break;
	default:
		/* do nothing */
		break;
	}

	return ret;
}

static int __get_package_app_name(int pid, char **app_name)
{
	char *name_token = NULL;
	char appid[APPID_MAX] = {0};
	int r;

	r = aul_app_get_appid_bypid(pid, appid, APPID_MAX);
	if (r != AUL_R_OK)
		return -1;

	if (appid[0] == '\0')
		return -1;

	name_token = strrchr(appid, '.');
	if (name_token == NULL)
		return -1;

	name_token++;

	*app_name = strdup(name_token);
	if (*app_name == NULL)
		return -1;

	return 0;
}

EXPORT_API int appcore_agent_set_event_callback(enum appcore_agent_event event,
					  int (*cb) (void *, void *), void *data)
{
	struct agent_appcore *ac = &core;
	struct sys_op *op;
	enum sys_event se;
	int r = 0;

	for (se = SE_UNKNOWN; se < SE_MAX; se++) {
		if (event == to_ae[se])
			break;
	}

	if (se == SE_UNKNOWN || se >= SE_MAX) {
		_ERR("Unregistered event");
		errno = EINVAL;
		return -1;
	}

	op = &ac->sops[se];

	op->func = cb;
	op->data = data;

	if (op->func && !appcore_agent_event_initialized[se]) {
		r = __add_vconf(ac, se);
		if (r < 0)
			_ERR("Add vconf callback failed");
		else
			appcore_agent_event_initialized[se] = 1;
	} else if (!op->func && appcore_agent_event_initialized[se]) {
		r = __del_vconf(se);
		if (r < 0)
			_ERR("Delete vconf callback failed");
		else
			appcore_agent_event_initialized[se] = 0;
	}

	return r;
}

#ifdef _APPFW_FEATURE_BACKGROUND_MANAGEMENT
static gboolean __init_suspend(gpointer data)
{
	int r;

	r = _appcore_agent_init_suspend_dbus_handler(&core);
	if (r == -1)
		_ERR("Initailzing suspended state handler failed");

	return FALSE;
}
#endif

static int __get_locale_resource_dir(char *locale_dir, int size)
{
	const char *res_path;

	res_path = aul_get_app_resource_path();
	if (res_path == NULL) {
		_ERR("Failed to get resource path");
		return -1;
	}

	snprintf(locale_dir, size, "%s" PATH_LOCALE, res_path);
	if (access(locale_dir, R_OK) != 0)
		return -1;

	return 0;
}

EXPORT_API int appcore_agent_init(const struct agent_ops *ops,
			    int argc, char **argv)
{
	int r;
	char locale_dir[PATH_MAX];
	char *app_name = NULL;

	if (core.state != 0) {
		errno = EALREADY;
		return -1;
	}

	if (ops == NULL || ops->cb_app == NULL) {
		errno = EINVAL;
		return -1;
	}

	r = __get_package_app_name(getpid(), &app_name);
	if (r < 0)
		return -1;

	r = __get_locale_resource_dir(locale_dir, sizeof(locale_dir));
	SECURE_LOGD("dir : %s", locale_dir);
	SECURE_LOGD("app name : %s", app_name);
	r = appcore_set_i18n(app_name, locale_dir);
	free(app_name);
	_retv_if(r == -1, -1);

#ifdef _APPFW_FEATURE_BACKGROUND_MANAGEMENT
	g_idle_add(__init_suspend, NULL);
#endif

	r = aul_launch_init(__aul_handler, &core);
	if (r < 0)
		goto err;

	r = aul_launch_argv_handler(argc, argv);
	if (r < 0)
		goto err;

	core.ops = ops;
	core.state = 1;		/* TODO: use enum value */
	core.tid = 0;
	core.suspended_state = false;
	core.allowed_bg = false;

	return 0;
 err:
	__del_vconf_list();
	return -1;
}

static void appcore_agent_get_app_core(struct agent_appcore **ac)
{
	*ac = &core;
}

static int __before_loop(struct agent_priv *agent, int argc, char **argv)
{
	int r;
	struct agent_appcore *ac = NULL;

	if (argc <= 0 || argv == NULL) {
		errno = EINVAL;
		return -1;
	}

	ecore_init();

	r = appcore_agent_init(&s_ops, argc, argv);
	_retv_if(r == -1, -1);

	appcore_agent_get_app_core(&ac);
	agent->app_core = ac;
	SECURE_LOGD("[__SUSPEND__] agent appcore initialized, appcore addr: 0x%x", ac);

	if (agent->ops && agent->ops->create) {
		r = agent->ops->create(agent->ops->data);
		if (r < 0) {
			if (agent->ops && agent->ops->terminate)
				agent->ops->terminate(agent->ops->data);
			errno = ECANCELED;
			return -1;
		}
	}
	agent->state = AGS_CREATED;

	return 0;
}

static void __after_loop(struct agent_priv *agent)
{
	__del_vconf_list();
	priv.state = AGS_DYING;
	if (agent->ops && agent->ops->terminate)
		agent->ops->terminate(agent->ops->data);
	ecore_shutdown();
}

EXPORT_API int appcore_agent_terminate()
{
	__del_vconf_list();
	ecore_main_loop_thread_safe_call_sync((Ecore_Data_Cb)__exit_loop, NULL);

	return 0;
}

EXPORT_API int appcore_agent_terminate_without_restart()
{
	__del_vconf_list();
	aul_status_update(STATUS_NORESTART);
	ecore_main_loop_thread_safe_call_sync((Ecore_Data_Cb)__exit_loop, NULL);

	return 0;
}

EXPORT_API int appcore_agent_main(int argc, char **argv,
				struct agentcore_ops *ops)
{
	int r;

	r = __set_data(&priv, ops);
	_retv_if(r == -1, -1);

	r = __before_loop(&priv, argc, argv);
	if (r == -1)
		return -1;

	ecore_main_loop_begin();

	aul_status_update(STATUS_DYING);

	__after_loop(&priv);

	return 0;
}

#ifdef _APPFW_FEATURE_BACKGROUND_MANAGEMENT
static void __suspend_dbus_signal_handler(GDBusConnection *connection,
					const gchar *sender_name,
					const gchar *object_path,
					const gchar *interface_name,
					const gchar *signal_name,
					GVariant *parameters,
					gpointer user_data)
{
	struct agent_appcore *ac = (struct agent_appcore *)user_data;
	gint suspend = APPCORE_SUSPENDED_STATE_DID_EXIT_FROM_SUSPEND;
	gint pid;
	gint status;

	if (g_strcmp0(signal_name, RESOURCED_FREEZER_SIGNAL) == 0) {
		g_variant_get(parameters, "(ii)", &status, &pid);
		if (pid == getpid() && status == 0) { /* thawed */
			if (ac && !ac->allowed_bg && ac->suspended_state) {
				__remove_suspend_timer(ac);
				__sys_do(ac, &suspend, SE_SUSPENDED_STATE);
				ac->suspended_state = false;
				__add_suspend_timer(ac);
			}
		}
	}
}

int _appcore_agent_init_suspend_dbus_handler(void *data)
{
	GError *err = NULL;

	if (__suspend_dbus_handler_initialized)
		return 0;

	if (!bus) {
		bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);
		if (!bus) {
			_ERR("Failed to connect to the D-BUS daemon: %s",
						err->message);
			g_error_free(err);
			return -1;
		}
	}

	__suspend_dbus_handler_initialized = g_dbus_connection_signal_subscribe(
						bus,
						NULL,
						RESOURCED_FREEZER_INTERFACE,
						RESOURCED_FREEZER_SIGNAL,
						RESOURCED_FREEZER_PATH,
						NULL,
						G_DBUS_SIGNAL_FLAGS_NONE,
						__suspend_dbus_signal_handler,
						data,
						NULL);
	if (__suspend_dbus_handler_initialized == 0) {
		_ERR("g_dbus_connection_signal_subscribe() is failed.");
		return -1;
	}

	_DBG("[__SUSPEND__] suspend signal initialized");

	return 0;
}

#endif
