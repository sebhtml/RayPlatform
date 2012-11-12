/*
 	RayPlatform: a message-passing development framework
    Copyright (C) 2010, 2011, 2012 Sébastien Boisvert

	http://DeNovoAssembler.SourceForge.Net/

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

#ifndef _MessageTagExecutor_h
#define _MessageTagExecutor_h

#include "MessageTagHandler.h"

#include <RayPlatform/core/types.h>
#include <RayPlatform/communication/Message.h>
#include <RayPlatform/communication/mpi_tags.h>

/**
 * This class is responsible to handling event 
 * for master modes.
 * The technology uses aggressive caching.
 *
 * \author Sébastien Boisvert
 */
class MessageTagExecutor{

#ifdef CONFIG_CACHE_OPERATION_CODES
	MessageTag m_cachedOperationCode;
	MessageTagHandlerReference m_cachedOperationHandler;
#endif /* CONFIG_CACHE_OPERATION_CODES */

/** table of object handlers */
	MessageTagHandlerReference m_objects[MAXIMUM_NUMBER_OF_TAG_HANDLERS];

public:

	/** call the correct handler for a tag on a message */
	void callHandler(MessageTag messageTag,Message*message);

/** set the object to call for a given tag */
	void setObjectHandler(MessageTag messageTag,MessageTagHandlerReference object);

/** set default object and method handlers */
	MessageTagExecutor();
};

#endif
