#include <stdio.h>
#include <string>
#include "parser.h"
using namespace std;

int listening_portno;
int *ary;
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
    ary = (int *) malloc( 5 * sizeof( int ) );

    ary[0]=0;
    ary[1]=1;
    ary[2]=2;
    ary[3]=3;
    ary[4]=4;
    for(int i=0; i<5; i++) {
        printf("%d\n",ary[i]);
    }

    return 0;
}
