cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(
	Kad
	VERSION 0.2
	DESCRIPTION "Kad is a simple HTTP proxy server that forwards all requests through curl-impersonate"
	HOMEPAGE_URL "https://github.com/AmanoTeam/Kad"
	LANGUAGES C CXX
)

option(KAD_LTO "Turn on compiler link-time optimizations" OFF)
option(KAD_UNICODE "Enable Unicode support for Windows builds" ON)
option(KAD_SSL_VERIFY "Enable SSL certificate verification in libcurl" ON)
option(KAD_ASAN "Build with AddressSanitizer" OFF)
option(KAD_PROFILE "Build with gprof instrumentation (-pg)" OFF)
option(KAD_LIBRARY "Build the project as a shared library" OFF)
option(KAD_STATIC "Build all dependencies as static libraries" OFF)

if (KAD_STATIC)
	set(KAD_LIBRARY_PREFIX "${CMAKE_STATIC_LIBRARY_PREFIX}")
	set(KAD_LIBRARY_SUFFIX "${CMAKE_STATIC_LIBRARY_SUFFIX}")
	
	set(KAD_CURL_LIBRARY "libcurl_static")
else()
	set(KAD_LIBRARY_PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}")
	set(KAD_LIBRARY_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
	
	set(KAD_CURL_LIBRARY "libcurl_shared")
endif()

set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)

set(CMAKE_PLATFORM_NO_VERSIONED_SONAME ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if (APPLE)
	set(CMAKE_INSTALL_RPATH "@loader_path/../lib" "@loader_path")
elseif (UNIX)
	set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib:$ORIGIN")
endif()

set(CURL_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/curl")
set(BORINGSSL_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/boringssl")
set(NGTCP2_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/ngtcp2")
set(NGHTTP2_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/nghttp2")
set(NGHTTP3_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/nghttp3")
set(ZLIB_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/zlib")
set(BROTLI_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/brotli")
set(CARES_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/c-ares")
set(CURL_IMPERSONATE_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/curl-impersonate")
set(PICOHTTPPARSER_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/picohttpparser")
set(ZSTD_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/zstd")

set(
	INCLUDE_DIRECTORIES
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
	"${BORINGSSL_SOURCE_DIRECTORY}/include"
	"${PICOHTTPPARSER_SOURCE_DIRECTORY}"
)

set(
	KAD_SOURCE_FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/buffer.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/errors.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/systemerror.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/ssl.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/callbacks.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/http.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/cleanup.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/main.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/argparse.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/threads.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/logging.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/fstream.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/getexec.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/parentpath.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/basename.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/realpath.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/absrel.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/hex.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/strsplit.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/strsub.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/urlencode.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/urldecode.c"
	"${PICOHTTPPARSER_SOURCE_DIRECTORY}/picohttpparser.c"
)

set(
	KAD_JNI_SOURCE_FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/kad-jni.c"
)


if (KAD_STATIC)
	set(
		TARGETS
		kad
		libcurl_static
		zlibstatic
		crypto
		ssl
		ngtcp2_static
		nghttp2_static
		nghttp3_static
		libzstd_static
		brotlidec
		c-ares
	)
	
	set(
		CURL_LIBRARY_DEPENDENCIES
		zlibstatic
		crypto
		ssl
		nghttp2_static
		ngtcp2_static
		ngtcp2_crypto_boringssl_static
		nghttp3_static
		brotlidec
		libzstd_static
		c-ares
	)
else()
	set(
		TARGETS
		kad
		libcurl_shared
		zlib
		crypto
		ssl
		ngtcp2
		nghttp2
		nghttp3
		brotlicommon
		libzstd_shared
		brotlidec
		c-ares
	)
	
	set(
		CURL_LIBRARY_DEPENDENCIES
		zlib
		crypto
		ssl
		nghttp2
		ngtcp2
		ngtcp2_crypto_boringssl_static
		nghttp3
		brotlidec
		libzstd_shared
		c-ares
	)
endif()

# curl
set(PICKY_COMPILER OFF)
set(BUILD_CURL_EXE OFF)
set(BUILD_EXAMPLES OFF)
set(HTTP_ONLY ON)
set(ENABLE_THREADED_RESOLVER OFF)
set(CURL_USE_OPENSSL ON)
set(USE_NGHTTP2 ON)
set(USE_NGTCP2 ON)
set(USE_ECH ON)
set(CURL_BROTLI ON)
set(ENABLE_ARES ON)
set(CURL_USE_LIBPSL OFF)
set(CURL_DISABLE_CRYPTO_AUTH ON)
set(CURL_DISABLE_HSTS ON)
set(CURL_DISABLE_HTTP_AUTH ON)
set(CURL_DISABLE_NETRC ON)
set(USE_LIBIDN2 OFF)
set(ENABLE_UNIX_SOCKETS OFF)
set(CURL_USE_LIBSSH2 OFF)
set(CURL_CA_BUNDLE "none")
set(CURL_CA_PATH "none")
set(CURL_WERROR OFF)

set(CURL_ZSTD ON)

if (WIN32)
	set(ENABLE_UNICODE ON)
endif()

# nghttp2
set(ENABLE_LIB_ONLY ON CACHE BOOL "")

# zstd
set(ZSTD_BUILD_PROGRAMS OFF)
set(ZSTD_BUILD_TESTS OFF)

if (KAD_STATIC)
	set(ZSTD_BUILD_STATIC ON)
	set(ZSTD_BUILD_SHARED OFF)
else()
	set(ZSTD_BUILD_STATIC OFF)
	set(ZSTD_BUILD_SHARED ON)
endif()

# BoringSSL
if (KAD_STATIC)
	set(BUILD_SHARED_LIBS OFF)
	set(BUILD_STATIC_LIBS ON)
	set(ZLIB_BUILD_SHARED OFF)
	set(ZLIB_BUILD_STATIC ON)
	set(ENABLE_SHARED_LIB OFF)
	set(CARES_SHARED OFF)
	set(CARES_STATIC ON)
	set(CARES_STATIC_PIC ON)
else()
	set(BUILD_SHARED_LIBS ON)
endif()

# zlib
set(ZLIB_BUILD_TESTING OFF)

set(BUILD_TESTING OFF)

file(LOCK "${CMAKE_CURRENT_SOURCE_DIR}/lock")

execute_process(
	COMMAND git submodule update --init --depth=1
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	COMMAND_ERROR_IS_FATAL ANY
)

execute_process(
	COMMAND git submodule update --init --depth=1 --recursive
		submodules/ngtcp2
		submodules/nghttp3
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	COMMAND_ERROR_IS_FATAL ANY
)

file(READ "${ZLIB_SOURCE_DIRECTORY}/CMakeLists.txt" FILE_CONTENTS)
string(REPLACE "(UNIX)" "(1)" FILE_CONTENTS "${FILE_CONTENTS}")
string(REPLACE "(NOT APPLE)" "(0)" FILE_CONTENTS "${FILE_CONTENTS}")
file(WRITE "${ZLIB_SOURCE_DIRECTORY}/CMakeLists.txt" "${FILE_CONTENTS}")

file(READ "${NGTCP2_SOURCE_DIRECTORY}/CMakeLists.txt" FILE_CONTENTS)
string(REPLACE "(check" "(check_v1" FILE_CONTENTS "${FILE_CONTENTS}")
file(WRITE "${NGTCP2_SOURCE_DIRECTORY}/CMakeLists.txt" "${FILE_CONTENTS}")

file(READ "${NGHTTP3_SOURCE_DIRECTORY}/CMakeLists.txt" FILE_CONTENTS)
string(REPLACE "(check" "(check_v2" FILE_CONTENTS "${FILE_CONTENTS}")
file(WRITE "${NGHTTP3_SOURCE_DIRECTORY}/CMakeLists.txt" "${FILE_CONTENTS}")

if (NOT EXISTS "${CURL_SOURCE_DIRECTORY}/.patched")
	message("-- Patching cURL")

	execute_process(
		COMMAND patch
		--directory=${CURL_SOURCE_DIRECTORY}
		--strip=1
		--input=${CURL_IMPERSONATE_SOURCE_DIRECTORY}/patches/curl.patch
		COMMAND_ERROR_IS_FATAL ANY
	)

	file(WRITE "${CURL_SOURCE_DIRECTORY}/.patched" "")
endif()

if (NOT EXISTS "${CURL_SOURCE_DIRECTORY}/.android-patched")
	message("-- Patching cURL for Android")

	execute_process(
		COMMAND patch
		--directory=${CURL_SOURCE_DIRECTORY}
		--strip=1
		--input=${CMAKE_CURRENT_SOURCE_DIR}/patches/android.patch
		COMMAND_ERROR_IS_FATAL ANY
	)

	file(WRITE "${CURL_SOURCE_DIRECTORY}/.android-patched" "")
endif()

if (NOT EXISTS "${BORINGSSL_SOURCE_DIRECTORY}/.patched")
	message("-- Patching BoringSSL")

	execute_process(
		COMMAND patch
		--directory=${BORINGSSL_SOURCE_DIRECTORY}
		--strip=1
		--input=${CURL_IMPERSONATE_SOURCE_DIRECTORY}/patches/boringssl.patch
		COMMAND_ERROR_IS_FATAL ANY
	)

	file(WRITE "${BORINGSSL_SOURCE_DIRECTORY}/.patched" "")
endif()

if (NOT EXISTS "${NGTCP2_SOURCE_DIRECTORY}/.patched")
	message("-- Patching ngtcp2")

	execute_process(
		COMMAND patch
		--directory=${NGTCP2_SOURCE_DIRECTORY}
		--strip=1
		--input=${CURL_IMPERSONATE_SOURCE_DIRECTORY}/patches/ngtcp2.patch
		COMMAND_ERROR_IS_FATAL ANY
	)
	
	file(WRITE "${NGTCP2_SOURCE_DIRECTORY}/.patched" "")
endif()

if (NOT EXISTS "${NGHTTP3_SOURCE_DIRECTORY}/.patched")
	message("-- Patching nghttp3")

	execute_process(
		COMMAND patch
		--directory=${NGHTTP3_SOURCE_DIRECTORY}
		--strip=1
		--input=${CURL_IMPERSONATE_SOURCE_DIRECTORY}/patches/nghttp3.patch
		COMMAND_ERROR_IS_FATAL ANY
	)
	
	file(WRITE "${NGHTTP3_SOURCE_DIRECTORY}/.patched" "")
endif()

file(LOCK "${CMAKE_CURRENT_SOURCE_DIR}/lock" RELEASE)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
	add_compile_options($<$<CONFIG:Release>:-fipa-pta>)
endif()

set(OPENSSL_INCLUDE_DIR "${BORINGSSL_SOURCE_DIRECTORY}/include")

set(OPENSSL_CRYPTO_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${KAD_LIBRARY_PREFIX}crypto${KAD_LIBRARY_SUFFIX}")
set(OPENSSL_SSL_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${KAD_LIBRARY_PREFIX}ssl${KAD_LIBRARY_SUFFIX}")

file(WRITE "${OPENSSL_SSL_LIBRARY}" "")
file(WRITE "${OPENSSL_CRYPTO_LIBRARY}" "")

set(
	NGHTTP2_INCLUDE_DIR
	"${NGHTTP2_SOURCE_DIRECTORY}/lib/includes"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/nghttp2/lib/includes"
)

set(NGHTTP2_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${KAD_LIBRARY_PREFIX}nghttp2${KAD_LIBRARY_SUFFIX}")

set(
	ZLIB_INCLUDE_DIR
	"${ZLIB_SOURCE_DIRECTORY}"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/zlib"
)

set(ZLIB_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${KAD_LIBRARY_PREFIX}z${KAD_LIBRARY_SUFFIX}")

set(BROTLI_INCLUDE_DIR "${BROTLI_SOURCE_DIRECTORY}/c/include")

set(BROTLICOMMON_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${KAD_LIBRARY_PREFIX}brotlicommon${KAD_LIBRARY_SUFFIX}")
set(BROTLIDEC_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${KAD_LIBRARY_PREFIX}brotlidec${KAD_LIBRARY_SUFFIX}")

set(
	ZSTD_INCLUDE_DIR
	"${ZSTD_SOURCE_DIRECTORY}/lib"
)

set(ZSTD_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${KAD_LIBRARY_PREFIX}zstd${KAD_LIBRARY_SUFFIX}")

set(
	CARES_INCLUDE_DIR
	"${CARES_SOURCE_DIRECTORY}/include"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/c-ares"
)

set(CARES_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${KAD_LIBRARY_PREFIX}cares${KAD_LIBRARY_SUFFIX}")

set(ENABLE_OPENSSL OFF)
set(ENABLE_GNUTLS OFF)
set(ENABLE_WOLFSSL OFF)
set(ENABLE_PICOTLS OFF)
set(ENABLE_BORINGSSL ON)
set(ENABLE_STATIC_LIB ON)

set(HAVE_BORINGSSL TRUE CACHE INTERNAL "" FORCE)
set(HAVE_SSL_SET_QUIC_TLS_CBS TRUE CACHE INTERNAL "" FORCE)
set(HAVE_SSL_SET1_ECH_CONFIG_LIST TRUE CACHE INTERNAL "" FORCE)
set(BORINGSSL_INCLUDE_DIR "${BORINGSSL_SOURCE_DIRECTORY}/include")

set(BORINGSSL_LIBRARIES "${OPENSSL_SSL_LIBRARY};${OPENSSL_CRYPTO_LIBRARY}")

set(
	NGTCP2_INCLUDE_DIR
	"${NGTCP2_SOURCE_DIRECTORY}/lib/includes"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/ngtcp2/lib/includes"
	"${NGTCP2_SOURCE_DIRECTORY}/crypto/includes"
)

set(NGTCP2_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${KAD_LIBRARY_PREFIX}ngtcp2${KAD_LIBRARY_SUFFIX}")

set(NGTCP2_CRYPTO_BORINGSSL_LIBRARY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${CMAKE_STATIC_LIBRARY_PREFIX}ngtcp2_crypto_boringssl${CMAKE_STATIC_LIBRARY_SUFFIX}")

set(
	NGHTTP3_INCLUDE_DIR
	"${NGHTTP3_SOURCE_DIRECTORY}/lib/includes"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/nghttp3/lib/includes"
)

set(NGHTTP3_LIBRARY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${KAD_LIBRARY_PREFIX}nghttp3${KAD_LIBRARY_SUFFIX}")

file(WRITE "${NGTCP2_LIBRARY}" "")
file(WRITE "${NGTCP2_CRYPTO_BORINGSSL_LIBRARY}" "")
file(WRITE "${NGHTTP3_LIBRARY}" "")
file(WRITE "${ZSTD_LIBRARY}" "")

file(MAKE_DIRECTORY
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/zlib"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/nghttp2/lib/includes"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/c-ares"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/nghttp3/lib/includes"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/ngtcp2/lib/includes"
)

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
	"${NGTCP2_SOURCE_DIRECTORY}"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/ngtcp2"
	EXCLUDE_FROM_ALL
)

add_subdirectory(
	"${NGHTTP3_SOURCE_DIRECTORY}"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/nghttp3"
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

add_subdirectory(
	"${ZSTD_SOURCE_DIRECTORY}/build/cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/zstd"
	EXCLUDE_FROM_ALL
)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "^i[3-6]86$")
	target_compile_options(
		crypto
		PRIVATE
		-msse2
	)
endif()

file(REMOVE "${OPENSSL_SSL_LIBRARY}")
file(REMOVE "${OPENSSL_CRYPTO_LIBRARY}")

file(REMOVE "${NGTCP2_LIBRARY}")
file(REMOVE "${NGTCP2_CRYPTO_BORINGSSL_LIBRARY}")

file(REMOVE "${NGHTTP3_LIBRARY}")

file(REMOVE "${ZSTD_LIBRARY}")

foreach(target ${CURL_LIBRARY_DEPENDENCIES})
	add_custom_command(
		OUTPUT ${target}
		COMMAND ${CMAKE_COMMAND} --build ./ --target ${target}
	)

	add_custom_target(
		"ensure_${target}" ALL DEPENDS ${target}
	)

	add_dependencies(
		${KAD_CURL_LIBRARY}
		"ensure_${target}"
	)
endforeach()

include_directories(${INCLUDE_DIRECTORIES})

if (KAD_LIBRARY)
	add_library(
		kad
		SHARED
		${KAD_SOURCE_FILES}
	)
	
	target_compile_definitions(
		kad
		PRIVATE
		KAD_LIBRARY
	)
	
	add_library(
		kad-jni
		SHARED
		${KAD_JNI_SOURCE_FILES}
	)
	
	target_link_libraries(
		kad-jni
		PRIVATE
		kad
	)
	
	set_target_properties(
		kad-jni
		PROPERTIES
		BUILD_RPATH "$ORIGIN"
	)
	
	list(APPEND TARGETS kad-jni)
	
	if (NOT ANDROID AND DEFINED ENV{JAVA_HOME})
		set(KAD_JNI_PLATFORM "linux")
		
		if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
			set(KAD_JNI_PLATFORM "darwin")
		elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
			set(KAD_JNI_PLATFORM "win32")
		endif()
		
		target_include_directories(
			kad-jni
			PRIVATE
			"$ENV{JAVA_HOME}/include"
			"$ENV{JAVA_HOME}/include/${KAD_JNI_PLATFORM}"
		)
	endif()
else()
	add_executable(
		kad
		${KAD_SOURCE_FILES}
	)
endif()

set_target_properties(
	kad
	PROPERTIES
	LINKER_LANGUAGE CXX
)

if (KAD_STATIC)
	target_link_options(
		kad
		PRIVATE
		-static-libstdc++ -static-libgcc
	)
endif()


if (NOT KAD_SSL_VERIFY)
	target_compile_definitions(
		kad
		PRIVATE
		KAD_DISABLE_SSL_VERIFY
	)
endif()

if (WIN32 AND KAD_UNICODE)
	target_sources(
		kad
		PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src/wio.c"
	)
	
	target_compile_definitions(
		kad
		PRIVATE
		UNICODE _UNICODE
	)
	
	target_link_options(
		kad
		BEFORE
		PRIVATE
		-municode
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

if (KAD_ASAN)
	target_compile_options(
		kad
		PRIVATE
		-fsanitize=address
		-fno-omit-frame-pointer
		-g
	)
	
	target_link_options(
		kad
		PRIVATE
		-fsanitize=address
	)
endif()

if (KAD_PROFILE)
	target_compile_options(
		kad
		PRIVATE
		-pg
		-O2
		-fno-omit-frame-pointer
	)
	
	target_link_options(
		kad
		PRIVATE
		-pg
	)
	
	if (NOT KAD_LIBRARY)
		target_link_options(
			kad
			PRIVATE
			-no-pie
		)
	endif()
endif()

target_compile_definitions(
	kad
	PRIVATE
	_GNU_SOURCE
)

find_package(Threads REQUIRED)

target_link_libraries(
	kad
	PRIVATE
	Threads::Threads
	ssl
	crypto
	${KAD_CURL_LIBRARY}
)

if (KAD_STATIC)
	install(
		TARGETS kad
		RUNTIME DESTINATION bin
	)
else()
	install(
		TARGETS ${TARGETS}
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
	)
endif()

install(
	FILES "${CMAKE_CURRENT_SOURCE_DIR}/submodules/cabundle/pem/cert.pem"
	DESTINATION etc/tls
	RENAME cert.pem
)
