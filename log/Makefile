#Makefile for log module

out_name := log
src_path := .
out_path := ../../output/lib
share_path := ../../output/lib
share_lib := ipc

.PHONY: all debug release clean cleand cleanr

all: debug release
clean: cleand cleanr
	
debug: out_path := $(out_path)/debug
debug: share_path := $(share_path)/debug
release: out_path := $(out_path)/release
release: share_path := $(share_path)/release

export CXX := gcc

debug:
	$(MAKE) -f ../Makefile.in lib MODE=debug OUT_NAME=$(out_name) SRC_PATH=$(src_path) OUT_PATH=$(out_path) SHARE_LIB=$(share_lib) SHARE_PATH=$(share_path)

release:
	$(MAKE) -f ../Makefile.in lib MODE=release OUT_NAME=$(out_name) SRC_PATH=$(src_path) OUT_PATH=$(out_path) SHARE_LIB=$(share_lib) SHARE_PATH=$(share_path)

cleand:
	$(MAKE) -f ../Makefile.in clean MODE=debug OUT_NAME=$(out_name) SRC_PATH=$(src_path)

cleanr:
	$(MAKE) -f ../Makefile.in clean MODE=release OUT_NAME=$(out_name) SRC_PATH=$(src_path)
