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

#include "KeyValueStoreRequest.h"

#define KEY_VALUE_STORE_OPERATION_NONE  0x0
#define KEY_VALUE_STORE_OPERATION_PULL_REQUEST 0x1
#define KEY_VALUE_STORE_OPERATION_PUSH_REQUEST 0x2

KeyValueStoreRequest::KeyValueStoreRequest() {
	m_type = KEY_VALUE_STORE_OPERATION_NONE;
}

void KeyValueStoreRequest::initialize(const string & key, const Rank & rank) {

	m_key = key;
	m_rank = rank;
}

void KeyValueStoreRequest::setTypeToPullRequest() {

	m_type = KEY_VALUE_STORE_OPERATION_PULL_REQUEST;
}

void KeyValueStoreRequest::setTypeToPushRequest() {

	m_type = KEY_VALUE_STORE_OPERATION_PUSH_REQUEST;
}

const Rank & KeyValueStoreRequest::getRank() const {

	return m_rank;
}

bool KeyValueStoreRequest::isAPullRequest() const {

	return m_type == KEY_VALUE_STORE_OPERATION_PULL_REQUEST;
}

bool KeyValueStoreRequest::isAPushRequest() const {

	return m_type == KEY_VALUE_STORE_OPERATION_PUSH_REQUEST;
}

const string & KeyValueStoreRequest::getKey() const {

	return m_key;
}
