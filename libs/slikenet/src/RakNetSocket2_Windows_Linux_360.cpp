/*
 *  Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  RakNet License.txt file in the licenses directory of this source tree. An additional grant
 *  of patent rights can be found in the RakNet Patents.txt file in the same directory.
 *
 *
 *  Modified work: Copyright (c) 2017, SLikeSoft UG (haftungsbeschränkt)
 *
 *  This source code was modified by SLikeSoft. Modifications are licensed under the MIT-style
 *  license found in the license.txt file in the root directory of this source tree.
 */

#include <stdio.h>
#include <slikenet/socket2.h>
#include <slikenet/SocketDefines.h>

using namespace SLNet;

#if (defined(_WIN32) || defined(__GNUC__)  || defined(__GCCXML__) || defined(__S3E__) ) && !defined(WINDOWS_STORE_RT) && !defined(__native_client__)

RNS2SendResult RNS2_Windows_Linux_360::Send_Windows_Linux_360NoVDP(RNS2Socket rns2Socket, RNS2_SendParameters *sendParameters, const char *file, unsigned int line)
{
	(void)file;
	(void)line;

	int len = 0;
	do
	{
		int oldTTL = -1;
		if(sendParameters->ttl > 0)
		{
			socklen_t opLen = sizeof(oldTTL);
			// Get the current TTL
			if(getsockopt__(rns2Socket, sendParameters->systemAddress.GetIPPROTO(), IP_TTL, (char*)&oldTTL, &opLen) != -1)
			{
				int newTTL = sendParameters->ttl;
				setsockopt__(rns2Socket, sendParameters->systemAddress.GetIPPROTO(), IP_TTL, (char*)&newTTL, sizeof(newTTL));
			}
		}


		// ofxRakNet patch: the length of this address family, not of the whole union (macOS: EINVAL)
		len = sendto__(rns2Socket, sendParameters->data, sendParameters->length, 0, &sendParameters->systemAddress.address.addr, (sendParameters->systemAddress.address.addr.sa_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in)));
		if(len < 0)
		{
			RAKNET_DEBUG_PRINTF("sendto failed with code %i for char %hhu and length %i.\n", len, sendParameters->data[0], sendParameters->length);
		}


		if(oldTTL != -1)
		{
			setsockopt__(rns2Socket, sendParameters->systemAddress.GetIPPROTO(), IP_TTL, (char*)&oldTTL, sizeof(oldTTL));
		}

	} while(len == 0);
	return len;
}

#endif // Windows, Linux, 360
