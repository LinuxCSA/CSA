# Copyright (c) 2000-2007 Silicon Graphics, Inc.
# All Rights Reserved.

noinst_SCRIPTS = job

DISTRO := $(shell rpmbuild --showrc | \
		awk '$$1 == "-14:" && $$2 == "_vendor" { print $$3}')

job: job_$(DISTRO).in
	rm -f $@
	sed -e 's%\@bindir\@%$(bindir)%g' \
	    -e 's%\@sbindir\@%$(sbindir)%g' $< > $@
	chmod 755 $@

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(jobinitdir)
	$(INSTALL_PROGRAM) job $(DESTDIR)$(jobinitdir)/job

uninstall-local:
	rm -f $(DESTDIR)$(jobinitdir)/job

CLEANFILES = job
