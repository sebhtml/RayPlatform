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

#ifndef CarriageableItemHeader
#define CarriageableItemHeader

#include <stdint.h>

/**
 * Interface to carry an item across a media.
 *
 *
 * TODO: add a dependency on the message size.
 * TODO add the concept of part numbers
 *
 * \author Sébastien Boisvert
 *
 */
class CarriageableItem {

public:

	/**
	 * load the object from a stream
	 *
	 * \returns bytes loaded
	 */
	virtual int load(const uint8_t * buffer) = 0;

	/**
	 *
	 * dump the object in a stream
	 *
	 * \returns bytes written
	 */
	virtual int dump(uint8_t * buffer) const = 0;

	//virtual int getRequiredNumberOfBytes() = 0;

	virtual ~CarriageableItem() {}
};

#endif
