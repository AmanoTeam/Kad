cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(
	Kad
	VERSION 0.2
	DESCRIPTION "Kad is a simple HTTP proxy server that forwards all requests through curl-impersonate"
	HOMEPAGE_URL "https://github.com/AmanoTeam/Kad"
	LANGUAGES C
)

option(KAD_LTO "Turn on compiler link-time optimizations" OFF)
option(KAD_UNICODE "Enable Unicode support for Windows builds" ON)
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
set(NGHTTP2_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/nghttp2")
set(ZLIB_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/zlib")
set(BROTLI_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/brotli")
set(CARES_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/c-ares")
set(CURL_IMPERSONATE_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/curl-impersonate")
set(PICOHTTPPARSER_SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/submodules/picohttpparser")

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
	TARGETS
	kad
	libcurl_shared
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
set(CURL_USE_OPENSSL ON)
set(USE_NGHTTP2 ON)
set(CURL_BROTLI ON)
set(ENABLE_ARES ON)
set(CURL_USE_LIBPSL OFF)
set(CURL_DISABLE_ALTSVC ON)
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
		--input=${CURL_IMPERSONATE_SOURCE_DIRECTORY}/patches/curl.patch
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
		--input=${CURL_IMPERSONATE_SOURCE_DIRECTORY}/patches/boringssl.patch
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

file(MAKE_DIRECTORY
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/zlib"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/nghttp2/lib/includes"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/c-ares"
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
		libcurl_shared
		"ensure_${target}"
	)
endforeach()

include_directories(${INCLUDE_DIRECTORIES})

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
endif()

# Enable Unicode on Windows builds
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

set_target_properties(
	kad
	PROPERTIES
	BUILD_RPATH "${EXEC_RPATH}"
	INSTALL_RPATH "${INSTALL_RPATH}"
	C_STANDARD 99
	C_STANDARD_REQUIRED ON
)

target_compile_definitions(
	kad
	PRIVATE
	_GNU_SOURCE
)

set_target_properties(
	libcurl_shared ssl brotlidec
	PROPERTIES
	BUILD_RPATH "${LIB_RPATH}"
	INSTALL_RPATH "${LIB_RPATH}"
)

target_link_libraries(
	kad
	ssl
	crypto
	libcurl_shared
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
