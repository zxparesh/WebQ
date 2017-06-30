### Scripts for running tests

The scripts are listed in the order of importance


1. `redeploy.sh` - The most frequently used script for deploying the
   code.  The script internally access `killall.sh` and `ips.sh`.  The
   host machine should have `sshpass` and `lynx` installed for this
   script to run successfully.  The script requires ssh access to all
   the component machines.  Use `makeKnownHost.sh` to configure
   passwordless ssh access to machines.  `redeploy.sh` requires access
   to root account of the participating machines. This has to be
   setup.  In Ubuntu you have to explicitly enable the root login.
   [Enable root login in Ubuntu](http://askubuntu.com/questions/44418/how-to-enable-root-login)
2. `ips.sh` - This file serves as as a single point for modifying ips
   of the current deployment. The variables in this script must be
   updated to correct values for `redeploy.sh` to function.
   Make sure if contains all ips of all TokeGens machines.
3. `makeKnownHost.sh` - Setup a password less login to target machine
   and user. (same job as `ssh-copy-id`)
4. `backup_plog.sh`, `copy_peers.sh`, `peer_list` and `run_peers.sh`
   are for ease of development, not necessary for running WebQ.
   Their usage method and description are inside script file itself.
