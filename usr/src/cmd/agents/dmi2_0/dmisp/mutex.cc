// Copyright 09/11/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)mutex.cc	1.3 96/09/11 Sun Microsystems"

#include <synch.h>
mutex_t component_mutex_lock;

void init_component_lock()
{
	mutex_init(&component_mutex_lock, USYNC_THREAD, NULL);
}

void acquire_component_lock()
{
	mutex_lock(&component_mutex_lock);
}

	
void release_component_lock()
{
	mutex_unlock(&component_mutex_lock);
}
