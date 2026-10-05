#if !defined(CONSTANTS_H)
#define CONSTANTS_H

static const char SCHEME_SEPARATOR[] = "://";

static const char PROTOCOL_NAME[] = "HTTP";
static const char CRLF[] = "\r\n";
static const char CRLFCRLF[] = "\r\n\r\n";

static const char HTTP_SCHEME[] = "http";
static const char HTTPS_SCHEME[] = "https";

static const char SLASH[] = "/";
static const char COLON[] = ":";
static const char SPACE[] = " ";

static const char HEADER_SEPARATOR[] = ": ";

static const int MAX_HTTP_HEADERS_SIZE = 10240;
static const int MAX_CHUNK_SIZE = 1024 * 4;

#endif
