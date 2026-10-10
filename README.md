# Kad

Kad is a simple HTTP proxy server that forwards all requests through curl-impersonate.

## Installation

You can obtain precompiled binaries from the [releases](https://github.com/AmanoTeam/Kad/releases) page.

## Building

Clone this repository and fetch all submodules

```bash
git clone --depth='1' 'https://github.com/AmanoTeam/Kad.git'
cd Kad
git submodule update --init --depth='1'
```

Configure, build and install:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build ./build
cmake --install ./build
```

## Usage

Available options:

```
$ kad --help
usage: kad [-h] [-v] [--host HOST] [--port PORT] [--target TARGET] [--loglevel LOGLEVEL]

A simple HTTP proxy server that forwards all requests through curl-impersonate.

options:
  -h, --help           Display this help text and exit.
  -v, --version        Display the Kad version and exit.
  --host HOST          Bind socket to this host. [default: 127.0.0.1]
  --port PORT          Bind socket to this port. [default: 4000]
  --target TARGET      Impersonate this target. [default: chrome116]
  --loglevel LOGLEVEL  Set output verbosity. Valid levels: 'quiet', 'standard', 'warning', 'error', 'info', 'verbose'. [default: verbose]

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

## HTTPS connections

Kad uses a self-signed [certificate](./tools/certificates/kad.crt) to decrypt requests made to HTTPS websites, so your HTTP client will refuse sending requests unless you disable SSL verification (not recommended).

## Limitations

- Only supports impersonating Chrome, Edge and Safari

