#include "protocol.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <map>
#include <string>
#include <iostream>
#include "sched.h"
#include "mysql_wrapper.h"
#include <sstream>
#include <stdlib.h>

#define _DBG
#define BACKLOG 10

#define MAX_DESCRIPTORS 5
#define MAX_REQUEST_LEN 100

#ifdef _DBG
#define dprintf printf
#else
#define dprintf
#endif

typedef std::map <std::string,std::string> PACKET;

typedef struct
{
    pthread_t thread;
    int flag;

}TQUEUE;

typedef struct
{
    int sfd;
    int flag;
}FDQUEUE;




TQUEUE tqueue_s[MAX_THREADS_QUEUE];
int threads_count = 0;
FDQUEUE  sfdqueue_s[MAX_DESCRIPTORS];
int sfd_count = 0;

pthread_mutex_t thread_lock_s = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sfd_lock_s = PTHREAD_MUTEX_INITIALIZER;

void* source_thread_worker(void* connector);

int parse_get_req(char* buf, PACKET &request);

int init_tqueue(TQUEUE* tqueue,void *(*start_routine) (void *))
{
    int ret = 0;
    pthread_attr_t tattr;
    pthread_t tid;
    pthread_attr_init(&tattr);
    for(int i = 0; i < MAX_THREADS_QUEUE; i++ )
    {
        if ( ( ret = pthread_create(&tid, &tattr, start_routine,NULL)) !=0 )
            return ret;
        tqueue[i].flag = 1;
        tqueue[i].thread = tid;
    }
    threads_count = MAX_THREADS_QUEUE;
    return 0;

}

void init_sfdqueue(FDQUEUE* sfdqueue)
{
    for(int i = 0; i < MAX_DESCRIPTORS; i++ )
        sfdqueue[i].flag = 0;
    sfd_count = -1;

}


int enqueue_sfd(int sfd,FDQUEUE* sfdqueue,pthread_mutex_t* sfd_lock)
{
    int err = 0;

    if ( ( err = pthread_mutex_lock ( sfd_lock )) != 0 )
        return err;

    while ( sfd_count == MAX_DESCRIPTORS -1 )
        sleep(0); //BUG!!!! use switch don't eat CPU's time bitch


    sfd_count ++;
    sfdqueue[sfd_count].flag = 1; //Locked descriptor
    sfdqueue[sfd_count].sfd = sfd;

    dprintf("enqueued count = %d\n",sfd_count);
    pthread_mutex_unlock( sfd_lock );
    return 0; //Success :))))))))))
}

int dequeue_sfd(FDQUEUE* sfdqueue,pthread_mutex_t* sfd_lock)
{
    if ( pthread_mutex_lock ( sfd_lock ) != 0 )
       return -1;
    if ( sfd_count <= -1 )
    {
        pthread_mutex_unlock( sfd_lock );
        return -1;
    }

    int sfd = sfdqueue[sfd_count].sfd;
    sfdqueue[sfd_count].flag = 0;
    sfd_count--;
    dprintf("dequeued count = %d\n",sfd_count);
    pthread_mutex_unlock( sfd_lock );
    return sfd;
}

int pk_search_end_d(char* buf,int right,int left)
{
    int found = 0;
    for(int i = right+1; i  >= left; i--)
        if(buf[i] == '\n')
        {
            dprintf("found = %d\n",found);
            if(found == 1)
                return 1;
            else
                found++;
        }else
            if(found) found = 0;
    return 0;

}

#define _SDBG
#ifdef _SDBG
void test_str(char* buf, int len)
{

    for(int i = len-1; i>=0; i--)
        dprintf("%02x",buf[i]);
    dprintf("\n");
}
#endif

int pk_search_end_all(char* buf, int len)
{
    int found = 0;

    for(int i = len-1; i>=0; i--)
        if(buf[i] == '\n')
        {


            if(found == 1)
                return 1;
            else
                found++;
        }else if(buf[i] != ' ')
        {
           found = 0;
          // dprintf("%c",buf[i]);// if(found) found = 0;
        }
   // dprintf("\n");
    return 0;
}

#define PK_BLOCKS_COUNT 10
#define MAX_RESPONSE_LEN 100
#define MAX_RESPONSE_TRY 4
WUCONTAINER wu_workset;

int process_request(char* inbuf,int len,int sfd)
{
    PACKET req;
    int err = 0, bytes_sent = 0;
    std::string param;
    std::stringstream resp(std::stringstream::in | std::stringstream::out) ;
    if ((  err = parse_get_req(inbuf,req)) < 0)
        return err;
    //process Act field
    param = req["Action"];
 // SHITTY CODE REFACTORY IT !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if( !param.compare("Auth"))
    {

        int u_id = auth_user(req["UserName"].c_str(),req["UserPassword"].c_str());
        dprintf("[DB]: u_id = %d\n",u_id);
        resp<<"Uid="<<u_id<<"\n\n";

        if((bytes_sent = send(sfd,resp.str().c_str(),resp.str().size(),0)) != -1)
        {
            dprintf("[server]: successfull send %d bytes\n",bytes_sent);
        }
    }else
        if( !param.compare("GetPids"))
        {

            if(get_user_pids(req["Uid"].c_str(),resp) == -1)
                return -1;
            if((bytes_sent = send(sfd,resp.str().c_str(),resp.str().size(),0)) != -1)
            {
                dprintf("[server]: success send %d bytes\n",bytes_sent);
            }
        }else
            if( !param.compare("GetPidDescription"))
            {
                if(get_project_description(req["Pid"].c_str(),resp) == -1)
                {
                    return -1;
                }

                if((bytes_sent = send(sfd,resp.str().c_str(),resp.str().size(),0)) != -1)
                {
                    dprintf("[server]: successfull send %d bytes\n",bytes_sent);
                }
            }else if ( !param.compare("GetWorkUnit") )
            {
                int u_id;
                int coeff = atoi(req["Freq"].c_str());
                if(( u_id =  auth_user(req["UserName"].c_str(),req["UserPassword"].c_str())) >= 0)
                {
                    key32 wu_id = -1;
                    key32 g_id = -1;
                    if((wu_id = wu_workset.allocate_wu_to_calc(u_id,coeff,&g_id)) >= 0)
                    {
                        resp<<"WuId="<<wu_id<<"\nGid="<<g_id<<"\n\n";
                    }else
                        return -1;
                }else
                    return -1;
            }
    return 0;
}

int send_resp(char*buf, int len)
{
 //
}

void* source_thread_worker(void* connector)
{
    char recvbuf[MAX_REQUEST_LEN];
    char sendbuf[MAX_RESPONSE_LEN];
    int position  = 0;
    while(1)
    {
        int sfd,error = 0;
        char found_end = 0;
        size_t r_size;
        position = 0;

        std::string repply = "File1=";
        if( (sfd = dequeue_sfd(sfdqueue_s,&sfd_lock_s)) != -1)
        {
            dprintf("[worker] i've got connection from queue\n");
            // ... and so we must process it
            //fir'\nst we must recive data from client and parse it
            struct timeval timeout;
            timeout.tv_sec = 1;
            int m = setsockopt(sfd , SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&timeout, sizeof(timeout));
            if(m == -1)
                error = 1;
            m = setsockopt(sfd , SOL_SOCKET, SO_SNDTIMEO, (struct timeval *)&timeout, sizeof(timeout));
            if(m == -1)
                error = 1;
#ifdef _DBG
            if ( error )
                dprintf("[server]: bad socket operation \n");
#endif
            while( !found_end && !error )
            {

                   int recived = recv(sfd,recvbuf + position, MAX_REQUEST_LEN / PK_BLOCKS_COUNT ,0);



              //  dprintf("recived %d bytes, position = %d\n",recived,position);
              //  dprintf("data: %s\n",recvbuf);
                if(recived < 1)
                {
                    dprintf("[server]: cannot recive request\n");
                    error = 1;
                }
                test_str(recvbuf,MAX_REQUEST_LEN);
                if( pk_search_end_all(recvbuf, MAX_REQUEST_LEN))
                {
                    found_end = 1;
                    dprintf("[server]: packet recived:))))\n");
                    if( (error = process_request(recvbuf,MAX_REQUEST_LEN,sfd)) == 0 )
                    {
                        dprintf("[server]: request parsed ok\n");
                        // process and send


                    }


                }else
                {
                    if( position + recived >= MAX_REQUEST_LEN )
                        error = 1;
                    else
                        position += recived;
                }
            }
            if(error)
                dprintf("[server]: bad request\n");
            if(found_end)
                dprintf("[server]: got  request with end \n");

            close(sfd);

        }
    }

}


int init_server(int sport,int eport ,int backlog,int* sock_source,int* sock_dest)
{
    int sfd_source;
    int sfd_dest ;
    int new_sfd;
    int yes = 1;
    int errcode = 0;
    struct sockaddr_in smy_addr,emy_addr;
    socklen_t peer_addr_size;

    smy_addr.sin_family = AF_INET;         // Host Byte Order
    smy_addr.sin_port = htons(sport);     // short, network byte order
    smy_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
    memset(&(smy_addr.sin_zero), '\0', 8);

    emy_addr.sin_family = AF_INET;         // Host Byte Order
    emy_addr.sin_port = htons(eport);     // short, network byte order
    emy_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
    memset(&(emy_addr.sin_zero), '\0', 8);

    sfd_source = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd_source == -1)
    {

        errcode = -1;
        goto error_init;
    }

    sfd_dest = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd_dest == -1)
    {
        errcode = -1;
        goto error_init;
    }



    if (bind(sfd_source, (struct sockaddr *)&smy_addr, sizeof(smy_addr))  == -1)
    {
        errcode = -3;
        goto error_init;
    }

    if (listen(sfd_source, backlog) == -1)
    {
        errcode = -4;
        goto error_init;  PACKET pk;
        char buf[50] = "Act=Auth\nUserName=root\nUserPassword=123\n\n";


        //parse_get_req(buf,pk);
       // printf("%s", pk["UserttrtrtName"].c_str());
    }
    //========================================

    if (bind(sfd_dest, (struct sockaddr *)&emy_addr, sizeof(emy_addr))  == -1)
    {
        errcode = -5;
        goto error_init;
    }

    if (listen(sfd_dest, backlog) == -1)
    {
        errcode = -6;
        goto error_init;
    }
    *sock_source = sfd_source;
    *sock_dest = sfd_dest;
    return 0;

error_init:
    dprintf("Error while initializing server with code %d\n",errcode);
    return errcode;
   // if()

}


void source_worker(int sfd_source)
{
    struct sockaddr_in client_addr;
    socklen_t sin_size;
    int new_fd;
    while(1)
    {
        sin_size = sizeof(struct sockaddr_in);
        if ((new_fd = accept(sfd_source, (struct sockaddr *)&client_addr, &sin_size)) == -1)
        {
            continue;
        }
        dprintf("server: got connection from %s\n",
                                           inet_ntoa(client_addr.sin_addr));
        //enqueue the task (one descriptor)
        enqueue_sfd(new_fd,sfdqueue_s,&sfd_lock_s);


    }

}

#define MAX_REQ_KEY 20
#define MAX_REQ_VALUE 20

int parse_get_req(char* buf, PACKET& request)
{
    std::string str = buf;
    std::string skey;
    std::string svalue;
    char key[MAX_REQ_KEY], value[MAX_REQ_VALUE];
    int line = 0;
    for(int i=0, line = 0;;)
    {
        line = str.find_first_of('\n',i);
#ifdef _STUPID_DEBUG        dprintf("line=%d i = %d\n",line,i);
#endif
        if(line == i) break;
        if(line != std::string::npos)
        {
            int k;
            if( ( k = str.find_first_of('=',i) ) != std::string::npos)
            {
                if( k >= i && k < line && k-i < MAX_REQ_KEY && line-k-1 < MAX_REQ_VALUE)
                {
                    skey = str.substr(i,k-i);
                    svalue = str.substr(k+1,line-k-1);
                    //std::cout<<skey<<'='<<svalue<<'\n';
                    request[skey] = svalue;
                    std::cout<<skey<<'='<<request[skey]<<'\n';
                }else return -1;
            }else return -1;
        }else return -1;
        i = line +1;
        if(i >= str.length())
            return -1;

    }
return 0;
}

/*
int parse_getsource_pcket(char buf[MAX_STRS], int* freq, int* mem, int* feee, char u_name[MAX_NAME], char u_pass[MAX_PASS])
{
    for( int i = 0; i < MAX_STRS; i++ )
    {
        switch( buf[i] )
        {
        case 'U':
            strcpy(u_name,buf[i])
            break;
        case 'P':1111
            break;
        case 'C':
            break;
        case 'H':
            break;
        default:
            return -1;

        }
    }

}
*/

int main()
{

    int err = 0,source = 0,dest = 0;
    if( mysql_init("root","123") !=0)
            return -1;
    if( wu_workset.connect() != 0)
        return -1;
    if(( err = init_server(2120,1112,100,&source,&dest)) != 0)
    {
        dprintf("[init]: failed starting server code %d\n",err);
        return err;
    }
    init_sfdqueue(sfdqueue_s);
    if (( err = init_tqueue(tqueue_s,source_thread_worker)) != 0)
    {
        dprintf("[init]: error createing threads pool with code %d\n",err);
        return err;
    }
    dprintf("[init]: server initialized ok. Now starting to recive data :)\n");
    source_worker(source);

    return 0;

}
