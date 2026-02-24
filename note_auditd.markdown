man : https://manpages.ubuntu.com/manpages/trusty/en/man8/auditd.8.html
(linux audit deamon)
auditd est un program qui va trace des events encore plus precisement pas seulement les events mais aussi ceux lié au uid, id, pid

    -f     leave the audit daemon in the foreground for debugging.
    Messages also go to stderr rather than the audit log.

    -l     allow the audit daemon to follow symlinks for config files.

    -n     no fork. This is useful for running off of inittab or systemd.

    -s=ENABLE_STATE

creé une regle pour surveille les "execve" de user ciblé :
sudo auditctl -a always,exit -F arch=b64 -S execve -F auid=1000 -k bde_exec

auditctl [options]
The  auditctl  program  is used to control the behavior, get status, and add or delete rules into the 2.6 kernel's audit system.

ausearch [options]
ausearch  is  a  tool  that  can  query  the audit daemon logs based for events based on different search criteria.


id : bde-wits
uid=1000(bde-wits) gid=1000(bde-wits) groups=1000(bde-wits),
4(adm),24(cdrom),27(sudo),30(dip),46(plugdev),100(users),989(docker)

difference entre les outils sur ubuntu / mac :

Ubuntu	     /      macOS
auditd       =      OpenBSM (Endpoint Security Framework, API Apple)
auditctl	 =      audit -s
ausearch	 =      praudit

