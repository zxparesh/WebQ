#include <unordered_map>
#include <string>
#include "functions.h"
#include "timer.h"
#include "parser.h"
#include <netinet/tcp.h>
#include <arpa/inet.h>   //for inet_aton
using namespace std;

int serv_capacity=720;

int listening_portno;
char ** ip_array ;
char * sending_port;
int no_of_proxy;
char * tokenCheckIp;
int branch_factor;
float gossip_interval;
long timestamp[PEERS];          // timestamp associated with temp_incoming_peers and peer_v_count
float peer_incomingRate[PEERS];         // global view of incoming rates
int temp_incoming_peers[PEERS];         // store here till all rates received
float sum_peer_incoming_rate;
double hardness[2];
char delimChar='t';
struct timeval tval;

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
    // make buffer to store the data from client
    int clientSocketFD = cd->sockfd;
    debug_printf( "read thread successfully created! thread id: %d ip:%s \n", clientSocketFD, cd->ip );
    int bytes = sizeof(char);   // read only char from client
    int bytesRead = 0;
    int*  buffer = (int *) malloc( bytes );
    
    while( ( bytesRead = read( clientSocketFD, buffer, bytes ) ) > 0)
    {
        debug_printf( "bytes read: %d data: %s from: %s \n", bytesRead, ((char*)buffer)+1, cd->ip );
        // the fist character indicates if the data is from CapacityEstimator or TokenGen
        // c - capacity data from CapacityEstimator
        // h - hardness data form CapacityEstimator
        // t - timestamp follwed by incoming rate and peer_v_count from peer TokenGen
        // check content of buffer == c or h or t
        if( *(char*)buffer == 'c' ){
            bytesRead = read( clientSocketFD, buffer, 20*sizeof(char) );
            debug_printf( "bytes: %d data from capacity estimator in c %s \n", bytesRead, (char*)buffer );
            // capacity = stoi((char*)buffer+1 );
            capacity = serv_capacity;
        }
        else if( *(char*)buffer == 'h' ){
            bytesRead = read( clientSocketFD, buffer, 40*sizeof(char) );
            debug_printf( "bytes: %d, data from capacity estimator in h %s \n", bytesRead, (char*)buffer );
            // error here due to not handling buffer which is not decimal
            hardness[0] = stod( (char*)buffer );
            hardness[1] = stod( strchr( (char*)buffer+1 , ' ')  );
            debug_printf( "hardness %f %f \n", hardness[0], hardness[1] );
        }
        else if( *(char*)buffer == 't' ){
            // if t is sent, timestamp follwed by incoming and peer_v_count will be sent ; read all that
            debug_printf( "read from: %s id: %d\n", cd->ip, ipToid[cd] );
            // store all 3 received arrays
            long recv_timestamp[PEERS];
            int recv_incoming_peers[PEERS];
            int recv_peer_v_count[PEERS][LIMIT];
            int bcount = 0;

            bcount = read( clientSocketFD, recv_timestamp, PEERS*sizeof(long));
            debug_printf("timestamp recieved, bytes: %d  ", bcount);
            for(int i=0; i<PEERS; i++)
                debug_printf("%ld ", recv_timestamp[i]);
            debug_printf("\n");

            bcount = read (clientSocketFD, recv_incoming_peers, PEERS*sizeof(int));
            debug_printf("peer_incomingRate recieved, bytes: %d  ", bcount);
            for(int i=0; i<PEERS; i++)
                debug_printf("%d ", recv_incoming_peers[i]);
            debug_printf("\n");

            bcount = 0;
            for(int i=0; i<PEERS; i++)
                for(int j=0; j<LIMIT; j++)
                    bcount += read (clientSocketFD, &recv_peer_v_count[i][j], sizeof(int));
            debug_printf("peer_v_count recieved, bytes: %d \n", bcount);

            // debug...
            debug_printf("before receive: timestamp-temp_inc_peers: ");
            for(int i=0; i<PEERS; i++) {
                debug_printf("%ld-%d  ", timestamp[i], temp_incoming_peers[i]);
            }
            debug_printf("\n");
            // update timestamp, imc_rate, peer_v_count if (received value is latest)
            // received timestamp is greater than existing timestamp for given proxy
            bool flag = true;    // incomingRate received from all proxies
            for(int i=0; i<PEERS; i++) {        // no need for chackeing entries >=no_of_proxy
                if(i!=localId) {                // no need to updaet own info from others!!
                    if(recv_timestamp[i] > timestamp[i]) {
                        debug_printf("latest info from peer %d\n", i);
                        timestamp[i] = recv_timestamp[i];
                        temp_incoming_peers[i] = recv_incoming_peers[i];
                        memcpy(peer_v_count[i], recv_peer_v_count[i], LIMIT*sizeof(int));
                    }
                    // if incoming rate from any proxy not received then set flag to false
                    if(temp_incoming_peers[i]==0 && i<no_of_proxy) {
                        debug_printf("incomingRate from %d not recieced!\n", i);
                        flag=false;
                    }
                }
            }
            // debug...
            debug_printf("after receive: timestamp-temp_inc_peers: ");
            for(int i=0; i<PEERS; i++) {
                debug_printf("%ld-%d  ", timestamp[i], temp_incoming_peers[i]);
            }
            debug_printf("\n");
            // if ncoming rate received from all proxies, then
            // copy it to original peer_incomingRate and set temp_incoming_peers to 0
            if(flag) {
                memcpy(incoming_peers, temp_incoming_peers, PEERS*sizeof(int));
                memset(temp_incoming_peers, 0, PEERS*sizeof(int));
                debug_printf("all incomingRate recieved! copied to imcoming_peers!\n");
            }
        }
        else{
            debug_printf( "bytesRead in else %d \n", bytesRead );
        }
        // Exit once the communication is over.
    }
    debug_printf( "gonna shutdown read thread \n");
    // 2- shutdown both send and recieve functions
    return close( clientSocketFD );
}

void * ThreadWorker( void * threadArgs) {
    readFromClient( (struct clientDetails *) threadArgs);
    pthread_exit(NULL);
}

void* create_server_socket(void*) {
    //
    // Creates server socket for communication between Proxy1 and Proxy2, and CapacityEstimator
    //
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
     * go in sleep mode and will wait for the incoming connection
     */
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    int option = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*) &option,
            sizeof(option)) < 0) {
        printf("setsockopt failed\n");
        close(sockfd);
        exit(2);
    }
    //setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &option, sizeof(int));
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

        // add cd to a hash
        ipToid[ cd ] = connectedClients;
        connectedClients++;
        debug_printf("peer connected: %s socket id: %d \n", cd->ip, ipToid[ cd ]);

        // create read thread
        if(pthread_create(&threadId, NULL, ThreadWorker, (void *)cd) < 0)
        {
            debug_printf("read thread creation failed");
            exit(1);
        }

    }
}

void writeToServer(char *ip_array_n){
    // FUNCTION DESC:
    // write incoming rate and visitor array to peer proxy 
    // ( which is a server as far as 'this' proxy is concerned, therefore the function name )
    // This function runs as a separate thread.for each peer
    // i.e. There will be as many instance of this function as the number of peer TokenGen

    int sockfd, portnum, n;
    struct sockaddr_in server_addr;
    // sending_port is already filled by the parser
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
    // Connect once to the current ip_array_n and keep on sending
    // incomingRate and waittime array at the below infinite while loop
    if(connect(sockfd,(struct sockaddr *)&server_addr,sizeof(server_addr)) >= 0)
    {
        // time frequnecy value is important = gossip interval
        float sec= 0.0;       // time frequency in which to communicate
        float sec_frac = gossip_interval;
        debug_printf("gossip_interval: %f localId: %d\n", sec_frac, localId);
        debug_printf( "write thread, connected: %s \n" , ip_array_n);
        struct timespec tim, rem;
        struct timespec t_tim, t_rem;
        tim.tv_sec = sec;
        tim.tv_nsec = sec_frac*1000000000;
        while( 1)
        {
            // get current time in milliseconds
            gettimeofday(&tval, 0);
            long t_msec = (tval.tv_sec * 1000) + (tval.tv_usec / 1000)
;
            debug_printf( "time: %ld writing to %s \n" , t_msec, ip_array_n);
            // debug print the visitor_count being written
            // for(int j =0 ; j < LIMIT ; j++ ){
            //     if ( visitor_count[j]!= 0 )
            //         debug_printf( "(%d) vc %d\n", j, visitor_count[j] );
            // }

            // send delimiter char 't' as indicator
            n = write(sockfd, &delimChar, sizeof(char));
            debug_printf("delimiter sent, bytes: %d \n", n);
            if (n < 0) { debug_printf("ERROR writing delimChar to peer socket\n"); }
            // before sending arrays, update own value in that array (for localId)

            // send timestamp array
            timestamp[localId] = t_msec;
            n = write(sockfd, timestamp, PEERS*sizeof(long) );
            debug_printf("timestamp sent, bytes: %d  ", n);
            for(int i=0; i<PEERS; i++)
                debug_printf("%ld ", timestamp[i]);
            debug_printf("\n");
            if (n < 0) { debug_printf("ERROR writing timestamp to peer socket\n"); }

            // send incoming rate array
            temp_incoming_peers[localId] = incoming;
            n = write(sockfd, temp_incoming_peers , PEERS*sizeof(int) );
            debug_printf("incomingrate sent, bytes: %d  ", n);
            for(int i=0; i<PEERS; i++)
                debug_printf("%d ", temp_incoming_peers[i]);
            debug_printf("\n");
            if (n < 0) { debug_printf("ERROR writing incoming_rate to peer socket\n"); }

            // sent visitor queue
            n = 0;
            memcpy(peer_v_count[localId], visitor_count, LIMIT*sizeof(int));
            for(int i=0; i<PEERS; i++)
                for(int j=0; j<LIMIT; j++)
                    n += write(sockfd, &peer_v_count[i][j], sizeof(int) );
            debug_printf("visitor array sent, bytes: %d \n", n);
            if (n < 0) { debug_printf("ERROR writing visitor_array to peer socket\n"); }
            debug_printf("data sent to %s\n", ip_array_n);
            // connect as a client;
            nanosleep( &tim, &rem );
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

int main(void) {/*{{{*/
    incoming = 0;
    failing = 0;
    total_in = 0;
    total_out = 0;
    capacity = 100000;
    connectedClients = 0;
    current_time = 0;
    int counter;

    // Initialize
    // visitor_count - number of incoming at current TokenGen
    // peer_v_count - number of incoming at peers
    for (counter = 0; counter < LIMIT; counter++)
    {
        visitor_count[counter] = 0;  // Initialize visitor_count
        int i ;
        for( i = 0 ; i < PEERS; i++ )
            peer_v_count[i][counter] = 0; // replace with memset
    }

    // parse the proxy.conf file to get the values 
    parse_config_file();
    
    // debug parsed data
    debug_printf("config file read success!\n");
    debug_printf("%d\n", no_of_proxy);
    debug_printf("%d\n", branch_factor);
    debug_printf("%f\n", gossip_interval);
    debug_printf("%d\n", localId);
    for(int i=0; i<no_of_proxy; i++)
        debug_printf("%d : %s : %d\n", i, ip_array[i], tgen_id[ip_array[i]]);


    pthread_t timer_log;
    pthread_create( &timer_log, NULL, start_logging, (void*) NULL);
    pthread_t make_connection;
    pthread_create(&make_connection, NULL, create_server_socket, (void*) NULL);
    pthread_t send_queue[PEERS];
    // first ip will be local machine ip, don't create thread for that (start from 1)
    for (counter = 1; counter < no_of_proxy ; counter++) {
        pthread_create( &send_queue[ counter ] , NULL , queue_sender, (void *) ip_array[ counter ] );
    }
    
    for(int i=0; i<PEERS; i++) {
        timestamp[i] = 0;
        temp_incoming_peers[i] = 0;
    }

    while (FCGI_Accept() >= 0) {
        change_values(&incoming, 1);
        total_in++;

        // Updated on 17.04
        time_t currtime = time(NULL);

        // we need to find share of capacity (share)
        // hostIncomingRate is updated in timer.h
        // sum_peer_incoming_rate - is updated below
        int iter = 0;
        int j;
        for( j=0; j<PEERS; j++) {
            peer_incomingRate[j] = 0; // TODO use memset
        }
        // sum the times .. actual avg found later outside the loop
        for( j=0; j<PEERS; j++)
        {
            peer_incomingRate[j] = incoming_peers[j];
            debug_printf( "%d inc_peers: %d peer_incRate: %f \n", j, incoming_peers[j] , peer_incomingRate[j]);
        }
        sum_peer_incoming_rate = 0;
        for( j=0; j<PEERS; j++)
        {
            sum_peer_incoming_rate += peer_incomingRate[j];
        }
        debug_printf( "before! hostIncRate: %.2f, sumPeerIncRate: %.2f \n", hostIncomingRate, sum_peer_incoming_rate);

        int usedCapacity = 0;
        int peerUsedCapacity = 0;
        char * caseof = "case: H1 SP1";
        // reserve a min value of capacity (0.1) for each servers
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
        // share found
        if ( share == 0 ) {
            // share can never be 0
            share = 1;
        }

        int excess_used;
        for (iter = 0; iter < LIMIT; iter++) {
            // find the current used capacity for THIS "iter" time instant 
            // current_time is updated by 1 in time.h
            peerUsedCapacity = 0;
            usedCapacity = get_array(&visitor_count[(current_time + iter) % LIMIT]);
            for( j=0; j<PEERS; j++)
            {
                peerUsedCapacity += get_array( &peer_v_count[j][(current_time + iter) % LIMIT] );
            }
            debug_printf( "usedCap-%d peerUsedCap-%d share-%d iter-%d \n", usedCapacity, peerUsedCapacity, share , iter);
            int total_usable_capacity = (share  - usedCapacity) ; // use a buffer here to compensate n/w delay!!!
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
                // found the time at which the request is to be
                // sheduled , now update the visitor_count array
                update_array(&visitor_count[(current_time + iter) % LIMIT], hardness[url]); // increment by hardness
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

            //changes made to enable logging of avg wait time (following line is uncommented)
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
}/*}}}*/
