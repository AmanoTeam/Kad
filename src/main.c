#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#if defined(_WIN32)
	#include <winsock2.h>
	#include <ws2tcpip.h>
#endif

#if !defined(_WIN32)
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <netdb.h>
#endif

#if defined(_WIN32) && defined(_UNICODE)
	#include "wio.h"
#endif

#include <curl/curl.h>

#include "http.h"
#include "ssl.h"
#include "cleanup.h"
#include "constants.h"
#include "callbacks.h"
#include "transferdata.h"

#if defined(KAD_CAPTURE)
	#include "capture.h"
#endif
#include "errors.h"
#include "systemerror.h"
#include "resources.h"
#include "argparse.h"
#include "kad.h"
#include "threads.h"
#include "logging.h"
#include "socketclose.h"

#if !defined(KAD_DISABLE_SSL_VERIFY)
	#include "fstream.h"
	#include "getexec.h"
	#include "sep.h"
#endif

#define KAD_DEFAULT_IMPERSONATE_TARGET "chrome116"

static const char KAD_DEFAULT_LISTEN_ADDRESS[] = "127.0.0.1";

#define KAD_DEFAULT_LISTEN_PORT (4000)
#define KAD_DEFAULT_LISTEN_BACKLOG (100)

static const char* const IMPERSONATE_HEADERS[] = {
	"Connection",
	"Upgrade",
	"HTTP2-Settings",
	"Sec-CH-UA",
	"Sec-CH-UA-Mobile",
	"Sec-CH-UA-Platform",
	"Sec-CH-UA-Arch",
	"Sec-CH-UA-Bitness",
	"Sec-CH-UA-Full-Version",
	"Sec-CH-UA-Full-Version-List",
	"Sec-CH-UA-Model",
	"Sec-CH-UA-Platform-Version",
	"UA",
	"UA-Arch",
	"UA-Bitness",
	"UA-Full-Version",
	"UA-Mobile",
	"UA-Model",
	"UA-Platform",
	"UA-Platform-Version",
	"Upgrade-Insecure-Requests",
	"Accept",
	"User-Agent",
	"X-Requested-With",
	"Sec-Fetch-Site",
	"Sec-Fetch-Mode",
	"Sec-Fetch-User",
	"Sec-Fetch-Dest",
	"Accept-Encoding",
	"Accept-Language"
};

#if !defined(KAD_DISABLE_SSL_VERIFY)
	static const char CA_CERT_FILENAME[] =
		PATHSEP_M
		"etc"
		PATHSEP_M
		"tls"
		PATHSEP_M
		"cert.pem";
	
	static char* ca_bundle_global = NULL;
#endif

static int fd = 0;

static CURLM* curl_multi = NULL;

/* the TLS and HTTP/2 fingerprints of Chrome only depend on its major version,
   so every supported combination of --target=chrome<version> and --platform is
   impersonated through the curl-impersonate target that matches the version
   the closest, while the HTTP headers, which do change per version and
   platform, are set here */

#define KAD_CHROME_ANDROID_TARGET "chrome131_android"

#define KAD_CHROME_MIN_VERSION (99)

#define KAD_CHROME_MAX_VERSION (157)

#define KAD_CHROME_MIN_ANDROID_VERSION (132)

#define KAD_CHROME_ANDROID_MIN_LEGACY_VERSION (99)

#define KAD_CHROME_ANDROID_MAX_LEGACY_VERSION (120)

/* real Chrome for Android used device specific user agents until the user agent
   reduction rolled out on Chrome 121, and older versions only ever shipped on
   a small set of devices. Use a fixed device token so the header matches a real
   WebView user agent of that era. */

static const char CHROME_ANDROID_LEGACY_UA_FORMAT[] =
	"User-Agent: Mozilla/5.0 (Linux; Android 17; Pixel 10 Pro Build/CP3A.261005.005; wv) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Chrome/%s Mobile Safari/537.36";

#define KAD_CHROME_PLATFORM_WINDOWS (0)
#define KAD_CHROME_PLATFORM_MACOS (1)
#define KAD_CHROME_PLATFORM_LINUX (2)
#define KAD_CHROME_PLATFORM_ANDROID (3)

#define KAD_BROWSER_CHROME (0)

#define KAD_BROWSER_EDGE (1)

#define KAD_BROWSER_FIREFOX (2)

struct chrome_platform {
	const char* name;
	const char* user_agent_format;
	const char* sec_ch_ua_mobile;
	const char* sec_ch_ua_platform;
};

static const struct chrome_platform CHROME_PLATFORMS[] = {
	{
		"windows",
		"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/%s Safari/537.36",
		"sec-ch-ua-mobile: ?0",
		"sec-ch-ua-platform: \"Windows\""
	},
	{
		"macos",
		"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/%s Safari/537.36",
		"sec-ch-ua-mobile: ?0",
		"sec-ch-ua-platform: \"macOS\""
	},
	{
		"linux",
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/%s Safari/537.36",
		"sec-ch-ua-mobile: ?0",
		"sec-ch-ua-platform: \"Linux\""
	},
	{
		"android",
		"User-Agent: Mozilla/5.0 (Linux; Android 10; K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/%s Mobile Safari/537.36",
		"sec-ch-ua-mobile: ?1",
		"sec-ch-ua-platform: \"Android\""
	}
};

/* the target providing the TLS and HTTP/2 fingerprints; versions without
   their own target fall back to the previous one */

static const char* const CHROME_BASE_TARGETS[] = {
	"chrome99",
	"chrome100",
	"chrome101", "chrome101", "chrome101",
	"chrome104", "chrome104", "chrome104",
	"chrome107", "chrome107", "chrome107",
	"chrome110", "chrome110", "chrome110", "chrome110", "chrome110", "chrome110",
	"chrome116", "chrome116", "chrome116",
	"chrome119",
	"chrome120", "chrome120", "chrome120",
	"chrome123",
	"chrome124", "chrome124", "chrome124", "chrome124", "chrome124", "chrome124", "chrome124",
	"chrome131",
	"chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android"
};

/* the sec-ch-ua client hint, whose brands rotate every milestone */

static const char* const CHROME_SEC_CH_UA[] = {
	"sec-ch-ua: \" Not A;Brand\";v=\"99\", \"Chromium\";v=\"99\", \"Google Chrome\";v=\"99\"",
	"sec-ch-ua: \" Not A;Brand\";v=\"99\", \"Chromium\";v=\"100\", \"Google Chrome\";v=\"100\"",
	"sec-ch-ua: \" Not A;Brand\";v=\"99\", \"Chromium\";v=\"101\", \"Google Chrome\";v=\"101\"",
	"sec-ch-ua: \" Not A;Brand\";v=\"99\", \"Chromium\";v=\"102\", \"Google Chrome\";v=\"102\"",
	"sec-ch-ua: \" Not A;Brand\";v=\"99\", \"Chromium\";v=\"103\", \"Google Chrome\";v=\"103\"",
	"sec-ch-ua: \"Chromium\";v=\"104\", \" Not A;Brand\";v=\"99\", \"Google Chrome\";v=\"104\"",
	"sec-ch-ua: \"Chromium\";v=\"105\", \" Not A;Brand\";v=\"99\", \"Google Chrome\";v=\"105\"",
	"sec-ch-ua: \"Chromium\";v=\"106\", \" Not A;Brand\";v=\"99\", \"Google Chrome\";v=\"106\"",
	"sec-ch-ua: \"Google Chrome\";v=\"107\", \"Chromium\";v=\"107\", \"Not=A?Brand\";v=\"24\"",
	"sec-ch-ua: \"Google Chrome\";v=\"108\", \"Chromium\";v=\"108\", \"Not=A?Brand\";v=\"24\"",
	"sec-ch-ua: \"Google Chrome\";v=\"109\", \"Chromium\";v=\"109\", \"Not=A?Brand\";v=\"24\"",
	"sec-ch-ua: \"Chromium\";v=\"110\", \"Not A(Brand\";v=\"24\", \"Google Chrome\";v=\"110\"",
	"sec-ch-ua: \"Chromium\";v=\"111\", \"Not A(Brand\";v=\"24\", \"Google Chrome\";v=\"111\"",
	"sec-ch-ua: \"Chromium\";v=\"112\", \"Not A(Brand\";v=\"24\", \"Google Chrome\";v=\"112\"",
	"sec-ch-ua: \"Chromium\";v=\"113\", \"Not A(Brand\";v=\"24\", \"Google Chrome\";v=\"113\"",
	"sec-ch-ua: \"Chromium\";v=\"114\", \"Not A(Brand\";v=\"24\", \"Google Chrome\";v=\"114\"",
	"sec-ch-ua: \"Chromium\";v=\"115\", \"Not A(Brand\";v=\"24\", \"Google Chrome\";v=\"115\"",
	"sec-ch-ua: \"Chromium\";v=\"116\", \"Not)A;Brand\";v=\"24\", \"Google Chrome\";v=\"116\"",
	"sec-ch-ua: \"Chromium\";v=\"117\", \"Not)A;Brand\";v=\"24\", \"Google Chrome\";v=\"117\"",
	"sec-ch-ua: \"Chromium\";v=\"118\", \"Not)A;Brand\";v=\"24\", \"Google Chrome\";v=\"118\"",
	"sec-ch-ua: \"Google Chrome\";v=\"119\", \"Chromium\";v=\"119\", \"Not?A_Brand\";v=\"24\"",
	"sec-ch-ua: \"Not_A Brand\";v=\"8\", \"Chromium\";v=\"120\", \"Google Chrome\";v=\"120\"",
	"sec-ch-ua: \"Not_A Brand\";v=\"8\", \"Chromium\";v=\"121\", \"Google Chrome\";v=\"121\"",
	"sec-ch-ua: \"Not_A Brand\";v=\"8\", \"Chromium\";v=\"122\", \"Google Chrome\";v=\"122\"",
	"sec-ch-ua: \"Google Chrome\";v=\"123\", \"Not:A-Brand\";v=\"8\", \"Chromium\";v=\"123\"",
	"sec-ch-ua: \"Chromium\";v=\"124\", \"Google Chrome\";v=\"124\", \"Not-A.Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"125\", \"Google Chrome\";v=\"125\", \"Not-A.Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"126\", \"Google Chrome\";v=\"126\", \"Not-A.Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"127\", \"Google Chrome\";v=\"127\", \"Not-A.Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"128\", \"Google Chrome\";v=\"128\", \"Not-A.Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"129\", \"Google Chrome\";v=\"129\", \"Not-A.Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"130\", \"Google Chrome\";v=\"130\", \"Not-A.Brand\";v=\"99\"",
	"sec-ch-ua: \"Google Chrome\";v=\"131\", \"Chromium\";v=\"131\", \"Not_A Brand\";v=\"24\"",
	"sec-ch-ua: \"Chromium\";v=\"132\", \"Not_A Brand\";v=\"24\", \"Google Chrome\";v=\"132\"",
	"sec-ch-ua: \"Not(A:Brand\";v=\"99\", \"Google Chrome\";v=\"133\", \"Chromium\";v=\"133\"",
	"sec-ch-ua: \"Not(A:Brand\";v=\"99\", \"Google Chrome\";v=\"134\", \"Chromium\";v=\"134\"",
	"sec-ch-ua: \"Not(A:Brand\";v=\"99\", \"Google Chrome\";v=\"135\", \"Chromium\";v=\"135\"",
	"sec-ch-ua: \"Chromium\";v=\"136\", \"Google Chrome\";v=\"136\", \"Not.A/Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"137\", \"Google Chrome\";v=\"137\", \"Not.A/Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"138\", \"Google Chrome\";v=\"138\", \"Not_A Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"139\", \"Google Chrome\";v=\"139\", \"Not_A Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"140\", \"Google Chrome\";v=\"140\", \"Not_A Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"141\", \"Google Chrome\";v=\"141\", \"Not_A Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"142\", \"Google Chrome\";v=\"142\", \"Not_A Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"143\", \"Google Chrome\";v=\"143\", \"Not_A Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"144\", \"Google Chrome\";v=\"144\", \"Not_A Brand\";v=\"99\"",
	"sec-ch-ua: \"Not:A-Brand\";v=\"99\", \"Google Chrome\";v=\"145\", \"Chromium\";v=\"145\"",
	"sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Google Chrome\";v=\"146\"",
	"sec-ch-ua: \"Chromium\";v=\"147\", \"Not-A.Brand\";v=\"24\", \"Google Chrome\";v=\"147\"",
	"sec-ch-ua: \"Chromium\";v=\"148\", \"Not-A.Brand\";v=\"24\", \"Google Chrome\";v=\"148\"",
	"sec-ch-ua: \"Chromium\";v=\"149\", \"Not-A.Brand\";v=\"24\", \"Google Chrome\";v=\"149\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"150\", \"Google Chrome\";v=\"150\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"151\", \"Google Chrome\";v=\"151\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"152\", \"Google Chrome\";v=\"152\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"153\", \"Google Chrome\";v=\"153\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"154\", \"Google Chrome\";v=\"154\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"155\", \"Google Chrome\";v=\"155\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"156\", \"Google Chrome\";v=\"156\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"157\", \"Google Chrome\";v=\"157\""
};

/* Chrome reported its full version in the user agent until Chrome 104, where
   the version reduction to the major only started */

static const char* const CHROME_FULL_UA_VERSIONS[] = {
	"99.0.4844.51",
	"100.0.4896.75",
	"101.0.4951.67",
	"101.0.4951.67",
	"101.0.4951.67"
};

/* the real Android patch versions of the last release of each milestone, as
   published by Google; the user agents of the legacy android platform (99 to
   120) report these so the header matches a real device of that era */

static const char* const CHROME_ANDROID_FULL_UA_VERSIONS[] = {
	"99.0.4844.88",
	"100.0.4896.127",
	"101.0.4951.74",
	"102.0.5005.125",
	"103.0.5060.129",
	"104.0.5112.97",
	"105.0.5195.136",
	"106.0.5249.126",
	"107.0.5304.141",
	"108.0.5359.128",
	"109.0.5414.118",
	"110.0.5481.154",
	"111.0.5563.116",
	"112.0.5615.136",
	"113.0.5672.163",
	"114.0.5735.227",
	"115.0.5790.168",
	"116.0.5845.173",
	"117.0.5938.156",
	"118.0.5993.112",
	"119.0.6045.194",
	"120.0.6099.231"
};

/* the remaining headers only changed three times: the signed-exchange quality
   factor was lowered on Chrome 110, zstd landed on Chrome 123 and the
   Priority header on Chrome 124 */

static const char* const CHROME_HEADERS_LEGACY[] = {
	"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9",
	"Sec-Fetch-Site: none",
	"Sec-Fetch-Mode: navigate",
	"Sec-Fetch-User: ?1",
	"Sec-Fetch-Dest: document",
	"Accept-Encoding: gzip, deflate, br",
	"Accept-Language: en-US,en;q=0.9"
};

static const char* const CHROME_HEADERS_PRE_ZSTD[] = {
	"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7",
	"Sec-Fetch-Site: none",
	"Sec-Fetch-Mode: navigate",
	"Sec-Fetch-User: ?1",
	"Sec-Fetch-Dest: document",
	"Accept-Encoding: gzip, deflate, br",
	"Accept-Language: en-US,en;q=0.9"
};

static const char* const CHROME_HEADERS_ZSTD[] = {
	"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7",
	"Sec-Fetch-Site: none",
	"Sec-Fetch-Mode: navigate",
	"Sec-Fetch-User: ?1",
	"Sec-Fetch-Dest: document",
	"Accept-Encoding: gzip, deflate, br, zstd",
	"Accept-Language: en-US,en;q=0.9"
};

static const char* const CHROME_HEADERS_MODERN[] = {
	"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7",
	"Sec-Fetch-Site: none",
	"Sec-Fetch-Mode: navigate",
	"Sec-Fetch-User: ?1",
	"Sec-Fetch-Dest: document",
	"Accept-Encoding: gzip, deflate, br, zstd",
	"Accept-Language: en-US,en;q=0.9",
	"Priority: u=0, i"
};


/* Edge shares the TLS and HTTP/2 fingerprints of the Chromium major of the same
   version, and its HTTP headers differ from the ones of Chrome only in the user
   agent, which is followed by the Edge version, and in the sec-ch-ua client
   hint, where "Microsoft Edge" replaces "Google Chrome" */

#define KAD_EDGE_MIN_VERSION (96)

#define KAD_EDGE_MAX_VERSION (157)

static const char* const EDGE_BASE_TARGETS[] = {
		"chrome99",
	"chrome99",
	"chrome99",
	"chrome99",
	"chrome100",
	"chrome101",
	"chrome101",
	"chrome101",
	"chrome104",
	"chrome104",
	"chrome104",
	"chrome107",
	"chrome107",
	"chrome107",
	"chrome110",
	"chrome110",
	"chrome110",
	"chrome110",
	"chrome110",
	"chrome110",
	"chrome116",
	"chrome116",
	"chrome116",
	"chrome119",
	"chrome120",
	"chrome120",
	"chrome120",
	"chrome123",
	"chrome124",
	"chrome124",
	"chrome124",
	"chrome124",
	"chrome124",
	"chrome124",
	"chrome124",
	"chrome131",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
	"chrome131_android",
};

static const char* const EDGE_SEC_CH_UA[] = {
	"sec-ch-ua: \" Not A;Brand\";v=\"99\", \"Chromium\";v=\"96\", \"Microsoft Edge\";v=\"96\"",
	"sec-ch-ua: \" Not A;Brand\";v=\"99\", \"Chromium\";v=\"97\", \"Microsoft Edge\";v=\"97\"",
	"sec-ch-ua: \" Not A;Brand\";v=\"99\", \"Chromium\";v=\"98\", \"Microsoft Edge\";v=\"98\"",
	"sec-ch-ua: \" Not A;Brand\";v=\"99\", \"Chromium\";v=\"99\", \"Microsoft Edge\";v=\"99\"",
	"sec-ch-ua: \" Not A;Brand\";v=\"99\", \"Chromium\";v=\"100\", \"Microsoft Edge\";v=\"100\"",
	"sec-ch-ua: \" Not A;Brand\";v=\"99\", \"Chromium\";v=\"101\", \"Microsoft Edge\";v=\"101\"",
	"sec-ch-ua: \" Not A;Brand\";v=\"99\", \"Chromium\";v=\"102\", \"Microsoft Edge\";v=\"102\"",
	"sec-ch-ua: \" Not A;Brand\";v=\"99\", \"Chromium\";v=\"103\", \"Microsoft Edge\";v=\"103\"",
	"sec-ch-ua: \"Chromium\";v=\"104\", \" Not A;Brand\";v=\"99\", \"Microsoft Edge\";v=\"104\"",
	"sec-ch-ua: \"Chromium\";v=\"105\", \" Not A;Brand\";v=\"99\", \"Microsoft Edge\";v=\"105\"",
	"sec-ch-ua: \"Chromium\";v=\"106\", \" Not A;Brand\";v=\"99\", \"Microsoft Edge\";v=\"106\"",
	"sec-ch-ua: \"Microsoft Edge\";v=\"107\", \"Chromium\";v=\"107\", \"Not=A?Brand\";v=\"24\"",
	"sec-ch-ua: \"Microsoft Edge\";v=\"108\", \"Chromium\";v=\"108\", \"Not=A?Brand\";v=\"24\"",
	"sec-ch-ua: \"Microsoft Edge\";v=\"109\", \"Chromium\";v=\"109\", \"Not=A?Brand\";v=\"24\"",
	"sec-ch-ua: \"Chromium\";v=\"110\", \"Not A(Brand\";v=\"24\", \"Microsoft Edge\";v=\"110\"",
	"sec-ch-ua: \"Chromium\";v=\"111\", \"Not A(Brand\";v=\"24\", \"Microsoft Edge\";v=\"111\"",
	"sec-ch-ua: \"Chromium\";v=\"112\", \"Not A(Brand\";v=\"24\", \"Microsoft Edge\";v=\"112\"",
	"sec-ch-ua: \"Chromium\";v=\"113\", \"Not A(Brand\";v=\"24\", \"Microsoft Edge\";v=\"113\"",
	"sec-ch-ua: \"Chromium\";v=\"114\", \"Not A(Brand\";v=\"24\", \"Microsoft Edge\";v=\"114\"",
	"sec-ch-ua: \"Chromium\";v=\"115\", \"Not A(Brand\";v=\"24\", \"Microsoft Edge\";v=\"115\"",
	"sec-ch-ua: \"Chromium\";v=\"116\", \"Not)A;Brand\";v=\"24\", \"Microsoft Edge\";v=\"116\"",
	"sec-ch-ua: \"Chromium\";v=\"117\", \"Not)A;Brand\";v=\"24\", \"Microsoft Edge\";v=\"117\"",
	"sec-ch-ua: \"Chromium\";v=\"118\", \"Not)A;Brand\";v=\"24\", \"Microsoft Edge\";v=\"118\"",
	"sec-ch-ua: \"Microsoft Edge\";v=\"119\", \"Chromium\";v=\"119\", \"Not?A_Brand\";v=\"24\"",
	"sec-ch-ua: \"Not_A Brand\";v=\"8\", \"Chromium\";v=\"120\", \"Microsoft Edge\";v=\"120\"",
	"sec-ch-ua: \"Not_A Brand\";v=\"8\", \"Chromium\";v=\"121\", \"Microsoft Edge\";v=\"121\"",
	"sec-ch-ua: \"Not_A Brand\";v=\"8\", \"Chromium\";v=\"122\", \"Microsoft Edge\";v=\"122\"",
	"sec-ch-ua: \"Microsoft Edge\";v=\"123\", \"Not:A-Brand\";v=\"8\", \"Chromium\";v=\"123\"",
	"sec-ch-ua: \"Chromium\";v=\"124\", \"Microsoft Edge\";v=\"124\", \"Not-A.Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"125\", \"Microsoft Edge\";v=\"125\", \"Not-A.Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"126\", \"Microsoft Edge\";v=\"126\", \"Not-A.Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"127\", \"Microsoft Edge\";v=\"127\", \"Not-A.Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"128\", \"Microsoft Edge\";v=\"128\", \"Not-A.Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"129\", \"Microsoft Edge\";v=\"129\", \"Not-A.Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"130\", \"Microsoft Edge\";v=\"130\", \"Not-A.Brand\";v=\"99\"",
	"sec-ch-ua: \"Microsoft Edge\";v=\"131\", \"Chromium\";v=\"131\", \"Not_A Brand\";v=\"24\"",
	"sec-ch-ua: \"Chromium\";v=\"132\", \"Not_A Brand\";v=\"24\", \"Microsoft Edge\";v=\"132\"",
	"sec-ch-ua: \"Not(A:Brand\";v=\"99\", \"Microsoft Edge\";v=\"133\", \"Chromium\";v=\"133\"",
	"sec-ch-ua: \"Not(A:Brand\";v=\"99\", \"Microsoft Edge\";v=\"134\", \"Chromium\";v=\"134\"",
	"sec-ch-ua: \"Not(A:Brand\";v=\"99\", \"Microsoft Edge\";v=\"135\", \"Chromium\";v=\"135\"",
	"sec-ch-ua: \"Chromium\";v=\"136\", \"Microsoft Edge\";v=\"136\", \"Not.A/Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"137\", \"Microsoft Edge\";v=\"137\", \"Not.A/Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"138\", \"Microsoft Edge\";v=\"138\", \"Not_A Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"139\", \"Microsoft Edge\";v=\"139\", \"Not_A Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"140\", \"Microsoft Edge\";v=\"140\", \"Not_A Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"141\", \"Microsoft Edge\";v=\"141\", \"Not_A Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"142\", \"Microsoft Edge\";v=\"142\", \"Not_A Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"143\", \"Microsoft Edge\";v=\"143\", \"Not_A Brand\";v=\"99\"",
	"sec-ch-ua: \"Chromium\";v=\"144\", \"Microsoft Edge\";v=\"144\", \"Not_A Brand\";v=\"99\"",
	"sec-ch-ua: \"Not:A-Brand\";v=\"99\", \"Microsoft Edge\";v=\"145\", \"Chromium\";v=\"145\"",
	"sec-ch-ua: \"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
	"sec-ch-ua: \"Chromium\";v=\"147\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"147\"",
	"sec-ch-ua: \"Chromium\";v=\"148\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"148\"",
	"sec-ch-ua: \"Chromium\";v=\"149\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"149\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"150\", \"Microsoft Edge\";v=\"150\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"151\", \"Microsoft Edge\";v=\"151\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"152\", \"Microsoft Edge\";v=\"152\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"153\", \"Microsoft Edge\";v=\"153\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"154\", \"Microsoft Edge\";v=\"154\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"155\", \"Microsoft Edge\";v=\"155\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"156\", \"Microsoft Edge\";v=\"156\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"157\", \"Microsoft Edge\";v=\"157\"",
};

/* the versions of the last release of every milestone shipped on Android, where
   Edge has its own versioning; the desktop ones are the same */

static const char* const EDGE_VERSIONS[] = {
		"96.0.1054.62",
	"97.0.1072.78",
	"98.0.1108.62",
	"99.0.1150.55",
	"100.0.1185.50",
	"101.0.1210.53",
	"102.0.1245.44",
	"103.0.1264.77",
	"104.0.1293.70",
	"105.0.1343.50",
	"106.0.1370.52",
	"107.0.1418.62",
	"108.0.1462.76",
	"109.0.1518.80",
	"110.0.1587.66",
	"111.0.1661.59",
	"112.0.1722.59",
	"113.0.1774.63",
	"114.0.1823.82",
	"115.0.1901.196",
	"116.0.1938.79",
	"117.0.2045.65",
	"118.0.2088.81",
	"119.0.2151.107",
	"120.0.2210.157",
	"121.0.2277.133",
	"122.0.2365.99",
	"123.0.2420.102",
	"124.0.2478.104",
	"125.0.2535.96",
	"126.0.2592.117",
	"127.0.2651.111",
	"128.0.2739.82",
	"129.0.2792.92",
	"130.0.2849.80",
	"131.0.2903.145",
	"132.0.2957.129",
	"133.0.3065.92",
	"134.0.3124.105",
	"135.0.3179.98",
	"136.0.3240.91",
	"137.0.3296.92",
	"138.0.3351.121",
	"139.0.3405.125",
	"140.0.3485.98",
	"141.0.3537.99",
	"142.0.3595.107",
	"143.0.3650.139",
	"144.0.3719.115",
	"145.0.3800.99",
	"146.0.3856.97",
	"147.0.3912.87",
	"148.0.3967.97",
	"149.0.4022.105",
	"150.0.4078.96",
	"151.0.4129.101",
	"152.0.4191.65",
	"153.0.4234.49",
	"154.0.4258.38",
	"155.0.0.0",
	"156.0.0.0",
	"157.0.0.0",
};

/* until the Chrome user agent reduction reached Android on Chrome 121, Edge for
   Android reported a device token like Chrome did, followed by its own version */

static const char EDGE_ANDROID_LEGACY_UA_FORMAT[] =
	"User-Agent: Mozilla/5.0 (Linux; Android 17; Pixel 10 Pro) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/%s Mobile Safari/537.36 EdgA/%s";

/* Firefox does not send client hints at all; the TLS and HTTP/2 fingerprints of
   Firefox only changed when curl-impersonate captured a new target, so every
   version falls back to the closest one. The remaining headers only changed
   twice: zstd and the Priority header landed on Firefox 133, and the
   Accept-Language quality factor was raised on Firefox 147 */

#define KAD_FIREFOX_MIN_VERSION (99)

#define KAD_FIREFOX_MAX_VERSION (157)

static const char* const FIREFOX_BASE_TARGETS[] = {
		"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox133",
	"firefox135",
	"firefox135",
	"firefox135",
	"firefox135",
	"firefox135",
	"firefox135",
	"firefox135",
	"firefox135",
	"firefox135",
	"firefox144",
	"firefox144",
	"firefox144",
	"firefox147",
	"firefox147",
	"firefox147",
	"firefox147",
	"firefox147",
	"firefox147",
	"firefox147",
	"firefox147",
	"firefox147",
	"firefox147",
	"firefox147",
};

static const char* const FIREFOX_HEADERS_LEGACY[] = {
	"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
	"Accept-Language: en-US,en;q=0.5",
	"Accept-Encoding: gzip, deflate, br",
	"Upgrade-Insecure-Requests: 1",
	"Sec-Fetch-Dest: document",
	"Sec-Fetch-Mode: navigate",
	"Sec-Fetch-Site: none",
	"Sec-Fetch-User: ?1"
};

static const char* const FIREFOX_HEADERS_MODERN[] = {
	"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
	"Accept-Language: en-US,en;q=0.5",
	"Accept-Encoding: gzip, deflate, br, zstd",
	"Upgrade-Insecure-Requests: 1",
	"Sec-Fetch-Dest: document",
	"Sec-Fetch-Mode: navigate",
	"Sec-Fetch-Site: none",
	"Sec-Fetch-User: ?1",
	"Priority: u=0, i"
};

static const char* const FIREFOX_HEADERS_CURRENT[] = {
	"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
	"Accept-Language: en-US,en;q=0.9",
	"Accept-Encoding: gzip, deflate, br, zstd",
	"Upgrade-Insecure-Requests: 1",
	"Sec-Fetch-Dest: document",
	"Sec-Fetch-Mode: navigate",
	"Sec-Fetch-Site: none",
	"Sec-Fetch-User: ?1",
	"Priority: u=0, i"
};

static const char* const FIREFOX_UA_FORMATS[] = {
	"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:%i.0) Gecko/20100101 Firefox/%i.0",
	"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:%i.0) Gecko/20100101 Firefox/%i.0",
	"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:%i.0) Gecko/20100101 Firefox/%i.0",
	"User-Agent: Mozilla/5.0 (Android 17; Mobile; rv:%i.0) Gecko/20100101 Firefox/%i.0"
};

static const char* impersonate_target_base = NULL;

static int impersonate_target_version = 0;

static int impersonate_target_platform = -1;

static int impersonate_target_browser = -1;

static int chrome_target_version(const char* const target, const int android) {

	int version = 0;
	int offset = 0;

	if (android) {
		if (sscanf(target, "chrome%i_android%n", &version, &offset) != 1 || offset == 0 || target[offset] != '\0') {
			return 0;
		}

		if (version < KAD_CHROME_MIN_ANDROID_VERSION || version > KAD_CHROME_MAX_VERSION) {
			return 0;
		}
	}
	else {
		if (sscanf(target, "chrome%i%n", &version, &offset) != 1 || offset == 0 || target[offset] != '\0') {
			return 0;
		}

		if (version < KAD_CHROME_MIN_VERSION || version > KAD_CHROME_MAX_VERSION) {
			return 0;
		}
	}

	return version;

}

static int target_version(const char* const target, const char* const browser, const int min, const int max) {

	int version = 0;
	int offset = 0;

	char format[16];

	snprintf(format, sizeof(format), "%s%%i%%n", browser);

	if (sscanf(target, format, &version, &offset) != 1 || offset == 0 || target[offset] != '\0') {
		return 0;
	}

	if (version < min || version > max) {
		return 0;
	}

	return version;

}

static int chrome_headers_append(struct curl_slist** headers, const char* const value) {
	
	struct curl_slist* item = curl_slist_append(*headers, value);
	
	if (item == NULL) {
		curl_slist_free_all(*headers);
		
		*headers = NULL;
		
		return -1;
	}
	
	*headers = item;
	
	return 0;
	
}

static int set_impersonate_headers(CURL* curl, const int browser, const int version, const int platform) {

	struct curl_slist* headers = NULL;

	const char* const* era_headers = NULL;
	size_t era_size = 0;

	const char* ua_version = NULL;
	const char* ua_format = NULL;

	char ua_version_buffer[24];
	char user_agent[256];

	size_t index = 0;

	if (browser == KAD_BROWSER_FIREFOX) {
		if (version < 133) {
			era_headers = FIREFOX_HEADERS_LEGACY;
			era_size = (sizeof(FIREFOX_HEADERS_LEGACY) / sizeof(FIREFOX_HEADERS_LEGACY[0]));
		}
		else if (version < 147) {
			era_headers = FIREFOX_HEADERS_MODERN;
			era_size = (sizeof(FIREFOX_HEADERS_MODERN) / sizeof(FIREFOX_HEADERS_MODERN[0]));
		}
		else {
			era_headers = FIREFOX_HEADERS_CURRENT;
			era_size = (sizeof(FIREFOX_HEADERS_CURRENT) / sizeof(FIREFOX_HEADERS_CURRENT[0]));
		}

		snprintf(user_agent, sizeof(user_agent), FIREFOX_UA_FORMATS[platform], version, version);

		if (chrome_headers_append(&headers, user_agent) != 0) {
			return -1;
		}
	}
	else {
		if (version < 110) {
			era_headers = CHROME_HEADERS_LEGACY;
			era_size = (sizeof(CHROME_HEADERS_LEGACY) / sizeof(CHROME_HEADERS_LEGACY[0]));
		}
		else if (version < 123) {
			era_headers = CHROME_HEADERS_PRE_ZSTD;
			era_size = (sizeof(CHROME_HEADERS_PRE_ZSTD) / sizeof(CHROME_HEADERS_PRE_ZSTD[0]));
		}
		else if (version < 124) {
			era_headers = CHROME_HEADERS_ZSTD;
			era_size = (sizeof(CHROME_HEADERS_ZSTD) / sizeof(CHROME_HEADERS_ZSTD[0]));
		}
		else {
			era_headers = CHROME_HEADERS_MODERN;
			era_size = (sizeof(CHROME_HEADERS_MODERN) / sizeof(CHROME_HEADERS_MODERN[0]));
		}

		if (browser == KAD_BROWSER_EDGE && platform == KAD_CHROME_PLATFORM_ANDROID && version <= KAD_CHROME_ANDROID_MAX_LEGACY_VERSION) {
			if (version >= KAD_CHROME_ANDROID_MIN_LEGACY_VERSION) {
				ua_version = CHROME_ANDROID_FULL_UA_VERSIONS[version - KAD_CHROME_ANDROID_MIN_LEGACY_VERSION];
			}
			else {
				snprintf(ua_version_buffer, sizeof(ua_version_buffer), "%i.0.0.0", version);

				ua_version = ua_version_buffer;
			}

			snprintf(user_agent, sizeof(user_agent), EDGE_ANDROID_LEGACY_UA_FORMAT, ua_version, EDGE_VERSIONS[version - KAD_EDGE_MIN_VERSION]);
		}
		else {
			if (browser == KAD_BROWSER_CHROME && platform == KAD_CHROME_PLATFORM_ANDROID && version >= KAD_CHROME_ANDROID_MIN_LEGACY_VERSION && version <= KAD_CHROME_ANDROID_MAX_LEGACY_VERSION) {
				ua_format = CHROME_ANDROID_LEGACY_UA_FORMAT;
				ua_version = CHROME_ANDROID_FULL_UA_VERSIONS[version - KAD_CHROME_ANDROID_MIN_LEGACY_VERSION];
			}
			else {
				ua_format = CHROME_PLATFORMS[platform].user_agent_format;

				if (version >= KAD_CHROME_MIN_VERSION && version < 104) {
					ua_version = CHROME_FULL_UA_VERSIONS[version - KAD_CHROME_MIN_VERSION];
				}
				else {
					snprintf(ua_version_buffer, sizeof(ua_version_buffer), "%i.0.0.0", version);

					ua_version = ua_version_buffer;
				}
			}

			snprintf(user_agent, sizeof(user_agent), ua_format, ua_version);

			if (browser == KAD_BROWSER_EDGE) {
				index = strlen(user_agent);

				snprintf(user_agent + index, (sizeof(user_agent) - index), (platform == KAD_CHROME_PLATFORM_ANDROID) ? " EdgA/%s" : " Edg/%s", EDGE_VERSIONS[version - KAD_EDGE_MIN_VERSION]);
			}
		}

		if (chrome_headers_append(&headers, (browser == KAD_BROWSER_EDGE) ? EDGE_SEC_CH_UA[version - KAD_EDGE_MIN_VERSION] : CHROME_SEC_CH_UA[version - KAD_CHROME_MIN_VERSION]) != 0 ||
			chrome_headers_append(&headers, CHROME_PLATFORMS[platform].sec_ch_ua_mobile) != 0 ||
			chrome_headers_append(&headers, CHROME_PLATFORMS[platform].sec_ch_ua_platform) != 0 ||
			chrome_headers_append(&headers, "Upgrade-Insecure-Requests: 1") != 0 ||
			chrome_headers_append(&headers, user_agent) != 0) {
			return -1;
		}
	}

	for (index = 0; index < era_size; index++) {
		if (chrome_headers_append(&headers, era_headers[index]) != 0) {
			return -1;
		}
	}

	if (curl_easy_setopt(curl, CURLOPT_HTTPBASEHEADER, headers) != CURLE_OK) {
		curl_slist_free_all(headers);

		return -1;
	}

	curl_slist_free_all(headers);

	return 0;

}

#if !defined(KAD_BUILD_SHARED)
static void sigint_handler(void) {
	
	socket_close(fd);
	exit(EXIT_SUCCESS);
	
}
#endif

static char target_impersonate[64] = {0};

static char target_platform[16] = {0};

static char proxy_url[4096] = {0};

static char doh_url[4096] = {0};

static char dns_servers[512] = {0};

#if !defined(KAD_DISABLE_SSL_VERIFY)
static int load_ssl_certificates(const char* const cacert) {
	/*
	Locates the CA certificate bundle file for cURL to use.
	
	If 'cacert' is not NULL and not empty, it is used as the path to the
	CA certificate bundle instead of the default one.
	
	Returns (0) on success, (-1) on error.
	*/

	int err = KADERR_SUCCESS;

	char* app_directory = NULL;
	char* ca_bundle = NULL;

	fstream_t* stream = NULL;

	int64_t file_size = 0;

	system_error_t error = {0};

	if (cacert != NULL && *cacert != '\0') {
		ca_bundle = malloc(strlen(cacert) + 1);

		if (ca_bundle == NULL) {
			error = get_system_error();
			loggln(LOG_ERROR, "[error] could not allocate memory: %s", error.message);

			err = KADERR_MEMORY_ALLOCATE_FAILURE;
			goto end;
		}

		strcpy(ca_bundle, cacert);
	} else {
		app_directory = get_app_directory();

		if (app_directory == NULL) {
			loggln(LOG_ERROR, "[error] could not get application directory: %s", strkaderr(KADERR_FS_GET_APP_DIRECTORY_FAILURE));

			err = KADERR_FS_GET_APP_DIRECTORY_FAILURE;
			goto end;
		}

		ca_bundle = malloc(strlen(app_directory) + strlen(CA_CERT_FILENAME) + 1);

		if (ca_bundle == NULL) {
			error = get_system_error();
			loggln(LOG_ERROR, "[error] could not allocate memory: %s", error.message);

			err = KADERR_MEMORY_ALLOCATE_FAILURE;
			goto end;
		}

		strcpy(ca_bundle, app_directory);
		strcat(ca_bundle, CA_CERT_FILENAME);

		free(app_directory);
		app_directory = NULL;
	}
	
	stream = fstream_open(ca_bundle, FSTREAM_READ);
	
	if (stream == NULL) {
		error = get_system_error();
		loggln(LOG_ERROR, "[error] could not open file at '%s': %s", ca_bundle, error.message);
		
		err = KADERR_FSTREAM_OPEN_FAILURE;
		goto end;
	}
	
	file_size = fstream_size(stream);
	
	if (file_size == FSTREAM_ERROR) {
		error = get_system_error();
		loggln(LOG_ERROR, "[error] could not get file size of '%s': %s", ca_bundle, error.message);
		
		fstream_close(stream);
		stream = NULL;
		
		err = KADERR_FSTREAM_TELL_FAILURE;
		goto end;
	}
	
	if (file_size == 0) {
		loggln(LOG_ERROR, "[error] file at '%s' is empty", ca_bundle);
		
		fstream_close(stream);
		stream = NULL;
		
		err = KADERR_FSTREAM_READ_EMPTY_FILE;
		goto end;
	}
	
	if (fstream_close(stream) == FSTREAM_ERROR) {
		error = get_system_error();
		loggln(LOG_ERROR, "[error] could not close file at '%s': %s", ca_bundle, error.message);
		
		stream = NULL;
		
		err = KADERR_FSTREAM_CLOSE_FAILURE;
		goto end;
	}
	
	stream = NULL;
	
	ca_bundle_global = ca_bundle;
	ca_bundle = NULL;
	
	end:;
	
	if (stream != NULL) {
		fstream_close(stream);
	}
	
	free(app_directory);
	free(ca_bundle);
	
	return err;
	
}
#endif

struct curl_pending {
	CURL* handle;
	struct curl_pending* next;
};

typedef struct curl_pending curl_pending_t;

static curl_pending_t* curl_pending = NULL;
static mutex_t curl_pending_mutex = MUTEX_INIT;

static void* event_loop(void* pointer) {
	
	int left = 0;
	int running = 0;
	
	int err = 0;
	
	CURLMsg* msg = NULL;
	CURLcode easy_code = 0;
	
	CURLMcode code = CURLM_OK;
	
	curl_pending_t* item = NULL;
	curl_pending_t* next = NULL;
	
	CURL* handle = NULL;
	
	transferdata_t* data = NULL;
	
	(void) pointer;
	
	while (1) {
		mutex_lock(&curl_pending_mutex);
		item = curl_pending;
		curl_pending = NULL;
		mutex_unlock(&curl_pending_mutex);
		
		while (item != NULL) {
			next = item->next;
			
			handle = item->handle;
			free(item);
			
			code = curl_multi_add_handle(curl_multi, handle);
			
			if (code != CURLM_OK) {
				loggln(LOG_ERROR, "[error] could not add handle to multi stack: %s", curl_multi_strerror(code));
				
				easy_code = curl_easy_getinfo(handle, CURLINFO_PRIVATE, &data);

				if (easy_code != CURLE_OK) {
					err = KADERR_CURL_GETINFO_FAILURE;
					goto end;
				}

				#if defined(KAD_CAPTURE)
					capture_end(&data->capture, handle);
				#endif

				curl_easy_cleanup(handle);
				transferdata_close(data);
			}
			
			item = next;
		}
		
		code = curl_multi_perform(curl_multi, &running);
		
		if (code != CURLM_OK) {
			err = KADERR_CURL_MULTI_PERFORM_FAILURE;
			goto end;
		}
		
		while ((msg = curl_multi_info_read(curl_multi, &left)) != NULL) {
			if (msg->msg != CURLMSG_DONE) {
				continue;
			}
			
			if (msg->data.result != CURLE_OK) {
				loggln(LOG_ERROR, "[error] curl: %s", curl_easy_strerror(msg->data.result));
			}
			
			easy_code = curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &data);
			
			if (easy_code != CURLE_OK) {
				err = KADERR_CURL_GETINFO_FAILURE;
				goto end;
			}
			
			code = curl_multi_remove_handle(curl_multi, msg->easy_handle);

			if (code != CURLM_OK) {
				err = KADERR_CURL_MULTI_REMOVE_FAILURE;
				goto end;
			}

			#if defined(KAD_CAPTURE)
				capture_end(&data->capture, msg->easy_handle);
			#endif

			curl_easy_cleanup(msg->easy_handle);
			transferdata_close(data);
		}
		
		code = curl_multi_poll(curl_multi, NULL, 0, 1000, NULL);
		
		if (code != CURLM_OK) {
			err = KADERR_CURL_MULTI_POLL_FAILURE;
			goto end;
		}
	}
	
	end:;
	
	if (easy_code != CURLE_OK) {
		loggln(LOG_ERROR, "[error] %s: %s", strkaderr(err), curl_easy_strerror(easy_code));
	} else {
		loggln(LOG_ERROR, "[error] %s: %s", strkaderr(err), curl_multi_strerror(code));
	}
	
	return NULL;
	
}

static int request_handler(void* pointer) {
	
	int err = 0;
	
	int fd = *(int*) pointer;
	
	transferdata_t* data __transferdata_close__ = NULL;
	
	http_header_t* header = NULL;
	
	char* hostname = NULL;
	char* uri = NULL;
	char* item = NULL;
	
	char buffer[MAX_HTTP_HEADERS_SIZE];
	
	CURL* curl __curl_easy_cleanup__ = NULL;
	
	struct curl_slist* tmp = NULL;
	
	curl_pending_t* pending = NULL;
	
	size_t index = 0;
	size_t subindex = 0;
	
	size_t content_length = 0;
	
	int status = 0;
	
	int is_secure = 0;
	
	ssize_t recv_size = 0;
	ssize_t size = 0;
	
	CURLcode value = 0;
	
	const char* name = NULL;
	
	static char curl_error_message[CURL_ERROR_SIZE] = {0};
	
	free(pointer);
	
	data = malloc(sizeof(*data));
	
	if (data == NULL) {
		socket_close(fd);
		
		err = KADERR_MEMORY_ALLOCATE_FAILURE;
		goto end;
	}
	
	memset(data, 0, sizeof(*data));
	data->fd = fd;
	
	recv_size = recv(fd, buffer, MAX_HTTP_HEADERS_SIZE, 0);
	
	if (recv_size <= 0) {
		err = KADERR_SOCKET_RECV_FAILURE;
		goto end;
	}
	
	http_request_init(&data->request);
	
	err = http_request_parse(&data->request, buffer, (size_t) recv_size);
	
	if (err != KADERR_SUCCESS) {
		goto end;
	}
	
	is_secure = (data->request.method == CONNECT);
	
	data->is_secure = is_secure;

	if (is_secure) {
		size = send(fd, "HTTP/1.0 200 OK\r\n\r\n", 19, 0);
		
		if (size == -1) {
			err = KADERR_SOCKET_SEND_FAILURE;
			goto end;
		}
		
		if (ssl_init(&data->context, &fd) == -1) {
			err = KADERR_SSL_INIT_FAILURE;
			goto end;
		}
		
		size = ssl_recv(&data->context, buffer, sizeof(buffer));
		
		if (size <= 0) {
			err = KADERR_SSL_RECV_FAILURE;
			goto end;
		}
		
		hostname = data->request.uri;
		
		data->request.uri = NULL;
		http_request_free(&data->request);
		
		err = http_request_parse(&data->request, buffer, (size_t) size);
		
		if (err != KADERR_SUCCESS) {
			goto end;
		}
		
		uri = malloc(strlen(HTTPS_SCHEME) + strlen(SCHEME_SEPARATOR) + strlen(hostname) + strlen(data->request.uri) + 1);
		
		if (uri == NULL) {
			err = KADERR_MEMORY_ALLOCATE_FAILURE;
			goto end;
		}
		
		strcpy(uri, HTTPS_SCHEME);
		strcat(uri, SCHEME_SEPARATOR);
		strcat(uri, hostname);
		strcat(uri, data->request.uri);
		
		free(hostname);
		hostname = NULL;
		
		data->request.uri = uri;
		uri = NULL;
	}
	
	loggln(LOG_INFO, "[info] client request to %s", data->request.uri);

	#if defined(KAD_CAPTURE)
		/* capturing is best-effort, failures never affect the request being forwarded */
		capture_init(&data->capture, &data->request);
	#endif

	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();

	if (curl == NULL) {
		err = KADERR_CURL_INIT_FAILURE;
		goto end;
	}

	#if defined(KAD_CAPTURE)
		data->curl = curl;
	#endif

	if (impersonate_target_base != NULL) {
		if (curl_easy_impersonate(curl, impersonate_target_base, 0) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
		
		if (set_impersonate_headers(curl, impersonate_target_browser, impersonate_target_version, impersonate_target_platform) != 0) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
	}
	else if (curl_easy_impersonate(curl, target_impersonate, 1) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}

	if (*proxy_url != '\0' && curl_easy_setopt(curl, CURLOPT_PROXY, proxy_url) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}

	if (*doh_url != '\0' && curl_easy_setopt(curl, CURLOPT_DOH_URL, doh_url) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_DNS_SERVERS, *dns_servers != '\0' ? dns_servers : "8.8.8.8,8.8.4.4") != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback_empty) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_ALTSVC, "") != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	#if defined(KAD_DISABLE_SSL_VERIFY)
		value = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

		if (value != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}

		value = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

		if (value != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}

		value = curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYPEER, 0L);

		if (value != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}

		value = curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYHOST, 0L);

		if (value != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
	#else
		if (curl_easy_setopt(curl, CURLOPT_CAINFO, ca_bundle_global) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
	#endif
	
	if (curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error_message) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}

	#if defined(KAD_CAPTURE)
		/* route cURL's verbose output to the debug callback so the outgoing
		   request headers can be captured */

		if (curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}

		if (curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_callback) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}

		if (curl_easy_setopt(curl, CURLOPT_DEBUGDATA, (void*) data) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
	#else
		if (loglevel_get() == LOG_VERBOSE) {
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		}
	#endif

	/* Advertise all compression algorithms supported by curl and let it transparently decode the response body */
	if (curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "") != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	for (index = 0; index < data->request.headers.offset; index++) {
		header = &data->request.headers.items[index];
		
		status = 0;
		
		for (subindex = 0; subindex < sizeof(IMPERSONATE_HEADERS) / sizeof(*IMPERSONATE_HEADERS); subindex++) {
			name = IMPERSONATE_HEADERS[subindex];
			
			status = (strcasecmp(header->key, name) == 0);
			
			if (!status) {
				continue;
			}
			
			break;
		}
		
		if (status) {
			continue;
		}
		
		item = malloc(strlen(header->key) + strlen(HEADER_SEPARATOR) + strlen(header->value) + 1);
		
		if (item == NULL) {
			err = KADERR_MEMORY_ALLOCATE_FAILURE;
			goto end;
		}
		
		strcpy(item, header->key);
		strcat(item, HEADER_SEPARATOR);
		strcat(item, header->value);
		
		tmp = curl_slist_append(data->headers, item);
		
		free(item);
		item = NULL;
		
		if (tmp == NULL) {
			err = KADERR_CURL_SLIST_FAILURE;
			goto end;
		}
		
		data->headers = tmp;
	}
	
	if (data->request.method == POST || data->request.method == PUT) {
		header = http_headers_get(&data->request.headers, "Expect");
		
		if (header == NULL) {
			tmp = curl_slist_append(data->headers, "Expect:");
			
			if (tmp == NULL) {
				err = KADERR_CURL_SLIST_FAILURE;
				goto end;
			}
			
			data->headers = tmp;
		}
	}
	
	if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, data->headers) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_URL, data->request.uri) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (data->request.method != HEAD) {
		if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
		
		if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*) data) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
	}
	
	header = http_headers_get(&data->request.headers, "Transfer-Encoding");
	
	status = (header != NULL && strcmp(header->value, "chunked") == 0);
	
	if (!status) {
		header = http_headers_get(&data->request.headers, "Content-Length");
		
		if (header != NULL) {
			content_length = strtoull(header->value, NULL, 10);
		}
	}
	
	if (data->request.body.size > 0 || content_length > 0 || status || recv_size >= MAX_HTTP_HEADERS_SIZE) {
		if (curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
		
		if (curl_easy_setopt(curl, CURLOPT_READDATA, (void*) data) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
		
		if (content_length > 0) {
			if (curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) content_length) != CURLE_OK) {
				err = KADERR_CURL_SETOPT_FAILURE;
				goto end;
			}
		}
		
		data->remaining = content_length;
	}
	
	switch (data->request.method) {
		case GET: {
			value = curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
			break;
		}
		case POST: {
			value = curl_easy_setopt(curl, CURLOPT_POST, 1L);
			break;
		}
		case HEAD: {
			value = curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
			break;
		}
		default: {
			name = http_method_stringify(data->request.method);
			value = curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, name);
			break;
		}
	}
	
	if (value != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void*) data) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_PRIVATE, (void*) data) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	pending = malloc(sizeof(*pending));
	
	if (pending == NULL) {
		err = KADERR_MEMORY_ALLOCATE_FAILURE;
		goto end;
	}
	
	pending->handle = curl;
	
	mutex_lock(&curl_pending_mutex);
	
	pending->next = curl_pending;
	curl_pending = pending;
	pending = NULL;
	
	mutex_unlock(&curl_pending_mutex);
	
	curl_multi_wakeup(curl_multi);
	
	curl = NULL;
	data = NULL;
	
	end:;
	
	free(hostname);
	free(uri);
	free(item);
	free(pending);
	
	return err;
	
}

static void* handle_request(void* pointer) {
	
	const int code = request_handler(pointer);
	
	if (code != KADERR_SUCCESS) {
		loggln(LOG_ERROR, "[error] %s", strkaderr(code));
	}
	
	return NULL;
	
}

int kad_main(int argc, argv_t** argv) {
	/*
	const struct sigaction action = {
		.sa_handler = &sigint_handler
	};
	
	if (sigaction(SIGINT, &action, NULL) < 0) {
		abort_with_perror("sigaction()");
	}
	*/
	
	#if !defined(_WIN32)
		struct sigaction sigpipe_action;
	#endif
	
	struct linger lingerv;
	
	struct sockaddr* sockaddress = NULL;
	
	struct sockaddr_in addr_in;
	struct sockaddr_in6 addr_in6;
	
	struct sockaddr_storage client_address;
	
	socklen_t client_address_size = 0;
	
	size_t addrsize = 0;
	
	char address[512] = {0};
	char loglevel[16] = "verbose";
	char cacert[4096] = {0};
	
	char host[NI_MAXHOST];
	char servname[NI_MAXSERV];
	
	const int reuseaddr = 1;
	
	int port = 0;
	int value = 0;
	
	int position = 0;
	int index = 0;
	int version = 0;
	
	int code = 0;
	int status = 0;
	int err = 0;
	
	size_t size = 0;
	
	argparse_t argparse = {0};
	const arg_t* argument = NULL;
	
	thread_t event_thread = {0};
	thread_t client_thread = {0};
	thread_t threads[KAD_DEFAULT_LISTEN_BACKLOG];
	
	thread_t* worker = NULL;
	
	int* cfd = NULL;
	
	logging_t level = LOG_QUIET;
	
	system_error_t error = {0};
	
	#if defined(_WIN32) && defined(_UNICODE)
		wio_enable_unicode();
	#endif
	
	#if defined(_WIN32)
		WSADATA wsa_data = {0};
		
		if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
			loggln(LOG_ERROR, "[error] could not initialize Winsock (error = %i)", WSAGetLastError());
			
			return EXIT_FAILURE;
		}
	#endif
	
	#if !defined(_WIN32)
		memset(&sigpipe_action, 0, sizeof(sigpipe_action));
		
		sigpipe_action.sa_handler = SIG_IGN;
	#endif
	
	if (resources_increase_maxfd() == -1) {
		loggln(LOG_ERROR, "[error] could not increase the maximum open files limit");
		
		return EXIT_FAILURE;
	}
	
	curl_multi = curl_multi_init();
	
	if (curl_multi == NULL) {
		loggln(LOG_ERROR, "[error] could not initialize multi stack");
		
		return EXIT_FAILURE;
	}
	
	thread_create(&event_thread, event_loop, NULL);
	
	#if !defined(_WIN32)
		if (sigaction(SIGPIPE, &sigpipe_action, NULL) == -1) {
			error = get_system_error();
			loggln(LOG_ERROR, "[error] could not set signal handler: %s", error.message);
			
			return EXIT_FAILURE;
		}
	#endif
	
	code = argparse_init(&argparse, argc, argv);
	
	if (code != KADERR_SUCCESS) {
		loggln(LOG_ERROR, "[error] %s", strkaderr(code));
		
		return EXIT_FAILURE;
	}
	
	while (1) {
		argument = argparse_getnext(&argparse);
		
		if (argument == NULL) {
			break;
		}
		
		if (strcmp(argument->key, "host") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			size = strlen(argument->value);
			
			if (size > (sizeof(address) - 1)) {
				loggln(LOG_ERROR, "[error] address string exceeds max buffer size: %s", argument->value);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			strcpy(address, argument->value);
		} else if (strcmp(argument->key, "port") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			value = atoi(argument->value);
			
			if (!(value >= 1 && value <= 65535)) {
				loggln(LOG_ERROR, "[error] bad port number: %i", value);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			port = value;
		} else if (strcmp(argument->key, "target") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			size = strlen(argument->value);
			
			if (size > (sizeof(target_impersonate) - 1)) {
				loggln(LOG_ERROR, "[error] target_impersonate string exceeds max buffer size: %s", argument->value);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			strcpy(target_impersonate, argument->value);
		} else if (strcmp(argument->key, "platform") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			size = strlen(argument->value);
			
			if (size > (sizeof(target_platform) - 1)) {
				loggln(LOG_ERROR, "[error] target_platform string exceeds max buffer size: %s", argument->value);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			strcpy(target_platform, argument->value);
		} else if (strcmp(argument->key, "loglevel") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			size = strlen(argument->value);
			
			if (size > (sizeof(loglevel) - 1)) {
				loggln(LOG_ERROR, "[error] loglevel string exceeds max buffer size: %s", argument->value);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			strcpy(loglevel, argument->value);
		} else if (strcmp(argument->key, "proxy") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);

				argparse_free(&argparse);

				return EXIT_FAILURE;
			}

			size = strlen(argument->value);

			if (size > (sizeof(proxy_url) - 1)) {
				loggln(LOG_ERROR, "[error] proxy url exceeds max buffer size: %s", argument->value);

				argparse_free(&argparse);

				return EXIT_FAILURE;
			}

			strcpy(proxy_url, argument->value);
		} else if (strcmp(argument->key, "doh-url") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);

				argparse_free(&argparse);

				return EXIT_FAILURE;
			}

			size = strlen(argument->value);

			if (size > (sizeof(doh_url) - 1)) {
				loggln(LOG_ERROR, "[error] doh url exceeds max buffer size: %s", argument->value);

				argparse_free(&argparse);

				return EXIT_FAILURE;
			}

			strcpy(doh_url, argument->value);
		} else if (strcmp(argument->key, "dns-servers") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);

				argparse_free(&argparse);

				return EXIT_FAILURE;
			}

			size = strlen(argument->value);

			if (size > (sizeof(dns_servers) - 1)) {
				loggln(LOG_ERROR, "[error] dns servers string exceeds max buffer size: %s", argument->value);

				argparse_free(&argparse);

				return EXIT_FAILURE;
			}

			strcpy(dns_servers, argument->value);
		} else if (strcmp(argument->key, "cacert") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);

				argparse_free(&argparse);

				return EXIT_FAILURE;
			}

			size = strlen(argument->value);

			if (size > (sizeof(cacert) - 1)) {
				loggln(LOG_ERROR, "[error] cacert path exceeds max buffer size: %s", argument->value);

				argparse_free(&argparse);

				return EXIT_FAILURE;
			}

			strcpy(cacert, argument->value);
		} else if (strcmp(argument->key, "v") == 0 || strcmp(argument->key, "version") == 0) {
			argparse_free(&argparse);
			
			printf("%s v%s (+%s)\n", KAD_NAME, KAD_VERSION, KAD_REPOSITORY);
			
			return EXIT_SUCCESS;
		} else if (strcmp(argument->key, "h") == 0 || strcmp(argument->key, "help") == 0) {
			argparse_free(&argparse);
			
			printf("%s\n", KAD_DESCRIPTION);
			
			return EXIT_SUCCESS;
		}
	}
	
	argparse_free(&argparse);
	
	if (*address == '\0') {
		strcpy(address, KAD_DEFAULT_LISTEN_ADDRESS);
	}
	
	if (port == 0) {
		port = KAD_DEFAULT_LISTEN_PORT;
	}
	
	if (*target_impersonate == '\0') {
		strcpy(target_impersonate, KAD_DEFAULT_IMPERSONATE_TARGET);
	}
	
	if (*target_platform == '\0') {
		version = chrome_target_version(target_impersonate, 1);

		if (version != 0) {
			impersonate_target_base = KAD_CHROME_ANDROID_TARGET;
			impersonate_target_version = version;
			impersonate_target_platform = KAD_CHROME_PLATFORM_ANDROID;
			impersonate_target_browser = KAD_BROWSER_CHROME;
		}
	} else {
		for (index = 0; index < (sizeof(CHROME_PLATFORMS) / sizeof(CHROME_PLATFORMS[0])); index++) {
			if (strcmp(target_platform, CHROME_PLATFORMS[index].name) == 0) {
				break;
			}
		}

		if (index == (int) (sizeof(CHROME_PLATFORMS) / sizeof(CHROME_PLATFORMS[0]))) {
			loggln(LOG_ERROR, "[error] unknown platform: %s", target_platform);

			argparse_free(&argparse);

			return EXIT_FAILURE;
		}

		version = target_version(target_impersonate, "edge", KAD_EDGE_MIN_VERSION, KAD_EDGE_MAX_VERSION);

		if (version != 0) {
			impersonate_target_base = EDGE_BASE_TARGETS[version - KAD_EDGE_MIN_VERSION];
			impersonate_target_browser = KAD_BROWSER_EDGE;
		}
		else {
			version = target_version(target_impersonate, "chrome", KAD_CHROME_MIN_VERSION, KAD_CHROME_MAX_VERSION);

			if (version != 0) {
				impersonate_target_base = CHROME_BASE_TARGETS[version - KAD_CHROME_MIN_VERSION];
				impersonate_target_browser = KAD_BROWSER_CHROME;
			}
			else {
				version = target_version(target_impersonate, "firefox", KAD_FIREFOX_MIN_VERSION, KAD_FIREFOX_MAX_VERSION);

				if (version != 0) {
					impersonate_target_base = FIREFOX_BASE_TARGETS[version - KAD_FIREFOX_MIN_VERSION];
					impersonate_target_browser = KAD_BROWSER_FIREFOX;
				}
			}
		}

		if (version == 0) {
			loggln(LOG_ERROR, "[error] --platform requires an edge (96 to 157), chrome (99 to 157) or firefox (99 to 157) target");

			argparse_free(&argparse);

			return EXIT_FAILURE;
		}

		impersonate_target_version = version;
		impersonate_target_platform = (int) index;
	}
	
	level = loglevel_unstringify(loglevel);
	
	if (level == LOG_STANDARD && strcmp(loglevel, "standard") != 0) {
		loggln(LOG_ERROR, "[error] %s: %s", strkaderr(KADERR_LOGGING_INVALID_LEVEL), loglevel);
		
		return EXIT_FAILURE;
	}
	
	loglevel_set(level);
	
	#if !defined(KAD_DISABLE_SSL_VERIFY)
		err = load_ssl_certificates(cacert);
		
		if (err != KADERR_SUCCESS) {
			loggln(LOG_ERROR, "[error] could not load CA bundle: %s", strkaderr(err));
			
			return err;
		}
	#endif
	
	fd = socket(AF_INET, SOCK_STREAM, 0);
	
	if (fd == -1) {
		error = get_system_error();
		loggln(LOG_ERROR, "[error] could not create socket: %s", error.message);
		
		return EXIT_FAILURE;
	}
	
	memset(&lingerv, 0, sizeof(lingerv));
	
	setsockopt(fd, SOL_SOCKET, SO_LINGER, (const char*) &lingerv, sizeof(lingerv));
	
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*) &reuseaddr, sizeof(reuseaddr));
	
	memset(&addr_in, 0, sizeof(addr_in));
	
	addr_in.sin_family = AF_INET;
	addr_in.sin_port = htons(port);
	
	if (inet_pton(addr_in.sin_family, address, &addr_in.sin_addr) == 1) {
		sockaddress = (struct sockaddr*) &addr_in;
		addrsize = sizeof(addr_in);
	}
	
	memset(&addr_in6, 0, sizeof(addr_in6));
	
	addr_in6.sin6_family = AF_INET6;
	addr_in6.sin6_port = htons(port);
	
	if (inet_pton(addr_in6.sin6_family, address, &addr_in6.sin6_addr) == 1) {
		sockaddress = (struct sockaddr*) &addr_in6;
		addrsize = sizeof(addr_in6);
	}
	
	if (sockaddress == NULL) {
		loggln(LOG_ERROR, "[error] invalid address: %s", address);
		
		return EXIT_FAILURE;
	}
	
	if (bind(fd, sockaddress, addrsize) == -1) {
		error = get_system_error();
		socket_close(fd);
		loggln(LOG_ERROR, "[error] could not bind socket: %s", error.message);
		
		return EXIT_FAILURE;
	}
	
	if (listen(fd, KAD_DEFAULT_LISTEN_BACKLOG) == -1) {
		error = get_system_error();
		socket_close(fd);
		loggln(LOG_ERROR, "[error] could not listen on socket: %s", error.message);
		
		return EXIT_FAILURE;
	}
	
	loggln(LOG_STANDARD, "Starting server at http://%s:%i (pid = %i)", address, port, getpid());
	
	while (1) {
		memset(&client_address, 0, sizeof(client_address));
		
		client_address_size = sizeof(client_address);
		
		cfd = malloc(sizeof(*cfd));
		
		if (cfd == NULL) {
			error = get_system_error();
			socket_close(fd);
			loggln(LOG_ERROR, "[error] could not allocate memory for client socket: %s", error.message);
			
			return EXIT_FAILURE;
		}
		
		*cfd = accept(fd, (struct sockaddr*) &client_address, &client_address_size);
		
		if (*cfd == -1) {
			error = get_system_error();
			socket_close(fd);
			loggln(LOG_ERROR, "[error] could not accept incoming socket connection: %s", error.message);
			
			return EXIT_FAILURE;
		}
		
		status = getnameinfo((struct sockaddr*) &client_address, client_address_size, host, sizeof(host), servname, sizeof(servname), NI_NUMERICHOST | NI_NUMERICSERV);
		
		if (status != 0) {
			socket_close(fd);
			loggln(LOG_ERROR, "[error] could not get address info: %s", gai_strerror(status));
			
			return EXIT_FAILURE;
		}
		
		loggln(LOG_INFO, "[info] got connection from %s on port %s", host, servname);
		
		memset(&client_thread, 0, sizeof(client_thread));
		
		thread_create(&client_thread, handle_request, (void*) cfd);
		threads[position++] = client_thread;
		
		if (position < KAD_DEFAULT_LISTEN_BACKLOG) {
			continue;
		}
		
		loggln(LOG_WARN, "[warn] max number of concurrent connections reached");
		
		index = 0;
		
		while (index < position) {
			worker = &threads[index++];
			thread_wait(worker);
		}
		
		position = 0;
	}
	
	return EXIT_SUCCESS;
	
}

#if !defined(KAD_BUILD_SHARED)
	#if defined(_WIN32) && defined(_UNICODE)
		int wmain(const int argc, wchar_t** const argv) {
			return kad_main(argc, argv);
		}
	#else
		int main(const int argc, char** const argv) {
			return kad_main(argc, argv);
		}
	#endif
#endif
