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

#ifndef PORT_H_
#define PORT_H_

#include <assert.h>

class switchModule;
class flitModule;

using namespace std;

class port {
protected:
	int label;
	unsigned short cosLevels;
	int numVCs;
	switchModule * m_sw;
public:
	port(unsigned short cosLevels, int numVCs, int portNumber, switchModule * sw);
	virtual ~port();
	inline virtual bool extract(unsigned short cos, int vc, flitModule* &flitExtracted, float length, int buffer = 0) {
		assert(0);
		return false;
	}
	inline virtual void insert(int vc, flitModule *flit, float txLength) {
		assert(0);
	}
};

#endif /* PORT_H_ */
