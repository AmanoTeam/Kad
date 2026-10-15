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

## Usage

Available options:

```
$ kad --help
usage: kad [-h] [-v] [--host HOST] [--port PORT] [--target TARGET] [--platform PLATFORM] [--loglevel LOGLEVEL] [--proxy URL] [--doh-url URL] [--cacert FILE] [--dns-servers LIST]

A simple HTTP proxy server that forwards all requests through curl-impersonate.

options:
  -h, --help           Display this help text and exit.
  -v, --version        Display the Kad version and exit.
  --host HOST          Bind socket to this host. [default: 127.0.0.1]
  --port PORT          Bind socket to this port. [default: 4000]
  --target TARGET      Impersonate this target. [default: chrome116]
  --platform PLATFORM  Impersonate this platform when the target is a generic Chrome, Edge or Firefox target (e.g. chrome131). Valid platforms: 'windows', 'macos', 'linux', 'android'.
  --loglevel LOGLEVEL  Set output verbosity. Valid levels: 'quiet', 'standard', 'warning', 'error', 'info', 'verbose'. [default: verbose]
  --proxy URL          Forward all requests through this proxy server.
  --doh-url URL        Use this DoH server for hostname resolution.
  --cacert FILE        Overwrite the path to the CA certificate bundle to use.
  --dns-servers LIST   Comma-separated list of DNS servers to use. [default: 8.8.8.8,8.8.4.4]

Note, options that take a value must use an equal sign. E.g. --host=HOST
```

You can start a server with all default options by simply running `kad`:

```
$ kad
Starting server at http://127.0.0.1:4000 (pid = 478)

```

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
