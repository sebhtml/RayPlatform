/*
 	RayPlatform
    Copyright (C) 2012  Sébastien Boisvert

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

#ifndef _MiniRank_h
#define _MiniRank_h

#include "ComputeCore.h"

/**
 * The class for a minirank.
 * This is the only place in RayPlatform where there is
 * any lock. Here, the design uses spinlocks.
 *
 * \author Sébastien Boisvert
 *
 * \see https://computing.llnl.gov/tutorials/pthreads/
 */
class MiniRank{

protected:
	ComputeCore m_computeCore;
public:

	virtual void run()=0;
	ComputeCore*getCore();
};

#endif
