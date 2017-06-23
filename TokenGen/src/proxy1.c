#include <unordered_map>
#include <string>
#include "functions.h"
#include "timer.h"
#include "parser.h"
#include <netinet/tcp.h>
#include <arpa/inet.h>
using namespace std;

int listening_portno;
char ** ip_array ;
char * sending_port;
int no_of_proxy=PEERS;
char * tokenCheckIp;
int branch_factor;
float gossip_interval;
int timestamp[PEERS];           // timestamp associated with temp_incoming_peers and peer_v_count
int temp_store_timestamp[PEERS];// store here till all data received
int temp_incoming_peers[PEERS]; // store here till all rates received
float peer_incomingRate[PEERS]; // global view of incoming rates
float sum_peer_incoming_rate;
int tick_count=1;               // used as a timestamp, instead of system time
double hardness[2];
bool lock[PEERS];       // for blocking write thread to simulate branch factor
int r_id[PEERS];        // for storing indexes of TGs to receive information
int start_del=-10, end_del=-20;
int time_del=-1, inc_del=-2, wait_del=-3;

struct clientDetails{
    int sockfd;
    char * ip;
    int port;
};
int localId;
unordered_map<char *, int> tgen_id;             // map ip of each tgen with its id
unordered_map<clientDetails *,int> ipToid;      // map socket+ip+port with id of that proxy or cap_est
int connectedClients;                           // for asigining array indices to ipToid

void* start_logging( void* ) {
    init_logger();
    start_timer();
}

int readFromClient( struct clientDetails * cd ) {
    /* This functions reads data from two types of clients 
     * that connect to TokenGen
     * Two types of clients are:
     *  1 Capacity Estimator
     *  2 other peer proxy */
    /* make buffer to store the data from client */
    int clientSocketFD = cd->sockfd;
    debug_printf( "read thread created! socket id: %d ip: %s \n", clientSocketFD, cd->ip );
    int bytes = 1000 * sizeof(int);   /* read only 1000 ints from client */
    int* buffer = (int*) malloc (bytes);

    int bytesRead = 0, bcount = 0;
    int t_received=0, w_received=1, r_count=0;
    int cur_read_type=0, cur_read_index=0, cur_r_id_index=-1, cur_seq_num=-1, end_seq_num=-1;
   
    int recv_timestamp[PEERS];
    int recv_inc_rate[PEERS];
    int recv_waittime[PEERS][LIMIT];
    
    while( ( bytesRead = read( clientSocketFD, buffer, bytes ) ) > 0)
    {
        // debug_printf( "bytes read: %d data: %s from: %s \n", bytesRead, ((char*)buffer), cd->ip );
        /* the fist character indicates if the data is from CapacityEstimator or TokenGen
           c - capacity data from CapacityEstimator
           h - hardness data form CapacityEstimator
           t - timestamp follwed by incoming rate and peer_v_count from peer TokenGen */
        /* check content of buffer == c or h or t */
        if( *(char*)buffer == 'c' ){
            debug_printf( "bytes: %d, data from capacity estimator in c %s \n", bytesRead, ((char*)buffer)+1 );
            capacity = stoi((char*)buffer+1 );
            debug_printf("capacity: %d\n", capacity);
        }
        else if( *(char*)buffer == 'h' ){
            debug_printf( "bytes: %d, data from capacity estimator in h %s \n", bytesRead, ((char*)buffer)+1 );
            /* error here due to not handling buffer which is not decimal */
            hardness[0] = stod( ((char*)buffer)+1 );
            hardness[1] = stod( strchr( ((char*)buffer)+2 , ' ') );
            debug_printf( "hardness %f %f \n", hardness[0], hardness[1] );
        }
        else {
            debug_printf( "read from: %s id: %d bytes: %d\n", cd->ip, ipToid[cd], bytesRead );
            int nums_read = bytesRead/4;
            for(int i=0; i<nums_read; i++)
            {
                // debug_printf("%d ", buffer[i]);
                if(buffer[i]<0) {
                    if(buffer[i]==start_del) {
                        i++;
                        cur_seq_num = buffer[i];
                        i++;
                        cur_read_type = buffer[i];
                        cur_read_index = 0;
                        if(cur_read_type==time_del) {
                            r_count = 0;
                            t_received = 0;
                            w_received = 1;
                            cur_r_id_index = 0;
                            memset(recv_timestamp, 0, PEERS*sizeof(long));
                            memset(recv_inc_rate, 0, PEERS*sizeof(int));
                            for(int i=0; i<PEERS; i++)
                                memset(recv_waittime[i], 0, LIMIT*sizeof(int));
                        }
                    }
                    else {
                        i++;
                        end_seq_num = buffer[i];
                        i++;
                        if(end_seq_num!=cur_seq_num) {
                            debug_printf("seq_num does not match!\n");
                        }
                        else {
                            if(cur_read_type==time_del) {
                                if(cur_read_index!=no_of_proxy) {
                                    debug_printf("timestamp receive error!\n");
                                    t_received = 0;
                                }
                                else {
                                    t_received = 1;
                                    printf("timestamp received!\n");
                                    for(int k=0; k<no_of_proxy; k++) {
                                        if(recv_timestamp[k]!=0) {
                                            r_id[r_count] = k;
                                            r_count++;
                                        }
                                    }
                                    debug_printf("ids to receive: ");
                                    for(int x=0; x<r_count; x++)
                                        debug_printf(" %d",r_id[x]);
                                    debug_printf("\n");
                                }
                            }
                            else if(cur_read_type==inc_del) {
                                if(cur_read_index!=no_of_proxy) {
                                    debug_printf("inc_rate receive error!\n");
                                }
                                else {
                                    if(t_received) {
                                        /* update inc_rate here */
                                        debug_printf("inc_rate received!\n");

                                        /* for debugging purpose */
                                        debug_printf("before receive: timestamp-temp_store_timestamp-temp_incoming_peers: ");
                                        for(int i=0; i<no_of_proxy; i++)
                                            debug_printf("%d-%d-%d  ", timestamp[i], temp_store_timestamp[i], temp_incoming_peers[i]);
                                        debug_printf("\n");

                                        /* update timestamp, inc_rate, if (received value is latest) received
                                          timestamp is greater than existing timestamp for given proxy */
                                        for(int i=0; i<no_of_proxy; i++) {
                                            if(i!=localId) {                /* no need to update own info from others!! */
                                                if(recv_timestamp[i] >= temp_store_timestamp[i]) {
                                                    // debug_printf("latest info from peer %d\n", i);
                                                    temp_store_timestamp[i] = recv_timestamp[i];
                                                    temp_incoming_peers[i] = recv_inc_rate[i];
                                                }
                                            }
                                        }
                           
                                        /* for debugging purpose */
                                        debug_printf("after receive: timestamp-temp_store_timestamp-temp_incoming_peers: ");
                                        for(int i=0; i<no_of_proxy; i++)
                                            debug_printf("%d-%d-%d  ", timestamp[i], temp_store_timestamp[i], incoming_peers[i]);
                                        debug_printf("\n");
                                    }
                                }
                            }
                            else {
                                if(cur_read_index!=1000) {
                                    debug_printf("watitime receive error!\n");
                                    w_received = 0;
                                }
                                else {
                                    // compare with last id to receive waitime
                                    if(cur_r_id_index==r_count-1) {
                                        if(t_received==1 && w_received!=0) {
                                            debug_printf("waittime received!\n");

                                            bool flag = true;    /* timestamp received from all proxies */

                                            /* update visitor array if (received value is latest) received
                                              timestamp is greater than existing timestamp for given proxy */
                                            for(int i=0; i<no_of_proxy; i++) {
                                                if(i!=localId) {                /* no need to update own info from others!! */
                                                    if(recv_timestamp[i] >= temp_store_timestamp[i]) {
                                                        memcpy(peer_v_count[i], recv_waittime[i], LIMIT*sizeof(int));
                                                    }
                                                    /* if timestamp from any proxy not received then set flag to false */
                                                    if(temp_store_timestamp[i]==0) {
                                                        // debug_printf("data from %d not received!\n", i);
                                                        flag=false;
                                                    }
                                                }
                                            }
                                            /* if timestamps received from all proxies, then copy all rates to incoming_peers
                                               and start collecting all over again by reseting temp_store_timestamp to 0 */
                                            if(flag) {
                                                memcpy(incoming_peers, temp_incoming_peers, no_of_proxy*sizeof(int));
                                                memset(temp_store_timestamp, 0, PEERS*sizeof(int));
                                                debug_printf("all timestamps received! reset temp_store_timestamp!\n");
                                            }
                                        }
                                    }
                                    cur_r_id_index++;
                                }
                            }
                        }
                    }
                }
                else {
                    if(cur_read_type==time_del || cur_read_type==inc_del) {
                        if(cur_read_type==time_del) {
                            if(cur_read_index<no_of_proxy) {
                                recv_timestamp[cur_read_index]=buffer[i];
                            }
                            else {
                                debug_printf("timestamp: index overhead!\n");
                                t_received = 0;
                            }
                        }
                        else if(cur_read_type==inc_del) {
                            if(cur_read_index<no_of_proxy) {
                                recv_inc_rate[cur_read_index]=buffer[i];
                            }
                            else {
                                debug_printf("inc_rate: index overhead!\n");
                            }
                        }
                    }
                    else {
                        if(cur_read_index<1000 && cur_r_id_index<r_count) {
                            recv_waittime[r_id[cur_r_id_index]][cur_read_index]=buffer[i];
                        }
                        else {
                            debug_printf("waitime: index overhead!\n");
                            w_received = 0;
                        }
                    }
                    cur_read_index++;
                }
            }       // for(nums) loop
        }       // while(read) loop
        /* Exit once the communication is over. */
    }
    debug_printf( "gonna shutdown read thread \n");
    /* 2- shutdown both send and receive functions */
    return close( clientSocketFD );
}

void * ThreadWorker( void * threadArgs) {
    readFromClient( (struct clientDetails *) threadArgs);
    pthread_exit(NULL);
}

void* create_server_socket(void*) {
    /*
     Creates server socket for communication between Proxy1 and Proxy2, and CapacityEstimator
    */
    int sockfd, newsockfd;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    /* First call to socket() function */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }
    /* Initialize socket structure */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    /* listening_portno = 5007; */


    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(listening_portno);

    /* Now bind the host address using bind() call.*/
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    /* Now start listening for the clients, here process will
       go in sleep mode and will wait for the incoming connection */
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    int option = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*) &option,
                sizeof(option)) < 0) {
        debug_printf("setsockopt failed\n");
        close(sockfd);
        exit(2);
    }
    // setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &option, sizeof(int));
    /* Accept actual connection from the client */
    pthread_t threadId;
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("ERROR on accept");
            exit(1);
        }

        struct clientDetails * cd;
        cd = (clientDetails * ) malloc( sizeof( struct clientDetails ) );
        cd->sockfd = newsockfd;
        cd->ip = (char *) malloc (20*sizeof(char));
        inet_ntop(AF_INET, &(cli_addr.sin_addr), cd->ip, INET_ADDRSTRLEN);

        /* add cd to a hash */
        ipToid[ cd ] = connectedClients;
        connectedClients++;
        debug_printf("peer connected: %s socket id: %d \n", cd->ip, ipToid[ cd ]);

        /* create read thread */
        if(pthread_create(&threadId, NULL, ThreadWorker, (void *)cd) < 0)
        {
            debug_printf("read thread creation failed");
            exit(1);
        }

    }
}

void writeToServer(char *ip_array_n){
    /* FUNCTION DESC:
       write incoming rate and visitor array to peer proxy 
       ( which is a server as far as 'this' proxy is concerned, therefore the function name )
       This function runs as a separate thread.for each peer
       i.e. There will be as many instance of this function as the number of peer TokenGen */

    int sockfd, portnum, n;
    int scount = 0;
    struct sockaddr_in server_addr;
    /* sending_port is already filled by the parser */
    portnum = atoi(sending_port);
    /* Create client socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        debug_printf( "ERROR opening socket while writing to server\n");
        exit(1);
    }
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    if(!inet_aton( ip_array_n  , &server_addr.sin_addr))
    {
        debug_printf( "ERROR invalid server IP address\n");
        debug_printf( ip_array_n);
        exit(1);
    }
    server_addr.sin_port = htons(portnum);
    /* Connect once to the current ip_array_n and keep on sending
       incomingRate and waittime array at the below infinite while loop */

    int seq_num = 0;
    int tmp_timestamp[6+no_of_proxy], tmp_inc_rate[6+no_of_proxy], tmp_waittime[LIMIT+6];

    /* initialize tmp arrays */
    tmp_timestamp[0] = start_del;
    tmp_timestamp[2] = time_del;
    tmp_timestamp[no_of_proxy+3] = time_del;
    tmp_timestamp[no_of_proxy+5] = end_del;
    tmp_inc_rate[0] = start_del;
    tmp_inc_rate[2] = inc_del;
    tmp_inc_rate[no_of_proxy+3] = inc_del;
    tmp_inc_rate[no_of_proxy+5] = end_del;
    tmp_waittime[0] = start_del;
    tmp_waittime[2] = wait_del;
    tmp_waittime[LIMIT+3] = wait_del;
    tmp_waittime[LIMIT+5] = end_del;
    
    if(connect(sockfd,(struct sockaddr *)&server_addr,sizeof(server_addr)) >= 0)
    {
        /* time frequnecy value is important = gossip interval
           bug corrected: nanosleep for seconds also */
        int isec = (int) gossip_interval;
        float sec = isec;       /* time frequency in which to communicate */
        float sec_frac = gossip_interval - isec;
        debug_printf("gossip_interval: %f localId: %d\n", gossip_interval, localId);
        debug_printf( "write thread created! ip: %s \n" , ip_array_n);
        struct timespec tim, rem;
        tim.tv_sec = sec;
        tim.tv_nsec = sec_frac*1000000000;
        while(1)
        {
            while(!get_lock(&lock[tgen_id[ip_array_n]]))
            {
                /* before sending arrays, update own value in that array (for localId) */
                temp_store_timestamp[localId] = tick_count++;
                temp_incoming_peers[localId] = hostIncomingRate;
                memcpy(peer_v_count[localId], visitor_count, LIMIT*sizeof(int));

                tmp_timestamp[1] = seq_num;
                tmp_timestamp[no_of_proxy+4] = seq_num;
                seq_num++;
                tmp_inc_rate[1] = seq_num;
                tmp_inc_rate[no_of_proxy+4] = seq_num;
                seq_num++;
                for(int i=0; i<no_of_proxy; i++) {
                    tmp_timestamp[i+3] = temp_store_timestamp[i];
                    tmp_inc_rate[i+3] = temp_incoming_peers[i];
                }

                /* send entire tmp_timestamp array */
                n = write(sockfd, &tmp_timestamp, (6+no_of_proxy)*sizeof(int));
                if (n < 0) { debug_printf("ERROR writing timestamp to peer socket\n"); }
                debug_printf("timestamp sent, bytes: %d\n", n);

                for(int i=0; i<no_of_proxy+6; i++)
                    debug_printf("%d ", tmp_timestamp[i]);
                debug_printf("\n");

                /* send entire incoming rate array */
                n = write(sockfd, &tmp_inc_rate, (6+no_of_proxy)*sizeof(int));
                if (n < 0) { debug_printf("ERROR writing incoming_rate to peer socket\n"); }
                debug_printf("inc_rate sent, bytes: %d\n", n);

                for(int i=0; i<no_of_proxy+6; i++)
                    debug_printf("%d ", tmp_inc_rate[i]);
                debug_printf("\n");

                /* send waittime of those ids whose info I have
                   timestamp != 0 means I have information */
                n = 0;
                for(int i=0; i<no_of_proxy; i++) {
                    if(tmp_timestamp[i+3]!=0) {
                        tmp_waittime[1] = seq_num;
                        tmp_waittime[LIMIT+4] = seq_num;
                        for(int k=0; k<LIMIT; k++)
                            tmp_waittime[k+3]=peer_v_count[i][k];
                        seq_num++;
                        n += write(sockfd, &tmp_waittime, (LIMIT+6)*sizeof(int));
                    }
                }   
                if (n < 0) { debug_printf("ERROR writing visitor_array to peer socket\n"); }
                debug_printf("visitor array sent, bytes: %d\n", n);

                debug_printf("data sent to %s, time: %d tstamp: %ld\n", ip_array_n, current_time, tick_count);
                /* connect as a client */
                set_lock(&lock[tgen_id[ip_array_n]], true);     /* set lock once again till next time selected */
                nanosleep( &tim, &rem );
            }
        }
    }else{
        debug_printf( "ERROR connecting to a peer ");
        /* sleep(1); */
        /* exit(1); */
    }
    debug_printf( "disconnected %s \n" , ip_array_n);
    close( sockfd );
}

void* queue_sender( void * args) {/*{{{*/
    sleep(1);
    writeToServer( (char *) args  );
}/*}}}*/

void* start_controller(void*)
{
    sleep(1);
    /* bug corrected: nanosleep for seconds also */
    int isec = (int) gossip_interval;
    float sec = isec;
    float sec_frac = gossip_interval - isec;
    struct timespec tim, rem;
    tim.tv_sec = sec;
    tim.tv_nsec = sec_frac*1000000000;
    debug_printf("branch factor: %d \n", branch_factor);
    while(1)
    {
        int rand_peers[branch_factor];
        lock_mutex();
        memset(lock, true, PEERS*sizeof(bool));
        memset(rand_peers, localId, branch_factor*sizeof(int));
        for(int i=0; i<branch_factor; i++) {
            int r=rand() % no_of_proxy;
            bool flag=false;
            for(int j=0; j<=i; j++) {
                if(r==rand_peers[j]) {
                    flag=true;
                    break;
                }
            }
            if(!flag) {
                rand_peers[i]=r;
                lock[r]=false;
            }
            else
                i--;
        }
        release_mutex();
        // debug_printf( "  time: %ld ~~~controller~~~ peers: ", tick_count);
        // for(int i=0; i<branch_factor; i++)
        //     debug_printf("%d ", rand_peers[i]);
        // debug_printf("\n");
        nanosleep( &tim, &rem );
    }
    debug_printf( "gonna shutdown controller thread \n");
}

int main(void) {
    incoming = 0;
    lastIncoming = 0;
    failing = 0;
    total_in = 0;
    total_out = 0;
    capacity = 720;
    hardness[0] = 1;
    hardness[1] = 1;
    connectedClients = 0;
    current_time = 0;
    int counter;

    /* Initialize
       visitor_count - number of incoming at current TokenGen
       peer_v_count - number of incoming at peers */
    memset(temp_store_timestamp, 0, PEERS*sizeof(int));
    memset(temp_incoming_peers, 0, PEERS*sizeof(int));
    memset(visitor_count, 0, LIMIT*sizeof(int));
    for(int i = 0 ; i < PEERS; i++ )
        memset(peer_v_count[i], 0, LIMIT*sizeof(int));

    /* parse the proxy.conf file to get the values */
    parse_config_file();

    /* debug parsed data */
    debug_printf("config file read success!\n");
    debug_printf("no_of_proxy: %d\n", no_of_proxy);
    debug_printf("branch_factor: %d\n", branch_factor);
    debug_printf("gossip_interval: %f\n", gossip_interval);
    debug_printf("localId: %d\n", localId);
    for(int i=0; i<no_of_proxy; i++)
        debug_printf("%d : %s : %d\n", i, ip_array[i], tgen_id[ip_array[i]]);

    /* bug: for br_fact>no_of_proxies it was not working */
    if(branch_factor>=no_of_proxy)
        branch_factor=no_of_proxy-1;

    pthread_t timer_log;
    pthread_create( &timer_log, NULL, start_logging, (void*) NULL);
    pthread_t control_thread;
    pthread_create( &control_thread, NULL, start_controller, NULL );
    pthread_t make_connection;
    pthread_create( &make_connection, NULL, create_server_socket, (void*) NULL);
    pthread_t send_queue[PEERS];
    /* first ip will be local machine ip, don't create thread for that (start from 1) */
    for (counter = 1; counter < no_of_proxy ; counter++) {
        pthread_create( &send_queue[ counter ] , NULL , queue_sender, (void *) ip_array[ counter ] );
    }

    memset(timestamp, 0, PEERS*sizeof(int));
    memset(temp_store_timestamp, 0, PEERS*sizeof(int));
    memset(lock, true, PEERS*sizeof(bool));

    while (FCGI_Accept() >= 0) {
        change_values(&incoming, 1);
        total_in++;

        /* Updated on 17.04 */
        time_t currtime = time(NULL);

        /* we need to find share of capacity (share)
           hostIncomingRate is updated in timer.h
           sum_peer_incoming_rate - is updated below */
        int iter = 0, j = 0;
        memset(peer_incomingRate, 0, PEERS*sizeof(int));
        /* sum the times .. actual avg found later outside the loop */
        for( j=0; j<no_of_proxy; j++) {
            peer_incomingRate[j] = incoming_peers[j];
            debug_printf( "%d inc_peers: %d peer_incRate: %f \n", j, incoming_peers[j] , peer_incomingRate[j]);
        }
        sum_peer_incoming_rate = 0;
        for( j=0; j<no_of_proxy; j++) {
            /* don't include own incoming rate */
            if(j!=localId) {
                sum_peer_incoming_rate += peer_incomingRate[j];
            }
        }
        debug_printf( "before! hostIncRate: %.2f, sumPeerIncRate: %.2f \n", hostIncomingRate, sum_peer_incoming_rate);

        int usedCapacity = 0;
        int peerUsedCapacity = 0;
        char * caseof = "case: H1 SP1";
        /* reserve a min value of capacity (0.1) for each servers */
        int percent = 10;
        if( hostIncomingRate == 0 && sum_peer_incoming_rate == 0 )
        {
            hostIncomingRate = 1;
            sum_peer_incoming_rate = 1;
            caseof="case: H0 SP0";
        }
        else if ( hostIncomingRate == 0 )
        {
            hostIncomingRate = percent * sum_peer_incoming_rate / ( 100 - percent );
            caseof="case: H0 SP1";
        }
        else if ( sum_peer_incoming_rate == 0 )
        {
            sum_peer_incoming_rate = percent * hostIncomingRate / ( 100 - percent );
            caseof="case: H1 SP0";
        }
        debug_printf( "after! hostIncRate: %.2f, sumPeerIncRate: %.2f %s \n", hostIncomingRate, sum_peer_incoming_rate, caseof);
        share = capacity * hostIncomingRate/(hostIncomingRate + sum_peer_incoming_rate);
        /* share found */
        if ( share == 0 ) {
            /* share can never be 0 */
            share = 1;
        }

        int excess_used;
        for (iter = 0; iter < LIMIT; iter++) {
            /* find the current used capacity for THIS "iter" time instant 
               current_time is updated by 1 in time.h */
            peerUsedCapacity = 0;
            usedCapacity = get_array(&visitor_count[(current_time + iter) % LIMIT]);
            for( j=0; j<no_of_proxy; j++)
            {
                peerUsedCapacity += get_array( &peer_v_count[j][(current_time + iter) % LIMIT] );
            }
            // debug_printf( "usedCap-%d peerUsedCap-%d share-%d iter-%d \n", usedCapacity, peerUsedCapacity, share , iter);
            int total_usable_capacity = (share  - usedCapacity) ; /* use a buffer here to compensate n/w delay!!! */
            if( peerUsedCapacity > 0 ){
                excess_used = (capacity - share)-peerUsedCapacity;
                total_usable_capacity += excess_used < 0 ? excess_used : 0;
            }
            char* req_url = getenv("QUERY_STRING");
            int url;
            if(strcmp(req_url,"req1.php")==0)
                url=0;
            else
                url=1;
            if ( total_usable_capacity > 0 )
            {
                debug_printf( "time %d %d \n" , current_time , iter) ;
                /* found the time at which the request is to be
                   sheduled , now update the visitor_count array */
                update_array(&visitor_count[(current_time + iter) % LIMIT], hardness[url]); /* increment by hardness */
                debug_printf( "usedCap-%d peerUsedCap-%d share-%d iter-%d \n", usedCapacity, peerUsedCapacity, share , iter);
                break;
            }
        }
        if (iter == LIMIT) {
            printf("Content-Type:text/html\n\n");

            printf(
                    "<title>Frontend</title> <body>Server Totally Busy</body></html>");
        } else {
            float float_wait_time = iter;

            int time_to_wait = float_wait_time;

            /* changes made to enable logging of avg wait time (following line is uncommented) */
            change_float_values(&total_waiting_time, float_wait_time, 0);
            char* gt = get_token(time_to_wait);
            char* env_var = getenv("QUERY_STRING");
            char* request_limit = strchr(env_var, '=') + 1;
            char url_to_visit[100];
            if( strchr(env_var, 'M') > 0 || strchr(env_var, 'S') > 0 ){
                strcpy(url_to_visit, "http://");
                strcat( url_to_visit,  tokenCheckIp);
                strcat( url_to_visit, "/");
            }
            else{
                /* fired url http://<ip>/proxy1?limit=moodleS/moodle/ */
                /* http://<ip>/moodleS/moodle/ */
                strcpy(url_to_visit, "http://");
                strcat( url_to_visit,  tokenCheckIp);
                strcat( url_to_visit, "/test.php?limit=");
            }
            strcat(url_to_visit, (const char*) request_limit);
            // debug_printf("time_to_wait : %d %d\n", time_to_wait, (currtime+iter)%LIMIT ) ;
            printf("Refresh: %d; url=%s&hash=%s&token=%s\n", time_to_wait,
                    url_to_visit,/*"aaa"*/getHash((unsigned char*) gt),
                    encrypt(gt));
            printf("JMeter: %0.3f; url=%s&hash=%s&token=%s\n", float_wait_time,
                    url_to_visit,/*"aaa"*/getHash((unsigned char*) gt),
                    encrypt(gt));
            printf("Content-Type:text/html\n\n");

            printf(
                    "<title>Frontend</title>"
                    "Total incoming this second: <b>%d</b> \n<br />"
                    "Total requests served this second: <b>%d</b> \n<br /> "
                    "Total incoming requests till now: <b>%d</b> \n<br /> "
                    "Total requests served till now: <b>%d</b> \n<br /> "
                    "currtime: <b>%ld</b> \n<br /> "
                    "You will be redirected in %d seconds<br/>"
                    "query limit : %s"
                    "</div></body>",
                    incoming,
                    outgoing,
                    total_in,
                    total_out,
                    currtime,
                    time_to_wait,
                    request_limit);
            printf(
                    "<br>"
                    "<a href='%s&hash=%s&token=%s\n' style='text-decoration:none;'>"
                    "<button style='cursor:pointer'>Click here to go without waiting</button>"
                    "</a></center></body></html>",
                    url_to_visit,
                    /*"aaa"*/getHash((unsigned char*) gt),
                    encrypt(gt));

            free(gt);
        }
    }
}
