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

package configurehelp;

use strict;
use warnings;
use Exporter;

use vars qw(
    @ISA
    @EXPORT_OK
    $Cpreprocessor
    );

@ISA = qw(Exporter);

@EXPORT_OK = qw(
    $Cpreprocessor
    );

$Cpreprocessor = '"/data/data/com.termux/files/usr/bin/gcc-toolchain/cc" -E  -I/sdcard/Kad/submodules/curl/include -I/sdcard/Kad/build/submodules/curl/lib -I/sdcard/Kad/submodules/curl/lib';

1;
