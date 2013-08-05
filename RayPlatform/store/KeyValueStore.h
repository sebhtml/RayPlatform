/*
	RayPlatform: a message-passing development framework
    Copyright (C) 2010, 2011, 2012, 2013 Sébastien Boisvert

	http://github.com/sebhtml/RayPlatform

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You have received a copy of the GNU Lesser General Public License
    along with this program (lgpl-3.0.txt).
	see <http://www.gnu.org/licenses/>

*/

#ifndef KeyValueStoreHeader
#define KeyValueStoreHeader

#include "KeyValueStoreItem.h"

#include <RayPlatform/core/types.h>
#include <RayPlatform/memory/RingAllocator.h>
#include <RayPlatform/memory/MyAllocator.h>
#include <RayPlatform/structures/StaticVector.h>

#include <map>
#include <string>
using namespace std;

#include <stdint.h>

/**
 *
 * This is an initial design for a MPI key-value store in
 * RayPlatform.
 *
 * Each Rank has its own KeyValueStore, and these can send and
 * receive keys between each others.
 *
 * The purpose of this is to ease synchronization between ranks
 * when the message size is bounded.
 *
 * \author Sébastien Boisvert
 */
class KeyValueStore {
	MyAllocator m_memoryAllocator;

	map<string,KeyValueStoreItem> m_items;

	Rank m_rank;
	int m_size;
	RingAllocator*m_outboxAllocator;
	StaticVector*m_inbox;
	StaticVector*m_outbox;

	bool getKey(const char * key, int keyLength, string & keyObject);

public:

	KeyValueStore();

	void initialize(Rank rank, int size, RingAllocator * outboxAllocator, StaticVector * inbox, StaticVector * outbox);

	bool insert(const char * key, int keyLength, char * value, int valueLength);
	bool remove(const char * key, int keyLength);
	bool get(const char * key, int keyLength, char ** value, int * valueLength);

	bool sendKeyAndValueToRank(const char * key, int keyLength, Rank destination);

	/**
	 * Get a key-value entry from a source.
	 *
	 * Usage:
	 *
	 * if(m_store.receiveKeyAndValueFromRank("joe", 3, 42)) {
	 *       char * value;
	 *       int valueLength;
	 *       m_store.get("joe", 3, &value, &valueLength);
	 *
	 *       cout << " retrieved KEY <joe> from rank 42, VALUE has length ";
	 *       cout << valueLength << " bytes" << endl;
	 * }
	 */
	bool receiveKeyAndValueFromRank(const char * key, int keyLength, Rank source);

	char * allocateMemory(int bytes);

	void clear();
	void destroy();
};

#endif /* KeyValueStoreHeader */
