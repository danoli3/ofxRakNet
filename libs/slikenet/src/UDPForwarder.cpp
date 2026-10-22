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

#include <slikenet/UDPForwarder.h>

#if _RAKNET_SUPPORT_UDPForwarder==1

#include <errno.h>
#include <slikenet/GetTime.h>
#include <slikenet/MTUSize.h>
#include <slikenet/SocketLayer.h>
#include <slikenet/WSAStartupSingleton.h>
#include <slikenet/sleep.h>
#include <slikenet/DS_OrderedList.h>
#include <slikenet/LinuxStrings.h>
#include <slikenet/SocketDefines.h>

#ifdef _WIN32
#include <tchar.h>
#else
#ifndef _T
#define _T(x) (x)
#endif
#include <sys/types.h>  // used for getaddrinfo()
#include <sys/socket.h> // used for getaddrinfo()
#include <netdb.h>      // used for getaddrinfo()
#endif

using namespace SLNet;
static const unsigned short DEFAULT_MAX_FORWARD_ENTRIES = 64;

namespace SLNet
{
RAK_THREAD_DECLARATION(UpdateUDPForwarderGlobal);
}

UDPForwarder::ForwardEntry::ForwardEntry()
{
	socket = INVALID_SOCKET;
	timeLastDatagramForwarded = SLNet::GetTimeMS();
	addr1Confirmed = UNASSIGNED_SYSTEM_ADDRESS;
	addr2Confirmed = UNASSIGNED_SYSTEM_ADDRESS;
}
UDPForwarder::ForwardEntry::~ForwardEntry()
{
	if(socket != INVALID_SOCKET)
		closesocket__(socket);
}

UDPForwarder::UDPForwarder()
{
#ifdef _WIN32
	WSAStartupSingleton::AddRef();
#endif

	maxForwardEntries = DEFAULT_MAX_FORWARD_ENTRIES;
	nextInputId = 0;
	startForwardingInput.SetPageSize(sizeof(StartForwardingInputStruct) * 16);
	stopForwardingCommands.SetPageSize(sizeof(StopForwardingStruct) * 16);
}
UDPForwarder::~UDPForwarder()
{
	Shutdown();

#ifdef _WIN32
	WSAStartupSingleton::Deref();
#endif
}
void UDPForwarder::Startup(void)
{
	if(isRunning)
		return;
	isRunning = true;

	int errorCode;



	errorCode = SLNet::RakThread::Create(UpdateUDPForwarderGlobal, this);

	if(errorCode != 0)
	{
		RakAssert(0);
		return;
	}

	while(!threadRunning)
		RakSleep(30);
}
void UDPForwarder::Shutdown(void)
{
	if(!isRunning)
		return;
	isRunning = false;

	while(threadRunning)
		RakSleep(30);

	unsigned int j;
	for(j = 0; j < forwardListNotUpdated.Size(); j++)
		SLNet::OP_DELETE(forwardListNotUpdated[j], _FILE_AND_LINE_);
	forwardListNotUpdated.Clear(false, _FILE_AND_LINE_);
}
void UDPForwarder::SetMaxForwardEntries(unsigned short maxEntries)
{
	RakAssert(maxEntries > 0 && maxEntries < 65535 / 2);
	maxForwardEntries = maxEntries;
}
int UDPForwarder::GetMaxForwardEntries(void) const
{
	return maxForwardEntries;
}
int UDPForwarder::GetUsedForwardEntries(void) const
{
	return (int)forwardListNotUpdated.Size();
}
UDPForwarderResult UDPForwarder::StartForwarding(SystemAddress source, SystemAddress destination, SLNet::TimeMS timeoutOnNoDataMS, const char *forceHostAddress, unsigned short socketFamily,
	unsigned short *forwardingPort, __UDPSOCKET__ *forwardingSocket)
{
	// Invalid parameters?
	if(timeoutOnNoDataMS == 0 || timeoutOnNoDataMS > UDP_FORWARDER_MAXIMUM_TIMEOUT || source == UNASSIGNED_SYSTEM_ADDRESS || destination == UNASSIGNED_SYSTEM_ADDRESS)
		return UDPFORWARDER_INVALID_PARAMETERS;

	if(!isRunning)
		return UDPFORWARDER_NOT_RUNNING;

	unsigned int inputId = nextInputId++;

	StartForwardingInputStruct *sfis;
	sfis = startForwardingInput.Allocate(_FILE_AND_LINE_);
	sfis->source = source;
	sfis->destination = destination;
	sfis->timeoutOnNoDataMS = timeoutOnNoDataMS;
	RakAssert(timeoutOnNoDataMS != 0);
	if(forceHostAddress && forceHostAddress[0])
		sfis->forceHostAddress = forceHostAddress;
	sfis->socketFamily = socketFamily;
	sfis->inputId = inputId;
	startForwardingInput.Push(sfis);

	for(;;)
	{
		RakSleep(0);
		startForwardingOutputMutex.Lock();
		for(unsigned int i = 0; i < startForwardingOutput.Size(); i++)
		{
			if(startForwardingOutput[i].inputId == inputId)
			{
				if(startForwardingOutput[i].result == UDPFORWARDER_SUCCESS)
				{
					if(forwardingPort)
						*forwardingPort = startForwardingOutput[i].forwardingPort;
					if(forwardingSocket)
						*forwardingSocket = startForwardingOutput[i].forwardingSocket;
				}
				UDPForwarderResult res = startForwardingOutput[i].result;
				startForwardingOutput.RemoveAtIndex(i);
				startForwardingOutputMutex.Unlock();
				return res;
			}
		}
		startForwardingOutputMutex.Unlock();
	}
}
void UDPForwarder::StopForwarding(SystemAddress source, SystemAddress destination)
{
	StopForwardingStruct *sfs;
	sfs = stopForwardingCommands.Allocate(_FILE_AND_LINE_);
	sfs->destination = destination;
	sfs->source = source;
	stopForwardingCommands.Push(sfs);
}
void UDPForwarder::RecvFrom(SLNet::TimeMS curTime, ForwardEntry *forwardEntry)
{
#ifndef __native_client__
	char data[MAXIMUM_MTU_SIZE];

	SystemAddress receivedAddr;
	socklen_t sockLen = sizeof(receivedAddr.address);

#ifdef __GNUC__
#ifdef MSG_DONTWAIT
	const int flag = MSG_DONTWAIT;
#else
	const int flag = 0x40;
#endif
#else
	const int flag = 0;
#endif

	int receivedDataLen;
	//unsigned short portnum=0;

	receivedDataLen = recvfrom__(forwardEntry->socket, data, MAXIMUM_MTU_SIZE, flag, &receivedAddr.address.addr, &sockLen);

	if(receivedDataLen < 0)
	{
#if defined(_WIN32) && defined(_DEBUG) && !defined(WINDOWS_PHONE_8) && !defined(WINDOWS_STORE_RT)
		DWORD dwIOError = WSAGetLastError();

		if(dwIOError != WSAECONNRESET && dwIOError != WSAEINTR && dwIOError != WSAETIMEDOUT && dwIOError != WSAEWOULDBLOCK)
		{
			LPTSTR messageBuffer;
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, dwIOError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Default language
				(LPTSTR)&messageBuffer, 0, nullptr);
			// something has gone wrong here...
			RAKNET_DEBUG_TPRINTF(_T("recvfrom failed:Error code - %lu\n%s"), dwIOError, messageBuffer);

			//Free the buffer.
			LocalFree(messageBuffer);
		}
#else
		if(errno != EAGAIN
			&& errno != 0
#if defined(__GNUC__)
			&& errno != EWOULDBLOCK
#endif
			)
		{
			printf("errno=%i\n", errno);
		}
#endif


	}

	if(receivedDataLen <= 0)
		return;

	//portnum=receivedAddr.GetPort();

	SystemAddress forwardTarget;

	bool confirmed1 = forwardEntry->addr1Confirmed != UNASSIGNED_SYSTEM_ADDRESS;
	bool confirmed2 = forwardEntry->addr2Confirmed != UNASSIGNED_SYSTEM_ADDRESS;
	bool matchConfirmed1 =
		confirmed1 &&
		forwardEntry->addr1Confirmed == receivedAddr;
	bool matchConfirmed2 =
		confirmed2 &&
		forwardEntry->addr2Confirmed == receivedAddr;
	bool matchUnconfirmed1 = forwardEntry->addr1Unconfirmed.EqualsExcludingPort(receivedAddr);
	bool matchUnconfirmed2 = forwardEntry->addr2Unconfirmed.EqualsExcludingPort(receivedAddr);

	if(matchConfirmed1 == true || (matchConfirmed2 == false && confirmed1 == false && matchUnconfirmed1 == true))
	{
		// Forward to addr2
		if(forwardEntry->addr1Confirmed == UNASSIGNED_SYSTEM_ADDRESS)
		{
			forwardEntry->addr1Confirmed = receivedAddr;
		}
		if(forwardEntry->addr2Confirmed != UNASSIGNED_SYSTEM_ADDRESS)
			forwardTarget = forwardEntry->addr2Confirmed;
		else
			forwardTarget = forwardEntry->addr2Unconfirmed;
	}
	else if(matchConfirmed2 == true || (confirmed2 == false && matchUnconfirmed2 == true))
	{
		// Forward to addr1
		if(forwardEntry->addr2Confirmed == UNASSIGNED_SYSTEM_ADDRESS)
		{
			forwardEntry->addr2Confirmed = receivedAddr;
		}
		if(forwardEntry->addr1Confirmed != UNASSIGNED_SYSTEM_ADDRESS)
			forwardTarget = forwardEntry->addr1Confirmed;
		else
			forwardTarget = forwardEntry->addr1Unconfirmed;
	}
	else
	{
		return;
	}

	// Forward to dest
	int len = 0;
	// 	sockaddr_in saOut;
	// 	saOut.sin_port = forwardTarget.GetPortNetworkOrder(); // User port
	// 	saOut.sin_addr.s_addr = forwardTarget.address.addr4.sin_addr.s_addr;
	// 	saOut.sin_family = AF_INET;
	if(forwardTarget.address.addr.sa_family == AF_INET)
	{
		do
		{
			len = sendto__(forwardEntry->socket, data, receivedDataLen, 0, &forwardTarget.address.addr, sizeof(sockaddr_in));
		} while(len == 0);
	}
	else
	{
		do
		{
			len = sendto__(forwardEntry->socket, data, receivedDataLen, 0, &forwardTarget.address.addr, sizeof(sockaddr_in6));
		} while(len == 0);
	}

	forwardEntry->timeLastDatagramForwarded = curTime;
#endif  // __native_client__
}
void UDPForwarder::UpdateUDPForwarder()
{
	/*
#if !defined(SN_TARGET_PSP2)
	timeval tv;
	tv.tv_sec=0;
	tv.tv_usec=0;
#endif
	*/

	SLNet::TimeMS curTime = SLNet::GetTimeMS();

	StartForwardingInputStruct *sfis;
	StartForwardingOutputStruct sfos;
	sfos.forwardingSocket = INVALID_SOCKET;
	sfos.forwardingPort = 0;
	sfos.inputId = 0;
	sfos.result = UDPFORWARDER_RESULT_COUNT;

	for(;;)
	{
		sfis = startForwardingInput.Pop();
		if(sfis == 0)
			break;

		if(GetUsedForwardEntries() > maxForwardEntries)
		{
			sfos.result = UDPFORWARDER_NO_SOCKETS;
		}
		else
		{
			sfos.result = UDPFORWARDER_RESULT_COUNT;

			for(unsigned int i = 0; i < forwardListNotUpdated.Size(); i++)
			{
				if((forwardListNotUpdated[i]->addr1Unconfirmed == sfis->source &&
					forwardListNotUpdated[i]->addr2Unconfirmed == sfis->destination)
				|| (forwardListNotUpdated[i]->addr1Unconfirmed == sfis->destination &&
					forwardListNotUpdated[i]->addr2Unconfirmed == sfis->source))
				{
					ForwardEntry *fe = forwardListNotUpdated[i];
					sfos.forwardingPort = SocketLayer::GetLocalPort(fe->socket);
					sfos.forwardingSocket = fe->socket;
					sfos.result = UDPFORWARDER_FORWARDING_ALREADY_EXISTS;
					break;
				}
			}

			if(sfos.result == UDPFORWARDER_RESULT_COUNT)
			{
				ForwardEntry *fe = SLNet::OP_NEW<UDPForwarder::ForwardEntry>(_FILE_AND_LINE_);
				fe->socket = INVALID_SOCKET;
				fe->addr1Unconfirmed = sfis->source;
				fe->addr2Unconfirmed = sfis->destination;
				fe->timeoutOnNoDataMS = sfis->timeoutOnNoDataMS;

				addrinfo *addrInfo;
				addrinfo hint = {}; hint.ai_flags = AI_PASSIVE;
				if(sfis->socketFamily == AF_UNSPEC)
					hint.ai_family = AF_INET6;
				else
					hint.ai_family = sfis->socketFamily;
				hint.ai_socktype = SOCK_DGRAM;

				int res;
				if(sfis->forceHostAddress.IsEmpty() || sfis->forceHostAddress == "UNASSIGNED_SYSTEM_ADDRESS")
					res = getaddrinfo(nullptr, "0", &hint, &addrInfo);
				else
					res = getaddrinfo(sfis->forceHostAddress.C_String(), "0", &hint, &addrInfo);
				if(res == 0)
				{
					for(addrinfo *ai = addrInfo; ai != nullptr; ai = ai->ai_next)
					{
						// Open socket. The address type depends on what getaddrinfo() gave us.
						fe->socket = socket__(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
						if(fe->socket != INVALID_SOCKET)
						{
							if(ai->ai_family == AF_INET6)
							{
								// Enable or disable dual stack mode
								int v6only = (sfis->socketFamily != AF_UNSPEC);
								setsockopt__(fe->socket, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&v6only, sizeof(v6only));
							}

							if(bind__(fe->socket, ai->ai_addr, ai->ai_addrlen) == 0)
							{
								break;
							}
							else
							{
								closesocket__(fe->socket);
								fe->socket = INVALID_SOCKET;
							}
						}
					}

					// Release address info after we have used it
					freeaddrinfo(addrInfo);
				}

				if(fe->socket == INVALID_SOCKET)
					sfos.result = UDPFORWARDER_BIND_FAILED;
				else
					sfos.result = UDPFORWARDER_SUCCESS;


				if(sfos.result == UDPFORWARDER_SUCCESS)
				{
					sfos.forwardingPort = SocketLayer::GetLocalPort(fe->socket);
					sfos.forwardingSocket = fe->socket;

					int sock_opt = 1024 * 256;
					setsockopt__(fe->socket, SOL_SOCKET, SO_RCVBUF, (char*)&sock_opt, sizeof(sock_opt));
					sock_opt = 0;
					setsockopt__(fe->socket, SOL_SOCKET, SO_LINGER, (char*)&sock_opt, sizeof(sock_opt));
#ifdef _WIN32
					unsigned long nonblocking = 1;
					ioctlsocket__(fe->socket, FIONBIO, &nonblocking);
#else
					fcntl(fe->socket, F_SETFL, O_NONBLOCK);
#endif

					forwardListNotUpdated.Insert(fe, _FILE_AND_LINE_);
				}
			}
		}

		// Push result
		sfos.inputId = sfis->inputId;
		startForwardingOutputMutex.Lock();
		startForwardingOutput.Push(sfos, _FILE_AND_LINE_);
		startForwardingOutputMutex.Unlock();

		startForwardingInput.Deallocate(sfis, _FILE_AND_LINE_);
	}

	StopForwardingStruct *sfs;

	for(;;)
	{
		sfs = stopForwardingCommands.Pop();
		if(sfs == 0)
			break;

		ForwardEntry *fe;
		for(unsigned int i = 0; i < forwardListNotUpdated.Size(); i++)
		{
			if(
				(forwardListNotUpdated[i]->addr1Unconfirmed == sfs->source &&
					forwardListNotUpdated[i]->addr2Unconfirmed == sfs->destination)
				||
				(forwardListNotUpdated[i]->addr1Unconfirmed == sfs->destination &&
					forwardListNotUpdated[i]->addr2Unconfirmed == sfs->source)
				)
			{
				fe = forwardListNotUpdated[i];
				forwardListNotUpdated.RemoveAtIndexFast(i);
				SLNet::OP_DELETE(fe, _FILE_AND_LINE_);
				break;
			}
		}

		stopForwardingCommands.Deallocate(sfs, _FILE_AND_LINE_);
	}

	unsigned int i = 0;
	while(i < forwardListNotUpdated.Size())
	{
		if(curTime > forwardListNotUpdated[i]->timeLastDatagramForwarded && // Account for timestamp wrap
			curTime > forwardListNotUpdated[i]->timeLastDatagramForwarded + forwardListNotUpdated[i]->timeoutOnNoDataMS)
		{
			SLNet::OP_DELETE(forwardListNotUpdated[i], _FILE_AND_LINE_);
			forwardListNotUpdated.RemoveAtIndex(i);
		}
		else
			i++;
	}

	ForwardEntry *forwardEntry;
	for(i = 0; i < forwardListNotUpdated.Size(); i++)
	{
		forwardEntry = forwardListNotUpdated[i];
		RecvFrom(curTime, forwardEntry);
	}
}

namespace SLNet
{
RAK_THREAD_DECLARATION(UpdateUDPForwarderGlobal)
{



	UDPForwarder * udpForwarder = (UDPForwarder *)arguments;


	udpForwarder->threadRunning = true;
	while(udpForwarder->isRunning)
	{
		udpForwarder->UpdateUDPForwarder();

		// 12/1/2010 Do not change from 0
		// See http://www.jenkinssoftware.com/forum/index.php?topic=4033.0;topicseen
		// Avoid 100% reported CPU usage
		if(udpForwarder->forwardListNotUpdated.Size() == 0)
			RakSleep(30);
		else
			RakSleep(0);
	}
	udpForwarder->threadRunning = false;




	return 0;


}

} // namespace SLNet

#endif // #if _RAKNET_SUPPORT_FileOperations==1
