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

#ifndef _CorePlugin_h
#define _CorePlugin_h

#include <RayPlatform/core/types.h>

#ifdef CONFIG_MINI_RANKS

#define __GetPlugin(corePlugin)
#define __GetMethod(handle)

#define __GetAdapterObject( corePlugin, handle) \
	m_adapter_  ## handle 

#define __BindAdapter(corePlugin,handle) \
	__GetAdapterObject( corePlugin, handle) . setObject( this )

/*
 * Get the adapter
 */
#define __GetAdapter(corePlugin,handle) \
	& __GetAdapterObject (corePlugin, handle)

/*
 * Add an adapter in a plugin
 */
#define __AddAdapter(corePlugin,handle) \
	Adapter_  ## handle m_adapter_  ## handle 

#define __CreatePlugin( corePlugin ) 

#define __BindPlugin( corePlugin ) 

#define __DeclarePlugin( corePlugin ) \
class corePlugin;

#else

/**
 * Without mini-ranks.
 */

#define __BindAdapter(corePlugin, handle)

/** get the static name for the variable **/
#define __GetPlugin(corePlugin) \
	staticPlugin_ ## corePlugin

/** get the method name in the plugin **/
#define __GetMethod(handle) \
	call_ ## handle

/** get the function pointer name for the adapter **/
#define __GetAdapter(corePlugin,handle) \
	Adapter_ ## corePlugin ## _call_ ## handle 

#define __AddAdapter(corePlugin,handle)

/* create the static core pluging thing. */
#define __CreatePlugin( corePlugin ) \
	static corePlugin * __GetPlugin( corePlugin ) ;

#define __BindPlugin( corePlugin ) \
	__GetPlugin( corePlugin ) = this;

#define __DeclarePlugin( corePlugin ) \

#endif /* CONFIG_MINI_RANKS */


class ComputeCore;

/** 
 * In the Ray Platform, plugins are registered onto the
 * core (ComputeCore).
 * The only method of this interface is to register the plugin.
 */
class CorePlugin{

protected:

	PluginHandle m_plugin;
	ComputeCore*m_core;

public:

/** register the plugin **/
	virtual void registerPlugin(ComputeCore*computeCore);

/** resolve the symbols **/
	virtual void resolveSymbols(ComputeCore*core);

	virtual ~CorePlugin();
};

#endif

