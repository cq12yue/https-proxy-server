#Makefile 

dst_exe_name := nti-proxy-2.0.1
dst_exe_dir := /usr/bin/$(dst_exe_name)
dst_lib_dir := /usr/local/lib
make_file := Makefile
out_dir := normal

.PHONY: all release debug clean cleand cleanr install uninstall
all: debug release
clean: cleand cleanr

debug:
	@mkdir -p ../output/lib/debug	
	@mkdir -p ../output/$(out_dir)/debug
	$(MAKE) debug -C base
	$(MAKE) debug -C mem
	$(MAKE) debug -C net 
	$(MAKE) debug -C ipc 
	$(MAKE) debug -C log 
	$(MAKE) -f $(make_file) debug -C logsrv
	$(MAKE) -f $(make_file) debug -C core

release:
	@mkdir -p ../output/lib/release	
	@mkdir -p ../output/$(out_dir)/release
	$(MAKE) release -C base
	$(MAKE) release -C mem
	$(MAKE) release -C net
	$(MAKE) release -C ipc
	$(MAKE) release -C log
	$(MAKE) -f $(make_file) release -C logsrv
	$(MAKE) -f $(make_file) release -C core

cleand:
	$(MAKE) cleand -C base
	$(MAKE) cleand -C mem
	$(MAKE) cleand -C net
	$(MAKE) cleand -C ipc
	$(MAKE) cleand -C log
	$(MAKE) -f $(make_file) cleand -C logsrv
	$(MAKE) -f $(make_file) cleand -C core
	@rm -rf ../output/lib/debug
	@rm -rf ../output/$(out_dir)/debug

cleanr:
	$(MAKE) cleanr -C base
	$(MAKE) cleanr -C mem
	$(MAKE) cleanr -C net
	$(MAKE) cleanr -C ipc
	$(MAKE) cleanr -C log
	$(MAKE) -f $(make_file) cleand -C logsrv
	$(MAKE) -f $(make_file) cleanr -C core
	@rm -rf ../output/lib/release
	@rm -rf ../output/$(out_dir)/release

installdirs:
	@mkdir -p $(dst_exe_dir)
	@mkdir -p /var/log/$(dst_exe_name)

install: installdirs
	@cp ../output/$(out_dir)/debug/nti_proxy $(dst_exe_dir)
	@cp ../output/$(out_dir)/release/nti_logsrv $(dst_exe_dir)
	@cp ../script/perf $(dst_exe_dir)
	@cp ../script/watch $(dst_exe_dir)
	@cp ../output/lib/release/* $(dst_lib_dir)
	@cp ../data/* $(dst_exe_dir)
	@cp ../config/server.cfg $(dst_exe_dir)
	@../script/coredump
	@chmod a+x $(dst_exe_dir)/perf
	@chmod a+x $(dst_exe_dir)/watch
	@ldconfig

uninstall:
	@rm -rf $(dst_exe_dir)
	@rm -f $(dst_lib_dir)/libmempool.* $(dst_lib_dir)/libnetcomm.* $(dst_lib_dir)/libbase.* $(dst_lib_dir)/libipc.* $(dst_lib_dir)/liblog.*
