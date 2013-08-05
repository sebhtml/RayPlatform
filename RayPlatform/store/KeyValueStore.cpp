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



#include "KeyValueStore.h"

KeyValueStore::KeyValueStore() {

	m_rank = 0;
	m_size = 0;
	m_outboxAllocator = NULL;
	m_inbox = NULL;
	m_outbox = NULL;
}

void KeyValueStore::initialize(Rank rank, int size, RingAllocator * outboxAllocator, StaticVector * inbox, StaticVector * outbox) {

	m_rank = rank;
	m_size = size;
	m_outboxAllocator = outboxAllocator;
	m_inbox = inbox;
	m_outbox = outbox;
}

bool KeyValueStore::getKey(const char * key, int keyLength, string & keyObject) {

	#define MAXIMUM_KEY_LENGTH 256
	char buffer[MAXIMUM_KEY_LENGTH];

#ifdef CONFIG_ASSERT
	assert(keyLength + 1 <= MAXIMUM_KEY_LENGTH);
#endif

	if(keyLength + 1 > MAXIMUM_KEY_LENGTH)
		return false;

	memcpy(buffer, key, keyLength);
	buffer[keyLength] = '\0';

	keyObject = buffer;

	return true;
}

/**
 * TODO: remove limit on key length
 */
bool KeyValueStore::insert(const char * key, int keyLength, char * value, int valueLength) {

	string keyObject;

	if(!getKey(key, keyLength, keyObject))
		return false;

	if(m_items.count(keyObject) != 0)
		return false;

	KeyValueStoreItem item(value, valueLength);
	m_items[keyObject] = item;

	cout << "[DEBUG] KeyValueStore registered key " << keyObject << " (length: " << keyLength;
	cout << ")";
	cout << " with value (length: " << valueLength << ")";
	cout << endl;

	return true;
}

bool KeyValueStore::remove(const char * key, int keyLength) {

	string keyObject;
	if(!getKey(key, keyLength, keyObject))
		return false;

	// nothing to remove
	if(m_items.count(keyObject) == 0)
		return true;

	m_items.erase(keyObject);

#ifdef CONFIG_ASSERT
	assert(m_items.count(keyObject) == 0);
#endif ////// CONFIG_ASSERT

	return true;
}

bool KeyValueStore::get(const char * key, int keyLength, char ** value, int * valueLength) {

	string keyObject;

	if(!getKey(key, keyLength, keyObject))
		return false;

	// nothing to remove
	if(m_items.count(keyObject) == 0)
		return false;

	KeyValueStoreItem & item = m_items[keyObject];

	(*value) = item.getValue();
	(*valueLength) = item.getValueLength();

	return true;
}

bool KeyValueStore::sendKeyAndValueToRank(const char * key, int keyLength, Rank destination) {
	return false;
}

bool KeyValueStore::receiveKeyAndValueFromRank(const char * key, int keyLength, Rank source) {
	return false;
}

void KeyValueStore::clear() {
	m_items.clear();
}

void KeyValueStore::destroy() {
	clear();
}

