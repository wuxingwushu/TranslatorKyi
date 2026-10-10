#pragma once
#include <string>

// ---------------------------------------------------------------------------
// 这里原来用的是 OpenSSL，现在改成 Windows 自带的 CNG(BCrypt) / Crypt32 实现，
// 工程因此不再依赖 OpenSSL。用到的算法与原来一一对应：
//   MD5               -> BCrypt(BCRYPT_MD5_ALGORITHM)
//   base64 编解码      -> CryptBinaryToStringA / CryptStringToBinaryA
//   AES-128-ECB 加解密 -> BCrypt(BCRYPT_AES_ALGORITHM, ECB 链模式)
// 需要链接 bcrypt.lib 与 crypt32.lib（已在根 CMakeLists.txt 里加好）。
// ---------------------------------------------------------------------------

// MD5 摘要的十六进制小写字符串（32 字符）
std::string md5(const std::string& input);

// base64 编解码（无换行）
std::string base64_encode(const unsigned char* input, size_t length);
std::string base64_decode(const std::string& input);

// AES-128-ECB 加解密（PKCS#7 填充；iv 参数保留以兼容原调用签名，ECB 不使用）
std::string aes_encrypt(const std::string& plaintext, const std::string& key, const std::string& iv);
std::string aes_decrypt(const std::string& ciphertext, const std::string& key, const std::string& iv);