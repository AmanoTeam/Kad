cmake_minimum_required(VERSION 3.13)

project(
	Kad
	VERSION 0.1
	DESCRIPTION "Kad is a simple HTTP proxy server that forwards all requests through curl-impersonate"
	HOMEPAGE_URL "https://github.com/AmanoTeam/Kad"
	LANGUAGES C
)

option(KAD_ENABLE_LTO "Turn on compiler Link Time Optimizations" OFF)
option(KAD_DISABLE_CERTIFICATE_VALIDATION "Disable SSL certificate validation in libcurl" OFF)

set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CMAKE_PLATFORM_NO_VERSIONED_SONAME ON)

# curl
set(PICKY_COMPILER OFF)
set(BUILD_CURL_EXE OFF)
set(HTTP_ONLY ON)
set(ENABLE_THREADED_RESOLVER OFF)
set(CURL_USE_BEARSSL OFF)
set(CURL_USE_OPENSSL ON)
set(USE_NGHTTP2 ON)
set(CURL_BROTLI ON)
set(ENABLE_ARES ON)
set(CURL_USE_LIBPSL OFF)
set(CURL_DISABLE_ALTSVC ON)
set(CURL_DISABLE_CRYPTO_AUTH ON)
set(CURL_DISABLE_COOKIES ON)
set(CURL_DISABLE_HSTS ON)
set(CURL_DISABLE_HTTP_AUTH ON)
set(CURL_DISABLE_NETRC ON)
set(USE_LIBIDN2 OFF)
set(ENABLE_UNIX_SOCKETS OFF)
set(CURL_USE_LIBSSH2 OFF)
set(CURL_CA_BUNDLE "none")
set(CURL_CA_PATH "none")
set(CURL_WERROR OFF)
set(CURL_DISABLE_DOH ON)

if (WIN32)
	set(ENABLE_UNICODE ON)
endif()

# ngtcp2
set(ENABLE_LIB_ONLY ON CACHE BOOL "")

# BoringSSL
set(BUILD_SHARED_LIBS ON)

file(READ "${CMAKE_SOURCE_DIR}/submodules/zlib/CMakeLists.txt" FILE_CONTENTS)
string(REPLACE "(UNIX)" "(1)" FILE_CONTENTS "${FILE_CONTENTS}")
string(REPLACE "(NOT APPLE)" "(0)" FILE_CONTENTS "${FILE_CONTENTS}")
file(WRITE "${CMAKE_SOURCE_DIR}/submodules/zlib/CMakeLists.txt" "${FILE_CONTENTS}")

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/submodules/curl/.patched")
	message("-- Patching cURL")
	
	execute_process(
		COMMAND patch
		--directory=${CMAKE_CURRENT_SOURCE_DIR}/submodules/curl
		--strip=1
		--input=${CMAKE_CURRENT_SOURCE_DIR}/submodules/curl-impersonate/chrome/patches/curl-impersonate.patch
		COMMAND_ERROR_IS_FATAL ANY
	)
	
	file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/submodules/curl/.patched" "")
endif()

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/submodules/boringssl/.patched")
	message("-- Patching BoringSSL")
	
	execute_process(
		COMMAND patch
		--directory=${CMAKE_CURRENT_SOURCE_DIR}/submodules/boringssl
		--strip=1
		--input=${CMAKE_CURRENT_SOURCE_DIR}/submodules/curl-impersonate/chrome/patches/boringssl-old-ciphers.patch
		COMMAND_ERROR_IS_FATAL ANY
	)
	
	file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/submodules/boringssl/.patched" "")
endif()

if (NOT APPLE)
	if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
		add_link_options(-s)
	endif()
endif()

if (CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
	add_compile_options(-Oz)
endif()

set(BEARSSL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/submodules/bearssl/inc")
set(BEARSSL_LIBRARY $<TARGET_FILE:bearssl>)

set(OPENSSL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/submodules/boringssl/include")

set(OPENSSL_CRYPTO_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}crypto${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(OPENSSL_SSL_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}ssl${CMAKE_SHARED_LIBRARY_SUFFIX}")

file(WRITE ${OPENSSL_SSL_LIBRARY} "")
file(WRITE ${OPENSSL_CRYPTO_LIBRARY} "")

set(
	NGHTTP2_INCLUDE_DIR
	"${CMAKE_CURRENT_SOURCE_DIR}/submodules/nghttp2/lib/includes"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/nghttp2/lib/includes"
)

set(NGHTTP2_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}nghttp2${CMAKE_SHARED_LIBRARY_SUFFIX}")

set(
	ZLIB_INCLUDE_DIR
	"${CMAKE_CURRENT_SOURCE_DIR}/submodules/zlib"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/zlib"
)

set(ZLIB_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}z${CMAKE_SHARED_LIBRARY_SUFFIX}")

set(BROTLI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/submodules/brotli/c/include")

set(BROTLICOMMON_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}brotlicommon${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(BROTLIDEC_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}brotlidec${CMAKE_SHARED_LIBRARY_SUFFIX}")

set(
	CARES_INCLUDE_DIR
	"${CMAKE_CURRENT_SOURCE_DIR}/submodules/c-ares/include"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/c-ares"
)

set(CARES_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}cares${CMAKE_SHARED_LIBRARY_SUFFIX}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)

if (ANDROID)
	set(ANDROID_STL "c++_static")
endif()

add_subdirectory(submodules/curl EXCLUDE_FROM_ALL)
add_subdirectory(submodules/zlib EXCLUDE_FROM_ALL)
add_subdirectory(submodules/boringssl EXCLUDE_FROM_ALL)
add_subdirectory(submodules/nghttp2 EXCLUDE_FROM_ALL)
add_subdirectory(submodules/brotli EXCLUDE_FROM_ALL)
add_subdirectory(submodules/c-ares EXCLUDE_FROM_ALL)

file(REMOVE "${OPENSSL_SSL_LIBRARY}")
file(REMOVE "${OPENSSL_CRYPTO_LIBRARY}")

foreach(target zlib crypto ssl nghttp2 brotlidec c-ares)
	add_custom_command(
		OUTPUT ${target}
		COMMAND ${CMAKE_COMMAND} --build ./ --target ${target}
	)
	
	add_custom_target(
		"ensure_${target}" ALL DEPENDS "${target}"
	)
	
	add_dependencies(
		libcurl
		"ensure_${target}"
	)
endforeach()

include_directories(
	"${CMAKE_SOURCE_DIR}/src"
	"${CMAKE_SOURCE_DIR}/submodules/bearssl/src"
	"${CMAKE_SOURCE_DIR}/submodules/bearssl/inc"
)

add_library(
	bearssl
	SHARED
	submodules/bearssl/src/settings.c
	submodules/bearssl/src/aead/ccm.c
	submodules/bearssl/src/aead/eax.c
	submodules/bearssl/src/aead/gcm.c
	submodules/bearssl/src/codec/ccopy.c
	submodules/bearssl/src/codec/dec16be.c
	submodules/bearssl/src/codec/dec16le.c
	submodules/bearssl/src/codec/dec32be.c
	submodules/bearssl/src/codec/dec32le.c
	submodules/bearssl/src/codec/dec64be.c
	submodules/bearssl/src/codec/dec64le.c
	submodules/bearssl/src/codec/enc16be.c
	submodules/bearssl/src/codec/enc16le.c
	submodules/bearssl/src/codec/enc32be.c
	submodules/bearssl/src/codec/enc32le.c
	submodules/bearssl/src/codec/enc64be.c
	submodules/bearssl/src/codec/enc64le.c
	submodules/bearssl/src/codec/pemdec.c
	submodules/bearssl/src/codec/pemenc.c
	submodules/bearssl/src/ec/ec_all_m15.c
	submodules/bearssl/src/ec/ec_all_m31.c
	submodules/bearssl/src/ec/ec_c25519_i15.c
	submodules/bearssl/src/ec/ec_c25519_i31.c
	submodules/bearssl/src/ec/ec_c25519_m15.c
	submodules/bearssl/src/ec/ec_c25519_m31.c
	submodules/bearssl/src/ec/ec_c25519_m62.c
	submodules/bearssl/src/ec/ec_c25519_m64.c
	submodules/bearssl/src/ec/ec_curve25519.c
	submodules/bearssl/src/ec/ec_default.c
	submodules/bearssl/src/ec/ec_keygen.c
	submodules/bearssl/src/ec/ec_p256_m15.c
	submodules/bearssl/src/ec/ec_p256_m31.c
	submodules/bearssl/src/ec/ec_p256_m62.c
	submodules/bearssl/src/ec/ec_p256_m64.c
	submodules/bearssl/src/ec/ec_prime_i15.c
	submodules/bearssl/src/ec/ec_prime_i31.c
	submodules/bearssl/src/ec/ec_pubkey.c
	submodules/bearssl/src/ec/ec_secp256r1.c
	submodules/bearssl/src/ec/ec_secp384r1.c
	submodules/bearssl/src/ec/ec_secp521r1.c
	submodules/bearssl/src/ec/ecdsa_atr.c
	submodules/bearssl/src/ec/ecdsa_default_sign_asn1.c
	submodules/bearssl/src/ec/ecdsa_default_sign_raw.c
	submodules/bearssl/src/ec/ecdsa_default_vrfy_asn1.c
	submodules/bearssl/src/ec/ecdsa_default_vrfy_raw.c
	submodules/bearssl/src/ec/ecdsa_i15_bits.c
	submodules/bearssl/src/ec/ecdsa_i15_sign_asn1.c
	submodules/bearssl/src/ec/ecdsa_i15_sign_raw.c
	submodules/bearssl/src/ec/ecdsa_i15_vrfy_asn1.c
	submodules/bearssl/src/ec/ecdsa_i15_vrfy_raw.c
	submodules/bearssl/src/ec/ecdsa_i31_bits.c
	submodules/bearssl/src/ec/ecdsa_i31_sign_asn1.c
	submodules/bearssl/src/ec/ecdsa_i31_sign_raw.c
	submodules/bearssl/src/ec/ecdsa_i31_vrfy_asn1.c
	submodules/bearssl/src/ec/ecdsa_i31_vrfy_raw.c
	submodules/bearssl/src/ec/ecdsa_rta.c
	submodules/bearssl/src/hash/dig_oid.c
	submodules/bearssl/src/hash/dig_size.c
	submodules/bearssl/src/hash/ghash_ctmul.c
	submodules/bearssl/src/hash/ghash_ctmul32.c
	submodules/bearssl/src/hash/ghash_ctmul64.c
	submodules/bearssl/src/hash/ghash_pclmul.c
	submodules/bearssl/src/hash/ghash_pwr8.c
	submodules/bearssl/src/hash/md5.c
	submodules/bearssl/src/hash/md5sha1.c
	submodules/bearssl/src/hash/mgf1.c
	submodules/bearssl/src/hash/multihash.c
	submodules/bearssl/src/hash/sha1.c
	submodules/bearssl/src/hash/sha2big.c
	submodules/bearssl/src/hash/sha2small.c
	submodules/bearssl/src/int/i15_add.c
	submodules/bearssl/src/int/i15_bitlen.c
	submodules/bearssl/src/int/i15_decmod.c
	submodules/bearssl/src/int/i15_decode.c
	submodules/bearssl/src/int/i15_decred.c
	submodules/bearssl/src/int/i15_encode.c
	submodules/bearssl/src/int/i15_fmont.c
	submodules/bearssl/src/int/i15_iszero.c
	submodules/bearssl/src/int/i15_moddiv.c
	submodules/bearssl/src/int/i15_modpow.c
	submodules/bearssl/src/int/i15_modpow2.c
	submodules/bearssl/src/int/i15_montmul.c
	submodules/bearssl/src/int/i15_mulacc.c
	submodules/bearssl/src/int/i15_muladd.c
	submodules/bearssl/src/int/i15_ninv15.c
	submodules/bearssl/src/int/i15_reduce.c
	submodules/bearssl/src/int/i15_rshift.c
	submodules/bearssl/src/int/i15_sub.c
	submodules/bearssl/src/int/i15_tmont.c
	submodules/bearssl/src/int/i31_add.c
	submodules/bearssl/src/int/i31_bitlen.c
	submodules/bearssl/src/int/i31_decmod.c
	submodules/bearssl/src/int/i31_decode.c
	submodules/bearssl/src/int/i31_decred.c
	submodules/bearssl/src/int/i31_encode.c
	submodules/bearssl/src/int/i31_fmont.c
	submodules/bearssl/src/int/i31_iszero.c
	submodules/bearssl/src/int/i31_moddiv.c
	submodules/bearssl/src/int/i31_modpow.c
	submodules/bearssl/src/int/i31_modpow2.c
	submodules/bearssl/src/int/i31_montmul.c
	submodules/bearssl/src/int/i31_mulacc.c
	submodules/bearssl/src/int/i31_muladd.c
	submodules/bearssl/src/int/i31_ninv31.c
	submodules/bearssl/src/int/i31_reduce.c
	submodules/bearssl/src/int/i31_rshift.c
	submodules/bearssl/src/int/i31_sub.c
	submodules/bearssl/src/int/i31_tmont.c
	submodules/bearssl/src/int/i32_add.c
	submodules/bearssl/src/int/i32_bitlen.c
	submodules/bearssl/src/int/i32_decmod.c
	submodules/bearssl/src/int/i32_decode.c
	submodules/bearssl/src/int/i32_decred.c
	submodules/bearssl/src/int/i32_div32.c
	submodules/bearssl/src/int/i32_encode.c
	submodules/bearssl/src/int/i32_fmont.c
	submodules/bearssl/src/int/i32_iszero.c
	submodules/bearssl/src/int/i32_modpow.c
	submodules/bearssl/src/int/i32_montmul.c
	submodules/bearssl/src/int/i32_mulacc.c
	submodules/bearssl/src/int/i32_muladd.c
	submodules/bearssl/src/int/i32_ninv32.c
	submodules/bearssl/src/int/i32_reduce.c
	submodules/bearssl/src/int/i32_sub.c
	submodules/bearssl/src/int/i32_tmont.c
	submodules/bearssl/src/int/i62_modpow2.c
	submodules/bearssl/src/kdf/hkdf.c
	submodules/bearssl/src/kdf/shake.c
	submodules/bearssl/src/mac/hmac.c
	submodules/bearssl/src/mac/hmac_ct.c
	submodules/bearssl/src/rand/aesctr_drbg.c
	submodules/bearssl/src/rand/hmac_drbg.c
	submodules/bearssl/src/rand/sysrng.c
	submodules/bearssl/src/rsa/rsa_default_keygen.c
	submodules/bearssl/src/rsa/rsa_default_modulus.c
	submodules/bearssl/src/rsa/rsa_default_oaep_decrypt.c
	submodules/bearssl/src/rsa/rsa_default_oaep_encrypt.c
	submodules/bearssl/src/rsa/rsa_default_pkcs1_sign.c
	submodules/bearssl/src/rsa/rsa_default_pkcs1_vrfy.c
	submodules/bearssl/src/rsa/rsa_default_priv.c
	submodules/bearssl/src/rsa/rsa_default_privexp.c
	submodules/bearssl/src/rsa/rsa_default_pss_sign.c
	submodules/bearssl/src/rsa/rsa_default_pss_vrfy.c
	submodules/bearssl/src/rsa/rsa_default_pub.c
	submodules/bearssl/src/rsa/rsa_default_pubexp.c
	submodules/bearssl/src/rsa/rsa_i15_keygen.c
	submodules/bearssl/src/rsa/rsa_i15_modulus.c
	submodules/bearssl/src/rsa/rsa_i15_oaep_decrypt.c
	submodules/bearssl/src/rsa/rsa_i15_oaep_encrypt.c
	submodules/bearssl/src/rsa/rsa_i15_pkcs1_sign.c
	submodules/bearssl/src/rsa/rsa_i15_pkcs1_vrfy.c
	submodules/bearssl/src/rsa/rsa_i15_priv.c
	submodules/bearssl/src/rsa/rsa_i15_privexp.c
	submodules/bearssl/src/rsa/rsa_i15_pss_sign.c
	submodules/bearssl/src/rsa/rsa_i15_pss_vrfy.c
	submodules/bearssl/src/rsa/rsa_i15_pub.c
	submodules/bearssl/src/rsa/rsa_i15_pubexp.c
	submodules/bearssl/src/rsa/rsa_i31_keygen.c
	submodules/bearssl/src/rsa/rsa_i31_keygen_inner.c
	submodules/bearssl/src/rsa/rsa_i31_modulus.c
	submodules/bearssl/src/rsa/rsa_i31_oaep_decrypt.c
	submodules/bearssl/src/rsa/rsa_i31_oaep_encrypt.c
	submodules/bearssl/src/rsa/rsa_i31_pkcs1_sign.c
	submodules/bearssl/src/rsa/rsa_i31_pkcs1_vrfy.c
	submodules/bearssl/src/rsa/rsa_i31_priv.c
	submodules/bearssl/src/rsa/rsa_i31_privexp.c
	submodules/bearssl/src/rsa/rsa_i31_pss_sign.c
	submodules/bearssl/src/rsa/rsa_i31_pss_vrfy.c
	submodules/bearssl/src/rsa/rsa_i31_pub.c
	submodules/bearssl/src/rsa/rsa_i31_pubexp.c
	submodules/bearssl/src/rsa/rsa_i32_oaep_decrypt.c
	submodules/bearssl/src/rsa/rsa_i32_oaep_encrypt.c
	submodules/bearssl/src/rsa/rsa_i32_pkcs1_sign.c
	submodules/bearssl/src/rsa/rsa_i32_pkcs1_vrfy.c
	submodules/bearssl/src/rsa/rsa_i32_priv.c
	submodules/bearssl/src/rsa/rsa_i32_pss_sign.c
	submodules/bearssl/src/rsa/rsa_i32_pss_vrfy.c
	submodules/bearssl/src/rsa/rsa_i32_pub.c
	submodules/bearssl/src/rsa/rsa_i62_keygen.c
	submodules/bearssl/src/rsa/rsa_i62_oaep_decrypt.c
	submodules/bearssl/src/rsa/rsa_i62_oaep_encrypt.c
	submodules/bearssl/src/rsa/rsa_i62_pkcs1_sign.c
	submodules/bearssl/src/rsa/rsa_i62_pkcs1_vrfy.c
	submodules/bearssl/src/rsa/rsa_i62_priv.c
	submodules/bearssl/src/rsa/rsa_i62_pss_sign.c
	submodules/bearssl/src/rsa/rsa_i62_pss_vrfy.c
	submodules/bearssl/src/rsa/rsa_i62_pub.c
	submodules/bearssl/src/rsa/rsa_oaep_pad.c
	submodules/bearssl/src/rsa/rsa_oaep_unpad.c
	submodules/bearssl/src/rsa/rsa_pkcs1_sig_pad.c
	submodules/bearssl/src/rsa/rsa_pkcs1_sig_unpad.c
	submodules/bearssl/src/rsa/rsa_pss_sig_pad.c
	submodules/bearssl/src/rsa/rsa_pss_sig_unpad.c
	submodules/bearssl/src/rsa/rsa_ssl_decrypt.c
	submodules/bearssl/src/ssl/prf.c
	submodules/bearssl/src/ssl/prf_md5sha1.c
	submodules/bearssl/src/ssl/prf_sha256.c
	submodules/bearssl/src/ssl/prf_sha384.c
	submodules/bearssl/src/ssl/ssl_ccert_single_ec.c
	submodules/bearssl/src/ssl/ssl_ccert_single_rsa.c
	submodules/bearssl/src/ssl/ssl_client.c
	submodules/bearssl/src/ssl/ssl_client_default_rsapub.c
	submodules/bearssl/src/ssl/ssl_client_full.c
	submodules/bearssl/src/ssl/ssl_engine.c
	submodules/bearssl/src/ssl/ssl_engine_default_aescbc.c
	submodules/bearssl/src/ssl/ssl_engine_default_aesccm.c
	submodules/bearssl/src/ssl/ssl_engine_default_aesgcm.c
	submodules/bearssl/src/ssl/ssl_engine_default_chapol.c
	submodules/bearssl/src/ssl/ssl_engine_default_descbc.c
	submodules/bearssl/src/ssl/ssl_engine_default_ec.c
	submodules/bearssl/src/ssl/ssl_engine_default_ecdsa.c
	submodules/bearssl/src/ssl/ssl_engine_default_rsavrfy.c
	submodules/bearssl/src/ssl/ssl_hashes.c
	submodules/bearssl/src/ssl/ssl_hs_client.c
	submodules/bearssl/src/ssl/ssl_hs_server.c
	submodules/bearssl/src/ssl/ssl_io.c
	submodules/bearssl/src/ssl/ssl_keyexport.c
	submodules/bearssl/src/ssl/ssl_lru.c
	submodules/bearssl/src/ssl/ssl_rec_cbc.c
	submodules/bearssl/src/ssl/ssl_rec_ccm.c
	submodules/bearssl/src/ssl/ssl_rec_chapol.c
	submodules/bearssl/src/ssl/ssl_rec_gcm.c
	submodules/bearssl/src/ssl/ssl_scert_single_ec.c
	submodules/bearssl/src/ssl/ssl_scert_single_rsa.c
	submodules/bearssl/src/ssl/ssl_server.c
	submodules/bearssl/src/ssl/ssl_server_full_ec.c
	submodules/bearssl/src/ssl/ssl_server_full_rsa.c
	submodules/bearssl/src/ssl/ssl_server_mine2c.c
	submodules/bearssl/src/ssl/ssl_server_mine2g.c
	submodules/bearssl/src/ssl/ssl_server_minf2c.c
	submodules/bearssl/src/ssl/ssl_server_minf2g.c
	submodules/bearssl/src/ssl/ssl_server_minr2g.c
	submodules/bearssl/src/ssl/ssl_server_minu2g.c
	submodules/bearssl/src/ssl/ssl_server_minv2g.c
	submodules/bearssl/src/symcipher/aes_big_cbcdec.c
	submodules/bearssl/src/symcipher/aes_big_cbcenc.c
	submodules/bearssl/src/symcipher/aes_big_ctr.c
	submodules/bearssl/src/symcipher/aes_big_ctrcbc.c
	submodules/bearssl/src/symcipher/aes_big_dec.c
	submodules/bearssl/src/symcipher/aes_big_enc.c
	submodules/bearssl/src/symcipher/aes_common.c
	submodules/bearssl/src/symcipher/aes_ct.c
	submodules/bearssl/src/symcipher/aes_ct64.c
	submodules/bearssl/src/symcipher/aes_ct64_cbcdec.c
	submodules/bearssl/src/symcipher/aes_ct64_cbcenc.c
	submodules/bearssl/src/symcipher/aes_ct64_ctr.c
	submodules/bearssl/src/symcipher/aes_ct64_ctrcbc.c
	submodules/bearssl/src/symcipher/aes_ct64_dec.c
	submodules/bearssl/src/symcipher/aes_ct64_enc.c
	submodules/bearssl/src/symcipher/aes_ct_cbcdec.c
	submodules/bearssl/src/symcipher/aes_ct_cbcenc.c
	submodules/bearssl/src/symcipher/aes_ct_ctr.c
	submodules/bearssl/src/symcipher/aes_ct_ctrcbc.c
	submodules/bearssl/src/symcipher/aes_ct_dec.c
	submodules/bearssl/src/symcipher/aes_ct_enc.c
	submodules/bearssl/src/symcipher/aes_pwr8.c
	submodules/bearssl/src/symcipher/aes_pwr8_cbcdec.c
	submodules/bearssl/src/symcipher/aes_pwr8_cbcenc.c
	submodules/bearssl/src/symcipher/aes_pwr8_ctr.c
	submodules/bearssl/src/symcipher/aes_pwr8_ctrcbc.c
	submodules/bearssl/src/symcipher/aes_small_cbcdec.c
	submodules/bearssl/src/symcipher/aes_small_cbcenc.c
	submodules/bearssl/src/symcipher/aes_small_ctr.c
	submodules/bearssl/src/symcipher/aes_small_ctrcbc.c
	submodules/bearssl/src/symcipher/aes_small_dec.c
	submodules/bearssl/src/symcipher/aes_small_enc.c
	submodules/bearssl/src/symcipher/aes_x86ni.c
	submodules/bearssl/src/symcipher/aes_x86ni_cbcdec.c
	submodules/bearssl/src/symcipher/aes_x86ni_cbcenc.c
	submodules/bearssl/src/symcipher/aes_x86ni_ctr.c
	submodules/bearssl/src/symcipher/aes_x86ni_ctrcbc.c
	submodules/bearssl/src/symcipher/chacha20_ct.c
	submodules/bearssl/src/symcipher/chacha20_sse2.c
	submodules/bearssl/src/symcipher/des_ct.c
	submodules/bearssl/src/symcipher/des_ct_cbcdec.c
	submodules/bearssl/src/symcipher/des_ct_cbcenc.c
	submodules/bearssl/src/symcipher/des_support.c
	submodules/bearssl/src/symcipher/des_tab.c
	submodules/bearssl/src/symcipher/des_tab_cbcdec.c
	submodules/bearssl/src/symcipher/des_tab_cbcenc.c
	submodules/bearssl/src/symcipher/poly1305_ctmul.c
	submodules/bearssl/src/symcipher/poly1305_ctmul32.c
	submodules/bearssl/src/symcipher/poly1305_ctmulq.c
	submodules/bearssl/src/symcipher/poly1305_i15.c
	submodules/bearssl/src/x509/asn1enc.c
	submodules/bearssl/src/x509/encode_ec_pk8der.c
	submodules/bearssl/src/x509/encode_ec_rawder.c
	submodules/bearssl/src/x509/encode_rsa_pk8der.c
	submodules/bearssl/src/x509/encode_rsa_rawder.c
	submodules/bearssl/src/x509/skey_decoder.c
	submodules/bearssl/src/x509/x509_decoder.c
	submodules/bearssl/src/x509/x509_knownkey.c
	submodules/bearssl/src/x509/x509_minimal.c
	submodules/bearssl/src/x509/x509_minimal_full.c
)

foreach(property RUNTIME_OUTPUT_DIRECTORY LIBRARY_OUTPUT_DIRECTORY)
	foreach(target libcurl zlib crypto ssl nghttp2 brotlicommon brotlidec c-ares)
		set_target_properties(
			${target}
			PROPERTIES
			${property} "${CMAKE_CURRENT_BINARY_DIR}"
		)
	endforeach()
endforeach()

add_executable(
	kad
	src/buffer.c
	src/errors.c
	src/ssl.c
	src/callbacks.c
	src/http.c
	src/cleanup.c
	src/main.c
	src/argparser.c
	src/threads.c
)

if (KAD_DISABLE_CERTIFICATE_VALIDATION) 
	target_compile_definitions(
		kad
		PRIVATE
		KAD_DISABLE_CERTIFICATE_VALIDATION
	)
else()
	target_sources(
		kad
		PRIVATE
		src/filesystem.c
		src/stringu.c
		src/fstream.c
	)
endif()

if (KAD_ENABLE_LTO)
	set(KAD_HAS_LTO OFF)
	
	include(CheckIPOSupported)
	check_ipo_supported(RESULT KAD_HAS_LTO LANGUAGES C)
	
	if (KAD_HAS_LTO)
		foreach(target kad bearssl libcurl zlib crypto ssl nghttp2 brotlicommon brotlidec c-ares)
			set_target_properties(
				${target}
				PROPERTIES
				INTERPROCEDURAL_OPTIMIZATION TRUE
			)
		endforeach()
	endif()
endif()

foreach(target libcurl ssl brotlidec)
	if (APPLE)
		foreach(property BUILD_RPATH INSTALL_RPATH)
			set_target_properties(
				${target}
				PROPERTIES
				${property} "@loader_path"
			)
		endforeach()
	elseif (UNIX)
		foreach(property BUILD_RPATH INSTALL_RPATH)
			set_target_properties(
				${target}
				PROPERTIES
				${property} "$ORIGIN"
			)
		endforeach()
	endif()
endforeach()

if (APPLE)
	foreach(property BUILD_RPATH INSTALL_RPATH)
		set_target_properties(
			kad
			PROPERTIES
			${property} "@loader_path/../lib"
		)
	endforeach()
elseif (UNIX)
	foreach(property BUILD_RPATH INSTALL_RPATH)
		set_target_properties(
			kad
			PROPERTIES
			${property} "$ORIGIN/../lib:$ORIGIN"
		)
	endforeach()
endif()

target_link_libraries(
	kad
	bearssl
	libcurl
)

foreach(target kad bearssl libcurl zlib crypto ssl nghttp2 brotlicommon brotlidec c-ares)
	install(
		TARGETS ${target}
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
	)
endforeach()

install(
	FILES "${CMAKE_SOURCE_DIR}/submodules/cabundle/pem/cert.pem"
	DESTINATION etc/tls
	RENAME cert.pem
)

target_link_options(
	kad
	BEFORE
	PRIVATE
	-pthread
)

if ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i386")
	target_link_options(
		ssl
		BEFORE
		PRIVATE
		-latomic
	)
endif()

if (NOT ANDROID)
	foreach(target ssl nghttp2)
		target_link_options(
			${target}
			BEFORE
			PRIVATE
			-static-libgcc -static-libstdc++
		)
	endforeach()
endif()

if (CMAKE_CROSSCOMPILING)
	target_link_options(
		kad
		BEFORE
		PRIVATE
		-Wl,-rpath-link -Wl,${CMAKE_CURRENT_BINARY_DIR}
	)
endif()
