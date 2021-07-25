all: lib src

.PHONY: lib
lib:
	make -C lib

.PHONY: src
src: lib
	make -C src

.PHONY: clean
clean:
	make -C lib clean
	make -C src clean

.PHONY: autobuild
autobuild:
	monex.py lib/libmc.a -c "make -C src clean all"
