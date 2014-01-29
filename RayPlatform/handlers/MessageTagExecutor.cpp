/*
 	RayPlatform: a message-passing development framework
    Copyright (C) 2010, 2011, 2012 Sébastien Boisvert

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

#include "MessageTagHandler.h"
#include "MessageTagExecutor.h"

#include <RayPlatform/communication/mpi_tags.h>

#ifdef CONFIG_ASSERT
#include <assert.h>
#endif
#include <stdlib.h> /* for NULL */

void MessageTagExecutor::callHandler(MessageTag messageTag,Message*message){

#ifdef CONFIG_CACHE_OPERATION_CODES
	MessageTagHandlerReference object=m_cachedOperationHandler;

	if(messageTag!=m_cachedOperationCode)
		object=m_objects[messageTag];
#else
	MessageTagHandlerReference object=m_objects[messageTag];
#endif /* CONFIG_CACHE_OPERATION_CODES */

	// it is useless to call base implementations
	// because they are empty
	if(object==NULL)
		return;

#ifdef CONFIG_MINI_RANKS
	object->call(message);
#else
	object(message);
#endif
}

MessageTagExecutor::MessageTagExecutor(){
	for(int i=0;i<MAXIMUM_NUMBER_OF_TAG_HANDLERS;i++){
		m_objects[i]=NULL;
	}

#ifdef CONFIG_CACHE_OPERATION_CODES
	m_cachedOperationCode=INVALID_HANDLE;
	m_cachedOperationHandler=NULL;
#endif
}

void MessageTagExecutor::setObjectHandler(MessageTag messageTag,MessageTagHandlerReference object){

	#ifdef CONFIG_ASSERT
	assert(messageTag>=0);
	assert(messageTag < MAXIMUM_NUMBER_OF_TAG_HANDLERS);
	#endif

	m_objects[messageTag]=object;
}

