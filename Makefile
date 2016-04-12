all: INDEX push

INDEX: *.json
	ls *.json | sort > INDEX

push:
	git push

.PHONY : INDEX push
