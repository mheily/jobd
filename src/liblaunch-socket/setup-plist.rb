#!/usr/bin/env ruby
#
# Copyright (c) 2015 Mark Heily <mark@heily.com>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

require 'socket'

pwd = `pwd`.chomp
plist = "sa-wrapper.json"
f = File.open(plist, "w")
f.puts <<"__MANIFEST__"
{
  "UserName": "nobody",
  "GroupName": "nogroup",
  "Program": "#{pwd}/test-wrapper",
  "EnvironmentVariables": {"LD_PRELOAD":"sa-wrapper.so"},
  "EnableGlobbing": true,
  "WorkingDirectory": "/",
  "RootDirectory": "/",
  "StandardInPath": "/dev/null",
  "StandardOutPath": "#{pwd}/test-wrapper.out",
  "StandardErrorPath": "#{pwd}/test-wrapper.err",
  "Sockets": {
    "MyService": {
      "SockServiceName": "8088",
    },
  },
  "Label": "test.sa-wrapper",
}
__MANIFEST__

f.close
system "../launchctl load #{plist}" or raise "launchctl failed"
sleep 2
puts "plist:\n"
system "cat #{plist}"
File.unlink plist

exit 0
