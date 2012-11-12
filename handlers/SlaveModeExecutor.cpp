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

#include "SlaveModeExecutor.h"

#ifdef ASSERT
#include <assert.h>
#endif
#include <stdlib.h> /* for NULL */

void SlaveModeExecutor::callHandler(SlaveMode mode){

#ifdef CONFIG_CACHE_OPERATION_CODES
	SlaveModeHandlerReference object=m_cachedOperationHandler;

	if(mode!=m_cachedOperationCode)
		object=m_objects[mode];
#else
	SlaveModeHandlerReference object=m_objects[mode];
#endif /* CONFIG_CACHE_OPERATION_CODES */

	// don't call it if it is NULL
	if(object==NULL)
		return;

	// call it
#ifdef CONFIG_MINI_RANKS
	object->call();
#else
	object();
#endif
}

SlaveModeExecutor::SlaveModeExecutor(){
	for(int i=0;i<MAXIMUM_NUMBER_OF_SLAVE_HANDLERS;i++){
		m_objects[i]=NULL;
	}

#ifdef CONFIG_CACHE_OPERATION_CODES
	m_cachedOperationCode=INVALID_HANDLE;
	m_cachedOperationHandler=NULL;
#endif
}

void SlaveModeExecutor::setObjectHandler(SlaveMode mode,SlaveModeHandlerReference object){

#ifdef ASSERT
	assert(mode>=0);
	assert(mode<MAXIMUM_NUMBER_OF_SLAVE_HANDLERS);
#endif

	m_objects[mode]=object;
}

