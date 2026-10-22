/*
*  Copyright (c) 2019-2020, SLikeSoft UG (haftungsbeschränkt)
*
*  This source code is  licensed under the MIT-style license found in the license.txt
*  file in the root directory of this source tree.
*/
#include <slikenet/crypto/cryptomanager.h>

#include <slikenet/assert.h> // used for RakAssert
#include <limits>            // used for std::numeric_limits<>

// prevent max/min macros getting defined (breaking numeric_limits<>::max() / ::min() usage) through the indirect windows.h include in the OpenSSL includes
#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#include <openssl/evp.h>     // used for EVP_xxxx
#include <openssl/rand.h>    // used for RAND_xxxx

using namespace SLNet::Experimental::Crypto;

static const EVP_CIPHER *GetCipher()
{
	// #high - review usage of the CBC mode here --- not the best nowadays
	return EVP_aes_256_cbc();
}

bool CCryptoManager::Initialize()
{
	if(m_Initialized)
	{
		return true; // already initialized
	}

	if(RAND_bytes(m_sessionKey, sizeof(m_sessionKey)) <= 0)
	{
		return false; // failed to initialize the random session key
	}
	if(RAND_bytes(m_initializationVector, sizeof(m_initializationVector)) <= 0)
	{
		return false; // failed to initialize the initialization vector
	}
	if((m_decryptionContext = EVP_CIPHER_CTX_new()) == nullptr)
	{
		return false;
	}
	if((m_encryptionContext = EVP_CIPHER_CTX_new()) == nullptr)
	{
		EVP_CIPHER_CTX_free(m_decryptionContext);
		m_decryptionContext = nullptr;
		return false;
	}

	m_Initialized = true;
	return true;
}

void CCryptoManager::Terminate()
{
	if(!m_Initialized)
		return; // not initialized yet

	EVP_CIPHER_CTX_free(m_encryptionContext);
	m_encryptionContext = nullptr;

	EVP_CIPHER_CTX_free(m_decryptionContext);
	m_decryptionContext = nullptr;

	SecureClearMemory(m_sessionKey, sizeof(m_sessionKey));
	SecureClearMemory(m_initializationVector, sizeof(m_initializationVector));

	m_Initialized = false;
}

bool CCryptoManager::EncryptSessionData(const unsigned char* plaintext, size_t dataLength, unsigned char* outBuffer, size_t& inOutBufferSize)
{
	if(!Initialize())
	{
		return false; // CryptoManager failed to initialize
	}
	size_t requiredBufferSize = dataLength;
	if(!GetRequiredEncryptionBufferSize(requiredBufferSize))
	{
		return false; // dataLength too large (integer overflow)
	}
	if(inOutBufferSize < requiredBufferSize)
	{
		return false; // out buffer (potentially) too small
	}

	// #med - add engine support to use HW-acceleration
	if(EVP_EncryptInit_ex(m_encryptionContext, GetCipher(), nullptr, m_sessionKey, m_initializationVector) == 0)
	{
		return false; // failed to initialize the encryption context
	}

	int bytesWritten1;
	// note: static_cast<> safe here, since GetRequiredEncryptionBufferSize()-check ensured dataLength is <= INT_MAX
	if(EVP_EncryptUpdate(m_encryptionContext, outBuffer, &bytesWritten1, plaintext, static_cast<int>(dataLength)) == 0)
	{
		return false; // encryption failed
	}
	RakAssert(static_cast<size_t>(bytesWritten1) <= inOutBufferSize);

	int bytesWritten2;
	if(EVP_EncryptFinal_ex(m_encryptionContext, outBuffer + bytesWritten1, &bytesWritten2) == 0)
	{
		return false; // failed final encryption step
	}

	RakAssert(static_cast<size_t>(bytesWritten1 + bytesWritten2) <= inOutBufferSize);
	inOutBufferSize = bytesWritten1 + bytesWritten2;
	return true;
}

bool CCryptoManager::DecryptSessionData(const unsigned char* encryptedtext, size_t dataLength, unsigned char* outBuffer, size_t& inOutBufferSize)
{
	if(!Initialize())
	{
		return false; // CryptoManager failed to initialize
	}
	// #med - extend support for inOutBufferSize > int::max()
	if(inOutBufferSize > static_cast<size_t>(std::numeric_limits<int>::max()))
	{
		// note: We check the inOutBufferSize here rather than the dataLength due to the indirect size limitation due to the EVP_DecryptUpdate()/EVP_DecryptFinal_ex() calls
		//       being limited to int::max() through their returned written bytes values. Due to the next check (inOutBufferSize < dataLength) it's implicitly ensured that
		//       dataLength doesn't exceed the limit either.
		return false; // specified length exceeds max supported size
	}
	if(inOutBufferSize < dataLength)
	{
		// Prevent potential buffer overflow. The encrypted data may contain padding and therefore
		// the actual plaintext can be smaller, but we cannot determine that before decryption
		return false;
	}
	// #high - review usage of the CBC mode here --- not the best nowadays
	// #med - add engine support to use HW-acceleration
	if(EVP_DecryptInit_ex(m_decryptionContext, GetCipher(), nullptr, m_sessionKey, m_initializationVector) == 0)
	{
		return false; // failed to initialize the decryption context
	}
	int bytesWritten1;

	// static cast safe due to size-check above
	if(EVP_DecryptUpdate(m_decryptionContext, outBuffer, &bytesWritten1, encryptedtext, static_cast<int>(dataLength)) == 0)
	{
		return false; // decryption failed
	}
	RakAssert(static_cast<size_t>(bytesWritten1) <= inOutBufferSize);

	int bytesWritten2;
	if(EVP_DecryptFinal_ex(m_decryptionContext, outBuffer + bytesWritten1, &bytesWritten2) == 0)
	{
		return false; // failed final decryption step
	}

	RakAssert(static_cast<size_t>(bytesWritten1 + bytesWritten2) <= inOutBufferSize);
	inOutBufferSize = bytesWritten1 + bytesWritten2;
	return true;
}

bool CCryptoManager::GetRequiredEncryptionBufferSize(size_t& encryptionDataByteLength)
{
	// note: not using EVP_CIPHER_CTX_block_size() here, since otherwise we would have to make sure that the encryption context was initialized already
	const int blockSize = EVP_CIPHER_block_size(GetCipher());

	// EVP_EncryptUpdate() can write up to dataLength + blockSize - 1.
	// EVP_EncryptFinal_ex() can write up to blockSize.
	// Hence the required encryption buffer size is dataLength + blockSize * 2 - 1
	// Practically AES-256-CBC with PKCS padding needs at most
	// dataLength + blockSize, but we keep the more conservative bound.

	if(encryptionDataByteLength + blockSize * 2 - 1 < encryptionDataByteLength)
	{
		return false; // prevent integer overflow
	}

	encryptionDataByteLength = encryptionDataByteLength + blockSize * 2 - 1;
	// EVP_EncryptUpdate()/EVP_EncryptFinal_ex() use int lengths, therefore the supported size is limited to INT_MAX
	return encryptionDataByteLength <= static_cast<size_t>(std::numeric_limits<int>::max());
}

void* CCryptoManager::AllocateSecureMemory(size_t size)
{
	// #high - route through memory manager (aka: same as OP_NEW_ARRAY)
	return OPENSSL_malloc(size);
}

void CCryptoManager::FreeSecureMemory(void* pointer, size_t size)
{
	// make sure the memory is cleared before it's freed again
	SecureClearMemory(pointer, size);

	// #high - route through memory manager (aka: same as OP_NEW_ARRAY)
	OPENSSL_free(pointer);
}

void CCryptoManager::SecureClearMemory(void* pointer, size_t size)
{
	OPENSSL_cleanse(pointer, size);
}
