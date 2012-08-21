#include <stdio.h>
#include <bundle.h>
#include <dlog.h>
#include <appcore-agent.h>

#undef LOG_TAG
#define LOG_TAG "AGENT_TEST"

static int __app_create(void *data)
{
	/*struct appdata *ad = data; */
	LOGD("__app_create");

	// Todo: add your code here.

	return 0;
}


static int __app_terminate(void *data)
{
	/*struct appdata *ad = data; */
	LOGD("__app_terminate");

	// Todo: add your code here.

	return 0;
}

static void __bundle_iterate(const char *key, const char *val, void *data)
{
	static int i=0;
	LOGD("%d %s %s", i++, key, val);
}


static int __app_reset(bundle* b, void *data)
{
	/*struct appdata *ad = data; */
	char *test_data = (char*)data;

	LOGD("requset : %s", test_data);

	bundle_iterate(b, __bundle_iterate, NULL);

	// Todo: add your code here.

	return 0;
}

int main(int argc, char* argv[])
{
	char ad[50] = "test data";

	struct agentcore_ops ops = {
		.create = __app_create,
		.terminate = __app_terminate,
		.reset = __app_reset,
	};

	ops.data = ad;

	appcore_agent_main(argc, argv, &ops);
}
