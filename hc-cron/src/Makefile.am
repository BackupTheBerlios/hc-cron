SUBDIRS = include
MOSTLYCLEANFILES = *~
MAINTAINERCLEANFILES = Makefile.in

sbin_PROGRAMS = crond
INCLUDES = -I$(srcdir)/include/

## crontab needs to be installed suid-root so we have to do that manually.
noinst_PROGRAMS = crontab
crond_SOURCES = cron.c database.c do_command.c entry.c env.c hccron.c job.c \
		misc.c popen.c user.c diskload.c
crontab_SOURCES = crontab.c misc.c entry.c env.c
crond_LDADD = @LIBOBJS@
install-exec-local:
	$(INSTALL_PROGRAM) -m 4755 -o root crontab $(bindir)
uninstall-local:
	rm -f $(sbindir)/crond
	rm -f $(bindir)/crontab
