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

#ifndef CREDITMSG_H
#define	CREDITMSG_H

class creditFlit {
public:
	creditFlit(float arrivalCycle, int numCreds, int vc, int flitId);
	virtual ~creditFlit();
	int getVc() const;
	int getNumCreds() const;
	float getArrivalCycle() const;
	bool operator <(const creditFlit& flit) const; /* Priority comparation */
	int getFlitId() const;
private:
	float m_arrivalCycle;
	int m_numCreds;
	int m_vc;
	int m_flitId; /* Message ID of the associated flitModule message */
};

#endif	/* CREDITMSG_H */

