/*
 FOGSim, simulator for interconnection networks.
 http://fuentesp.github.io/fogsim/
 Copyright (C) 2017 University of Cantabria

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

#ifndef PBSTATE_H
#define	PBSTATE_H

#include "global.h"
#include "flit/pbFlit.h"

/* 
 * Class to store and manage global link state 
 * information within a group.
 */
class pbState {
public:
	pbState(int switchApos);
	virtual ~pbState();
	void readFlit(const pbFlit& flit);
	pbFlit* createFlit(int latency);
	void update(int port, unsigned short cos, int channel, bool linkCongested);
	bool isCongested(int link, unsigned short cos, int channel);
private:
	int m_switchApos;
	bool*** m_globalLinkCongested;
	int m_offset; /* offset of the current switch's first global link
	 *				 within the group's global links */
};

int port2groupGlobalLinkID(int port, int switchApos);
int groupGlobalLinkID2port(int id, int switchApos);

#endif	/* PBSTATE_H */

