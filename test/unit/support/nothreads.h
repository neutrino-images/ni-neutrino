/*
 * nothreads.h - a scope in which no thread can be made
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __support_nothreads_h__
#define __support_nothreads_h__

#include <pthread.h>

/* While one lives, a thread made with the default attributes cannot be made: the
   default stack is larger than the address space, so the mapping fails whatever
   the process limits are, and no stack a finished thread left behind is that
   size. canMake() says whether a thread can be made at all, so a case shows the
   scope works before it counts on it. */
struct NoNewThreads
{
	pthread_attr_t saved;

	NoNewThreads()
	{
		pthread_getattr_default_np(&saved);
		pthread_attr_t huge;
		pthread_attr_init(&huge);
		pthread_attr_setstacksize(&huge, (size_t) 1 << 50);
		pthread_setattr_default_np(&huge);
		pthread_attr_destroy(&huge);
	}

	~NoNewThreads()
	{
		pthread_setattr_default_np(&saved);
		pthread_attr_destroy(&saved);
	}

	static bool canMake()
	{
		pthread_t t;
		if (pthread_create(&t, NULL, &nothing, NULL) != 0)
			return false;
		pthread_join(t, NULL);
		return true;
	}

private:
	static void *nothing(void *) { return NULL; }
};

#endif
