#include <stdio.h>
#include <bundle.h>
#include <dlog.h>
#include <appcore-agent.h>

#undef LOG_TAG
#define LOG_TAG "AGENT_TEST"

static int __agent_create(void *data)
{
	/*struct appdata *ad = data; */
	LOGD("__agent_create");

	// Todo: add your code here.

	return 0;
}


static int __agent_terminate(void *data)
{
	/*struct appdata *ad = data; */
	LOGD("__agent_terminate");

	// Todo: add your code here.

	return 0;
}

static void __bundle_iterate(const char *key, const char *val, void *data)
{
	static int i=0;
	LOGD("%d %s %s", i++, key, val);
}


static int __agent_reset(bundle* b, void *data)
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
		.create = __agent_create,
		.terminate = __agent_terminate,
		.reset = __agent_reset,
	};

	ops.data = ad;

	appcore_agent_main(argc, argv, &ops);
}
