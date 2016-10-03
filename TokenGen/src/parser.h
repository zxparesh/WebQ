#include<string.h>
#include<stdio.h>
#include<stdlib.h>
extern int listening_portno;
extern char ** ip_array;
extern char * sending_port;
extern char * tokenCheckIp;
extern int no_of_proxy;
extern int branch_factor;
extern float gossip_interval;
extern int localId;
extern std::unordered_map<char *, int> tgen_id;

void parse_config_file(){
    int size = 1024, pos;
    int nl =0;
    int c;
    int i = 0;
    char *buffer = (char *)malloc(size);
    ip_array = (char **) malloc( 5 * sizeof( char* ) ); // allocate space for 5 ip ) )
    sending_port = (char *) malloc( 8 * sizeof( char) );
    tokenCheckIp = (char *) malloc( 8 * sizeof( char) );

    FILE *f = fopen("proxy.conf", "r");
    if(f) {
        do { // read all lines in file
            pos = 0;
            do{ // read till the first space in each line
                c = fgetc(f);
                if(c != EOF && c!= ' ') buffer[pos++] = (char)c;
                if(pos >= size - 1) {
                    // increase buffer length - leave room for 0
                    size *=2;
                    buffer = (char*)realloc(buffer, size);
                }
            }while(c != EOF && c != ' ');
            // proceed till the end of the line is reached
            buffer[pos] = 0;
            nl ++;
            // line is now in buffer
            switch( nl ){
                case 1:
                    listening_portno = atoi( buffer );
                    strcpy( sending_port, buffer );
                    break;
                case 2:
                    strcpy( tokenCheckIp, buffer );
                    break;
		case 3:
		    branch_factor = atoi( buffer );
		    break;
		case 4:
		    gossip_interval = atof( buffer );
		    break;
                case 5:
                    no_of_proxy = atoi( buffer );
                    break;
                default:
                    ip_array[i] = (char*) malloc( 20 * sizeof(char) );
                    strcpy( ip_array[i] , buffer );
                    // now read the id for that ip
                    pos = 0;
                    do{ // read till space encountered
                        c = fgetc(f);
                        if(c != EOF && c!= ' ') buffer[pos++] = (char)c;
                        if(pos >= size - 1) {
                            // increase buffer length - leave room for 0
                            size *=2;
                            buffer = (char*)realloc(buffer, size);
                        }
                    }while(c != EOF && c != ' ');
                    buffer[pos]=0;

                    // map ip with its id
                    tgen_id[ip_array[i]] = atoi( buffer );
                    i++;
                    // first entry indicates localIp, save its id
                    if(nl == 6)
                        localId = atoi( buffer );
                    break;
            }
            // proceed till the end of the line is reached
            while( c!= EOF && c != '\n') c=fgetc(f);
        } while(c != EOF);
        fclose(f);
    }
    free(buffer);
}
