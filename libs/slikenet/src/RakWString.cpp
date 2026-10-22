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

#include <utility>
#include <slikenet/wstring.h>
#include <slikenet/BitStream.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include <slikenet/crtstring_adapter.h>

using namespace SLNet;

// From http://www.joelonsoftware.com/articles/Unicode.html
// Only code points 128 and above are stored using 2, 3, in fact, up to 6 bytes.
#define MAX_BYTES_PER_UNICODE_CHAR sizeof(wchar_t)

RakWString::RakWString(RakWString&& right) noexcept
	: data(std::exchange(right.data, nullptr)),
	length(std::exchange(right.length, 0))
{
}

RakWString::~RakWString()
{
	rakFree_Ex(data, _FILE_AND_LINE_);
}
RakWString& RakWString::operator=(const RakWString& right)
{
	Clear();
	if(right.IsEmpty())
		return *this;
	data = (wchar_t *)rakMalloc_Ex((right.GetLength() + 1) * MAX_BYTES_PER_UNICODE_CHAR, _FILE_AND_LINE_);
	if(!data)
	{
		length = 0;
		notifyOutOfMemory(_FILE_AND_LINE_);
		return *this;
	}
	length = right.GetLength();
	memcpy(data, right.C_String(), (right.GetLength() + 1) * MAX_BYTES_PER_UNICODE_CHAR);

	return *this;
}
RakWString& RakWString::operator=(RakWString&& right) noexcept
{
	if(this != &right)
	{
		Clear();
		data = std::exchange(right.data, nullptr);
		length = std::exchange(right.length, 0);
	}
	return *this;
}
RakWString& RakWString::operator = (const RakString& right)
{
	return *this = right.C_String();
}
RakWString& RakWString::operator = (const wchar_t * const str)
{
	Clear();
	if(str == 0)
		return *this;
	length = wcslen(str);
	if(length == 0)
		return *this;
	data = (wchar_t *)rakMalloc_Ex((length + 1) * MAX_BYTES_PER_UNICODE_CHAR, _FILE_AND_LINE_);
	if(!data)
	{
		length = 0;
		notifyOutOfMemory(_FILE_AND_LINE_);
		return *this;
	}
	wcscpy_s(data, length + 1, str);

	return *this;
}

RakWString& RakWString::operator = (const char * const str)
{
	Clear();

	// Not supported on android
#if !defined(ANDROID)
	if(str == 0)
		return *this;
	if(str[0] == 0)
		return *this;

	mbstowcs_s(&length, nullptr, 0, str, 0);
	data = (wchar_t *)rakMalloc_Ex((length + 1) * MAX_BYTES_PER_UNICODE_CHAR, _FILE_AND_LINE_);
	if(!data)
	{
		length = 0;
		notifyOutOfMemory(_FILE_AND_LINE_);
		return *this;
	}

	mbstowcs_s(&length, data, length + 1, str, length);
	if(length == (size_t)(-1))
	{
		RAKNET_DEBUG_PRINTF("Couldn't convert string--invalid multibyte character.\n");
		Clear();
		return *this;
	}
#else
	// mbstowcs not supported on android
	RakAssert("mbstowcs not supported on Android" && 0);
#endif // defined(ANDROID)

	return *this;
}

RakWString& RakWString::operator +=(const RakWString& right)
{
	if(right.IsEmpty())
		return *this;
	size_t newCharLength = length + right.GetLength();
	wchar_t *newCStr;
	bool isEmpty = IsEmpty();
	if(isEmpty)
		newCStr = (wchar_t *)rakMalloc_Ex((newCharLength + 1) * MAX_BYTES_PER_UNICODE_CHAR, _FILE_AND_LINE_);
	else
		newCStr = (wchar_t *)rakRealloc_Ex(data, (newCharLength + 1) * MAX_BYTES_PER_UNICODE_CHAR, _FILE_AND_LINE_);
	if(!newCStr)
	{
		notifyOutOfMemory(_FILE_AND_LINE_);
		return *this;
	}
	data = newCStr;
	length = newCharLength;
	if(isEmpty)
	{
		memcpy(newCStr, right.C_String(), (right.GetLength() + 1) * MAX_BYTES_PER_UNICODE_CHAR);
	}
	else
	{
		wcscat_s(data, newCharLength + 1, right.C_String());
	}

	return *this;
}
RakWString& RakWString::operator += (const wchar_t * const right)
{
	if(right == 0)
		return *this;
	size_t rightLength = wcslen(right);
	size_t newCharLength = length + rightLength;
	wchar_t *newCStr;
	bool isEmpty = IsEmpty();
	if(isEmpty)
		newCStr = (wchar_t *)rakMalloc_Ex((newCharLength + 1) * MAX_BYTES_PER_UNICODE_CHAR, _FILE_AND_LINE_);
	else
		newCStr = (wchar_t *)rakRealloc_Ex(data, (newCharLength + 1) * MAX_BYTES_PER_UNICODE_CHAR, _FILE_AND_LINE_);
	if(!newCStr)
	{
		notifyOutOfMemory(_FILE_AND_LINE_);
		return *this;
	}
	data = newCStr;
	length = newCharLength;
	if(isEmpty)
	{
		memcpy(newCStr, right, (rightLength + 1) * MAX_BYTES_PER_UNICODE_CHAR);
	}
	else
	{
		wcscat_s(data, newCharLength + 1, right);
	}

	return *this;
}
bool RakWString::operator==(const RakWString &right) const
{
	if(GetLength() != right.GetLength())
		return false;
	return wcscmp(C_String(), right.C_String()) == 0;
}
bool RakWString::operator < (const RakWString& right) const
{
	return wcscmp(C_String(), right.C_String()) < 0;
}
bool RakWString::operator <= (const RakWString& right) const
{
	return wcscmp(C_String(), right.C_String()) <= 0;
}
bool RakWString::operator > (const RakWString& right) const
{
	return wcscmp(C_String(), right.C_String()) > 0;
}
bool RakWString::operator >= (const RakWString& right) const
{
	return wcscmp(C_String(), right.C_String()) >= 0;
}
bool RakWString::operator!=(const RakWString &right) const
{
	if(GetLength() != right.GetLength())
		return true;
	return wcscmp(C_String(), right.C_String()) != 0;
}

unsigned long RakWString::ToInteger(const RakWString &rs)
{
	unsigned long hash = 0;
	int c;

	const char *str = (const char *)rs.C_String();
	size_t i;
	for(i = 0; i < rs.GetLength() * MAX_BYTES_PER_UNICODE_CHAR * sizeof(wchar_t); i++)
	{
		c = *str++;
		hash = c + (hash << 6) + (hash << 16) - hash;
	}

	return hash;
}
int RakWString::StrCmp(const RakWString &right) const
{
	return wcscmp(C_String(), right.C_String());
}
int RakWString::StrICmp(const RakWString &right) const
{
#ifdef _WIN32
	return _wcsicmp(C_String(), right.C_String());
#else
	// Not supported
	return wcscmp(C_String(), right.C_String());
#endif
}
void RakWString::Clear()
{
	rakFree_Ex(data, _FILE_AND_LINE_);
	data = nullptr;
	length = 0;
}
void RakWString::Printf(void)
{
	printf("%ls", C_String());
}
void RakWString::FPrintf(FILE *fp)
{
	fprintf(fp, "%ls", C_String());
}
void RakWString::Serialize(BitStream *bs) const
{
	Serialize(C_String(), bs);
}
void RakWString::Serialize(const wchar_t * const str, BitStream *bs)
{
#if 0
	char *multiByteBuffer;
	size_t allocated = wcslen(str) * MAX_BYTES_PER_UNICODE_CHAR;
	multiByteBuffer = (char*)rakMalloc_Ex(allocated, _FILE_AND_LINE_);
	size_t used = wcstombs(multiByteBuffer, str, allocated);
	bs->WriteCasted<unsigned short>(used);
	bs->WriteAlignedBytes((const unsigned char*)multiByteBuffer, (const unsigned int)used);
	rakFree_Ex(multiByteBuffer, _FILE_AND_LINE_);
#else
	size_t mbByteLength = wcslen(str);
	bs->WriteCasted<unsigned short>(mbByteLength);
	for(unsigned int i = 0; i < mbByteLength; i++)
	{
		uint16_t t;
		t = (uint16_t)str[i];
		// Force endian swapping, and write to 16 bits
		bs->Write(t);
	}
#endif
}
bool RakWString::Deserialize(BitStream *bs)
{
	Clear();

	size_t mbByteLength;
	bs->ReadCasted<unsigned short>(mbByteLength);
	if(mbByteLength > 0)
	{
#if 0
		char *multiByteBuffer;
		multiByteBuffer = (char*)rakMalloc_Ex(mbByteLength + 1, _FILE_AND_LINE_);
		bool result = bs->ReadAlignedBytes((unsigned char*)multiByteBuffer, (const unsigned int)mbByteLength);
		if(result == false)
		{
			rakFree_Ex(multiByteBuffer, _FILE_AND_LINE_);
			return false;
		}
		multiByteBuffer[mbByteLength] = 0;
		data = (wchar_t *)rakMalloc_Ex((mbByteLength + 1) * MAX_BYTES_PER_UNICODE_CHAR, _FILE_AND_LINE_);
		mbstowcs_s(&length, data, mbByteLength + 1, multiByteBuffer, mbByteLength);
		rakFree_Ex(multiByteBuffer, _FILE_AND_LINE_);
		data[length] = 0;
#else
		data = (wchar_t*)rakMalloc_Ex((mbByteLength + 1) * MAX_BYTES_PER_UNICODE_CHAR, _FILE_AND_LINE_);
		length = mbByteLength;
		for(unsigned int i = 0; i < mbByteLength; i++)
		{
			uint16_t t;
			// Force endian swapping, and read 16 bits
			bs->Read(t);
			data[i] = t;
		}
		data[mbByteLength] = 0;
#endif
		return true;
	}
	else
	{
		return true;
	}
}
bool RakWString::Deserialize(wchar_t *str, BitStream *bs)
{
	size_t mbByteLength;
	bs->ReadCasted<unsigned short>(mbByteLength);
	if(mbByteLength > 0)
	{
#if 0
		char *multiByteBuffer;
		multiByteBuffer = (char*)rakMalloc_Ex(mbByteLength + 1, _FILE_AND_LINE_);
		bool result = bs->ReadAlignedBytes((unsigned char*)multiByteBuffer, (const unsigned int)mbByteLength);
		if(result == false)
		{
			rakFree_Ex(multiByteBuffer, _FILE_AND_LINE_);
			return false;
		}
		multiByteBuffer[mbByteLength] = 0;
		size_t length;
		mbstowcs(&length, str, multiByteBuffer, mbByteLength);
		rakFree_Ex(multiByteBuffer, _FILE_AND_LINE_);
		str[length] = 0;
#else
		for(unsigned int i = 0; i < mbByteLength; i++)
		{
			uint16_t t;
			// Force endian swapping, and read 16 bits
			bs->Read(t);
			str[i] = t;
		}
		str[mbByteLength] = 0;
#endif
		return true;
	}
	else
	{
#pragma warning(push)
#pragma warning(disable:4996)
		wcscpy(str, L"");
#pragma warning(pop)
	}
	return true;
}
bool RakWString::Deserialize(wchar_t *str, size_t strLength, BitStream *bs)
{
	size_t mbByteLength;
	bs->ReadCasted<unsigned short>(mbByteLength);
	if(mbByteLength > 0)
	{
#if 0
		char *multiByteBuffer;
		multiByteBuffer = (char*)rakMalloc_Ex(mbByteLength + 1, _FILE_AND_LINE_);
		bool result = bs->ReadAlignedBytes((unsigned char*)multiByteBuffer, (const unsigned int)mbByteLength);
		if(result == false)
		{
			rakFree_Ex(multiByteBuffer, _FILE_AND_LINE_);
			return false;
		}
		multiByteBuffer[mbByteLength] = 0;
		size_t length;
		mbstowcs_s(&length, str, strLength, multiByteBuffer, mbByteLength);
		rakFree_Ex(multiByteBuffer, _FILE_AND_LINE_);
		str[length] = 0;
#else
		for(unsigned int i = 0; i < mbByteLength; i++)
		{
			uint16_t t;
			// Force endian swapping, and read 16 bits
			bs->Read(t);
			str[i] = t;
		}
		str[mbByteLength] = 0;
#endif
		return true;
	}
	else
	{
		wcscpy_s(str, strLength, L"");
	}
	return true;
}

const SLNet::RakWString operator+(const SLNet::RakWString &lhs, const SLNet::RakWString &rhs)
{
	SLNet::RakWString returnvalue(lhs);
	returnvalue += rhs;
	return returnvalue;
}

/*
SLNet::BitStream bsTest;
SLNet::RakWString testString("cat"), testString2;
testString = "Hllo";
testString = L"Hello";
testString += L" world";
testString2 += testString2;
SLNet::RakWString ts3(L" from here");
testString2+=ts3;
SLNet::RakWString ts4(L" 222");
testString2=ts4;
SLNet::RakString rs("rakstring");
testString2+=rs;
testString2=rs;
bsTest.Write(L"one");
bsTest.Write(testString2);
bsTest.SetReadOffset(0);
SLNet::RakWString ts5, ts6;
wchar_t buff[99];
wchar_t *wptr = (wchar_t*)buff;
bsTest.Read(wptr);
bsTest.Read(ts5);
*/
