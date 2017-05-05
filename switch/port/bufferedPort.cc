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

#include "bufferedPort.h"

bufferedPort::bufferedPort(unsigned short cosLevels, int numVCs, int bufferNumber, int bufferCapacity, float delay,
		int reservedBufferCapacity) {
	assert(cosLevels > 0 && cosLevels <= g_cos_levels);
	this->cosLevels = cosLevels;
	this->numVCs = g_local_link_channels + g_global_link_channels;
	this->reservedBufferCapacity = reservedBufferCapacity;
	this->aggregatedBufferCapacity = numVCs * (bufferCapacity / g_flit_size - this->reservedBufferCapacity);
	assert(this->aggregatedBufferCapacity >= 0);
	this->vcBuffers = new buffer**[this->cosLevels];
	for (int cos = 0; cos < this->cosLevels; cos++) {
		this->vcBuffers[cos] = new buffer*[this->numVCs];
		for (int vc = 0; vc < this->numVCs; vc++)
			this->vcBuffers[cos][vc] = new buffer(bufferNumber + cos * this->numVCs + vc, bufferCapacity, delay);
	}
}

bufferedPort::~bufferedPort() {
	for (int cos = 0; cos < this->cosLevels; cos++) {
		for (int vc = 0; vc < numVCs; vc++)
			delete vcBuffers[cos][vc];
		delete[] vcBuffers[cos];
	}
	delete[] vcBuffers;
}

/* Returns number of free slots in the buffer (in PHITS). For flexible length buffers, this
 * number is the addition of free reserved space for the channel, plus total free shared space
 * in the buffer.  */
int bufferedPort::getSpace(unsigned short cos, int vc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return 0; /* Little hack to avoid errors when checking nonexistent buffers */
	return vcBuffers[cos][vc]->getSpace();
}

/*
 * Returns head of buffer flit for a given cos and vc. When an offset is given, instead of returning head of buffer,
 * it returns flit at head+offset position.
 */
void bufferedPort::checkFlit(unsigned short cos, int vc, flitModule* &nextFlit, int offset) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return;
	vcBuffers[cos][vc]->checkFlit(nextFlit, offset);
}

bool bufferedPort::unLocked(unsigned short cos, int vc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return true;
	return vcBuffers[cos][vc]->unLocked();
}

/*
 * Return the buffer occupancy for a given cos and vc.
 */
int bufferedPort::getBufferOccupancy(unsigned short cos, int vc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return 0;
	return vcBuffers[cos][vc]->getBufferOccupancy();
}

bool bufferedPort::emptyBuffer(unsigned short cos, int vc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return true;
	return vcBuffers[cos][vc]->emptyBuffer();
}

bool bufferedPort::canSendFlit(unsigned short cos, int vc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return false;
	return vcBuffers[cos][vc]->canSendFlit();
}

bool bufferedPort::canReceiveFlit(unsigned short cos, int vc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return false;
	return vcBuffers[cos][vc]->canReceiveFlit();
}

bool bufferedPort::isBufferSending(unsigned short cos, int vc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return false; /* Little hack to avoid errors when checking unexisting buffers */
	return vcBuffers[cos][vc]->isBufferSending();
}

void bufferedPort::reorderBuffer(unsigned short cos, int vc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return;
	vcBuffers[cos][vc]->reorderBuffer();
}

float bufferedPort::getDelay(unsigned short cos, int vc) const {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return 0;
	return vcBuffers[cos][vc]->getDelay();
}

float bufferedPort::getHeadEntryCycle(unsigned short cos, int vc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return 0;
	return vcBuffers[cos][vc]->getHeadEntryCycle();
}

void bufferedPort::setCurPkt(unsigned short cos, int vc, int id) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return;
	vcBuffers[cos][vc]->m_currentPkt = id;
}

int bufferedPort::getCurPkt(unsigned short cos, int vc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return -1;
	return vcBuffers[cos][vc]->m_currentPkt;
}

void bufferedPort::setOutCurPkt(unsigned short cos, int vc, int port) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return;
	vcBuffers[cos][vc]->m_outPort_currentPkt = port;
}

int bufferedPort::getOutCurPkt(unsigned short cos, int vc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return -1;
	return vcBuffers[cos][vc]->m_outPort_currentPkt;
}

void bufferedPort::setNextVcCurPkt(unsigned short cos, int vc, int nextVc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return;
	vcBuffers[cos][vc]->m_nextVC_currentPkt = nextVc;
}

int bufferedPort::getNextVcCurPkt(unsigned short cos, int vc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return -1;
	return vcBuffers[cos][vc]->m_nextVC_currentPkt;
}

int bufferedPort::getBufferCapacity(unsigned short cos, int vc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return 0; /* Little hack to avoid errors when checking unexisting buffers */
	return vcBuffers[cos][vc]->bufferCapacity;
}

void bufferedPort::setPktLock(unsigned short cos, int vc, int id) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return;
	vcBuffers[cos][vc]->m_pktLock = id;
}

int bufferedPort::getPktLock(unsigned short cos, int vc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return -1;
	return vcBuffers[cos][vc]->m_pktLock;
}

void bufferedPort::setPortPktLock(unsigned short cos, int vc, int port) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return;
	vcBuffers[cos][vc]->m_portLock = port;
}

void bufferedPort::setVcPktLock(unsigned short cos, int vc, int prevVc) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return;
	vcBuffers[cos][vc]->m_vcLock = prevVc;
}

void bufferedPort::setUnlocked(unsigned short cos, int vc, int unlocked) {
	assert(vc >= 0);
	assert(cos >= 0 && cos < this->cosLevels);
	if (vc >= this->numVCs) return;
	vcBuffers[cos][vc]->m_unLocked = unlocked;
}
