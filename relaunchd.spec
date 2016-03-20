#
# Copyright (c) 2016 Mark Heily <mark@heily.com>
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

Name:       relaunchd
Summary:    a service manager similar to Darwin's relauchd(8)
Version:    0.4.1
Release:    1
License:    BSD
Vendor:     Mark Heily
Group:      System Environment/Daemons
Source0:    %{name}-%version.tar.gz

BuildRequires: glibc-devel

%description

relaunchd is a service management daemon that is similar to the launchd(8)
facility found in the Darwin operating environment.

It is currently under heavy development, and should not be used for anything
important. Be especially mindful that there is NO WARRANTY provided with this
software.

%prep
%setup -q -n relaunchd-0.4.1

%build
./configure --prefix=/usr
make

%install
make DESTDIR=%{buildroot} install

%clean
[ %{buildroot} != "/" ] && rm -rf %{buildroot}

%files
%defattr(-,root,root)

%{_bindir}/launchctl
%{_sbindir}/launchd
%{_sysconfdir}/rc.d/init.d/httpd
%{_mandir}/man1/launchctl.1.gz
%{_mandir}/man5/launchd.plist.5.gz
%{_mandir}/man/man8/launchd.8.gz
%dir %{_sysconfdir}/launchd/agents
%dir %{_sysconfdir}/launchd/daemons
%dir %{_datadir}/launchd/agents
%dir %{_datadir}/launchd/daemons

%changelog
* Sat Mar 19 2016 Mark Heily <mark@heily.com> - 0.4.1-1
- initial release of the spec file
