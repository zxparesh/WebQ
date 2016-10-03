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
unordered_map<char *, int> ipToid;

int main(void) {

    // parse the proxy.conf file to get the values 
    parse_config_file();

    printf("%s\n", sending_port );
    printf("%d\n", no_of_proxy);
    printf("%d\n", branch_factor);
    printf("%f\n", gossip_interval);
    printf("%d\n", localId);

    for(int i=0; i<no_of_proxy; i++)
        printf("%d : %s : %d\n", i, ip_array[i], ipToid[ip_array[i]]);

    return 0;
}
