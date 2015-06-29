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

#include "bufferedPort.h"

bufferedPort::bufferedPort(int numVCs, int bufferNumber, int bufferCapacity, float delay, int reservedBufferCapacity) {
	this->numVCs = g_local_link_channels + g_global_link_channels;//numVCs;
	this->reservedBufferCapacity = reservedBufferCapacity;
	this->aggregatedBufferCapacity = numVCs * (bufferCapacity / g_flit_size - this->reservedBufferCapacity);
	assert(this->aggregatedBufferCapacity >= 0);
	this->vcBuffers = new buffer*[this->numVCs];
	for (int i = 0; i < this->numVCs; i++) {
		this->vcBuffers[i] = new buffer(bufferNumber + i, bufferCapacity, delay);
	}
}

bufferedPort::~bufferedPort() {
	for (int i = 0; i < numVCs; i++) {
		delete vcBuffers[i];
	}
	delete[] vcBuffers;
}

/* Returns number of free slots in the buffer (in PHITS). For flexible length buffers, this
 * number is the addition of free reserved space for the channel, plus total free shared space
 * in the buffer.  */
int bufferedPort::getSpace(int vc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return 0; /* Little hack to avoid errors when checking unexisting buffers */
	return vcBuffers[vc]->getSpace();
}

void bufferedPort::checkFlit(int vc, flitModule* &nextFlit) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return;
	vcBuffers[vc]->checkFlit(nextFlit);
}

bool bufferedPort::unLocked(int vc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return true;
	return vcBuffers[vc]->unLocked();
}

int bufferedPort::getBufferOccupancy(int vc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return 0;
	return vcBuffers[vc]->getBufferOccupancy();
}

bool bufferedPort::emptyBuffer(int vc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return true;
	return vcBuffers[vc]->emptyBuffer();
}

bool bufferedPort::canSendFlit(int vc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return false;
	return vcBuffers[vc]->canSendFlit();
}

bool bufferedPort::canReceiveFlit(int vc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return false;
	return vcBuffers[vc]->canReceiveFlit();
}

bool bufferedPort::isBufferSending(int vc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return false; /* Little hack to avoid errors when checking unexisting buffers */
	return vcBuffers[vc]->isBufferSending();
}

void bufferedPort::reorderBuffer(int vc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return;
	vcBuffers[vc]->reorderBuffer();
}

float bufferedPort::getDelay(int vc) const {
	assert(vc >= 0);
	if (vc >= this->numVCs) return 0;
	return vcBuffers[vc]->getDelay();
}

float bufferedPort::getHeadEntryCycle(int vc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return 0;
	return vcBuffers[vc]->getHeadEntryCycle();
}

void bufferedPort::setCurPkt(int vc, int id) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return;
	vcBuffers[vc]->m_currentPkt = id;
}

int bufferedPort::getCurPkt(int vc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return -1;
	return vcBuffers[vc]->m_currentPkt;
}

void bufferedPort::setOutCurPkt(int vc, int port) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return;
	vcBuffers[vc]->m_outPort_currentPkt = port;
}

int bufferedPort::getOutCurPkt(int vc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return -1;
	return vcBuffers[vc]->m_outPort_currentPkt;
}

void bufferedPort::setNextVcCurPkt(int vc, int nextVc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return;
	vcBuffers[vc]->m_nextVC_currentPkt = nextVc;
}

int bufferedPort::getNextVcCurPkt(int vc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return -1;
	return vcBuffers[vc]->m_nextVC_currentPkt;
}

int bufferedPort::getBufferCapacity(int vc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return 0; /* Little hack to avoid errors when checking unexisting buffers */
	return vcBuffers[vc]->bufferCapacity;
}

void bufferedPort::setPktLock(int vc, int id) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return;
	vcBuffers[vc]->m_pktLock = id;
}

int bufferedPort::getPktLock(int vc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return -1;
	return vcBuffers[vc]->m_pktLock;
}

void bufferedPort::setPortPktLock(int vc, int port) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return;
	vcBuffers[vc]->m_portLock = port;
}

void bufferedPort::setVcPktLock(int vc, int prevVc) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return;
	vcBuffers[vc]->m_vcLock = prevVc;
}

void bufferedPort::setUnlocked(int vc, int unlocked) {
	assert(vc >= 0);
	if (vc >= this->numVCs) return;
	vcBuffers[vc]->m_unLocked = unlocked;
}
