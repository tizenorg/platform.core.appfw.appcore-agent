/*
 *  app-core
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



#ifndef __AGENT_APPCORE_H__
#define __AGENT_APPCORE_H__


#include <libintl.h>
#include <bundle.h>

#include "app_service.h"


#ifdef __cplusplus
extern "C" {
#endif

struct agentcore_ops {
	void *data;
	    /**< Callback data */
	int (*create) (void *);
	int (*terminate) (void *);
	int (*service) (service_h, void *);

	void *reserved[6];
		   /**< Reserved */
};


int appcore_agent_main(int argc, char **argv, struct agentcore_ops *ops);

int appcore_agent_terminate();



#ifdef __cplusplus
}
#endif

#endif				/* __AGENT_APPCORE_H__ */
