# -*- Mode: makefile-gmake -*-

all:
%:
	@$(MAKE) -C test_conf $*
	@$(MAKE) -C test_creds $*
	@$(MAKE) -C test_launch $*
	@$(MAKE) -C test_launch_hook $*
	@$(MAKE) -C test_rules $*

clean: unitclean
	rm -f *~
	rm -f common/*~
	rm -f coverage/*~
	rm -f coverage/*.gcov
	rm -fr coverage/report
