#ifndef class_priorityglobalarbiter
#define class_priorityglobalarbiter

#include "globalArbiter.h"

using namespace std;

class priorityGlobalArbiter: public globalArbiter {
public:
	priorityGlobalArbiter(int outPortNumber, int ports, switchModule *switchM);
	~priorityGlobalArbiter();
	void reorderLRSPortList(int servedPort);

};

#endif
