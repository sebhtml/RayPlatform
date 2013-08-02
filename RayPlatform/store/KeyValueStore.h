/*
 *  Ray -- Parallel genome assemblies for parallel DNA sequencing
 *  Copyright (C) 2013 Sébastien Boisvert
 *
 *  http://DeNovoAssembler.SourceForge.Net/
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You have received a copy of the GNU General Public License
 *  along with this program (gpl-3.0.txt).
 *  see <http://www.gnu.org/licenses/>
 */

#ifndef KeyValueStoreHeader
#define KeyValueStoreHeader

#include <RayPlatform/core/types.h>
#include <RayPlatform/memory/MyAllocator.h>

#include <map>
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
	
	map<string,char* > m_values;
	map<string,int> m_sizes;

	Rank m_rank;
	int m_size;
	RingAllocator*m_outboxAllocator;
	StaticVector*m_inbox;
	StaticVector*m_outbox;

public:

	KeyValueStore();

	void initialize(Rank rank, int size, RingAllocator * outboxAllocator, StaticVector * inbox, StaticVector * outbox);

	void insert(const char * key, int keyLength, const char * value, int valueLength);
	void remove(const char * key, int keyLength);
	void get(const char * key, int keyLength, const char ** value, int * valueLength);

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

	void clear();
	void destroy();
};

#endif /* KeyValueStoreHeader */
