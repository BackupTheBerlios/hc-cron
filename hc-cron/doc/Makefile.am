man_MANS = cron.8 crontab.1 crontab.5
noinst_MANS = bitstring.3
EXTRA_DIST = $(man_MANS) $(noinst_MANS)
MOSTLYCLEANFILES = *~
MAINTAINERCLEANFILES = Makefile.in
install-data-local:
	$(LN_S) $(man8dir)/cron.8 $(man8dir)/crond.8
uninstall-local:
	rm -f $(man8dir)/crond.8
