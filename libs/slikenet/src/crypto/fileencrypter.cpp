/*
 *  Copyright (c) 2018-2020, SLikeSoft UG (haftungsbeschränkt)
 *
 *  This source code is  licensed under the MIT-style license found in the license.txt
 *  file in the root directory of this source tree.
 */
#include <slikenet/crypto/fileencrypter.h>

#include <cstring> // used for strlen

#include <openssl/err.h> // used for ERR_xxxx
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/decoder.h> // used for OSSL_DECODER_CTX_xxxx
#else
#include <openssl/pem.h> // used for PEM_read_bio_RSAPrivateKey, PEM_read_bio_RSA_PUBKEY, BIO_xxx
#include <openssl/rsa.h> // used for RSA_xxxx
#endif

#include <slikenet/crypto/cryptomanager.h> // used for SLNet::Experimental::Crypto::CCryptoManager

namespace SLNet::Experimental::Crypto
{
CFileEncrypter::CFileEncrypter()
{
	CCryptoManager::Initialize();
}

CFileEncrypter::CFileEncrypter(const char *publicKey, size_t publicKeyLength)
{
	CCryptoManager::Initialize();

	// #high - error / exception handling
	(void)SetPublicKey(publicKey, publicKeyLength);
}

CFileEncrypter::CFileEncrypter(const char *publicKey, size_t publicKeyLength, const char *privateKey, size_t privateKeyLength, CSecureString &password)
{
	CCryptoManager::Initialize();

	// #high - error / exception handling
	(void)SetPrivateKey(privateKey, privateKeyLength, password);
	(void)SetPublicKey(publicKey, publicKeyLength);
}

CFileEncrypter::~CFileEncrypter()
{
	if(m_publicKey != nullptr)
	{
		EVP_PKEY_free(m_publicKey);
	}

	if(m_privateKey != nullptr)
	{
		EVP_PKEY_free(m_privateKey);
	}
}

const unsigned char *CFileEncrypter::SignData(const unsigned char *data, const size_t dataLength)
{
	if(m_privateKey == nullptr)
	{
		// #high - error/exception handling
		return nullptr;
	}

	EVP_MD_CTX *const rsaSigningContext = EVP_MD_CTX_new();
	// #med - double check this - it's not documented whether EVP_MD_CTX_new() can actually fail (and return nullptr)
	if(rsaSigningContext == nullptr)
	{
		// #high - error/exception handling
		return nullptr;
	}

	if(EVP_SignInit_ex(rsaSigningContext, EVP_sha512(), nullptr) == 0)
	{
		// #high - error/exception handling
		EVP_MD_CTX_free(rsaSigningContext);
		return nullptr;
	}

	if(EVP_SignUpdate(rsaSigningContext, data, dataLength) == 0)
	{
		// #high - error/exception handling
		EVP_MD_CTX_free(rsaSigningContext);
		return nullptr;
	}

	// #med - use ArraySize<>()?
	unsigned int bufferSize = 1024;
	// #high - verify returned bufferSize...
	const bool success = (EVP_SignFinal(rsaSigningContext, m_sigBuffer, &bufferSize, m_privateKey) != 0);
	EVP_MD_CTX_free(rsaSigningContext);
	return success ? m_sigBuffer : nullptr;
}

const char *CFileEncrypter::SignDataBase64(const unsigned char *data, const size_t dataLength)
{
	const unsigned char* signature = SignData(data, dataLength);
	if(signature == nullptr)
	{
		// #high - error reporting
		return nullptr;
	}

	// #high - reinterpret_cast
	// 1024 binary -> 1368 base64-encoded (excluding the written null-terminator)
	if(EVP_EncodeBlock(reinterpret_cast<unsigned char*>(m_sigBufferBase64), signature, 1024) != 1368)
	{
		// #high - error reporting
		return nullptr;
	}
	return m_sigBufferBase64;
}

// #med - consider dropping signatureLength and replace it with a fixed size unsigned char array
//        since the signature must be 1024 chars long
bool CFileEncrypter::VerifyData(const unsigned char *data, const size_t dataLength, const unsigned char *signature, const size_t signatureLength)
{
	if(m_publicKey == nullptr)
	{
		// #high - error/exception handling
		return false;
	}

	EVP_MD_CTX *const rsaVerifyContext = EVP_MD_CTX_new();
	// #med - double check this - it's not documented whether EVP_MD_CTX_new() can actually fail (and return nullptr)
	if(rsaVerifyContext == nullptr)
	{
		// #high - error/exception handling
		return false;
	}

	// missing EVP_PKEY_free() call --- actually this will also destroy the RSA_KEY!!!!
	if(EVP_DigestVerifyInit(rsaVerifyContext, nullptr, EVP_sha512(), nullptr, m_publicKey) <= 0)
	{
		// #high - error/exception handling
		EVP_MD_CTX_free(rsaVerifyContext);
		return false;
	}

	if(EVP_DigestVerifyUpdate(rsaVerifyContext, data, dataLength) <= 0)
	{
		// #high - error/exception handling
		EVP_MD_CTX_free(rsaVerifyContext);
		return false;
	}

	// #high - review code simplifications
	const bool success = (EVP_DigestVerifyFinal(rsaVerifyContext, signature, signatureLength) == 1);
	EVP_MD_CTX_free(rsaVerifyContext);
	// #high - add error logging/reporting on failure
	return success;
}

bool CFileEncrypter::VerifyDataBase64(const unsigned char *data, const size_t dataLength, const char *signature, const size_t signatureLength)
{
	if(signatureLength != 1368)
	{
		// #high error reporting
		return false; // signature has an incorrect size (1024 binary -> 1368 Base64)
	}

	// #high - casts... / signatureLength should be 1026 (1024 plus 2 padding bytes)
	if(EVP_DecodeBlock(m_sigBuffer, reinterpret_cast<const unsigned char*>(signature), static_cast<int>(signatureLength)) != 1026)
	{
		// #high error reporting
		return false;
	}

	// 1026 minus two padding bytes -> 1024
	return VerifyData(data, dataLength, m_sigBuffer, 1024);
}

const char *CFileEncrypter::SetPrivateKey(const char *privateKey, size_t privateKeyLength, CSecureString &password)
{
	if(m_privateKey != nullptr)
	{
		EVP_PKEY_free(m_privateKey);
		m_privateKey = nullptr;
	}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	OSSL_DECODER_CTX *dctx = OSSL_DECODER_CTX_new_for_pkey(&m_privateKey, "PEM", nullptr, "RSA", OSSL_KEYMGMT_SELECT_PRIVATE_KEY, nullptr, nullptr);
	if(dctx == nullptr)
	{
		return ERR_error_string(ERR_get_error(), nullptr);
	}
	if(OSSL_DECODER_CTX_get_num_decoders(dctx) == 0) // no suitable decoders found
	{
		const char *error = ERR_error_string(ERR_get_error(), nullptr);
		OSSL_DECODER_CTX_free(dctx);
		EVP_PKEY_free(m_privateKey);
		m_privateKey = nullptr;
		return error;
	}

	const char *decryptedPassword = password.Decrypt();
	if(OSSL_DECODER_CTX_set_passphrase(dctx, reinterpret_cast<const unsigned char*>(decryptedPassword), strlen(decryptedPassword)) == 0)
	{
		const char *error = ERR_error_string(ERR_get_error(), nullptr);
		password.FlushUnencryptedData();
		OSSL_DECODER_CTX_free(dctx);
		EVP_PKEY_free(m_privateKey);
		m_privateKey = nullptr;
		return error;
	}
	password.FlushUnencryptedData();
	const unsigned char *data = reinterpret_cast<const unsigned char*>(privateKey);
	size_t length = privateKeyLength;
	if(OSSL_DECODER_from_data(dctx, &data, &length) == 0) // failed to load the private key
	{
		const char *error = ERR_error_string(ERR_get_error(), nullptr);
		OSSL_DECODER_CTX_free(dctx);
		EVP_PKEY_free(m_privateKey);
		m_privateKey = nullptr;
		return error;
	}
	OSSL_DECODER_CTX_free(dctx);
	return "";

#else // OPENSSL_VERSION_NUMBER < 0x30000000L
	// #med - review interface handling
	// #high - size_t -> int cast...
	BIO *const keyBIO = BIO_new_mem_buf(privateKey, static_cast<int>(privateKeyLength));

	const char *decryptedPassword = password.Decrypt();
	// #high - error/exception handling
	RSA *rsaKey = PEM_read_bio_RSAPrivateKey(keyBIO, nullptr, nullptr, (void*)decryptedPassword);
	if(rsaKey == nullptr) // failed to load the private key
	{
		const char *error = ERR_error_string(ERR_get_error(), nullptr);
		password.FlushUnencryptedData();
		BIO_free(keyBIO);
		return error;
	}
	password.FlushUnencryptedData();
	// #high - add error check/handling if BIO_free() fails
	BIO_free(keyBIO);

	// create the corresponding EVP_PKEY
	m_privateKey = EVP_PKEY_new();
	if(m_privateKey == nullptr) // failed to allocate the PKEY struct
	{
		// #high - error/exception handling
		const char *error = ERR_error_string(ERR_get_error(), nullptr);
		RSA_free(rsaKey);
		return error;
	}

	// #med - review interface handling
	if(EVP_PKEY_assign_RSA(m_privateKey, rsaKey) == 0)
	{
		// #high - error/exception handling
		const char *error = ERR_error_string(ERR_get_error(), nullptr);
		// #high - review the error handling here --- if EVP_PKEY_assign_RSA() succeeds, freeing the pkey will implicitly also free the public key
		//         but what is the behavior upon a failure in that case?
		//         for now better accept a potential resource/mem leak rather than provoking a crash
		EVP_PKEY_free(m_privateKey);
		m_privateKey = nullptr;
		return error;
	}
	return "";
#endif
}

const char *CFileEncrypter::SetPublicKey(const char* publicKey, size_t publicKeyLength)
{
	if(m_publicKey != nullptr)
	{
		EVP_PKEY_free(m_publicKey);
		m_publicKey = nullptr;
	}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	OSSL_DECODER_CTX *dctx = OSSL_DECODER_CTX_new_for_pkey(&m_publicKey, "PEM", nullptr, "RSA", OSSL_KEYMGMT_SELECT_PUBLIC_KEY, nullptr, nullptr);
	if(dctx == nullptr)
	{
		return ERR_error_string(ERR_get_error(), nullptr);
	}
	if(OSSL_DECODER_CTX_get_num_decoders(dctx) == 0) // no suitable decoders found
	{
		const char *error = ERR_error_string(ERR_get_error(), nullptr);
		OSSL_DECODER_CTX_free(dctx);
		EVP_PKEY_free(m_publicKey);
		m_publicKey = nullptr;
		return error;
	}

	const unsigned char *data = reinterpret_cast<const unsigned char*>(publicKey);
	size_t length = publicKeyLength;
	if(OSSL_DECODER_from_data(dctx, &data, &length) == 0) // failed to load the public key
	{
		const char *error = ERR_error_string(ERR_get_error(), nullptr);
		OSSL_DECODER_CTX_free(dctx);
		EVP_PKEY_free(m_publicKey);
		m_publicKey = nullptr;
		return error;
	}
	OSSL_DECODER_CTX_free(dctx);
	return "";

#else // OPENSSL_VERSION_NUMBER < 0x30000000L
	// #med - review interface handling
	// #high - size_t -> int cast...
	BIO *const keyBIO = BIO_new_mem_buf(publicKey, static_cast<int>(publicKeyLength));

	// #high - error/exception handling
	// #high - review &m_publicKey
	RSA *rsaKey = PEM_read_bio_RSA_PUBKEY(keyBIO, nullptr, nullptr, nullptr);
	if(rsaKey == nullptr) // failed to load the public key
	{
		const char *error = ERR_error_string(ERR_get_error(), nullptr);
		// #high - add error check/handling if BIO_free() fails
		BIO_free(keyBIO);
		return error;
	}
	// #high - add error check/handling if BIO_free() fails
	BIO_free(keyBIO);

	// create the corresponding EVP_PKEY
	m_publicKey = EVP_PKEY_new();
	if(m_publicKey == nullptr) // failed to allocate the PKEY struct
	{
		// #high - error/exception handling
		const char *error = ERR_error_string(ERR_get_error(), nullptr);
		RSA_free(rsaKey);
		return error;
	}

	// #med - review interface handling
	if(EVP_PKEY_assign_RSA(m_publicKey, rsaKey) == 0)
	{
		// #high - error/exception handling
		const char *error = ERR_error_string(ERR_get_error(), nullptr);
		// #high - review the error handling here --- if EVP_PKEY_assign_RSA() succeeds, freeing the pkey will implicitly also free the public key
		//         but what is the behavior upon a failure in that case?
		//         for now better accept a potential resource/mem leak rather than provoking a crash
		EVP_PKEY_free(m_publicKey);
		m_publicKey = nullptr;
		return error;
	}
	return "";
#endif
}
}
