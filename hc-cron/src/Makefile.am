SUBDIRS = include
EXTRA_DIST = cron.conf
MOSTLYCLEANFILES = *~ *.s
MAINTAINERCLEANFILES = Makefile.in

sbin_PROGRAMS = crond
INCLUDES = -I$(srcdir)/include/

## crontab needs to be installed suid so we have to do that manually.
noinst_PROGRAMS = crontab
crond_SOURCES = cron.c database.c do_command.c entry.c env.c hccron.c job.c \
		misc.c read_config.c popen.c user.c diskload.c
crontab_SOURCES = crontab.c misc.c read_config.c entry.c env.c
crond_LDADD = @LIBOBJS@
install-exec-local:
	$(INSTALL_PROGRAM) -m 4755 -o daemon crontab $(bindir)
	$(INSTALL_PROGRAM) cron.conf $(sysconfdir)
uninstall-local:
	rm -f $(sbindir)/crond
	rm -f $(bindir)/crontab
