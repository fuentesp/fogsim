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

#ifndef CAFLIT_H_
#define CAFLIT_H_

#include "../global.h"

class caFlit {
public:
	caFlit(int aPos, int * counter, float arrivalCycle);
	caFlit(const caFlit& original);
	~caFlit();
	float getArrivalCycle() const;

	static long long id;
	int * partial_counter;
	int m_aPos;

private:
	float m_arrivalCycle;
	float m_generatedCycle;
	long long m_id;
};

#endif /* CAFLIT_H_ */
