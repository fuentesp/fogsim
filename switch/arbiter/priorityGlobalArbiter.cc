#include "priorityGlobalArbiter.h"

using namespace std;

priorityGlobalArbiter::priorityGlobalArbiter(int outPortNumber, int ports, switchModule *switchM) :
		globalArbiter(outPortNumber, ports, switchM) {
}

priorityGlobalArbiter::~priorityGlobalArbiter() {
}

/*
 * Reorders port list when one port has been attended.
 * This function is redefined after arbiter class to
 * preserve transit over injection ports preference.
 */
void priorityGlobalArbiter::reorderLRSPortList(int servedPort) {
	assert(servedPort >= 0 && servedPort < ports);
	int i, j, reorder_range_init = 0, reorder_range_end = ports;
	if (servedPort < g_p_computing_nodes_per_router)
		/* Injection port */
		reorder_range_init = ports - g_p_computing_nodes_per_router;
	else
		/* Transit port */
		reorder_range_end = ports - g_p_computing_nodes_per_router;

	for (i = reorder_range_init; i < reorder_range_end; i++) {
		if (LRSPortList[i] == servedPort) {
			for (j = i; j < (reorder_range_end - 1); j++) {
				LRSPortList[j] = LRSPortList[j + 1];
			}
			LRSPortList[reorder_range_end - 1] = servedPort;
		}
	}
}
