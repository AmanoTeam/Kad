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
usage: kad [-h] [-v] [--host HOST] [--port PORT] [--target TARGET]

A simple HTTP proxy server that forwards all requests through curl-impersonate.

options:
  -h, --help       Show this help message and exit.
  -v, --version    Display the Kad version and exit.
  --host HOST      Bind socket to this host. [default: 127.0.0.1]
  --port PORT      Bind socket to this port. [default: 4000]
  --target TARGET  Impersonate this target. [default: chrome116]

Note, options that take an argument require a equal sign. E.g. --host=HOST
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
$ curl --proxy 'http://127.0.0.1:4000' --url 'http://example.com'
```

With Python + Requests:

```python
import requests

proxies = {
    "http": "http://127.0.0.1:4000",
    "https": "http://127.0.0.1:4000"
}

response = requests.get(url = "http://example.com", proxies = proxies)
```

With PHP + curl:

```php
<?php
$handle = curl_init();
curl_setopt($handle, CURLOPT_URL, "http://example.com");
curl_setopt($handle, CURLOPT_PROXY, "http://127.0.0.1:4000");

$response = curl_exec($handle);
```

## HTTPS connections

Kad uses a self-signed certificate so it can decrypt requests made to HTTPS websites. Most HTTP clients will refuse sending requests to servers like this.

To circumvent this, you need to disable SSL certificate validation in your HTTP client (not recommended) or add Kad's [certificate](./tools/certificates/kad.crt) to your trust store (e.g., `/etc/ssl/certs`).

## Limitations

- Windows is not supported for now ([#1](https://github.com/AmanoTeam/Kad/issues/1))
- Only supports impersonating Chrome, Edge and Safari

