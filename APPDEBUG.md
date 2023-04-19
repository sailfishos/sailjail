# Debugging sailjail application sandboxing

Basically debugging existing saljail application profiles is very
similar to creating and debugging firejail profile in learn mode: enable
sufficiently verbose tracing, reproduce the problem and inspect traces
to find out what is causing the sandboxed application to misbehave.

The biggest difference is that the process needs to be adjusted to take
into account the fact that all permission mapping and granting
privileges must go through sailjail and other relevant compontents.

## 1. Tracing unprivileged applications

When one is dealing with an application that does not need to be
executed with privileged effective group id, one can simply execute
sailjail from command line.

As an example of unprivileged application: jolla-calculator.

### 1.1. Determining how to launch application

Locate desktop files coming from the same package as the application
itself.

    [defaultuser@Sailfish ~]$ rpm -qf /usr/bin/jolla-calculator
    jolla-calculator-1.0.5-1.12.1.jolla.armv7hl

    [defaultuser@Sailfish ~]$ rpm -ql jolla-calculator
     :
    /usr/share/applications/jolla-calculator.desktop
     :

    [defaultuser@Sailfish ~]$ grep Exec /usr/share/applications/jolla-calculator.desktop
    Exec=/usr/bin/sailjail -p jolla-calculator.desktop /usr/bin/jolla-calculator

### 1.2. Execute the application

Use the Exec line from desktop file, with `--trace` added in the
mix. `--trace` takes an existing directory as an argument.

    [defaultuser@Sailfish ~]$ mkdir jolla-calculator-trace
    [defaultuser@Sailfish ~]$ /usr/bin/sailjail --trace=jolla-calculator-trace -p jolla-calculator.desktop /usr/bin/jolla-calculator

After exiting application, log files should be in the directory given
to --trace.

    [defaultuser@Sailfish ~]$ ls -1 jolla-calculator-trace/firejail-*.log*
    firejail-dbus.log
    firejail-stderr.log
    firejail-stderr.log.1
    firejail-trace.log

## 2. Tracing privileged applications

When one is dealing with applications that need access to privileged
user data, things are a bit more complicated.

As an example of privileged application: jolla-notes.

### 2.1. Determining application launch files

Depending on application, one needs to patch application desktop and/or
dbus autolauch configuration files

    [root@Sailfish ~]# rpm -qf /usr/bin/jolla-notes
    jolla-notes-1.0.9-1.34.1.jolla.armv7hl

    [root@Sailfish ~]# rpm -ql jolla-notes
      :
    /usr/share/applications/jolla-notes.desktop
    /usr/share/dbus-1/services/com.jolla.notes.service
      :

### 2.2. Enable sailjail/firejail tracing

Basically one just needs to add suitable --trace option for sailjail
Exec line. In case several programs need to be traced simultaneously,
it might be better to use application specific target directory.

In this example we use "/tmp/notest-trace" as destination directory.

Create trace output directory, make sure it is writable by appropriate
user:

> Note: Sailjail exits if trace destination directory does not exist
> -&gt; If tmpfs destinations are used, these directories must be
> recreated after each reboot.

    [root@Sailfish ~]# mkdir /tmp/notes-trace
    [root@Sailfish ~]# chown defaultuser:defaultuser /tmp/notes-trace

To make restoring normal state easier, you might want to grab copies of
unmodified launch files,

    [root@Sailfish ~]# cp /usr/share/dbus-1/services/com.jolla.notes.service\
        /usr/share/dbus-1/services/com.jolla.notes.service.original
    [root@Sailfish ~]# cp /usr/share/applications/jolla-notes.desktop\
        /usr/share/applications/jolla-notes.desktop.original

Then modify Exec line so that --trace=/tmp/notes-trace option is passed
to sailjail.

    [root@Sailfish ~]# vim /usr/share/dbus-1/services/com.jolla.notes.service
    [root@Sailfish ~]# vim /usr/share/applications/jolla-notes.desktop

The changes should be something like this

    [root@Sailfish ~]# diff -Naur /usr/share/dbus-1/services/com.jolla.notes.service.original /usr/share/dbus-1/services/com.jolla.notes.service
    --- /usr/share/dbus-1/services/com.jolla.notes.service.original
    +++ /usr/share/dbus-1/services/com.jolla.notes.service
    @@ -1,3 +1,3 @@
     [D-BUS Service]
     Name=com.jolla.notes
    -Exec=/usr/bin/sailjail -p jolla-notes.desktop /usr/bin/jolla-notes -prestart
    +Exec=/usr/bin/sailjail --trace=/tmp/notes-trace -p jolla-notes.desktop /usr/bin/jolla-notes -prestart

    [root@Sailfish ~]# diff -Naur /usr/share/applications/jolla-notes.desktop.original /usr/share/applications/jolla-notes.desktop
    --- /usr/share/applications/jolla-notes.desktop.original
    +++ /usr/share/applications/jolla-notes.desktop
    @@ -2,7 +2,7 @@
     Type=Application
     Name=Notes
     Icon=icon-launcher-notes
    -Exec=/usr/bin/sailjail -p jolla-notes.desktop /usr/bin/jolla-notes
    +Exec=/usr/bin/sailjail --trace=/tmp/notes-trace -p jolla-notes.desktop /usr/bin/jolla-notes
     X-MeeGo-Logical-Id=notes-de-name
     X-MeeGo-Translation-Catalog=notes
     X-Maemo-Service=com.jolla.notes

### 2.3. Execute the application

The same way as one usually does, e.g. by clicking icon in the
application launcher grid.

After potential permissions issue has been reproduced, exit the
application.

    [root@Sailfish ~]# ls -1 /tmp/notes-trace
    firejail-dbus.log
    firejail-stderr.log
    firejail-stderr.log.1
    firejail-trace.log

## 3. Interpreting the trace files

After the traced application has exited, the trace directory should
contain following kinds of files: firejail output, listing of libc
function calls trapped via libtrace preload library and dbus traffic
logging from xdg-dbus-proxy instance.

Brief outline of content is listed below.

### 3.1. firejail-stderr.log

This file contains logging from firejail process itself.

The output is meant for human consumption, and any serious problems
firejail has should be fairly obvious to spot.

Additional firejail-stderr.log.N files can exist and contain output from
binaries executed for / within sandboxing.

### 3.2. firejail-trace.log

Output from libtrace.so, which is used for trapping and logging select
syscalls / libc function calls (mostly ones dealing with file i/o) that
the application makes.

If application is misbehaving but does not clearly log problems it might
be having with fs access, one can go through this log file and look for
signs of problems.

The format is one line / function call with the following fields:

    <PID>:<APPNAME>:<FUNCTION> <ARGS>:<RESULT>

For example:

    16:jolla-notes:fopen /property_contexts:(nil)
    16:jolla-notes:fopen /plat_property_contexts:0xf7f1ab8
    ^ looking up from various places, eventually succeeds -> ok

    16:jolla-notes:open /sys/class/timed_output/vibrator/enable:-1
    ^ failure to open vibrator device, known cosmetic problem

    16:jolla-notes:mkdir /run/user/100000/dconf:-1
    ^ fails because directory already exists. unfortunately
      errno is not logged so it is not always easy to tell
      apart false positives from genuine problems

### 3.3. firejail-dbus.log

This file contains logging from xdg-dbus-proxy instance.

The output is very noisy and ill suited for human consumption, but
obvious problems such missing permission to contact some dbus service /
own a service name should still be relatively easy to spot via
concentrating on hidden/skipped entries:

    grep -B2 -E '\*(HIDDEN|SKIPPED)\*' firejail-dbus.log

### 3.4 Tips

Once problems have been found in the firejail trace logs, quite often
it's easy to locate the missing permissions by grepping the sailjail
and sailjail-permissions repositories.
