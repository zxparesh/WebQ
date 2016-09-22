#include <stdio.h>
#include <string>
#include "parser.h"
using namespace std;

int listening_portno;
char ** ip_array ;
char * sending_port;
int no_of_proxy;
char delimChar='i';
char * tokenCheckIp;
int branch_factor;
float gossip_interval;

int main(void) {

    // parse the proxy.conf file to get the values 
    parse_config_file();

    printf("%s\n", sending_port );
    printf("%d\n", no_of_proxy);
    printf("%d\n", branch_factor);
    printf("%f\n", gossip_interval);

    return 0;
}
