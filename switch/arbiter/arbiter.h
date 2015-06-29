/*
 FOGSim, simulator for interconnection networks.
 https://code.google.com/p/fogsim/
 Copyright (C) 2015 University of Cantabria

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef class_arbiter
#define class_arbiter

#include "../../global.h"

using namespace std;

class switchModule;
class baseRouting;

class arbiter {
protected:
	int *LRSPortList; /* Stores the order for assigned resources to arbitrate */
	int ports;

	virtual bool checkPort() = 0;
	virtual bool attendPetition(int port) = 0;
	virtual void updateStatistics(int port) = 0;

public:
	int label; /* Arbiter ID */
	switchModule *switchM; /* Pointer to parental switch */

	arbiter(switchModule *switchM);
	~arbiter();
	int action();
	int getLRSPort(int offset);
	virtual void reorderLRSPortList(int servedPort);
};

#endif
