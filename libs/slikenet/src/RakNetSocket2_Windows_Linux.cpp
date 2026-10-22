/*
 *  Original work: Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  RakNet License.txt file in the licenses directory of this source tree. An additional grant
 *  of patent rights can be found in the RakNet Patents.txt file in the same directory.
 *
 *
 *  Modified work: Copyright (c) 2016-2019, SLikeSoft UG (haftungsbeschränkt)
 *
 *  This source code was modified by SLikeSoft. Modifications are licensed under the MIT-style
 *  license found in the license.txt file in the root directory of this source tree.
 */

#if !defined(WINDOWS_STORE_RT) && !defined(__native_client__)

#include <string.h>
#include <slikenet/socket2.h>

using namespace SLNet;

#ifdef _WIN32

// based on https://stackoverflow.com/questions/122208/get-the-ip-address-of-local-computer#1317284
void RNS2_Windows::GetMyIP(SystemAddress addresses[MAXIMUM_NUMBER_OF_INTERNAL_IDS])
{
	ULONG outBufLen = 45 * 1024; // reserve 45 KB of memory which is the upper limit taken from the sample in MSDN (ie. 15 KB * 3 iterations)
	PIP_ADAPTER_ADDRESSES pAddresses = static_cast<IP_ADAPTER_ADDRESSES*>(rakMalloc_Ex(outBufLen, _FILE_AND_LINE_));
	if(pAddresses == nullptr)
	{
		// #med - error logging and/or throw exception?
		return;
	}

#if RAKNET_SUPPORT_IPV6
	ULONG addressFamily = AF_UNSPEC;
#else
	ULONG addressFamily = AF_INET;
#endif

	DWORD error = GetAdaptersAddresses(addressFamily, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_FRIENDLY_NAME, nullptr, pAddresses, &outBufLen);
	if(error != ERROR_SUCCESS)
	{
		// #med - error logging and/or throw exception?
		rakFree_Ex(pAddresses, _FILE_AND_LINE_);
		return;
	}

	PIP_ADAPTER_ADDRESSES pCurAdapter = pAddresses;
	size_t outAddressIndex = 0;
	while((pCurAdapter != nullptr) && (outAddressIndex < MAXIMUM_NUMBER_OF_INTERNAL_IDS))
	{
		// skip loopback adapters
		if(pCurAdapter->IfType != IF_TYPE_SOFTWARE_LOOPBACK)
		{

			// parse the adapter's unicast addresses
			for(PIP_ADAPTER_UNICAST_ADDRESS curAddress = pCurAdapter->FirstUnicastAddress;
			    curAddress != nullptr && outAddressIndex < MAXIMUM_NUMBER_OF_INTERNAL_IDS;
			    curAddress = curAddress->Next)
			{
				const ADDRESS_FAMILY curAddressFamily = curAddress->Address.lpSockaddr->sa_family;
				if(curAddressFamily == AF_INET)
				{
					sockaddr_in *curSocketAddress = reinterpret_cast<sockaddr_in*>(curAddress->Address.lpSockaddr);

					// #med - review, is this really necessary?
					// skip source only addresses (aka: 0.0.0.0/8 - see RFC1700 p.4)
					if(*(char*)&curSocketAddress->sin_addr.s_addr != 0)
					{
						// store the adapter's address
						addresses[outAddressIndex++].address.addr4 = *curSocketAddress;
					}
				}
				else if(curAddressFamily == AF_INET6)
				{
					sockaddr_in6 *curSocketAddress = reinterpret_cast<sockaddr_in6*>(curAddress->Address.lpSockaddr);

					// skip non-external addresses
					if(IN6_IS_ADDR_LINKLOCAL(&curSocketAddress->sin6_addr))
						continue;

					// skip special addresses starting with 2001::/32
					unsigned char specialAddressStart[] = {0x20, 0x01, 0x00, 0x00};
					if(memcmp(curSocketAddress->sin6_addr.s6_addr, specialAddressStart, sizeof(specialAddressStart)) == 0)
						continue;

					// store the adapter's address
					addresses[outAddressIndex++].address.addr6 = *curSocketAddress;
				}
				// else skip the address (neither IPv4 nor IPv6 address)
			}
		}

		// continue with next adapter's addresses
		pCurAdapter = pCurAdapter->Next;
	}
	rakFree_Ex(pAddresses, _FILE_AND_LINE_);

	while(outAddressIndex < MAXIMUM_NUMBER_OF_INTERNAL_IDS)
	{
		addresses[outAddressIndex++] = UNASSIGNED_SYSTEM_ADDRESS;
	}
}

#else // _WIN32

#include <sys/types.h> // used for getifaddrs()
#include <net/if.h> // used for IFF_LOOPBACK
#include <ifaddrs.h>   // used for getifaddrs()

// based on https://stackoverflow.com/questions/212528/get-the-ip-address-of-the-machine#265978
void RNS2_Linux::GetMyIP(SystemAddress addresses[MAXIMUM_NUMBER_OF_INTERNAL_IDS])
{
	struct ifaddrs *pAddresses = nullptr;

	// #med - add error check to getifaddrs()-call
	getifaddrs(&pAddresses);

	size_t outAddressIndex = 0;
	for(ifaddrs *pCurAdapter = pAddresses;
	    pCurAdapter != nullptr && outAddressIndex < MAXIMUM_NUMBER_OF_INTERNAL_IDS;
	    pCurAdapter = pCurAdapter->ifa_next)
	{
		// skip interfaces which don't have any address assigned (according to the manual,
		// this would only apply for BSD, but still we'd check for nullptr here just in case)
		if(pCurAdapter->ifa_addr == nullptr)
		{
			continue;
		}
		// skip loopback adapters
		if(pCurAdapter->ifa_flags & IFF_LOOPBACK)
		{
			continue;
		}

		if(pCurAdapter->ifa_addr->sa_family == AF_INET)
		{
			sockaddr_in *curSocketAddress = reinterpret_cast<sockaddr_in*>(pCurAdapter->ifa_addr);

			// #med - review, is this really necessary?
			// skip source only addresses (aka: 0.0.0.0/8 - see RFC1700 p.4)
			if(*(char*)&curSocketAddress->sin_addr.s_addr != 0)
			{
				// store the adapter's address
				addresses[outAddressIndex++].address.addr4 = *curSocketAddress;
			}
		}
#if RAKNET_SUPPORT_IPV6
		else if(pCurAdapter->ifa_addr->sa_family == AF_INET6)
		{
			sockaddr_in6 *curSocketAddress = reinterpret_cast<sockaddr_in6*>(pCurAdapter->ifa_addr);

			// skip non-external addresses
			if(IN6_IS_ADDR_LINKLOCAL(&curSocketAddress->sin6_addr))
				continue;

			// skip special addresses starting with 2001::/32
			unsigned char specialAddressStart[] = {0x20, 0x01, 0x00, 0x00};
			if(memcmp(curSocketAddress->sin6_addr.s6_addr, specialAddressStart, sizeof(specialAddressStart)) == 0)
				continue;

			// store the adapter's address
			addresses[outAddressIndex++].address.addr6 = *curSocketAddress;
		}
#endif // RAKNET_SUPPORT_IPV6
		// else skip the address (neither IPv4 nor IPv6 address)
	}

	if(pAddresses != nullptr)
		freeifaddrs(pAddresses);

	while(outAddressIndex < MAXIMUM_NUMBER_OF_INTERNAL_IDS)
	{
		addresses[outAddressIndex++] = UNASSIGNED_SYSTEM_ADDRESS;
	}
}

#endif // _WIN32

#endif // !defined(WINDOWS_STORE_RT) && !defined(__native_client__)
