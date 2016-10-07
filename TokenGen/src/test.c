#include <stdio.h>
#include <string>
#include <unordered_map>
#include "parser.h"
using namespace std;

int listening_portno;
char ** ip_array ;
char * sending_port;
int no_of_proxy;
char * tokenCheckIp;
int branch_factor;
float gossip_interval;
int localId;
unordered_map<char *, int> tgen_id;

struct clientDetails{
    int sockfd;
    int ip1;
    int ip2;
    int ip3;
    int ip4;
    int port;
};

int main(void) {

    // parse the proxy.conf file to get the values 
    parse_config_file();

    printf("%s\n", sending_port );
    printf("%d\n", no_of_proxy);
    printf("%d\n", branch_factor);
    printf("%f\n", gossip_interval);
    printf("%d\n", localId);

    for(int i=0; i<no_of_proxy; i++)
        printf("%d : %s : %d\n", i, ip_array[i], tgen_id[ip_array[i]]);

    struct clientDetails* cd;
    cd = (clientDetails * ) malloc( sizeof( struct clientDetails ) );
    cd->sockfd = 100;
    cd->ip1=10;
    cd->ip2=129;
    cd->ip3=28;
    cd->ip3=160;
    cd->port=10000;

    //char get_ip[]=itoa(cd->ip1,)+itoa(cd->ip2);
    char get_ip[5];
    itoa(cd->ip1, get_ip, 10);

    return 0;
}
