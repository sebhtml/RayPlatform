/*
 	RayPlatform: a message-passing development framework
    Copyright (C) 2010, 2011, 2012, 2013 Sébastien Boisvert

	http://github.com/sebhtml/RayPlatform: a message-passing development framework

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




#ifndef _crypto
#define _crypto

#include <stdint.h>

uint64_t uniform_hashing_function_1_64_64(uint64_t key);
uint64_t uniform_hashing_function_2_64_64(uint64_t key);

uint32_t computeCyclicRedundancyCode32(uint8_t * bytes, uint32_t numberOfBytes);

#endif
