/*
 * Author: Sébastien Boisvert
 * Project: RayPlatform
 * Licence: LGPL3
 */

#include "KeyValueStoreItem.h"

KeyValueStoreItem::KeyValueStoreItem() {

}

KeyValueStoreItem::KeyValueStoreItem(char * value, int valueLength) {
	m_value = value;
	m_size = valueLength;
}

char * KeyValueStoreItem::getValue() {
	return m_value;
}

int KeyValueStoreItem::getValueLength() {
	return m_size;
}

