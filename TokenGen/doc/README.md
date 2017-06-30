### Installing TokenGen

**Machine Setup**
Configure FastCGI for apache in the target machine. Sample `apache2_etc.conf` file is provided in
the repo for reference.

**Configuration**
The configuration setting for each TokenGen is stored in proxy.conf.
Each of the lines store a respective setting:

1. port at where proxy1 should listen.
2. token check ip.
3. branch factor
4. gossip interval
5. number of total proxies.

Subsequent lines have the ips and ids of the peers ( there should be as many lines
below as the number of proxies mentioned above). First entry should be for local
TokenGen machine and rest can be in any order. TokenGen ids should be assigned
continues, starting with 0.

sample contents of `proxy.conf`

    10000                           // port at which proxy 1 should listen
    10.129.41.67:9000               // token check ip
    1                               // branch factor
    0.5                             // gossip interval
    2                               // number of peers (first entry should be local proxy)
    10.129.28.150 0                 // ip_of_peer id_of_peer
    10.129.26.133 1                 // ip_of_peer id_of_peer

**Installation**
To make and install TokenGen manually run `./make_script.sh`.
Usually this is handled automatically by the `bin/redeploy.sh`

**Logs**
The logs of TokenGen can be found at ` /usr/lib/cgi-bin/proxy.log `
