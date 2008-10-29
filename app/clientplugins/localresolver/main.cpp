/**************************************************************************
*   Copyright 2005-2008 Last.fm Ltd.                                      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
***************************************************************************/

#include <string.h>
#include "TrackResolver.h"


/** always exporting, never importing  **/
#if defined(_WIN32) || defined(WIN32)
    #define RESOLVER_DLLEXPORT __declspec(dllexport)
#else
    #define RESOLVER_DLLEXPORT
#endif


// creation and deletion always occurs on the same thread
// todo: put some asserts in to maintain this
static TrackResolver *g_trackResolver = 0;
static unsigned g_refCount = 0;

void release()
{
    if (0 == --g_refCount) {
        delete g_trackResolver;
    }
}

extern "C" {

RESOLVER_DLLEXPORT
void *
lastfm_getService(const char *service)
{
	if (0 != strcmp("TrackResolver", service))
		return 0;

	if (0 == g_refCount++)
		g_trackResolver = new TrackResolver();

	return g_trackResolver;
}

}

