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

#ifndef class_rrArbiter
#define class_rrArbiter

#include "arbiter.h"

class switchModule;

class rrArbiter: public arbiter {
public:
	rrArbiter(PortType type, int portNumber, unsigned short cos, int numPorts, switchModule *switchM);
	~rrArbiter();
	virtual void reorderPortList(int servedPort);
	void markServedPort(int servedPort);
};

#endif /* class_rrArbiter */
