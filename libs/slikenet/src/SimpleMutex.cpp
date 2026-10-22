/*
 *  Original work: Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  RakNet License.txt file in the licenses directory of this source tree. An additional grant
 *  of patent rights can be found in the RakNet Patents.txt file in the same directory.
 *
 *
 *  Modified work: Copyright (c) 2017-2020, SLikeSoft UG (haftungsbeschränkt)
 *
 *  This source code was modified by SLikeSoft. Modifications are licensed under the MIT-style
 *  license found in the license.txt file in the root directory of this source tree.
 */

 /// \file
 ///

#include <slikenet/SimpleMutex.h>
#include <slikenet/assert.h>

using namespace SLNet;

SimpleMutex::SimpleMutex()
{
#if defined(WINDOWS_PHONE_8) || defined(WINDOWS_STORE_RT)
	InitializeCriticalSectionEx(&cs, 0, CRITICAL_SECTION_NO_DEBUG_INFO);
#elif defined(_WIN32)
	InitializeCriticalSection(&cs);
#else
	int error = pthread_mutex_init(&mut, 0);
	RakAssert(error == 0);
#endif
}

SimpleMutex::~SimpleMutex()
{
#ifdef _WIN32
	DeleteCriticalSection(&cs);
#else
	pthread_mutex_destroy(&mut);
#endif
}

void SimpleMutex::Lock()
{
#ifdef _WIN32
	EnterCriticalSection(&cs);
#else
	int error = pthread_mutex_lock(&mut);
	RakAssert(error == 0);
#endif
}

void SimpleMutex::Unlock()
{
#ifdef _WIN32
	LeaveCriticalSection(&cs);
#else
	int error = pthread_mutex_unlock(&mut);
	RakAssert(error == 0);
#endif
}
