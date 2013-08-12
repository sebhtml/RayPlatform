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

#ifndef _KeyValueStore_h
#define _KeyValueStore_h

#include <RayPlatform/core/types.h>

#include <string>
using namespace std;

/**
 * A key-value store request.
 *
 * \author Sébastien Boisvert
 */
class KeyValueStoreRequest {

	int m_type;
	string m_key;
	Rank m_rank;

public:
	void initialize(const string & key, const Rank & rank);
	void setTypeToPullRequest();
	void setTypeToPushRequest();
	const Rank & getRank() const;
	bool isAPullRequest() const;
	bool isAPushRequest() const;
	const string & getKey() const;
};

#endif
