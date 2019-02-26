int command_heartbeat_report(CPacket &packet, int client_sockfd);
int command_server_restart(CPacket &packet, int client_sockfd);
int command_perf_report(CPacket &packet, int client_sockfd);
int command_alive_report(CPacket &packet, int client_sockfd);
int command_refresh_info(CPacket &packet, int client_sockfd);

/////////////////////////////////////////////////////////////////
//
// FileManager
/////////////////////////////////////////////////////////////////
void *monitor_process(void *arg)
{
    int client_sockfd, result, threadnum;
    int isClose;
    struct epoll_event event;

    threadnum = *((int *)arg);
    delete (int *)arg;

    mysql_thread_init();
    srand((unsigned)time(NULL));

    struct timeval start_time, end_time;

    while(TRUE)
    {
        sem_wait(&g_processPoolSemaphore);
        pthread_mutex_lock(&g_processPoolLock);

        int *sockfd = g_processPoolQueue.front();
        g_processPoolQueue.pop_front();

        pthread_mutex_unlock(&g_processPoolLock);

        client_sockfd = *sockfd;
        isClose = FALSE;

        //CMiscUtil::WriteLog("MONITOR PROCESS START", client_sockfd, 1, &threadnum);
        
        pthread_mutex_lock(&g_ConnectLock);
        g_processActiveCount++;
        pthread_mutex_unlock(&g_ConnectLock);

        //CPerfInfo::AddCount(1, 0, 0, 0);

        gettimeofday(&start_time, NULL);

        int nread;
        ioctl(client_sockfd, FIONREAD, &nread);
        if( nread == 0 )
        {
            CMiscUtil::WriteLog("PEER MANAGER PROCESS - SOCKET CLOSED", client_sockfd, 0);
            isClose = TRUE;
        }
        else
        {
            CPacket packet;
            packet.InitPacket(client_sockfd);

            result = TRUE;

            if( !packet.ReceivePacket() )
            {
                CMiscUtil::WriteErrorLog("RecivePakcet() FAILD", client_sockfd, 0);
                isClose = TRUE;
            }
            else
            {
                switch(packet.m_nCommand)
                {
                    case REQ_PERF_REPORT:
                        result = command_perf_report(packet, client_sockfd);
                        break;
                    
                    case REQ_SERVER_RESTART:
                        result = command_server_restart(packet, client_sockfd);
                        break;

                    case REQ_REGISTRY_HEARTBEAT:
                        result = command_heartbeat_report(packet, client_sockfd);
                        break;
                    case REQ_ALIVE_STATUS:
                        result = command_alive_report(packet, client_sockfd);
                        break;
                    case REQ_REFRESH_INFO:
                        result = command_refresh_info(packet, client_sockfd);
                        break;
                    default:
                        CMiscUtil::WriteLog("MONITOR PROCESS - UNKNOWN COMMAND", client_sockfd, 1, &packet.m_nCommand);
                        result = FALSE;
                        break;
                }
            }

            if( !result )
            {
                CPacket send_packet;
                send_packet.InitPacket(CAT_RESPONSE, packet.m_nCommand, client_sockfd);

                send_packet.WriteInt(ERR_FAILED);
                send_packet.SendPacket();

                //CPerfInfo::AddCount(0, 1, 0, 0);
            }
        }

        if( isClose )
        {
            delete sockfd;
            close(client_sockfd);
        }
        else
        {
            // EPOLL 
            event.events = EPOLLIN | EPOLLET;
            event.data.ptr = sockfd;

            if( epoll_ctl(g_eventpoll, EPOLL_CTL_ADD, client_sockfd, &event) < 0 )
            {
                CMiscUtil::WriteErrorLog("wait_host_thread() - EPOLL ADD ERROR", client_sockfd, 0);
                delete sockfd;
                close(client_sockfd);
            }
        }
        
        //CMiscUtil::WriteLog("MONITOR PROCESS END", client_sockfd, 1, &threadnum);

        gettimeofday(&end_time, NULL);
        if( end_time.tv_usec < start_time.tv_usec )
        {
            end_time.tv_sec--;
            end_time.tv_usec += 1000000;
        }
        /*double interval = (end_time.tv_sec - start_time.tv_sec)*1000 + (end_time.tv_usec - start_time.tv_usec)/1000;
        CPerfInfo::AddTransTime(interval); */
    }

    mysql_thread_end();
    pthread_exit(NULL);
}


int command_heartbeat_report(CPacket &packet,int client_sockfd)
{
    char hostname[129];
    char uuid[128];
    char client_ip[128];
    int type = 0;

    int size;
    struct sockaddr_in sock;
    char* ip;
    size = sizeof(sock);
    memset(&sock, 0x00, size);
    getpeername(client_sockfd, (struct sockaddr *)&sock, (socklen_t *)&size);
    ip = inet_ntoa(sock.sin_addr);
    CMiscUtil::WriteLog("COMMAND CLIENT ADDRESS", client_sockfd, 3, ip);

    if( !packet.ReadString(hostname, 128))
        return FALSE;
    if( !packet.ReadInt(type))
        return FALSE;
    if( !packet.ReadString(uuid, 128))
        return FALSE;
    if( !packet.ReadString(client_ip, 128))
        return FALSE;
    if( !packet.IsEOD())
        return FALSE;

    CMiscUtil::WriteLog("COMMAND HEARTBEAT REPORT", client_sockfd, 3, hostname);

    MYSQL_RES* res_ptr;
    MYSQL_ROW retrow;

    char query[512];

    CPacket send_packet;
    send_packet.InitPacket(CAT_RESPONSE, RSP_REGISTRY_HEARTBEAT, client_sockfd);
    //sprintf(query, "Select id from ne_server where host='%s' and type=%d", hostname, type);
    //

    pthread_mutex_lock(&g_AccessDBLock);
    sprintf(query, "Select id from ne_server where host='%s' and type=%d and uuid='%s' and failcnt <= %d and deleted != 1", hostname , type , uuid, FAIL_COUNT_LIMIT);
    CMiscUtil::WriteLog("DB Statements from ne_server", client_sockfd, 0);
    if( !CDatabase::QueryResult(query, &res_ptr)){
        pthread_mutex_unlock(&g_AccessDBLock);
        return FALSE;
    }


    int id = 0;
    if( (retrow = mysql_fetch_row(res_ptr)) )
    {
        sscanf(retrow[0], "%d", &id);
        sprintf(query, "Update ne_server SET alive_status=1, ip='%s',updated_at=NOW(), failcnt=0 WHERE id=%d and host='%s'", client_ip,id,hostname);
        CMiscUtil::WriteLog("Update to new_server existed host", client_sockfd, 0);
        if( !CDatabase::ExecuteSQL(query)){
            pthread_mutex_unlock(&g_AccessDBLock);
            return FALSE;
        }
    }
    else
    {
        sprintf(query, "Insert into ne_server (host, type, created_at, updated_at, alive_status, uuid, ip) values ('%s', %d, NOW(), NOW(), 1, '%s', '%s') ", hostname, type, uuid, client_ip);
        CMiscUtil::WriteLog("Insert into ne_server new host", client_sockfd, 3, hostname);
        if( !CDatabase::ExecuteSQL(query)){
            pthread_mutex_unlock(&g_AccessDBLock);
            return FALSE;
        }

    }

    pthread_mutex_unlock(&g_AccessDBLock);

    if( !send_packet.WriteInt(ERR_NONE))
        return FALSE;
    if( !send_packet.SendPacket() )
        return FALSE;
    return TRUE;
}


int command_server_restart(CPacket &packet,int client_sockfd)
{
/*    int serveridx;
    char query[1024];
    
    if( !packet.ReadInt(serveridx) )
        return FALSE;
    if( !packet.IsEOD() )
        return FALSE;

    CPacket send_packet;
    send_packet.InitPacket(CAT_RESPONSE, RSP_SERVER_RESTART, client_sockfd);

    sprintf(query, "UPDATE server_info SET writedate=NOW() WHERE serveridx=%d", serveridx);
    if( !CDatabase::ExecuteSQL(query) )
        return FALSE;

    // À´äì À¼Û  if( !send_packet.WriteInt(ERR_NONE) )

    if( !send_packet.WriteInt(ERR_NONE))
        return FALSE;
    if( !send_packet.SendPacket() )
        return FALSE;       
*/ 
    int serveridx;
    if( !packet.ReadInt(serveridx))
        return FALSE;

    if( !packet.IsEOD())
        return FALSE;

    CPacket send_packet;
    send_packet.InitPacket(CAT_RESPONSE,RSP_SERVER_RESTART, client_sockfd);

    if ( !send_packet.WriteInt(ERR_NONE))
        return FALSE;
    if ( !send_packet.SendPacket())
        return FALSE;

    return TRUE;
}



int command_perf_report(CPacket &packet,int client_sockfd)
{
    int serveridx, connect_cnt;
    char agent_ip[50];

    if( !packet.ReadInt(serveridx))
        return FALSE;

    if( !packet.ReadInt(connect_cnt))
        return FALSE;

    if( !packet.IsEOD() )
        return FALSE;

    CPacket send_packet;
    send_packet.InitPacket(CAT_RESPONSE, RSP_PERF_REPORT, client_sockfd);

    int client_len;
    struct sockaddr_in client_address;

    client_len = sizeof(client_address);
    getpeername(client_sockfd, (struct sockaddr *)&client_address, (socklen_t *)&client_len);

    inet_ntop(AF_INET, (void *)&(client_address.sin_addr), agent_ip, sizeof(agent_ip));

    if ( !send_packet.WriteInt(ERR_NONE))
        return FALSE;
    if ( !send_packet.SendPacket())
        return FALSE;

    return TRUE;

}

int command_alive_report(CPacket &packet, int client_sockfd)
{
    char hostname[128];
    char uuid[128];

    if( !packet.ReadString(hostname, 128))
        return FALSE;
    if( !packet.ReadString(uuid, 128))
        return FALSE;
    if( !packet.IsEOD())
        return FALSE;

    CMiscUtil::WriteLog("COMMAND ALIVE REPORT", client_sockfd, 3, hostname);

    //MYSQL_RES* res_ptr;
    //MYSQL_ROW retrow;

    char query[512];

    CPacket send_packet;
    send_packet.InitPacket(CAT_RESPONSE, RSP_ALIVE_STATUS, client_sockfd);

    pthread_mutex_lock(&g_AccessDBLock);
    sprintf(query,"Insert into alive_status (hostname, cpu_resource, network_latency, reported_at, uuid) values ('%s', 0, 0, NOW(), '%s') ", hostname, uuid);
    if( !CDatabase::ExecuteSQL(query)){
        pthread_mutex_unlock(&g_AccessDBLock);
        return FALSE;
    }
    pthread_mutex_unlock(&g_AccessDBLock);

    sprintf(query, "Update ne_server SET alive_status=1,updated_at=NOW(),failcnt=0 WHERE uuid='%s' and host='%s' and deleted != 1 and deleted_at is NULL", uuid,hostname);
    CMiscUtil::WriteLog("Update to ne_server alivestatus of host", client_sockfd, 0);
    pthread_mutex_lock(&g_AccessDBLock);
    if( !CDatabase::ExecuteSQL(query)){
        pthread_mutex_unlock(&g_AccessDBLock);
        return FALSE;
    }
    pthread_mutex_unlock(&g_AccessDBLock);

    if( !send_packet.WriteInt(ERR_NONE))
        return FALSE;
    if( !send_packet.SendPacket())
        return FALSE;

    //mysql_thread_end();
    return TRUE;
}

int command_refresh_info(CPacket &packet, int client_sockfd)
{
    char hostname[129];
    char uuid[128];
    char client_ip[128];
    int type = 0;

    int size;
    struct sockaddr_in sock;
    char* ip;
    size = sizeof(sock);
    memset(&sock, 0x00, size);
    getpeername(client_sockfd, (struct sockaddr *)&sock, (socklen_t *)&size);
    ip = inet_ntoa(sock.sin_addr);
    CMiscUtil::WriteLog("COMMAND CLIENT ADDRESS", client_sockfd, 3, ip);

    if( !packet.ReadString(hostname, 128))
        return FALSE;
    if( !packet.ReadInt(type))
        return FALSE;
    if( !packet.ReadString(uuid, 128))
        return FALSE;
    if( !packet.ReadString(client_ip, 128))
        return FALSE;
    if( !packet.IsEOD())
        return FALSE;

    CMiscUtil::WriteLog("COMMAND REFRESH INFO REPORT", client_sockfd, 3, hostname);

    MYSQL_RES* res_ptr;
    MYSQL_ROW retrow;

    char query[512];

    CPacket send_packet;
    send_packet.InitPacket(CAT_RESPONSE, RSP_REFRESH_INFO, client_sockfd);
    //sprintf(query, "Select id from ne_server where host='%s' and type=%d", hostname, type);
    //

    pthread_mutex_lock(&g_AccessDBLock);
    sprintf(query, "Select id from ne_server where type=%d and uuid='%s' and failcnt <= %d and deleted != 1 and deleted_at is NULL", type , uuid, FAIL_COUNT_LIMIT);
    CMiscUtil::WriteLog("DB Statements from ne_server", client_sockfd, 0);
    if( !CDatabase::QueryResult(query, &res_ptr)){
        pthread_mutex_unlock(&g_AccessDBLock);
        return FALSE;
    }


    int id = 0;
    if( (retrow = mysql_fetch_row(res_ptr)) )
    {
        sscanf(retrow[0], "%d", &id);
        sprintf(query, "Update ne_server SET alive_status=1, ip='%s', host='%s',updated_at=NOW(),failcnt=0 WHERE id=%d", client_ip,hostname,id);
        CMiscUtil::WriteLog("Update to ne_server existed host", client_sockfd, 0);
        if( !CDatabase::ExecuteSQL(query)){
            pthread_mutex_unlock(&g_AccessDBLock);
            return FALSE;
        }
    }
    else
    {
        sprintf(query, "Insert into ne_server (host, type, created_at, alive_status, uuid, ip) values ('%s', %d, NOW(), 1, '%s', '%s') ", hostname, type, uuid, client_ip);
        CMiscUtil::WriteLog("Insert into ne_server new host", client_sockfd, 3, hostname);
        if( !CDatabase::ExecuteSQL(query)){
            pthread_mutex_unlock(&g_AccessDBLock);
            return FALSE;
        }

    }

    pthread_mutex_unlock(&g_AccessDBLock);

    if( !send_packet.WriteInt(ERR_NONE))
        return FALSE;
    if( !send_packet.SendPacket() )
        return FALSE;
    return TRUE;
}

