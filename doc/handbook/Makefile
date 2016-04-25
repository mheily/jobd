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

all: clean build-html build-html-nochunks
	
build-html:
	xmlto -o html html book.xml

build-html-nochunks:
	xmlto -o html-nochunks html-nochunks book.xml

preview: clean build-html-nochunks
	firefox --new-window ./html-nochunks/book.html

publish:
	git checkout gh-pages
	git merge master
	xmlto html *.xml
	git add *.html
	git commit -m'publishing it'
	git push
	git checkout master

clean:
	rm -rf html html-nochunks
	mkdir html html-nochunks

.PHONY: all preview publish clean
