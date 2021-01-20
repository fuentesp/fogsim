/*
 FOGSim, simulator for interconnection networks.
 http://fuentesp.github.io/fogsim/
 Copyright (C) 2014-2021 University of Cantabria

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
	int *portList; /* Stores the order for assigned resources to arbitrate */
	short *qcnList; /* Stores the information about QCN usage for each head of buffer flit */

public:
	int label; /* Arbiter ID */
	unsigned short cos;
	int ports;
	switchModule *switchM; /* Pointer to parental switch */
	PortType type;

	arbiter(PortType type, int portNumber, unsigned short cos, int numPorts, switchModule *switchM);
	virtual ~arbiter();
	virtual int getServingPort(int offset);
	virtual void markServedPort(int servedPort);
	void reorderListQcn();
	void updateQcn(int servedPort);
};

#endif
