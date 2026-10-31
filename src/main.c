#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#if defined(_WIN32)
	#include <winsock2.h>
	#include <ws2tcpip.h>
#endif

#if !defined(_WIN32)
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <netdb.h>
	#include <fcntl.h>
#endif

#if defined(_WIN32) && defined(_UNICODE)
	#include "wio.h"
#endif

#include <curl/curl.h>

#include <event2/event.h>

#include "http.h"
#include "ssl.h"
#include "cleanup.h"
#include "client.h"
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
#include "logging.h"
#include "socketclose.h"
#include "unboundshim.h"

#if !defined(KAD_DISABLE_SSL_VERIFY)
	#include "fstream.h"
	#include "getexec.h"
	#include "sep.h"
#endif

#define KAD_DEFAULT_IMPERSONATE_TARGET "chrome116"

static const char KAD_DEFAULT_LISTEN_ADDRESS[] = "127.0.0.1";

#define KAD_DEFAULT_LISTEN_PORT (4000)
#define KAD_DEFAULT_LISTEN_BACKLOG (100)

#define KAD_RETRY_MIN (0)
#define KAD_RETRY_MAX (32)

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
	"Accept-Encoding",
	"User-Agent",
	"X-Requested-With"
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

static struct event_base* event_base = NULL;

static struct event* curl_timeout_event = NULL;

/* the TLS and HTTP/2 fingerprints of Chrome only depend on its major version,
   so every supported combination of --target=chrome<version> and --platform is
   impersonated through the curl-impersonate target that matches the version
   the closest, while the HTTP headers, which do change per version and
   platform, are set here */

#define KAD_CHROME_ANDROID_TARGET "chrome131_android"

#define KAD_CHROME_MIN_VERSION (99)

#define KAD_CHROME_MAX_VERSION (160)

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
	"chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android", "chrome131_android",
	"chrome150", "chrome150", "chrome150"
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
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"157\", \"Google Chrome\";v=\"157\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"158\", \"Google Chrome\";v=\"158\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"159\", \"Google Chrome\";v=\"159\"",
	"sec-ch-ua: \"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"160\", \"Google Chrome\";v=\"160\""
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

static int retry_limit = KAD_RETRY_MIN;

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

#define CLIENT_OUTPUT_MAX_SIZE (1024 * 1024)
#define CLIENT_OUTPUT_MIN_SIZE (256 * 1024)

/* how long to wait for the client to finish sending before closing after
   the answer was already delivered (lingering close) */

#define CLIENT_DRAIN_TIMEOUT (3)

struct curl_socket_context {
	struct event* event;
	curl_socket_t sockfd;
};

typedef struct curl_socket_context curl_socket_context_t;

static void client_event_cb(evutil_socket_t fd, short events, void* arg);

static void curl_check_completed(void);

static int socket_nonblocking(const int fd) {

#if defined(_WIN32)
	unsigned long mode = 1;

	return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? 0 : -1;
#else
	const int flags = fcntl(fd, F_GETFL, 0);

	if (flags == -1) {
		return -1;
	}

	return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0) ? 0 : -1;
#endif

}

static int socket_wouldblock(void) {

#if defined(_WIN32)
	const int error = WSAGetLastError();

	return (error == WSAEWOULDBLOCK || error == WSAEINPROGRESS);
#else
	return (errno == EAGAIN || errno == EWOULDBLOCK);
#endif

}

static ssize_t socket_read(const int fd, char* const buffer, const size_t size, int* const wait) {

	ssize_t rsize = -1;

	*wait = 0;

#if !defined(_WIN32)
	do {
		rsize = recv(fd, buffer, size, 0);
	} while (rsize == -1 && errno == EINTR);
#else
	rsize = recv(fd, buffer, size, 0);
#endif

	if (rsize == -1) {
		if (socket_wouldblock()) {
			*wait = SSL_WAIT_READ;
			return 0;
		}

		return -1;
	}

	if (rsize == 0) {
		/* orderly shutdown of the connection */

		return -2;
	}

	return rsize;

}

static ssize_t socket_write(const int fd, const char* const buffer, const size_t size, int* const wait) {

	size_t offset = 0;

	ssize_t wsize = 0;

	*wait = 0;

	while (offset < size) {
		wsize = send(fd, buffer + offset, size - offset, 0);

		if (wsize == -1) {
		#if !defined(_WIN32)
			if (errno == EINTR) {
				continue;
			}
		#endif
			if (socket_wouldblock()) {
				*wait = SSL_WAIT_WRITE;
				return (ssize_t) offset;
			}

			return -1;
		}

		offset += (size_t) wsize;
	}

	return (ssize_t) offset;

}

static int socket_shutdown_write(const int fd) {

#if defined(_WIN32)
	return (shutdown(fd, SD_SEND) == 0) ? 0 : -1;
#else
	return (shutdown(fd, SHUT_WR) == 0) ? 0 : -1;
#endif

}

/* depth of the nested cURL calls made from within the client callbacks; a
   client is never destroyed while nested inside its own callback, as that
   would leave a dangling pointer on the stack of the caller */

static int curl_depth = 0;

static void client_destroy(transferdata_t* const data) {
	transferdata_close(data);
}

static void client_kill(transferdata_t* const data) {

	if (data->curl != NULL) {
		curl_multi_remove_handle(curl_multi, data->curl);
		curl_easy_cleanup(data->curl);

		data->curl = NULL;
	}

	client_destroy(data);

}

static void client_event_update(transferdata_t* const data) {

	short events = EV_PERSIST;

	struct timeval tv;
	struct timeval* timeout = NULL;

	if (data->state == CLIENT_STATE_HANDSHAKE) {
		if ((data->wait & SSL_WAIT_WRITE) != 0) {
			events |= EV_WRITE;
		} else {
			events |= EV_READ;
		}
	} else {
		if ((data->flags & TD_EOF) == 0) {
			events |= EV_READ;
		}

		if (data->out.slength > 0 || (data->wait & SSL_WAIT_WRITE) != 0) {
			events |= EV_WRITE;
		}
	}

	/* while lingering, the watcher bounds how long the client can keep
	   sending after its answer was already delivered */

	if (data->state == CLIENT_STATE_CLOSING && (data->flags & TD_FIN) != 0) {
		tv.tv_sec = CLIENT_DRAIN_TIMEOUT;
		tv.tv_usec = 0;

		timeout = &tv;
	}

	/* the watcher only has to be rearmed when the watched conditions
	   actually changed; armed timeouts are remembered through the sign,
	   as rearmed watchers would restart the lingering deadline */

	int armed = timeout ? -(int) events : (int) events;

	if (data->events == armed) {
		return;
	}

	event_del(data->event);
	event_assign(data->event, event_base, data->fd, events, client_event_cb, data);
	event_add(data->event, timeout);

	data->events = armed;

}

static void client_linger_close(transferdata_t* const data) {
	/*
	The client may still be sending the rest of its request when its answer
	was already delivered; shut the write half down and keep draining
	whatever it sends, as closing right away would reset the connection and
	destroy the response queued for it.
	*/

	if ((data->flags & TD_SECURE) != 0) {
		/* best-effort TLS close notify; the write half is shut down
		   right after regardless */

		ssl_close(&data->context);
	}

	socket_shutdown_write(data->fd);

	data->flags |= TD_FIN;

	/* force the watcher to be rearmed along with the drain deadline */

	data->events = -1;

	client_event_update(data);

}

/*
Closes the client connection, lingering first when it may still be sending
the rest of its request; returns (-1) when the client was destroyed.
*/

static int client_close_or_destroy(transferdata_t* const data) {

	if (data->remaining > 0 && (data->flags & TD_EOF) == 0 && data->state != CLIENT_STATE_HANDSHAKE && data->state != CLIENT_STATE_REQUEST) {
		data->state = CLIENT_STATE_CLOSING;

		client_linger_close(data);

		return 0;
	}

	client_destroy(data);

	return -1;

}

static int client_flush(transferdata_t* const data) {

	size_t offset = 0;
	ssize_t size = 0;

	int wait = 0;

	while (offset < data->out.slength) {
		wait = 0;

		size = ((data->flags & TD_SECURE) != 0)
			? ssl_send(&data->context, data->out.s + offset, data->out.slength - offset, &wait)
			: socket_write(data->fd, data->out.s + offset, data->out.slength - offset, &wait);

		if (size == -1) {
			return -1;
		}

		if (size == 0 && wait != 0) {
			data->wait |= wait;
			break;
		}

		offset += (size_t) size;
	}

	if (offset > 0) {
		data->out.slength -= offset;

		memmove(data->out.s, data->out.s + offset, data->out.slength);
	}

	return 0;

}

int client_write(transferdata_t* const data, const char* const buffer, const size_t size) {

	char* s = NULL;

	if (size > 0) {
		s = realloc(data->out.s, data->out.slength + size);

		if (s == NULL) {
			return -1;
		}

		memcpy(s + data->out.slength, buffer, size);

		data->out.s = s;
		data->out.slength += size;
	}

	if (client_flush(data) != 0) {
		return -1;
	}

	/* apply backpressure once too much response data is waiting to be
	   sent to a slow client */

	if (data->curl != NULL && (data->flags & TD_PAUSE_DOWN) == 0 && data->out.slength >= CLIENT_OUTPUT_MAX_SIZE) {
		if (curl_easy_pause(data->curl, CURLPAUSE_RECV) == CURLE_OK) {
			data->flags |= TD_PAUSE_DOWN;
		}
	}

	client_event_update(data);

	return 0;

}

/* resume the paused transfer of a client and drive the multi stack once, as
   resuming a paused handle needs an external kickstart */

static int client_kick(transferdata_t* const data) {

	int running = 0;

	CURLMcode code = CURLM_OK;

	code = curl_multi_socket_action(curl_multi, CURL_SOCKET_TIMEOUT, 0, &running);

	if (code != CURLM_OK) {
		loggln(LOG_ERROR, "[error] %s: %s", strkaderr(KADERR_CURL_MULTI_ACTION_FAILURE), curl_multi_strerror(code));
	}

	curl_depth++;

	curl_check_completed();

	curl_depth--;

	if ((data->flags & TD_DESTROY) != 0) {
		data->flags &= ~TD_DESTROY;

		client_close_or_destroy(data);
	}

	return 0;

}

static int client_headers_complete(const buffer_t* const buffer) {

	size_t index = 0;
	size_t limit = 0;

	if (buffer->slength < strlen(CRLFCRLF)) {
		return 0;
	}

	limit = buffer->slength - strlen(CRLFCRLF);

	for (index = 0; index <= limit; index++) {
		if (memcmp(buffer->s + index, CRLFCRLF, strlen(CRLFCRLF)) == 0) {
			return 1;
		}
	}

	return 0;

}

static int client_start_transfer(transferdata_t* const data) {
	/*
	Creates a cURL easy handle for the parsed request and adds it to the
	multi stack.

	Returns (0) on success, a KADERR_* code on error, or (-1) when the
	failure has already been handled and logged.
	*/

	int err = KADERR_SUCCESS;

	const http_header_t* header = NULL;

	char* item = NULL;

	struct curl_slist* tmp = NULL;

	data->retryable = true;

	size_t index = 0;
	size_t subindex = 0;

	size_t content_length = 0;

	int status = 0;

	CURLcode value = 0;

	const char* name = NULL;

	static char curl_error_message[CURL_ERROR_SIZE] = {0};

	CURL* curl __curl_easy_cleanup__ = NULL;

	CURLMcode code = CURLM_OK;

	#if defined(KAD_CAPTURE)
		/* capturing is best-effort, failures never affect the request being forwarded */
		capture_init(&data->capture, &data->request);
	#endif

	curl = curl_easy_init();

	if (curl == NULL) {
		err = KADERR_CURL_INIT_FAILURE;
		goto end;
	}

	if (impersonate_target_base != NULL) {
		if (curl_easy_impersonate(curl, impersonate_target_base, 0) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}

		if (set_impersonate_headers(curl, impersonate_target_browser, impersonate_target_version, impersonate_target_platform) != 0) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
	} else if (curl_easy_impersonate(curl, target_impersonate, 1) != CURLE_OK) {
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

	if (curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}

	if (curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 45L) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}

	if (curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 15L) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}

	if (curl_easy_setopt(curl, CURLOPT_MAXAGE_CONN, 300L) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}

	if (curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}

	if (curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 4096L) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}

	if (curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 8L) != CURLE_OK) {
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

	header = http_headers_get(&data->request.headers, "Sec-Fetch-Mode");

	/*
	* Requests made using fetch() don't send these headers in real Chrome,
	* and sending them in these cases actually breaks some sites, so strip them out.
	*/
	if (!(header == NULL || header->value == NULL) && strcmp(header->value, "navigate") != 0) {
		tmp = curl_slist_append(data->headers, "Upgrade-Insecure-Requests:");

		if (tmp == NULL) {
			err = KADERR_CURL_SLIST_FAILURE;
			goto end;
		}

		tmp = curl_slist_append(tmp, "Sec-Fetch-User:");

		if (tmp == NULL) {
			err = KADERR_CURL_SLIST_FAILURE;
			goto end;
		}
		
		header = http_headers_get(&data->request.headers, "Accept");
		
		if (header == NULL) {
			tmp = curl_slist_append(tmp, "Accept:");

			if (tmp == NULL) {
				err = KADERR_CURL_SLIST_FAILURE;
				goto end;
			}
		} else {
			item = malloc(strlen(header->key) + strlen(HEADER_SEPARATOR) + strlen(header->value) + 1);

			if (item == NULL) {
				err = KADERR_MEMORY_ALLOCATE_FAILURE;
				goto end;
			}

			strcpy(item, header->key);
			strcat(item, HEADER_SEPARATOR);
			strcat(item, header->value);

			tmp = curl_slist_append(tmp, item);

			free(item);
			item = NULL;
		}
		
		data->headers = tmp;
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

			if (strcasecmp(header->key, "Requested-With") == 0 && strcmp(header->value, "XMLHttpRequest") == 0) {
				status = 0;
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

	if (data->request.body.size > 0 || content_length > 0 || status || data->request_size >= MAX_HTTP_HEADERS_SIZE) {
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

	code = curl_multi_add_handle(curl_multi, curl);

	if (code != CURLM_OK) {
		loggln(LOG_ERROR, "[error] could not add handle to multi stack: %s", curl_multi_strerror(code));

		#if defined(KAD_CAPTURE)
			capture_end(&data->capture, curl);
		#endif

		curl_easy_cleanup(curl);
		curl = NULL;

		err = -1;
		goto end;
	}

	data->curl = curl;
	curl = NULL;

	data->state = CLIENT_STATE_FORWARD;

	end:;

	return err;

}

static int client_request_data(transferdata_t* const data, const char* const chunk, const size_t size) {
	/*
	Buffers the incoming request bytes and parses the request once its
	headers are complete.

	Returns (0) on success, (-1) when the client was destroyed.
	*/

	char* s = NULL;
	char* uri = NULL;

	int code = 0;

	size_t parse_size = 0;
	size_t excess_size = 0;

	s = realloc(data->in.s, data->in.slength + size + 1);

	if (s == NULL) {
		loggln(LOG_ERROR, "[error] %s", strkaderr(KADERR_MEMORY_ALLOCATE_FAILURE));

		client_kill(data);

		return -1;
	}

	memcpy(s + data->in.slength, chunk, size);

	data->in.s = s;
	data->in.slength += size;
	data->in.s[data->in.slength] = '\0';

	if (!client_headers_complete(&data->in) && data->in.slength < MAX_HTTP_HEADERS_SIZE) {
		return 0;
	}

	/* the headers are complete, or the maximum size was exceeded; this
	   emulates the single recv() call of the previous threaded design, so
	   at most MAX_HTTP_HEADERS_SIZE bytes are parsed and anything beyond
	   them is request body that is kept buffered */

	parse_size = (data->in.slength > MAX_HTTP_HEADERS_SIZE) ? MAX_HTTP_HEADERS_SIZE : data->in.slength;

	data->request_size = parse_size;

	http_request_init(&data->request);

	code = http_request_parse(&data->request, data->in.s, parse_size);

	if (code != KADERR_SUCCESS) {
		loggln(LOG_ERROR, "[error] %s", strkaderr(code));

		client_kill(data);

		return -1;
	}

	excess_size = data->in.slength - parse_size;

	if (excess_size > 0) {
		s = realloc(data->request.body.content, data->request.body.size + excess_size);

		if (s == NULL) {
			loggln(LOG_ERROR, "[error] %s", strkaderr(KADERR_MEMORY_ALLOCATE_FAILURE));

			client_kill(data);

			return -1;
		}

		memcpy(s + data->request.body.size, data->in.s + parse_size, excess_size);

		data->request.body.content = s;
		data->request.body.size += excess_size;
	}

	buffer_free(&data->in);

	if (data->request.method == CONNECT && (data->flags & TD_SECURE) == 0) {
		/* the client wants to tunnel a secure connection through the
		   proxy; accept the tunnel and start the TLS handshake */

		data->hostname = data->request.uri;
		data->request.uri = NULL;

		http_request_free(&data->request);

		if (client_write(data, "HTTP/1.0 200 OK\r\n\r\n", 19) != 0) {
			loggln(LOG_ERROR, "[error] %s", strkaderr(KADERR_SOCKET_SEND_FAILURE));

			client_kill(data);

			return -1;
		}

		if (ssl_init(&data->context, &data->fd) == -1) {
			loggln(LOG_ERROR, "[error] %s", strkaderr(KADERR_SSL_INIT_FAILURE));

			client_kill(data);

			return -1;
		}

		data->flags |= TD_SECURE;
		data->state = CLIENT_STATE_HANDSHAKE;

		return 0;
	}

	if (data->hostname != NULL) {
		/* the request was tunneled through a TLS connection; rebuild the
		   absolute HTTPS url from the tunnel target and the request path */

		uri = malloc(strlen(HTTPS_SCHEME) + strlen(SCHEME_SEPARATOR) + strlen(data->hostname) + strlen(data->request.uri) + 1);

		if (uri == NULL) {
			loggln(LOG_ERROR, "[error] %s", strkaderr(KADERR_MEMORY_ALLOCATE_FAILURE));

			client_kill(data);

			return -1;
		}

		strcpy(uri, HTTPS_SCHEME);
		strcat(uri, SCHEME_SEPARATOR);
		strcat(uri, data->hostname);
		strcat(uri, data->request.uri);

		free(data->hostname);
		data->hostname = NULL;

		free(data->request.uri);
		data->request.uri = uri;
	}

	loggln(LOG_INFO, "[info] client request to %s", data->request.uri);

	code = client_start_transfer(data);

	if (code > 0) {
		loggln(LOG_ERROR, "[error] %s", strkaderr(code));

		client_kill(data);

		return -1;
	}

	if (code < 0) {
		client_kill(data);

		return -1;
	}

	return 0;

}

static int client_read(transferdata_t* const data) {
	/*
	Drives the client connection: the TLS handshake, the reading of the
	requests, and the forwarding of request bodies to cURL.

	Returns (0) on success, (-1) when the client was destroyed.
	*/

	char chunk[MAX_CHUNK_SIZE];

	char* s = NULL;

	ssize_t size = 0;

	size_t forward_size = 0;

	int wait = 0;
	int status = 0;

	int readable = 1;

	while (readable && (data->flags & TD_EOF) == 0) {
		readable = 0;

		if (data->state == CLIENT_STATE_HANDSHAKE) {
			wait = 0;

			status = ssl_accept(&data->context, &wait);

			if (status == 1) {
				data->state = CLIENT_STATE_REQUEST;
				readable = 1;

				continue;
			}

			if (status == 0) {
				data->wait |= wait;

				break;
			}

			loggln(LOG_ERROR, "[error] %s", strkaderr(KADERR_SSL_INIT_FAILURE));

			client_kill(data);

			return -1;
		}

		wait = 0;

		/* the lingering phase reads and discards the raw bytes left in
		   the connection, as the TLS layer is gone by then */

		size = ((data->flags & TD_SECURE) != 0 && (data->flags & TD_FIN) == 0)
			? ssl_recv(&data->context, chunk, sizeof(chunk), &wait)
			: socket_read(data->fd, chunk, sizeof(chunk), &wait);

		if (size == -2) {
			/* the client orderly closed the connection */

			if (data->state == CLIENT_STATE_REQUEST || (data->state == CLIENT_STATE_FORWARD && data->remaining > 0)) {
				loggln(LOG_ERROR, "[error] %s", strkaderr(KADERR_SOCKET_RECV_FAILURE));

				client_kill(data);

				return -1;
			}

			data->flags |= TD_EOF;

			break;
		}

		if (size == -1) {
			loggln(LOG_ERROR, "[error] %s", strkaderr(KADERR_SOCKET_RECV_FAILURE));

			client_kill(data);

			return -1;
		}

		if (size == 0) {
			data->wait |= wait;

			break;
		}

		if (data->state == CLIENT_STATE_REQUEST) {
			if (client_request_data(data, chunk, (size_t) size) != 0) {
				return -1;
			}

			readable = 1;

			continue;
		}

		/* forward what is left of the request body to cURL; anything
		   beyond its declared size is discarded, exactly like in the
		   previous threaded design */

		if (data->state == CLIENT_STATE_FORWARD && data->remaining > data->request.body.size) {
			forward_size = data->remaining - data->request.body.size;

			if (forward_size > (size_t) size) {
				forward_size = (size_t) size;
			}

			s = realloc(data->request.body.content, data->request.body.size + forward_size);

			if (s == NULL) {
				loggln(LOG_ERROR, "[error] %s", strkaderr(KADERR_MEMORY_ALLOCATE_FAILURE));

				client_kill(data);

				return -1;
			}

			memcpy(s + data->request.body.size, chunk, forward_size);

			data->request.body.content = s;
			data->request.body.size += forward_size;

			/* data->remaining is only decremented as cURL consumes the
			   buffered body, exactly like in the previous design */

			if ((data->flags & TD_PAUSE_UP) != 0 && data->curl != NULL) {
				curl_easy_pause(data->curl, CURLPAUSE_SEND_CONT);

				data->flags &= ~TD_PAUSE_UP;

				/* the client cannot be destroyed while the transfer is
				   being resumed, but it may be destroyed right after */

				if (client_kick(data) != 0) {
					return -1;
				}

				return 0;
			}
		}

		readable = 1;
	}

	return 0;

}

static void client_event_cb(evutil_socket_t fd, short events, void* arg) {

	transferdata_t* const data = (transferdata_t*) arg;

	int dead = 0;

	(void) fd;

	data->wait = 0;

	if ((events & EV_TIMEOUT) != 0) {
		/* the lingering deadline ran out */

		client_destroy(data);

		return;
	}

	if ((events & EV_WRITE) != 0) {
		if (data->state == CLIENT_STATE_HANDSHAKE) {
			if (client_read(data) != 0) {
				dead = 1;
			}
		} else if (client_flush(data) != 0) {
			client_kill(data);

			dead = 1;
		} else if (data->state == CLIENT_STATE_CLOSING && data->out.slength == 0) {
			if (client_close_or_destroy(data) != 0) {
				dead = 1;
			}
		}
	}

	if (!dead && (events & EV_READ) != 0) {
		if (client_read(data) != 0) {
			dead = 1;
		}
	}

	if (!dead && data->state == CLIENT_STATE_CLOSING && (data->flags & TD_FIN) != 0 && (data->flags & TD_EOF) != 0) {
		/* the client finished sending after its answer was delivered */

		client_destroy(data);

		dead = 1;
	}

	if (!dead) {
		/* resume the download once the client caught up with the data that
		   was previously buffered */

		if ((data->flags & TD_PAUSE_DOWN) != 0 && data->curl != NULL && data->out.slength < CLIENT_OUTPUT_MIN_SIZE) {
			curl_easy_pause(data->curl, CURLPAUSE_RECV_CONT);

			data->flags &= ~TD_PAUSE_DOWN;

			if (client_kick(data) != 0) {
				dead = 1;
			}
		}
	}

	if (!dead) {
		client_event_update(data);
	}

}

static void curl_socket_event_cb(evutil_socket_t fd, short events, void* arg) {

	curl_socket_context_t* const context = (curl_socket_context_t*) arg;

	int running = 0;
	int flags = 0;

	CURLMcode code = CURLM_OK;

	(void) fd;

	if ((events & EV_READ) != 0) {
		flags |= CURL_CSELECT_IN;
	}

	if ((events & EV_WRITE) != 0) {
		flags |= CURL_CSELECT_OUT;
	}

	code = curl_multi_socket_action(curl_multi, context->sockfd, flags, &running);

	if (code != CURLM_OK) {
		loggln(LOG_ERROR, "[error] %s: %s", strkaderr(KADERR_CURL_MULTI_ACTION_FAILURE), curl_multi_strerror(code));

		return;
	}

	curl_check_completed();

}

static curl_socket_context_t* curl_socket_context_new(const curl_socket_t sockfd) {

	curl_socket_context_t* const context = malloc(sizeof(*context));

	if (context == NULL) {
		return NULL;
	}

	context->sockfd = sockfd;
	context->event = event_new(event_base, sockfd, 0, curl_socket_event_cb, context);

	if (context->event == NULL) {
		free(context);

		return NULL;
	}

	return context;

}

static int curl_handle_socket(CURL* easy, curl_socket_t s, int action, void* userp, void* socketp) {

	curl_socket_context_t* context = NULL;

	short events = 0;

	(void) easy;
	(void) userp;

	switch (action) {
		case CURL_POLL_IN:
		case CURL_POLL_OUT:
		case CURL_POLL_INOUT: {
			context = (socketp != NULL) ? (curl_socket_context_t*) socketp : curl_socket_context_new(s);

			if (context == NULL) {
				return -1;
			}

			curl_multi_assign(curl_multi, s, (void*) context);

			if (action != CURL_POLL_IN) {
				events |= EV_WRITE;
			}

			if (action != CURL_POLL_OUT) {
				events |= EV_READ;
			}

			events |= EV_PERSIST;

			event_del(context->event);
			event_assign(context->event, event_base, context->sockfd, events, curl_socket_event_cb, context);
			event_add(context->event, NULL);

			break;
		}
		case CURL_POLL_REMOVE: {
			if (socketp != NULL) {
				context = (curl_socket_context_t*) socketp;

				event_del(context->event);
				event_free(context->event);

				free(context);

				curl_multi_assign(curl_multi, s, NULL);
			}

			break;
		}
		default: {
			return -1;
		}
	}

	return 0;

}

static int curl_handle_timeout(CURLM* multi, long timeout_ms, void* userp) {

	struct timeval tv;

	(void) multi;
	(void) userp;

	if (timeout_ms < 0) {
		evtimer_del(curl_timeout_event);
	} else {
		if (timeout_ms == 0) {
			timeout_ms = 1;
		}

		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = (timeout_ms % 1000) * 1000;

		evtimer_del(curl_timeout_event);
		evtimer_add(curl_timeout_event, &tv);
	}

	return 0;

}

static void curl_timeout_event_cb(evutil_socket_t fd, short events, void* arg) {

	int running = 0;

	CURLMcode code = CURLM_OK;

	(void) fd;
	(void) events;
	(void) arg;

	code = curl_multi_socket_action(curl_multi, CURL_SOCKET_TIMEOUT, 0, &running);

	if (code != CURLM_OK) {
		loggln(LOG_ERROR, "[error] %s: %s", strkaderr(KADERR_CURL_MULTI_ACTION_FAILURE), curl_multi_strerror(code));

		return;
	}

	curl_check_completed();

}

static int curl_retryable(CURL* const curl, const CURLcode code) {

	switch (code) {
		case CURLE_HTTP_RETURNED_ERROR: {
			long status_code = 0;

			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

			if (status_code == 408 || status_code == 429 || status_code == 500 ||
				status_code == 502 || status_code == 503 || status_code == 504) {
				return 1;
			}

			break;
		}
		case CURLE_SEND_ERROR:
		case CURLE_OPERATION_TIMEDOUT:
		case CURLE_PARTIAL_FILE:
		case CURLE_RECV_ERROR:
		case CURLE_SSL_CONNECT_ERROR:
			return 1;
		default:
			break;
	}

	return 0;

}

static int client_retryable(transferdata_t* const data, CURL* const curl, const CURLcode code) {

	switch (data->request.method) {
		case GET:
		case HEAD:
		case PUT:
		case DELETE:
		case OPTIONS:
		case TRACE:
			break;
		default:
			return 0;
	}

	/* the request can only be replayed while no response bytes were
	   delivered to the client and its body was not consumed by cURL */

	if (!data->retryable) {
		return 0;
	}

	if (data->retries >= retry_limit) {
		return 0;
	}

	return curl_retryable(curl, code);

}

static void curl_check_completed(void) {

	CURLMsg* msg = NULL;

	transferdata_t* data = NULL;

	CURLcode easy_code = 0;
	CURLMcode code = CURLM_OK;

	int left = 0;

	while ((msg = curl_multi_info_read(curl_multi, &left)) != NULL) {
		if (msg->msg != CURLMSG_DONE) {
			continue;
		}

		if (msg->data.result != CURLE_OK) {
			loggln(LOG_ERROR, "[error] curl: %s", curl_easy_strerror(msg->data.result));
		}

		easy_code = curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &data);

		if (easy_code != CURLE_OK) {
			loggln(LOG_ERROR, "[error] %s: %s", strkaderr(KADERR_CURL_GETINFO_FAILURE), curl_easy_strerror(easy_code));

			curl_multi_remove_handle(curl_multi, msg->easy_handle);
			curl_easy_cleanup(msg->easy_handle);

			continue;
		}

		code = curl_multi_remove_handle(curl_multi, msg->easy_handle);

		if (code != CURLM_OK) {
			loggln(LOG_ERROR, "[error] %s: %s", strkaderr(KADERR_CURL_MULTI_REMOVE_FAILURE), curl_multi_strerror(code));
		}

		if (msg->data.result != CURLE_OK && client_retryable(data, msg->easy_handle, msg->data.result)) {
			data->retries++;

			/* the transfer failed, but it is still retryable; discard any
			   partially received response data and queue it again */

			buffer_free(&data->buffer);

			data->flags &= ~(TD_PAUSE_DOWN | TD_PAUSE_UP);

			loggln(LOG_WARN, "[warning] request to %s failed (%s), retrying (attempt %i of %i)", data->request.uri, curl_easy_strerror(msg->data.result), data->retries, retry_limit);

			code = curl_multi_add_handle(curl_multi, msg->easy_handle);

			if (code == CURLM_OK) {
				continue;
			}

			loggln(LOG_ERROR, "[error] could not add handle to multi stack: %s", curl_multi_strerror(code));
		}

		#if defined(KAD_CAPTURE)
			capture_end(&data->capture, msg->easy_handle);
		#endif

		curl_easy_cleanup(msg->easy_handle);

		data->curl = NULL;

		if (data->out.slength > 0) {
			/* the transfer is done; flush the pending response bytes to
			   the client before closing the connection */

			data->state = CLIENT_STATE_CLOSING;

			client_event_update(data);
		} else if (curl_depth > 0 && (data->remaining == 0 || (data->flags & TD_EOF) != 0)) {
			/* nested inside the callback of the client itself; its memory
			   must not be released yet */

			data->flags |= TD_DESTROY;
		} else if (curl_depth > 0) {
			/* nested, but the client may still be sending; linger until
			   the callback returns, then drain */

			data->state = CLIENT_STATE_CLOSING;

			client_linger_close(data);
		} else {
			client_close_or_destroy(data);
		}
	}

}

static void accept_event_cb(evutil_socket_t listenfd, short events, void* arg) {

	struct sockaddr_storage client_address;

	socklen_t client_address_size = 0;

	transferdata_t* data = NULL;

	system_error_t error = {0};

	char host[NI_MAXHOST];
	char servname[NI_MAXSERV];

	int cfd = -1;
	int status = 0;

	(void) events;
	(void) arg;

	while (1) {
		memset(&client_address, 0, sizeof(client_address));

		client_address_size = sizeof(client_address);

		cfd = accept(listenfd, (struct sockaddr*) &client_address, &client_address_size);

		if (cfd == -1) {
#if defined(_WIN32)
			status = WSAGetLastError();

			if (status == WSAEWOULDBLOCK || status == WSAEINTR || status == WSAECONNRESET) {
				break;
			}
#else
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == ECONNABORTED) {
				break;
			}
#endif
			error = get_system_error();

			loggln(LOG_ERROR, "[error] could not accept incoming socket connection: %s", error.message);

			break;
		}

		if (socket_nonblocking(cfd) == -1) {
			error = get_system_error();

			loggln(LOG_ERROR, "[error] could not set socket to non-blocking mode: %s", error.message);

			socket_close(cfd);

			continue;
		}

		status = getnameinfo((struct sockaddr*) &client_address, client_address_size, host, sizeof(host), servname, sizeof(servname), NI_NUMERICHOST | NI_NUMERICSERV);

		if (status != 0) {
			loggln(LOG_ERROR, "[error] could not get address info: %s", gai_strerror(status));

			socket_close(cfd);

			continue;
		}

		loggln(LOG_INFO, "[info] got connection from %s on port %s", host, servname);

		data = calloc(1, sizeof(*data));

		if (data == NULL) {
			loggln(LOG_ERROR, "[error] %s", strkaderr(KADERR_MEMORY_ALLOCATE_FAILURE));

			socket_close(cfd);

			continue;
		}

		data->fd = cfd;
		data->state = CLIENT_STATE_REQUEST;

		data->event = event_new(event_base, cfd, 0, client_event_cb, data);

		if (data->event == NULL) {
			loggln(LOG_ERROR, "[error] %s", strkaderr(KADERR_EVENT_BASE_FAILURE));

			client_destroy(data);

			continue;
		}

		client_event_update(data);
	}

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

	struct event* listener_event = NULL;

	size_t addrsize = 0;

	char address[512] = {0};
	char loglevel[16] = "verbose";
	char cacert[4096] = {0};

	const int reuseaddr = 1;

	int port = 0;
	int value = 0;

	int index = 0;
	int version = 0;

	int code = 0;
	int err = 0;

	size_t size = 0;

	argparse_t argparse = {0};
	const arg_t* argument = NULL;

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
	
	curl_global_init(CURL_GLOBAL_ALL);

	curl_multi = curl_multi_init();

	if (curl_multi == NULL) {
		loggln(LOG_ERROR, "[error] could not initialize multi stack");

		return EXIT_FAILURE;
	}

	event_base = event_base_new();

	if (event_base == NULL) {
		loggln(LOG_ERROR, "[error] %s", strkaderr(KADERR_EVENT_BASE_FAILURE));

		return EXIT_FAILURE;
	}

	curl_timeout_event = evtimer_new(event_base, curl_timeout_event_cb, NULL);

	if (curl_timeout_event == NULL) {
		loggln(LOG_ERROR, "[error] %s", strkaderr(KADERR_EVENT_BASE_FAILURE));

		return EXIT_FAILURE;
	}

	curl_multi_setopt(curl_multi, CURLMOPT_MAXCONNECTS, 32L);
	curl_multi_setopt(curl_multi, CURLMOPT_SOCKETFUNCTION, curl_handle_socket);
	curl_multi_setopt(curl_multi, CURLMOPT_TIMERFUNCTION, curl_handle_timeout);

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
		} else if (strcmp(argument->key, "retry") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);

				argparse_free(&argparse);

				return EXIT_FAILURE;
			}

			value = atoi(argument->value);

			if (value < KAD_RETRY_MIN || value > KAD_RETRY_MAX) {
				loggln(LOG_ERROR, "[error] retry count out of range: %i (valid: %i to %i)", value, KAD_RETRY_MIN, KAD_RETRY_MAX);

				argparse_free(&argparse);

				return EXIT_FAILURE;
			}

			retry_limit = value;
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
			loggln(LOG_ERROR, "[error] --platform requires an edge (96 to 157), chrome (99 to 160) or firefox (99 to 157) target");

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
	
	if (socket_nonblocking(fd) == -1) {
		error = get_system_error();
		socket_close(fd);
		loggln(LOG_ERROR, "[error] could not set socket to non-blocking mode: %s", error.message);

		return EXIT_FAILURE;
	}

	listener_event = event_new(event_base, fd, EV_READ | EV_PERSIST, accept_event_cb, NULL);

	if (listener_event == NULL) {
		loggln(LOG_ERROR, "[error] %s", strkaderr(KADERR_EVENT_BASE_FAILURE));

		socket_close(fd);

		return EXIT_FAILURE;
	}

	event_add(listener_event, NULL);

	/*
	Unless the user picked explicit DNS servers, hostname resolution goes
	through an embedded libunbound recursive resolver; if it cannot be
	started (e.g. this build does not bundle one) the connection setup
	falls back to the default servers below.
	*/
	if (dns_servers[0] == '\0') {
		const uint16_t unbound_port = unbound_shim_start(event_base);

		if (unbound_port > 0) {
			snprintf(dns_servers, sizeof(dns_servers), "127.0.0.1:%u", unbound_port);

			loggln(LOG_STANDARD, "Embedded resolver listening on 127.0.0.1:%u", unbound_port);
		}
	}

	loggln(LOG_STANDARD, "Starting server at http://%s:%i (pid = %i)", address, port, getpid());

	event_base_dispatch(event_base);

	unbound_shim_stop();

	event_free(listener_event);
	event_free(curl_timeout_event);
	curl_multi_cleanup(curl_multi);
	event_base_free(event_base);

	curl_global_cleanup();

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
