/*
 *  Original work: Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  RakNet License.txt file in the licenses directory of this source tree. An additional grant
 *  of patent rights can be found in the RakNet Patents.txt file in the same directory.
 *
 *
 *  Modified work: Copyright (c) 2016-2020, SLikeSoft UG (haftungsbeschränkt)
 *
 *  This source code was modified by SLikeSoft. Modifications are licensed under the MIT-style
 *  license found in the license.txt file in the root directory of this source tree.
 */

 // Every platform except windows store 8 and native client supports Berkley sockets
#if !defined(WINDOWS_STORE_RT) && !defined(__native_client__)

#include <stdio.h>
#include <string.h> // memcpy
#include <slikenet/socket2.h>
#include <slikenet/SocketDefines.h>
#include <slikenet/GetTime.h>

using namespace SLNet;

#ifdef _WIN32
#include <tchar.h>	// used for _tprintf() (via RAKNET_DEBUG_TPRINTF)
#else
#include <slikenet/LinuxStrings.h> // used for _stricmp()
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>  // error numbers
#if !defined(ANDROID)
#include <ifaddrs.h>
#endif
#include <netinet/in.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h> // used for getaddrinfo()
#endif

void RNS2_Berkley::SetSocketOptions(void)
{
	int r;
	// This doubles the max throughput rate
	int sock_opt = 1024 * 256;
	r = setsockopt__(rns2Socket, SOL_SOCKET, SO_RCVBUF, (char *)&sock_opt, sizeof(sock_opt));
	RakAssert(r == 0);

	// Immediate hard close. Don't linger the socket, or recreating the socket quickly on Vista fails.
	// Fail with voice and xbox

	sock_opt = 0;
	r = setsockopt__(rns2Socket, SOL_SOCKET, SO_LINGER, (char *)&sock_opt, sizeof(sock_opt));
	// Do not assert, ignore failure



	// This doesn't make much difference: 10% maybe
	// Not supported on console 2
	sock_opt = 1024 * 16;
	r = setsockopt__(rns2Socket, SOL_SOCKET, SO_SNDBUF, (char *)&sock_opt, sizeof(sock_opt));
	RakAssert(r == 0);

}

void RNS2_Berkley::SetNonBlockingSocket(unsigned long nonblocking)
{
#ifdef _WIN32
	SLNET_VERIFY(ioctlsocket__(rns2Socket, FIONBIO, &nonblocking) == 0);
#else
	if(nonblocking)
		fcntl(rns2Socket, F_SETFL, O_NONBLOCK);
#endif
}
void RNS2_Berkley::SetBroadcastSocket(int broadcast)
{
	setsockopt__(rns2Socket, SOL_SOCKET, SO_BROADCAST, (char *)&broadcast, sizeof(broadcast));
}
void RNS2_Berkley::SetIPHdrIncl(int ipHdrIncl)
{

	setsockopt__(rns2Socket, IPPROTO_IP, IP_HDRINCL, (char *)&ipHdrIncl, sizeof(ipHdrIncl));

}
void RNS2_Berkley::SetDoNotFragment(int opt)
{
#if defined( IP_DONTFRAGMENT )
#if defined(_WIN32) && !defined(_DEBUG)
	// If this assert hit you improperly linked against WSock32.h
	RakAssert(IP_DONTFRAGMENT == 14);
#endif
	setsockopt__(rns2Socket, boundAddress.GetIPPROTO(), IP_DONTFRAGMENT, (char *)&opt, sizeof(opt));
#endif
}

void RNS2_Berkley::GetSystemAddress(RNS2Socket rns2Socket, SystemAddress *systemAddressOut)
{
	SystemAddress::In6OrIn4 addr;
	socklen_t addrLen = sizeof(addr);

	if(getsockname__(rns2Socket, &addr.addr, &addrLen) != 0)
	{
	#if defined(_WIN32) && defined(_DEBUG)
		DWORD dwIOError = GetLastError();
		LPTSTR messageBuffer;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, dwIOError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Default language
			(LPTSTR)&messageBuffer, 0, nullptr);
		// something has gone wrong here...
		RAKNET_DEBUG_TPRINTF(_T("getsockname failed:Error code - %lu\n%s"), dwIOError, messageBuffer);

		//Free the buffer.
		LocalFree(messageBuffer);
	#endif
		systemAddressOut->FromString(nullptr);
		return;
	}

	memcpy(&systemAddressOut->address, &addr, addrLen);
	if(addr.addr.sa_family == AF_INET)
	{
		uint32_t zero = INADDR_ANY;
		if(memcmp(&systemAddressOut->address.addr4.sin_addr.s_addr, &zero, sizeof(zero)) == 0)
			systemAddressOut->SetToLoopback(4);
		//	systemAddressOut->address.addr4.sin_port=ntohs(systemAddressOut->address.addr4.sin_port);
	}
	else
	{
		if(memcmp(&systemAddressOut->address.addr6.sin6_addr, &in6addr_any, sizeof(in6addr_any)) == 0)
			systemAddressOut->SetToLoopback(6);
		//	systemAddressOut->address.addr6.sin6_port=ntohs(systemAddressOut->address.addr6.sin6_port);
	}
}

RNS2BindResult RNS2_Berkley::BindSharedIPV4And6(RNS2_BerkleyBindParameters *bindParameters, const char *file, unsigned int line)
{
	(void)file;
	(void)line;

	addrinfo *addrInfo;
	addrinfo hint = {}; hint.ai_flags = AI_PASSIVE;
	if(bindParameters->addressFamily == AF_UNSPEC)
		hint.ai_family = AF_INET6;
	else
		hint.ai_family = bindParameters->addressFamily;
	hint.ai_socktype = bindParameters->type;
	hint.ai_protocol = bindParameters->protocol;
	char portStr[6];
	sprintf(portStr, "%hu", bindParameters->port);


	// On Ubuntu, "" returns "No address associated with hostname" while 0 works.
	const char *hostAddress = nullptr;
	if(bindParameters->hostAddress && bindParameters->hostAddress[0] != '\0' && _stricmp(bindParameters->hostAddress, "UNASSIGNED_SYSTEM_ADDRESS") != 0)
	{
		hostAddress = bindParameters->hostAddress;
	}
	if(getaddrinfo(hostAddress, portStr, &hint, &addrInfo) != 0)
	{
		return BR_FAILED_TO_BIND_SOCKET;
	}

	// Try all returned addresses until one works
	for(addrinfo *ai = addrInfo; ai != nullptr; ai = ai->ai_next)
	{
		// Open socket. The address type depends on what getaddrinfo() gave us.
		rns2Socket = socket__(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if(rns2Socket != INVALID_SOCKET)
		{
			if(ai->ai_family == AF_INET6)
			{
				// Enable or disable dual stack mode
				int v6only = (bindParameters->addressFamily != AF_UNSPEC);
				setsockopt__(rns2Socket, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&v6only, sizeof(v6only));
			}

			if(bind__(rns2Socket, ai->ai_addr, ai->ai_addrlen) == 0)
			{
				memcpy(&boundAddress.address.addr, ai->ai_addr, ai->ai_addrlen);
				freeaddrinfo(addrInfo);

				SetSocketOptions();
				SetNonBlockingSocket(bindParameters->nonBlockingSocket);
				SetBroadcastSocket(bindParameters->setBroadcast);
				SetIPHdrIncl(bindParameters->setIPHdrIncl);

				GetSystemAddress(rns2Socket, &boundAddress);

				return BR_SUCCESS;
			}
			else
			{
				closesocket__(rns2Socket);
			}
		}
	}
	freeaddrinfo(addrInfo);

	return BR_FAILED_TO_BIND_SOCKET;
}

void RNS2_Berkley::RecvFromBlocking(RNS2RecvStruct *recvFromStruct)
{
	SystemAddress::In6OrIn4 their_addr;
	socklen_t sockLen = sizeof(their_addr);

	recvFromStruct->bytesRead = recvfrom__(rns2Socket, recvFromStruct->data, sizeof(recvFromStruct->data), 0, &their_addr.addr, &sockLen);
	if(recvFromStruct->bytesRead == -1)
	{
	#if defined(_WIN32) && defined(_DEBUG) && !defined(WINDOWS_PHONE_8)
		int ioError = WSAGetLastError();
		if (ioError == WSAECONNRESET)
		{
			RAKNET_DEBUG_TPRINTF(_T("A previous send operation resulted in an ICMP Port Unreachable message.\n"));
		}
		else if (ioError != WSAEWOULDBLOCK && ioError != WSAEADDRNOTAVAIL)
		{
			LPTSTR messageBuffer;
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, ioError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Default language
				(LPTSTR)&messageBuffer, 0, nullptr);
			// I see this hit on XP with IPV6 for some reason
			RAKNET_DEBUG_TPRINTF(_T("Warning: recvfrom failed:Error code - %d\n%s"), ioError, messageBuffer);
			LocalFree(messageBuffer);
		}
	#endif
		return;
	}

	recvFromStruct->timeRead = SLNet::GetTimeUS();

	memcpy(&recvFromStruct->systemAddress.address.addr, &their_addr.addr, sockLen);
}

#endif // !defined(WINDOWS_STORE_RT) && !defined(__native_client__)
