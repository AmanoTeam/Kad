cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

project(
	Kad
	VERSION 0.2
	DESCRIPTION "Kad is a simple HTTP proxy server that forwards all requests through curl-impersonate"
	HOMEPAGE_URL "https://github.com/AmanoTeam/Kad"
	LANGUAGES C
)

option(KAD_LTO "Turn on compiler link-time optimizations" OFF)
option(KAD_SSL_VERIFY "Enable SSL certificate verification in libcurl" ON)

set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CMAKE_PLATFORM_NO_VERSIONED_SONAME ON)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

set(EXEC_RPATH "")
set(LIB_RPATH "")

if (APPLE)
	set(EXEC_RPATH "@loader_path/../lib")
	set(LIB_RPATH "@loader_path")
elseif (UNIX)
	set(EXEC_RPATH "$ORIGIN/../lib:$ORIGIN")
	set(LIB_RPATH "$ORIGIN")
endif()

set(CURL_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/curl")
set(BORINGSSL_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/boringssl")
set(BEARSSL_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/bearssl")
set(NGHTTP2_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/nghttp2")
set(ZLIB_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/zlib")
set(BROTLI_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/brotli")
set(CARES_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/c-ares")
set(CURL_IMPERSONATE_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/curl-impersonate")

set(
	INCLUDE_DIRECTORIES
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
	"${BEARSSL_SOURCE_DIRECTORY}/src"
	"${BEARSSL_SOURCE_DIRECTORY}/inc"
)

set(
	KAD_SOURCE_FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/buffer.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/errors.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/ssl.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/callbacks.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/http.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/cleanup.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/main.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/argparser.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/threads.c"
)

set(
	BEARSSL_SOURCE_FILES
	"${BEARSSL_SOURCE_DIRECTORY}/src/settings.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/aead/ccm.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/aead/eax.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/aead/gcm.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/ccopy.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/dec16be.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/dec16le.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/dec32be.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/dec32le.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/dec64be.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/dec64le.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/enc16be.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/enc16le.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/enc32be.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/enc32le.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/enc64be.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/enc64le.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/pemdec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/codec/pemenc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_all_m15.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_all_m31.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_c25519_i15.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_c25519_i31.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_c25519_m15.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_c25519_m31.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_c25519_m62.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_c25519_m64.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_curve25519.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_default.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_keygen.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_p256_m15.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_p256_m31.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_p256_m62.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_p256_m64.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_prime_i15.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_prime_i31.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_pubkey.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_secp256r1.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_secp384r1.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ec_secp521r1.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_atr.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_default_sign_asn1.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_default_sign_raw.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_default_vrfy_asn1.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_default_vrfy_raw.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_i15_bits.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_i15_sign_asn1.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_i15_sign_raw.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_i15_vrfy_asn1.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_i15_vrfy_raw.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_i31_bits.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_i31_sign_asn1.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_i31_sign_raw.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_i31_vrfy_asn1.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_i31_vrfy_raw.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ec/ecdsa_rta.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/hash/dig_oid.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/hash/dig_size.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/hash/ghash_ctmul.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/hash/ghash_ctmul32.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/hash/ghash_ctmul64.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/hash/ghash_pclmul.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/hash/ghash_pwr8.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/hash/md5.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/hash/md5sha1.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/hash/mgf1.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/hash/multihash.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/hash/sha1.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/hash/sha2big.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/hash/sha2small.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_add.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_bitlen.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_decmod.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_decode.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_decred.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_encode.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_fmont.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_iszero.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_moddiv.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_modpow.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_modpow2.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_montmul.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_mulacc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_muladd.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_ninv15.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_reduce.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_rshift.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_sub.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i15_tmont.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_add.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_bitlen.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_decmod.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_decode.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_decred.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_encode.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_fmont.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_iszero.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_moddiv.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_modpow.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_modpow2.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_montmul.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_mulacc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_muladd.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_ninv31.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_reduce.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_rshift.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_sub.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i31_tmont.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_add.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_bitlen.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_decmod.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_decode.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_decred.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_div32.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_encode.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_fmont.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_iszero.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_modpow.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_montmul.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_mulacc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_muladd.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_ninv32.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_reduce.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_sub.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i32_tmont.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/int/i62_modpow2.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/kdf/hkdf.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/kdf/shake.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/mac/hmac.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/mac/hmac_ct.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rand/aesctr_drbg.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rand/hmac_drbg.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rand/sysrng.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_default_keygen.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_default_modulus.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_default_oaep_decrypt.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_default_oaep_encrypt.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_default_pkcs1_sign.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_default_pkcs1_vrfy.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_default_priv.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_default_privexp.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_default_pss_sign.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_default_pss_vrfy.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_default_pub.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_default_pubexp.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i15_keygen.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i15_modulus.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i15_oaep_decrypt.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i15_oaep_encrypt.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i15_pkcs1_sign.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i15_pkcs1_vrfy.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i15_priv.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i15_privexp.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i15_pss_sign.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i15_pss_vrfy.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i15_pub.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i15_pubexp.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i31_keygen.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i31_keygen_inner.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i31_modulus.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i31_oaep_decrypt.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i31_oaep_encrypt.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i31_pkcs1_sign.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i31_pkcs1_vrfy.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i31_priv.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i31_privexp.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i31_pss_sign.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i31_pss_vrfy.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i31_pub.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i31_pubexp.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i32_oaep_decrypt.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i32_oaep_encrypt.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i32_pkcs1_sign.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i32_pkcs1_vrfy.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i32_priv.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i32_pss_sign.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i32_pss_vrfy.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i32_pub.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i62_keygen.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i62_oaep_decrypt.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i62_oaep_encrypt.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i62_pkcs1_sign.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i62_pkcs1_vrfy.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i62_priv.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i62_pss_sign.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i62_pss_vrfy.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_i62_pub.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_oaep_pad.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_oaep_unpad.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_pkcs1_sig_pad.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_pkcs1_sig_unpad.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_pss_sig_pad.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_pss_sig_unpad.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/rsa/rsa_ssl_decrypt.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/prf.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/prf_md5sha1.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/prf_sha256.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/prf_sha384.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_ccert_single_ec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_ccert_single_rsa.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_client.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_client_default_rsapub.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_client_full.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_engine.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_engine_default_aescbc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_engine_default_aesccm.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_engine_default_aesgcm.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_engine_default_chapol.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_engine_default_descbc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_engine_default_ec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_engine_default_ecdsa.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_engine_default_rsavrfy.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_hashes.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_hs_client.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_hs_server.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_io.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_keyexport.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_lru.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_rec_cbc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_rec_ccm.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_rec_chapol.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_rec_gcm.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_scert_single_ec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_scert_single_rsa.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_server.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_server_full_ec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_server_full_rsa.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_server_mine2c.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_server_mine2g.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_server_minf2c.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_server_minf2g.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_server_minr2g.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_server_minu2g.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/ssl/ssl_server_minv2g.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_big_cbcdec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_big_cbcenc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_big_ctr.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_big_ctrcbc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_big_dec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_big_enc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_common.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_ct.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_ct64.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_ct64_cbcdec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_ct64_cbcenc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_ct64_ctr.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_ct64_ctrcbc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_ct64_dec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_ct64_enc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_ct_cbcdec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_ct_cbcenc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_ct_ctr.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_ct_ctrcbc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_ct_dec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_ct_enc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_pwr8.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_pwr8_cbcdec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_pwr8_cbcenc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_pwr8_ctr.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_pwr8_ctrcbc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_small_cbcdec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_small_cbcenc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_small_ctr.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_small_ctrcbc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_small_dec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_small_enc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_x86ni.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_x86ni_cbcdec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_x86ni_cbcenc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_x86ni_ctr.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/aes_x86ni_ctrcbc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/chacha20_ct.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/chacha20_sse2.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/des_ct.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/des_ct_cbcdec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/des_ct_cbcenc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/des_support.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/des_tab.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/des_tab_cbcdec.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/des_tab_cbcenc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/poly1305_ctmul.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/poly1305_ctmul32.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/poly1305_ctmulq.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/symcipher/poly1305_i15.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/x509/asn1enc.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/x509/encode_ec_pk8der.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/x509/encode_ec_rawder.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/x509/encode_rsa_pk8der.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/x509/encode_rsa_rawder.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/x509/skey_decoder.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/x509/x509_decoder.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/x509/x509_knownkey.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/x509/x509_minimal.c"
	"${BEARSSL_SOURCE_DIRECTORY}/src/x509/x509_minimal_full.c"
)

set(
	TARGETS
	kad
	bearssl
	libcurl
	zlib
	crypto
	ssl
	nghttp2
	brotlicommon
	brotlidec
	c-ares
)

set(
	CURL_LIBRARY_DEPENDENCIES
	zlib
	crypto
	ssl
	nghttp2
	brotlidec
	c-ares
)

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

# nghttp2
set(ENABLE_LIB_ONLY ON CACHE BOOL "")

# BoringSSL
set(BUILD_SHARED_LIBS ON)

execute_process(
	COMMAND git submodule update --init --depth=1
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	COMMAND_ERROR_IS_FATAL ANY
)

file(READ "${ZLIB_SOURCE_DIRECTORY}/CMakeLists.txt" FILE_CONTENTS)
string(REPLACE "(UNIX)" "(1)" FILE_CONTENTS "${FILE_CONTENTS}")
string(REPLACE "(NOT APPLE)" "(0)" FILE_CONTENTS "${FILE_CONTENTS}")
file(WRITE "${ZLIB_SOURCE_DIRECTORY}/CMakeLists.txt" "${FILE_CONTENTS}")

if (NOT EXISTS "${CURL_SOURCE_DIRECTORY}/.patched")
	message("-- Patching cURL")

	execute_process(
		COMMAND patch
		--directory=${CURL_SOURCE_DIRECTORY}
		--strip=1
		--input=${CURL_IMPERSONATE_SOURCE_DIRECTORY}/chrome/patches/curl-impersonate.patch
		COMMAND_ERROR_IS_FATAL ANY
	)

	file(WRITE "${CURL_SOURCE_DIRECTORY}/.patched" "")
endif()

if (NOT EXISTS "${BORINGSSL_SOURCE_DIRECTORY}/.patched")
	message("-- Patching BoringSSL")

	execute_process(
		COMMAND patch
		--directory=${BORINGSSL_SOURCE_DIRECTORY}
		--strip=1
		--input=${CURL_IMPERSONATE_SOURCE_DIRECTORY}/chrome/patches/boringssl-old-ciphers.patch
		COMMAND_ERROR_IS_FATAL ANY
	)

	file(WRITE "${BORINGSSL_SOURCE_DIRECTORY}/.patched" "")
endif()

if (NOT APPLE)
	if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
		add_link_options(-s)
	endif()
endif()

if (CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
	add_compile_options(-Oz)
endif()

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
	add_compile_options($<$<CONFIG:Release>:-fipa-pta>)
endif()

set(BEARSSL_INCLUDE_DIRS "${BEARSSL_SOURCE_DIRECTORY}/inc")
set(BEARSSL_LIBRARY $<TARGET_FILE:bearssl>)

set(OPENSSL_INCLUDE_DIR "${BORINGSSL_SOURCE_DIRECTORY}/include")

set(OPENSSL_CRYPTO_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}crypto${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(OPENSSL_SSL_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}ssl${CMAKE_SHARED_LIBRARY_SUFFIX}")

file(WRITE "${OPENSSL_SSL_LIBRARY}" "")
file(WRITE "${OPENSSL_CRYPTO_LIBRARY}" "")

set(
	NGHTTP2_INCLUDE_DIR
	"${NGHTTP2_SOURCE_DIRECTORY}/lib/includes"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/nghttp2/lib/includes"
)

set(NGHTTP2_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}nghttp2${CMAKE_SHARED_LIBRARY_SUFFIX}")

set(
	ZLIB_INCLUDE_DIR
	"${ZLIB_SOURCE_DIRECTORY}"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/zlib"
)

set(ZLIB_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}z${CMAKE_SHARED_LIBRARY_SUFFIX}")

set(BROTLI_INCLUDE_DIR "${BROTLI_SOURCE_DIRECTORY}/c/include")

set(BROTLICOMMON_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}brotlicommon${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(BROTLIDEC_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}brotlidec${CMAKE_SHARED_LIBRARY_SUFFIX}")

set(
	CARES_INCLUDE_DIR
	"${CARES_SOURCE_DIRECTORY}/include"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/c-ares"
)

set(CARES_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}cares${CMAKE_SHARED_LIBRARY_SUFFIX}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)

if (ANDROID)
	set(ANDROID_STL "c++_static")
endif()

add_subdirectory(
	"${CURL_SOURCE_DIRECTORY}"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/curl"
	EXCLUDE_FROM_ALL
)

add_subdirectory(
	"${ZLIB_SOURCE_DIRECTORY}"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/zlib"
	EXCLUDE_FROM_ALL
)

add_subdirectory(
	"${BORINGSSL_SOURCE_DIRECTORY}"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/boringssl"
	EXCLUDE_FROM_ALL
)

add_subdirectory(
	"${NGHTTP2_SOURCE_DIRECTORY}"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/nghttp2"
	EXCLUDE_FROM_ALL
)

add_subdirectory(
	"${BROTLI_SOURCE_DIRECTORY}"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/brotli"
	EXCLUDE_FROM_ALL
)

add_subdirectory(
	"${CARES_SOURCE_DIRECTORY}"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/c-ares"
	EXCLUDE_FROM_ALL
)

file(REMOVE "${OPENSSL_SSL_LIBRARY}")
file(REMOVE "${OPENSSL_CRYPTO_LIBRARY}")

foreach(target ${CURL_LIBRARY_DEPENDENCIES})
	add_custom_command(
		OUTPUT ${target}
		COMMAND ${CMAKE_COMMAND} --build ./ --target ${target}
	)

	add_custom_target(
		"ensure_${target}" ALL DEPENDS ${target}
	)

	add_dependencies(
		libcurl
		"ensure_${target}"
	)
endforeach()

include_directories(${INCLUDE_DIRECTORIES})

add_library(
	bearssl
	SHARED
	${BEARSSL_SOURCE_FILES}
)

add_executable(
	kad
	${KAD_SOURCE_FILES}
)

if (NOT KAD_SSL_VERIFY)
	target_compile_definitions(
		kad
		PRIVATE
		KAD_DISABLE_SSL_VERIFY
	)
else()
	target_sources(
		kad
		PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src/filesystem.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/stringu.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/fstream.c"
	)
endif()

if (KAD_LTO)
	set(KAD_HAS_LTO OFF)

	include(CheckIPOSupported)
	check_ipo_supported(RESULT KAD_HAS_LTO LANGUAGES C)

	if (KAD_HAS_LTO)
		set_target_properties(
			${TARGETS}
			PROPERTIES
			INTERPROCEDURAL_OPTIMIZATION TRUE
		)
	endif()
endif()

set_target_properties(
	kad
	PROPERTIES
	BUILD_RPATH "${EXEC_RPATH}"
	INSTALL_RPATH "${EXEC_RPATH}"
)

set_target_properties(
	libcurl ssl brotlidec
	PROPERTIES
	BUILD_RPATH "${LIB_RPATH}"
	INSTALL_RPATH "${LIB_RPATH}"
)

target_link_libraries(
	kad
	bearssl
	libcurl
)

install(
	TARGETS ${TARGETS}
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
)

install(
	FILES "${CMAKE_CURRENT_SOURCE_DIR}/submodules/cabundle/pem/cert.pem"
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
