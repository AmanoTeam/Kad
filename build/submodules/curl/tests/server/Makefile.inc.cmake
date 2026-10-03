#***************************************************************************
#                                  _   _ ____  _
#  Project                     ___| | | |  _ \| |
#                             / __| | | | |_) | |
#                            | (__| |_| |  _ <| |___
#                             \___|\___/|_| \_\_____|
#
# Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution. The terms
# are also available at https://curl.se/docs/copyright.html.
#
# You may opt to use, copy, modify, merge, publish, distribute and/or sell
# copies of the Software, and permit persons to whom the Software is
# furnished to do so, under the terms of the COPYING file.
#
# This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# KIND, either express or implied.
#
# SPDX-License-Identifier: curl
#
###########################################################################
# Shared between CMakeLists.txt and Makefile.am

set(BUNDLE servers)

# Files referenced from the bundle source
set(FIRST_C first.c)
set(FIRST_H first.h)

# Common files used by test programs
set(UTILS_C getpart.c util.c)
set(UTILS_H )

set(CURLX_C 
  ../../lib/curlx/base64.c 
  ../../lib/curlx/fopen.c 
  ../../lib/curlx/inet_ntop.c 
  ../../lib/curlx/inet_pton.c 
  ../../lib/curlx/multibyte.c 
  ../../lib/curlx/nonblock.c 
  ../../lib/curlx/snprintf.c 
  ../../lib/curlx/strcopy.c 
  ../../lib/curlx/strerr.c 
  ../../lib/curlx/strparse.c 
  ../../lib/curlx/timediff.c 
  ../../lib/curlx/timeval.c 
  ../../lib/curlx/wait.c 
  ../../lib/curlx/warnless.c 
  ../../lib/curlx/winapi.c)

set(TOOLX_C 
  ../../src/toolx/tool_time.c)

# All test servers
set(TESTS_C 
  dnsd.c 
  mqttd.c 
  resolve.c 
  rtspd.c 
  sockfilt.c 
  socksd.c 
  sws.c 
  tftpd.c)
