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

 /// \file
 ///

#include <slikenet/types.h>
#include <slikenet/assert.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <slikenet/WindowsIncludes.h>
#include <slikenet/WSAStartupSingleton.h>
#include <slikenet/SocketDefines.h>
#include <slikenet/socket2.h>

#if   defined(_WIN32)
// extern __int64 _strtoui64(const char*, char**, int); // needed for Code::Blocks. Does not compile on Visual Studio 2010
// IP_DONTFRAGMENT is different between winsock 1 and winsock 2.  Therefore, Winsock2.h must be linked againt Ws2_32.lib
// winsock.h must be linked against WSock32.lib.  If these two are mixed up the flag won't work correctly
#include <slikenet/WindowsIncludes.h>

#else
#include <sys/socket.h> // used for getnameinfo()
#include <netdb.h>      // used for getnameinfo()
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <slikenet/SocketLayer.h>
#include <slikenet/SuperFastHash.h>
#include <stdlib.h>
#include <slikenet/crtstring_adapter.h>

using namespace SLNet;

constexpr auto IPV6_LOOPBACK = "::1";
constexpr auto IPV4_LOOPBACK = "127.0.0.1";

AddressOrGUID::AddressOrGUID(const Packet *packet)
	: rakNetGuid(packet->guid),
	systemAddress(packet->systemAddress)
{
}

unsigned long AddressOrGUID::ToInteger(const AddressOrGUID& aog)
{
	if(aog.rakNetGuid != UNASSIGNED_RAKNET_GUID)
		return RakNetGUID::ToUint32(aog.rakNetGuid);
	return SystemAddress::ToInteger(aog.systemAddress);
}
const char *AddressOrGUID::ToString(bool writePort) const
{
	if(rakNetGuid != UNASSIGNED_RAKNET_GUID)
		return rakNetGuid.ToString();
	return systemAddress.ToString(writePort);
}
void AddressOrGUID::ToString(bool writePort, char *dest) const
{
	if(rakNetGuid != UNASSIGNED_RAKNET_GUID)
		return rakNetGuid.ToString(dest);
	return systemAddress.ToString(writePort, dest);
}
void AddressOrGUID::ToString(bool writePort, char *dest, size_t destLength) const
{
	if(rakNetGuid != UNASSIGNED_RAKNET_GUID)
		return rakNetGuid.ToString(dest, destLength);
	return systemAddress.ToString(writePort, dest, destLength);
}

SocketDescriptor::SocketDescriptor(unsigned short port, const char *_hostAddress)
	: port(port)
#ifdef __native_client__
	, blockingSocket(false)
#endif
{
	if(_hostAddress)
		strcpy_s(hostAddress, _hostAddress);
}

// Defaults to not in peer to peer mode for NetworkIDs.  This only sends the localSystemAddress portion in the BitStream class
// This is what you want for client/server, where the server assigns all NetworkIDs and it is unnecessary to transmit the full structure.
// For peer to peer, this will transmit the systemAddress of the system that created the object in addition to localSystemAddress.  This allows
// Any system to create unique ids locally.
// All systems must use the same value for this variable.
//bool RAK_DLL_EXPORT NetworkID::peerToPeerMode=false;

SystemAddress& SystemAddress::operator=(const SystemAddress& input)
{
	memcpy(&address, &input.address, sizeof(address));
	systemIndex = input.systemIndex;
	return *this;
}
bool SystemAddress::EqualsExcludingPort(const SystemAddress& right) const
{
	if(address.addr.sa_family != right.address.addr.sa_family)
		return false;

	if(address.addr.sa_family == AF_INET)
		return (address.addr4.sin_addr.s_addr == right.address.addr4.sin_addr.s_addr);
	return IN6_ARE_ADDR_EQUAL(&address.addr6.sin6_addr, &right.address.addr6.sin6_addr);
}
unsigned short SystemAddress::GetPort() const
{
	return ntohs(address.addr4.sin_port);
}
void SystemAddress::SetPortHostOrder(unsigned short port)
{
	address.addr4.sin_port = htons(port);
}
void SystemAddress::SetPortNetworkOrder(unsigned short port)
{
	address.addr4.sin_port = port;
}
bool SystemAddress::operator==(const SystemAddress& right) const
{
	return address.addr4.sin_port == right.address.addr4.sin_port && EqualsExcludingPort(right);
}

bool SystemAddress::operator!=(const SystemAddress& right) const
{
	return !(*this == right);
}

bool SystemAddress::operator>(const SystemAddress& right) const
{
	if(address.addr4.sin_port == right.address.addr4.sin_port)
	{
		if(address.addr4.sin_family == AF_INET)
			return address.addr4.sin_addr.s_addr > right.address.addr4.sin_addr.s_addr;
		return memcmp(address.addr6.sin6_addr.s6_addr, right.address.addr6.sin6_addr.s6_addr, sizeof(address.addr6.sin6_addr.s6_addr)) > 0;
	}
	return address.addr4.sin_port > right.address.addr4.sin_port;
}

bool SystemAddress::operator<(const SystemAddress& right) const
{
	if(address.addr4.sin_port == right.address.addr4.sin_port)
	{
		if(address.addr4.sin_family == AF_INET)
			return address.addr4.sin_addr.s_addr < right.address.addr4.sin_addr.s_addr;
		return memcmp(address.addr6.sin6_addr.s6_addr, right.address.addr6.sin6_addr.s6_addr, sizeof(address.addr6.sin6_addr.s6_addr)) > 0;
	}
	return address.addr4.sin_port < right.address.addr4.sin_port;
}

unsigned long SystemAddress::ToInteger(const SystemAddress &sa)
{
	unsigned int lastHash = SuperFastHashIncremental((const char*)&sa.address.addr4.sin_port, sizeof(sa.address.addr4.sin_port), sizeof(sa.address.addr4.sin_port));
	if(sa.address.addr.sa_family == AF_INET)
		return SuperFastHashIncremental((const char*)&sa.address.addr4.sin_addr, sizeof(sa.address.addr4.sin_addr), lastHash);
	else
		return SuperFastHashIncremental((const char*)&sa.address.addr6.sin6_addr, sizeof(sa.address.addr6.sin6_addr), lastHash);
}

void SystemAddress::SetToLoopback(unsigned char ipVersion)
{
	if(ipVersion == 4)
		FromString(IPV4_LOOPBACK, '\0');
	else
		FromString(IPV6_LOOPBACK, '\0');
}
bool SystemAddress::IsLoopback() const
{
	if(GetIPVersion() == 4)
	{
		if(address.addr4.sin_addr.s_addr == htonl(INADDR_LOOPBACK))
			return true;
		if(address.addr4.sin_addr.s_addr == htonl(INADDR_ANY))
			return true;
	}
	else
	{
		return IN6_IS_ADDR_LOOPBACK(&address.addr6.sin6_addr);
	}
	return false;
}

void SystemAddress::ToString(bool writePort, char *dest, size_t destLength, char portDelineator) const
{
	if(*this == UNASSIGNED_SYSTEM_ADDRESS)
	{
		strcpy_s(dest, destLength, "UNASSIGNED_SYSTEM_ADDRESS");
		return;
	}

	// ofxRakNet patch: the length of this address family, not of the whole union (macOS: EAI_FAMILY)
	if(getnameinfo(&address.addr, (address.addr.sa_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in)), dest, destLength, nullptr, 0, NI_NUMERICHOST) != 0)
	{
		dest[0] = '\0';
		return;
	}

	if(writePort)
	{
		if(GetIPVersion() == 6 && portDelineator == ':')
		{
			// IPv6 addresses need to be enclosed in brackets when writing the port
			char temp[INET6_ADDRSTRLEN];
			strcpy_s(temp, dest);
			sprintf_s(dest, destLength, "[%s]", temp);
		}
		char portStr[] = {portDelineator, '\0'};
		strcat_s(dest, destLength, portStr);
		sprintf(dest + strlen(dest), "%hu", GetPort());
	}
}
void SystemAddress::ToString(bool writePort, char *dest, char portDelineator) const
{
	ToString(writePort, dest, INET6_ADDRSTRLEN + 2 + 1 + 5, portDelineator);
}
const char *SystemAddress::ToString(bool writePort, char portDelineator) const
{
	static unsigned char strIndex = 0;
	constexpr size_t strLength = INET6_ADDRSTRLEN + 2 + 1 + 5;
	static char str[8][strLength];

	unsigned char lastStrIndex = strIndex;
	strIndex++;
	ToString(writePort, str[lastStrIndex & 7], strLength, portDelineator);
	return (char*)str[lastStrIndex & 7];
}
void SystemAddress::FixForIPVersion(const SystemAddress &boundAddressToSocket)
{
	// TODO - what about 255.255.255.255?
	if(GetIPVersion() == 6 && IN6_IS_ADDR_LOOPBACK(&address.addr6.sin6_addr))
	{
		if(boundAddressToSocket.GetIPVersion() == 4)
		{
			FromString(IPV4_LOOPBACK, 0, 4);
		}
	}
	else if(address.addr4.sin_addr.s_addr == INADDR_LOOPBACK)
	{
		if(boundAddressToSocket.GetIPVersion() == 6)
		{
			FromString(IPV6_LOOPBACK, 0, 6);
		}

		// 		if (boundAddressToSocket.GetIPVersion()==4)
		// 		{
		// 			// Some kind of bug with sendto: returns "The requested address is not valid in its context." if loopback doesn't have the same IP address
		// 			address.addr4.sin_addr.s_addr=boundAddressToSocket.address.addr4.sin_addr.s_addr;
		// 		}
	}
}
bool SystemAddress::IsLANAddress()
{
	return GetIPVersion() == 4 && ((address.addr4.sin_addr.s_addr >> 24) == 10 || (address.addr4.sin_addr.s_addr >> 24) == 192);
}

bool SystemAddress::FromString(const char *str, char portDelineator, int ipVersion)
{
	if(!str || str[0] == '\0')
	{
		address = {};
		address.addr.sa_family = AF_INET;
		return true;
	}

	char ipPart[INET6_ADDRSTRLEN] = "";
	if(str[0] == '[')
	{
		const char *ipStart = str + 1;
		const char *ipEnd = strchr(ipStart, ']');
		if(!ipEnd)
		{
			return false;
		}

		strncpy_s(ipPart, ipStart, size_t(ipEnd - ipStart));
		if(portDelineator != '\0' && ipEnd[1] == portDelineator)
			address.addr4.sin_port = htons(atoi(ipEnd + 2));
	}
	else
	{
		if(const char *portPart; portDelineator != '\0' && (portPart = strrchr(str, portDelineator)))
		{
			address.addr4.sin_port = htons(atoi(portPart + 1));
			strncpy_s(ipPart, str, size_t(portPart - str));
		}
		else
		{
			strcpy_s(ipPart, str);
		}
	}

	// This could be a domain, or a printable address such as "192.0.2.1" or "2001:db8:63b3:1::3490"
	addrinfo *addrInfo = nullptr;
	addrinfo hint = {};
	hint.ai_flags = AI_ADDRCONFIG; // return only addresses we can actually connect to
	hint.ai_socktype = SOCK_DGRAM;
	if(ipVersion == 6)
	{
		hint.ai_family = AF_INET6;
		hint.ai_flags |= AI_V4MAPPED;
	}
	else
	{
	#if !RAKNET_SUPPORT_IPV6
		hint.ai_family = AF_INET;
	#else
		if(ipVersion == 4)
			hint.ai_family = AF_INET;
	#endif
	}

	WSAStartupSingleton::AddRef();
	if(getaddrinfo(ipPart, nullptr, &hint, &addrInfo) != 0)
	{
		return false;
	}
	WSAStartupSingleton::Deref();
	RakAssert(addrInfo);

	((sockaddr_in*)addrInfo->ai_addr)->sin_port = address.addr4.sin_port;
	memcpy(&address.addr, addrInfo->ai_addr, addrInfo->ai_addrlen);

	freeaddrinfo(addrInfo); // free the linked list
	return true;
}
bool SystemAddress::FromStringExplicitPort(const char *str, unsigned short port, int ipVersion)
{
	bool b = FromString(str, '\0', ipVersion);
	if(b == false)
	{
		*this = UNASSIGNED_SYSTEM_ADDRESS;
		return false;
	}
	address.addr4.sin_port = htons(port);
	return true;
}
void SystemAddress::CopyPort(const SystemAddress& right)
{
	address.addr4.sin_port = right.address.addr4.sin_port;
}

const char *RakNetGUID::ToString(void) const
{
	static unsigned char strIndex = 0;
	static char str[8][64];

	unsigned char lastStrIndex = strIndex;
	strIndex++;
	ToString(str[lastStrIndex & 7], 64);
	return (char*)str[lastStrIndex & 7];
}
void RakNetGUID::ToString(char *dest) const
{
	if(*this == UNASSIGNED_RAKNET_GUID)
		strcpy(dest, "UNASSIGNED_RAKNET_GUID");
	else
		//sprintf_s(dest, destLength, "%u.%u.%u.%u.%u.%u", g[0], g[1], g[2], g[3], g[4], g[5]);
		sprintf(dest, "%" PRIu64, (uint64_t)g);
	// sprintf_s(dest, destLength, "%u.%u.%u.%u.%u.%u", g[0], g[1], g[2], g[3], g[4], g[5]);
}
void RakNetGUID::ToString(char *dest, size_t destLength) const
{
	if(*this == UNASSIGNED_RAKNET_GUID)
		strcpy_s(dest, destLength, "UNASSIGNED_RAKNET_GUID");
	else
		//sprintf_s(dest, destLength, "%u.%u.%u.%u.%u.%u", g[0], g[1], g[2], g[3], g[4], g[5]);
		sprintf_s(dest, destLength, "%" PRIu64, (uint64_t) g);
	// sprintf_s(dest, destLength, "%u.%u.%u.%u.%u.%u", g[0], g[1], g[2], g[3], g[4], g[5]);
}
bool RakNetGUID::FromString(const char *source)
{
	if(!source)
		return false;
	g = strtoull(source, nullptr, 10);
	return true;
}

unsigned long RakNetGUID::ToUint32(const RakNetGUID &g)
{
	return ((unsigned long)(g.g >> 32)) ^ ((unsigned long)(g.g & 0xFFFFFFFF));
}
