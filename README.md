# Kad

Kad is a simple HTTP proxy server that forwards all requests through curl-impersonate.

## Installation

You can obtain precompiled binaries from the [releases](https://github.com/AmanoTeam/Kad/releases) page.

## Building

Clone this repository and fetch all submodules

```bash
git clone 'https://github.com/AmanoTeam/Kad.git'
git -C 'Kad' submodule update --init --depth='1'
```

Configure, build and install:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build ./build
cmake --install ./build
```

An embedded libunbound-based recursive DNS resolver is always bundled: the unbound submodule is built statically against the bundled BoringSSL and libevent and linked into the executable, and is used for hostname resolution by default instead of the fallback 8.8.8.8 and 8.8.4.4 servers; no external libunbound installation is needed.

## Usage

Available options:

```
$ kad --help
usage: kad [-h] [-v] [-k] [--host HOST] [--port PORT] [--target TARGET] [--platform PLATFORM] [--loglevel LOGLEVEL] [--proxy URL|FILE] [--doh-url URL] [--cacert FILE] [--dns-servers LIST] [--retry COUNT]

A simple HTTP proxy server that forwards all requests through curl-impersonate.

options:
  -h, --help           Display this help text and exit.
  -v, --version        Display the Kad version and exit.
  -k, --insecure       Disable SSL certificate verification.
  --host HOST          Bind socket to this host. [default: 127.0.0.1]
  --port PORT          Bind socket to this port. [default: 4000]
  --target TARGET      Impersonate this target. [default: chrome116]
  --platform PLATFORM  Impersonate this platform when the target is a generic Chrome, Edge or Firefox target (e.g. chrome131). Valid platforms: 'windows', 'macos', 'linux', 'android'.
  --loglevel LOGLEVEL  Set output verbosity. Valid levels: 'quiet', 'standard', 'warning', 'error', 'info', 'verbose'. [default: verbose]
  --proxy URL|FILE     Forward all requests through this proxy server. When FILE is given, it must contain one proxy per line and a random proxy is picked for each request.
  --doh-url URL        Use this DoH server for hostname resolution.
  --cacert FILE        Overwrite the path to the CA certificate bundle to use.
  --dns-servers LIST   Comma-separated list of DNS servers to use. [default: the embedded recursive resolver, falling back to 8.8.8.8,8.8.4.4]
  --retry COUNT        Retry idempotent requests whose transfer failed with a retryable error (send, receive or timeout failures) up to COUNT times, when no response bytes were delivered to the client yet. [default: 0]

Note, options that take a value must use an equal sign. E.g. --host=HOST
```

You can start a server with all default options by simply running `kad`:

```
$ kad
Embedded resolver listening on 127.0.0.1:38645
Starting server at http://127.0.0.1:4000 (pid = 478)

```

To forward requests through a rotating list of upstream proxies, put one proxy per line in a file and pass its path to `--proxy`:

```
$ cat proxies.txt
http://127.0.0.1:8080
socks5://127.0.0.1:9050

$ kad --proxy=proxies.txt
```

Kad picks a random proxy from the list for every request.

## Examples

Kad is just a proxy server; you need an HTTP client to start using it.

With curl:

```bash
$ curl --proxy 'http://127.0.0.1:4000' --insecure --url 'https://example.com'
```

With Python + Requests:

```python
import requests

proxies = {
    "http": "http://127.0.0.1:4000",
    "https": "http://127.0.0.1:4000"
}

response = requests.get(url = "https://example.com", proxies = proxies, verify = False)
```

With PHP + curl:

```php
<?php
$handle = curl_init();
curl_setopt($handle, CURLOPT_URL, "https://example.com");
curl_setopt($handle, CURLOPT_PROXY, "http://127.0.0.1:4000");
curl_setopt($handle, CURLOPT_SSL_VERIFYPEER, false);
curl_setopt($handle, CURLOPT_SSL_VERIFYHOST, 0);

$response = curl_exec($handle);
```

With Java + OkHttp:

```java
import java.net.InetSocketAddress;
import java.net.Proxy;
import java.security.cert.X509Certificate;

import javax.net.ssl.SSLContext;
import javax.net.ssl.X509TrustManager;

import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;

public class Main {
    public static void main(String[] args) throws Exception {
        X509TrustManager trustManager = new X509TrustManager() {
            @Override
            public void checkClientTrusted(X509Certificate[] chain, String authType) { }
            @Override
            public void checkServerTrusted(X509Certificate[] chain, String authType) { }
            @Override
            public X509Certificate[] getAcceptedIssuers() { return new X509Certificate[0]; }
        };

        SSLContext sslContext = SSLContext.getInstance("TLS");
        sslContext.init(null, new javax.net.ssl.TrustManager[] { trustManager }, null);

        OkHttpClient client = new OkHttpClient.Builder()
            .proxy(new Proxy(Proxy.Type.HTTP, new InetSocketAddress("127.0.0.1", 4000)))
            .sslSocketFactory(sslContext.getSocketFactory(), trustManager)
            .hostnameVerifier((hostname, session) -> true)
            // Kad closes the proxy connection after each response, so OkHttp's
            // pooled connections go stale; make sure it retries (the default)
            // instead of surfacing them as IOExceptions
            .retryOnConnectionFailure(true)
            .build();

        Request request = new Request.Builder().url("https://example.com").build();

        try (Response response = client.newCall(request).execute()) {
            System.out.println(response.body().string());
        }
    }
}
```

> [!IMPORTANT]
> Kad uses a self-signed [certificate](./tools/certificates/kad.crt) to decrypt requests made to HTTPS websites, so your HTTP client will refuse sending requests unless you disable SSL verification.

## WebSocket

WebSocket connections (`ws://` and `wss://`) are transparently supported: once the
handshake request passes through Kad, the raw frames of both sides are relayed
as they are, without any interpretation. Note that the impersonated handshake
request may not always match the fingerprints a browser would produce.
