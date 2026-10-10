#include "Crypto.h"
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <vector>
#include <cstdio>

// 用 CNG 计算 MD5 摘要（16 字节）
static bool tk_md5(const unsigned char* data, size_t length, unsigned char digest[16]) {
	BCRYPT_ALG_HANDLE hAlg = nullptr;
	BCRYPT_HASH_HANDLE hHash = nullptr;
	DWORD cbObject = 0, cbHash = 0, cbDummy = 0;
	bool ok = false;

	if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_MD5_ALGORITHM, nullptr, 0) != 0)
		return false;

	if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&cbObject), sizeof(cbObject), &cbDummy, 0) == 0 &&
		BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&cbHash), sizeof(cbHash), &cbDummy, 0) == 0 &&
		cbHash == 16) {
		std::vector<unsigned char> object(cbObject);
		if (BCryptCreateHash(hAlg, &hHash, object.data(), cbObject, nullptr, 0, 0) == 0) {
			if (BCryptHashData(hHash, const_cast<PUCHAR>(data), static_cast<ULONG>(length), 0) == 0 &&
				BCryptFinishHash(hHash, digest, 16, 0) == 0)
				ok = true;
			BCryptDestroyHash(hHash);
		}
	}

	BCryptCloseAlgorithmProvider(hAlg, 0);
	return ok;
}

// AES-128-ECB 逐块加解密（与原来 OpenSSL 低层 AES_encrypt/AES_decrypt 的行为一致：
// 不做任何填充，由调用方自己处理 PKCS#7；数据长度必须是 16 的整数倍）
static bool tk_aes_ecb(bool encrypt, const unsigned char* data, size_t length,
	const std::string& key, std::string& output) {
	BCRYPT_ALG_HANDLE hAlg = nullptr;
	BCRYPT_KEY_HANDLE hKey = nullptr;
	DWORD cbObject = 0, cbDummy = 0, cbDone = 0;
	bool ok = false;

	if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0)
		return false;

	if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_ECB,
		sizeof(BCRYPT_CHAIN_MODE_ECB), 0) == 0 &&
		BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&cbObject), sizeof(cbObject), &cbDummy, 0) == 0) {

		// 与 AES_set_encrypt_key(key, 128, ...) 一致：只取 key 的前 16 字节
		std::string raw_key = key.substr(0, 16);
		raw_key.append(16 - raw_key.size(), '\0');

		std::vector<unsigned char> object(cbObject);
		if (BCryptGenerateSymmetricKey(hAlg, &hKey, object.data(), cbObject,
			reinterpret_cast<PUCHAR>(const_cast<char*>(raw_key.data())), static_cast<ULONG>(raw_key.size()), 0) == 0) {

			output.assign(length, '\0');
			NTSTATUS status = encrypt
				? BCryptEncrypt(hKey, const_cast<PUCHAR>(data), static_cast<ULONG>(length), nullptr, nullptr, 0,
					reinterpret_cast<PUCHAR>(&output[0]), static_cast<ULONG>(length), &cbDone, 0)
				: BCryptDecrypt(hKey, const_cast<PUCHAR>(data), static_cast<ULONG>(length), nullptr, nullptr, 0,
					reinterpret_cast<PUCHAR>(&output[0]), static_cast<ULONG>(length), &cbDone, 0);
			if (status == 0) {
				output.resize(cbDone);
				ok = true;
			}
			BCryptDestroyKey(hKey);
		}
	}

	BCryptCloseAlgorithmProvider(hAlg, 0);
	return ok;
}

static const size_t AES_BLOCK_SIZE = 16;   // 原来是 openssl/aes.h 里的宏

std::string md5(const std::string& input) {
	unsigned char digest[16] = { 0 };
	tk_md5(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);

	char md5string[33] = { 0 };
	for (int i = 0; i < 16; i++)
		sprintf(&md5string[i * 2], "%02x", (unsigned int)digest[i]);

	return std::string(md5string);
}

std::string base64_encode(const unsigned char* input, size_t length) {
	DWORD cch = 0;
	if (!CryptBinaryToStringA(input, static_cast<DWORD>(length),
		CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &cch))
		return std::string();

	std::string buffer(cch, '\0');
	DWORD written = cch;
	if (!CryptBinaryToStringA(input, static_cast<DWORD>(length),
		CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &buffer[0], &written))
		return std::string();

	// 不同版本的 CryptBinaryToStringA 对「结尾的 '\0' 是否计入 written」不一致，
	// 这里按实际内容裁剪；否则会把 base64 结尾的 '=' 也一起裁掉，导致解不回来。
	if (written > 0 && written <= buffer.size() && buffer[written - 1] == '\0')
		--written;
	buffer.resize(written <= buffer.size() ? written : buffer.size());
	return buffer;
}

std::string base64_decode(const std::string& input) {
	DWORD cb = 0;
	if (!CryptStringToBinaryA(input.c_str(), static_cast<DWORD>(input.size()),
		CRYPT_STRING_BASE64, nullptr, &cb, nullptr, nullptr))
		return std::string();

	std::string buffer(cb, '\0');
	if (!CryptStringToBinaryA(input.c_str(), static_cast<DWORD>(input.size()),
		CRYPT_STRING_BASE64, reinterpret_cast<BYTE*>(&buffer[0]), &cb, nullptr, nullptr))
		return std::string();

	buffer.resize(cb);
	return buffer;
}

std::string aes_encrypt(const std::string& plaintext, const std::string& key, const std::string& iv) {
	(void)iv;   // 与原来一致：ECB 模式不使用 IV

	int padding = AES_BLOCK_SIZE - (plaintext.length() % AES_BLOCK_SIZE);
	std::string padded_plaintext = plaintext + std::string(padding, static_cast<char>(padding));

	std::string ciphertext;
	if (!tk_aes_ecb(true, reinterpret_cast<const unsigned char*>(padded_plaintext.data()),
		padded_plaintext.length(), key, ciphertext))
		return std::string();

	return base64_encode(reinterpret_cast<const unsigned char*>(ciphertext.data()), ciphertext.length());
}

std::string aes_decrypt(const std::string& ciphertext, const std::string& key, const std::string& iv) {
	(void)iv;

	std::string decoded_ciphertext = base64_decode(ciphertext);
	if (decoded_ciphertext.empty() || decoded_ciphertext.length() % AES_BLOCK_SIZE != 0)
		return std::string();

	std::string plaintext;
	if (!tk_aes_ecb(false, reinterpret_cast<const unsigned char*>(decoded_ciphertext.data()),
		decoded_ciphertext.length(), key, plaintext))
		return std::string();

	size_t padding = static_cast<size_t>(static_cast<unsigned char>(plaintext[plaintext.length() - 1]));
	if (padding == 0 || padding > AES_BLOCK_SIZE || padding > plaintext.length())
		return plaintext;
	return plaintext.substr(0, plaintext.length() - padding);
}