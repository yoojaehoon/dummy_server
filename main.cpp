#include <stdio.h>

#include <mysql.h>

#include "header.h"
#include "Packet.h"
#include "Database.h"
//#include "PerfInfo.h"

int  SERVER_INDEX;
int  MONITOR_SERVER_PORT;
char MONITOR_SERVER_LOG_PATH[256];
int  LIMIT_SIZE;
int  PROCESS_POOL_SIZE;
char DATABASE_SERVER[256];
char DATABASE_USER[128];
char DATABASE_PASSWORD[128];
char DATABASE_DATABASE[128];
int  DATABASE_POOL_SIZE;
char LOCAL_IP[32];

int ALIVE_CHECK_INTERVAL;
int FAIL_COUNT_LIMIT;
int ALERT_COUNT;

typedef std::deque<int *> socket_deque;

sem_t g_processPoolSemaphore;
pthread_mutex_t g_processPoolLock = PTHREAD_MUTEX_INITIALIZER;
socket_deque g_processPoolQueue;

int g_processActiveCount;
pthread_mutex_t g_ConnectLock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t g_AccessDBLock = PTHREAD_MUTEX_INITIALIZER;



#include "main_monitorserver.cpp"
#include "main_clean.cpp"
#include "main_report.cpp"

int main(int argc, char *argv[])
{
    FILE *file;

    file = (FILE *)fopen("/opt/inno_srv2/conf/monitorserver.conf", "r");

    if (file == NULL)
    {
        fprintf(stderr, "conf/monitorserver.conf NOT FIND.\n");
        exit(EXIT_FAILURE);
    }

    char buffer[500];
    int confcount = 0;
    
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        fscanf(file, "%[^\n]\n", buffer);
        
        if (strlen(buffer) == 0)
            break;
            
        switch (confcount)
        {
            case 0:
                sscanf(buffer, "%d", &SERVER_INDEX);
                break;
            case 1:
                sscanf(buffer, "%d", &MONITOR_SERVER_PORT);
                break;
            case 2:
                sscanf(buffer, "%s", MONITOR_SERVER_LOG_PATH);
                break;
            case 3:
                sscanf(buffer, "%d", &LIMIT_SIZE);
                break;
            case 4:
                sscanf(buffer, "%d", &PROCESS_POOL_SIZE);
                break;
            case 5:
                sscanf(buffer, "%s", DATABASE_SERVER);
                break;
            case 6:
                sscanf(buffer, "%s", DATABASE_USER);
                break;
            case 7:
                sscanf(buffer, "%s", DATABASE_PASSWORD);
                break;
            case 8:
                sscanf(buffer, "%s", DATABASE_DATABASE);
                break;
            case 9:
                sscanf(buffer, "%d", &DATABASE_POOL_SIZE);
                break;
            case 10:
                sscanf(buffer, "%s", LOCAL_IP);
                break;
        }
        
        confcount++;
    }   
    
    fclose(file);
    
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

    signal(SIGPIPE, SIG_IGN);

    struct rlimit new_rl;

    new_rl.rlim_cur = LIMIT_SIZE;
    new_rl.rlim_max = LIMIT_SIZE;

    if( setrlimit(RLIMIT_NOFILE, &new_rl) < 0 )
    {
        CMiscUtil::WriteErrorLog("Set Limit Error", -1, 0);
        exit(EXIT_SUCCESS);
    }

    CMiscUtil::InitLog(MONITOR_SERVER_LOG_PATH);

    //CPerfInfo::Init(SERVER_INDEX, LOCAL_IP);
    
    //my_init();
    mysql_thread_init();

    for (int i=0; i<100; i++)
    {
        int result = CDatabase::Init(DATABASE_SERVER, DATABASE_USER, DATABASE_PASSWORD, DATABASE_DATABASE, DATABASE_POOL_SIZE);

        if (!result)
        {
            CMiscUtil::WriteErrorLog("Database Ã±â µµÁ ¿7ù", -1, 0);
        
            if (i == 99)
            {
                CMiscUtil::WriteErrorLog("Database ", -1,0);
                exit(EXIT_SUCCESS); 
            }
            else
            {
                CDatabase::Destroy();
                sleep(30);
            }
        }
        else
            break;
    }

    MYSQL_RES *res_ptr;
    MYSQL_ROW sqlrow;

    char query[1024];
    sprintf(query, "SELECT check_interval, fail_count, alert_count FROM monitor_policy WHERE status = 1 ORDER BY created_at DESC LIMIT 0, 1");

    if( CDatabase::QueryResult(query, &res_ptr) )
    {
        if( (sqlrow = mysql_fetch_row(res_ptr)))
        {
            sscanf(sqlrow[0], "%d", &ALIVE_CHECK_INTERVAL);
            sscanf(sqlrow[1], "%d", &FAIL_COUNT_LIMIT);
            sscanf(sqlrow[2], "%d", &ALERT_COUNT);
        }
        else{
            CMiscUtil::WriteErrorLog("Policy data empty", -1, 0);
            ALIVE_CHECK_INTERVAL = 10;
            FAIL_COUNT_LIMIT = 3;
            ALERT_COUNT = 3;
            //exit(EXIT_SUCCESS);
        }
    }
    else{
        CMiscUtil::WriteErrorLog("Policy read error", -1, 0);
        exit(EXIT_SUCCESS);
    }

    //char query[512];
    //sprintf(query, "UPDATE server_info SET writedate=NOW() WHERE serveridx=%d", SERVER_INDEX);
    //CDatabase::ExecuteSQL(query);
    
    pthread_t c_thread;
    pthread_attr_t thread_attr;
    
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);


    int result = sem_init(&g_processPoolSemaphore, 0, 0);
    if( result )
    {
        CMiscUtil::WriteErrorLog("PeerManager Process Pool Semaphore Create Error", -1, 0);
        exit(EXIT_SUCCESS);
    }

    for(int i=0; i<PROCESS_POOL_SIZE; i++)
    {
        int *thread_number = new int;
        *thread_number = i+1;

        result = pthread_create(&c_thread, &thread_attr, monitor_process, (void *)thread_number);

        if( result )
        {
            CMiscUtil::WriteErrorLog("PeerManager Process Thread Create Error", -1, 0);
            exit(EXIT_FAILURE);
        }
    }


    result = pthread_create(&c_thread, &thread_attr, report_thread, NULL);
    if (result != 0)
    {
       CMiscUtil::WriteErrorLog("Report Thread »ýÁ ¿7ù", -1, 0);
       exit(EXIT_SUCCESS);      
    }

    /*result = pthread_create(&c_thread, &thread_attr, report_perf_thread, NULL);
    if (result != 0)
    {
       CMiscUtil::WriteErrorLog("Report Perf Thread »ýÁ ¿7ù", -1, 0);
       exit(EXIT_SUCCESS);      
    }
    */

    result = pthread_create(&c_thread, &thread_attr, clean_thread, NULL);
    if (result != 0)
    {
        CMiscUtil::WriteErrorLog("Clean Thread »ýÁ ¿7ù", -1, 0);
        exit(EXIT_SUCCESS);     
    }

    int server_sockfd, server_len;
    struct sockaddr_in server_address;

    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(MONITOR_SERVER_PORT);
    
    server_len = sizeof(server_address);

    int optionval = 1;
    setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &optionval, 4);
    bind(server_sockfd, (struct sockaddr *)&server_address, server_len);
    
    listen(server_sockfd, 100);


    while(1)
    {
        int client_sockfd;
        int client_len;
        struct sockaddr_in client_address;



        client_len = sizeof(client_address);
        client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_address, (socklen_t *)&client_len);
        if( client_sockfd == -1 )
        {
            CMiscUtil::WriteErrorLog("accept()1", -1, 0);
            sleep(1);
            continue;
        }
        
        int so_keepalive = TRUE;
        int result = setsockopt(client_sockfd, SOL_SOCKET, SO_KEEPALIVE, &so_keepalive, sizeof(int));
        if (result)
            CMiscUtil::WriteErrorLog("KeepAlive Option Error", client_sockfd, 0);

        int optionval = TRUE;
        result = setsockopt(client_sockfd, SOL_SOCKET, SO_REUSEADDR, &optionval, sizeof(int));
        if (result)
            CMiscUtil::WriteErrorLog("REUSEADDR Option Error", client_sockfd, 0);
        
        optionval = TRUE;
        result = setsockopt(client_sockfd, IPPROTO_TCP, TCP_NODELAY, &optionval, sizeof(int));
            
        if (result)
            CMiscUtil::WriteErrorLog("No Delay Option Error", client_sockfd, 0);

        optionval = SOCKET_BUFFER_SIZE;
        result = setsockopt(client_sockfd, SOL_SOCKET, SO_RCVBUF, &optionval, sizeof(int));
        if (result)
            CMiscUtil::WriteErrorLog("Receive Buffer Option Error", client_sockfd, 0);

        optionval = SOCKET_BUFFER_SIZE;
        result = setsockopt(client_sockfd, SOL_SOCKET, SO_SNDBUF, &optionval, sizeof(int));
        if (result)
            CMiscUtil::WriteErrorLog("Send Buffer Option Error", client_sockfd, 0);

        struct linger ling;
        ling.l_onoff = 1;
        ling.l_linger = 0;
        result = setsockopt(client_sockfd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
        if (result)
            CMiscUtil::WriteErrorLog("LINGER Option Error", client_sockfd, 0);

        int *sockfd = new int();
        *sockfd = client_sockfd;

        pthread_mutex_lock(&g_processPoolLock);
        g_processPoolQueue.push_back(sockfd);
        pthread_mutex_unlock(&g_processPoolLock);
        
        sem_post(&g_processPoolSemaphore);
    }
    
    close(server_sockfd);
    exit(EXIT_SUCCESS);     
}


