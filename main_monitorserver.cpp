int command_heartbeat_report(CPacket &packet, int client_sockfd);
int command_server_restart(CPacket &packet, int client_sockfd);
int command_perf_report(CPacket &packet, int client_sockfd);
int command_alive_report(CPacket &packet, int client_sockfd);
int command_refresh_info(CPacket &packet, int client_sockfd);
int command_metadata_report(CPacket &packet, int client_sockfd);


/////////////////////////////////////////////////////////////////
//
// FileManager
/////////////////////////////////////////////////////////////////
void *monitor_process(void *arg)
{
    int client_sockfd, result, threadnum, nread;
    fd_set readfds, testfds;
    struct timeval timeout;

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
        delete (int *)sockfd;

        //CMiscUtil::WriteLog("MONITOR PROCESS START", client_sockfd, 1, &threadnum);
        
        while(1)
        {
            FD_ZERO(&readfds);
            FD_SET(client_sockfd, &readfds);

            testfds = readfds;
            timeout.tv_sec = 3;
            timeout.tv_usec = 0;

            result = select(FD_SETSIZE, &testfds, (fd_set *)NULL, (fd_set *)NULL, &timeout);

            if( result == 0)
            {
                CMiscUtil::WriteLog("monitor_thread - TIME OUT", client_sockfd, 0);
                break;
            }
            if( result == -1)
            {
                CMiscUtil::WriteLog("monitor_thread - SOCKET CLOSED", client_sockfd, 0);
                break;
            }

            ioctl(client_sockfd, FIONREAD, &nread);

            if( nread == 0)
            {
                CMiscUtil::WriteLog("monitor_thread - SOCKET CLOSET-1", client_sockfd, 0);
                break;
            }
            gettimeofday(&start_time, NULL);

            CPacket packet;
            packet.InitPacket(client_sockfd);

            if( !packet.ReceivePacket() )
            {
                CMiscUtil::WriteErrorLog("RecivePakcet() FAILD", client_sockfd, 0);
                result = FALSE;
            }
            else
            {
                CMiscUtil::WriteLog("monitor_thread - COMMAND", client_sockfd, 1, &packet.m_nCommand);
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
                    case REQ_METADATA_REPORT:
                        result = command_metadata_report(packet, client_sockfd);
                        break;
                    default:
                        CMiscUtil::WriteLog("MONITOR PROCESS - UNKNOWN COMMAND", client_sockfd, 1, &packet.m_nCommand);
                        result = FALSE;
                        break;
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
            gettimeofday(&end_time, NULL);
            if( end_time.tv_usec < start_time.tv_usec )
            {
                end_time.tv_sec--;
                end_time.tv_usec += 1000000;
            }

        }

        close(client_sockfd);
    }

    mysql_thread_end();
    pthread_exit(NULL);
}

int command_heartbeat_report(CPacket &packet,int client_sockfd)
{
    char metaname[128];
    char host[128];
    char uuid[128];
    char client_ip[128];
    char availability_zone[32];
    char external_ip[32];
    int type = 0;

    int size;
    struct sockaddr_in sock;
    char* ip;
    size = sizeof(sock);
    memset(&sock, 0x00, size);
    getpeername(client_sockfd, (struct sockaddr *)&sock, (socklen_t *)&size);
    ip = inet_ntoa(sock.sin_addr);
    CMiscUtil::WriteLog("COMMAND CLIENT ADDRESS", client_sockfd, 3, ip);

    if( !packet.ReadString(metaname, 128))
        return FALSE;
    if( !packet.ReadString(host, 128))
        return FALSE;
    if( !packet.ReadInt(type))
        return FALSE;
    if( !packet.ReadString(uuid, 128))
        return FALSE;
    if( !packet.ReadString(client_ip, 128))
        return FALSE;
    if( !packet.ReadString(availability_zone, 32))
        return FALSE;
    if( !packet.ReadString(external_ip, 32))
        return FALSE;
    if( !packet.IsEOD())
        return FALSE;

    CMiscUtil::WriteLog("COMMAND HEARTBEAT REPORT", client_sockfd, 3, metaname);

    MYSQL_RES* res_ptr;
    MYSQL_ROW retrow;

    char query[512];

    CPacket send_packet;
    send_packet.InitPacket(CAT_RESPONSE, RSP_REGISTRY_HEARTBEAT, client_sockfd);
    //sprintf(query, "Select id from ne_server where host='%s' and type=%d", hostname, type);
    //

    pthread_mutex_lock(&g_AccessDBLock);
    sprintf(query, "Select id from ne_server where type=%d and uuid='%s' and failcnt <= %d and deleted != 1", type , uuid, FAIL_COUNT_LIMIT);
    CMiscUtil::WriteLog("DB Statements from ne_server", client_sockfd, 0);
    if( !CDatabase::QueryResult(query, &res_ptr)){
        pthread_mutex_unlock(&g_AccessDBLock);
        return FALSE;
    }


    int id = 0;
    if( (retrow = mysql_fetch_row(res_ptr)) )
    {
        sscanf(retrow[0], "%d", &id);
        sprintf(query, "Update ne_server SET alive_status=1, ip='%s',updated_at=NOW(),availability_zone='%s',external_ip='%s',hostname='%s',host='%s', failcnt=0 WHERE id=%d", client_ip,availability_zone,external_ip,metaname,host,id);
        CMiscUtil::WriteLog("Update to new_server existed host", client_sockfd, 0);
        if( !CDatabase::ExecuteSQL(query)){
            pthread_mutex_unlock(&g_AccessDBLock);
            return FALSE;
        }
    }
    else
    {
        sprintf(query, "Insert into ne_server (hostname, type, created_at, updated_at, alive_status, uuid, ip,external_ip,host,availability_zone) values ('%s', %d, NOW(), NOW(), 1, '%s', '%s','%s','%s','%s') ", metaname, type, uuid, client_ip, external_ip,host,availability_zone);
        CMiscUtil::WriteLog("Insert into ne_server new host", client_sockfd, 3, metaname);
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

int command_metadata_report(CPacket &packet,int client_sockfd)
{
    char uuid[64];
    char external_ip[64];
    int meta_health = -1;
    if( !packet.ReadString(uuid,64))
        return FALSE;
    if( !packet.ReadString(external_ip,64))
        return FALSE;
    if( !packet.ReadInt(meta_health))
        return FALSE;
    if( !packet.IsEOD())
        return FALSE;

    CPacket send_packet;
    send_packet.InitPacket(CAT_RESPONSE,RSP_METADATA_REPORT, client_sockfd);
    CMiscUtil::WriteLog("COMMAND METADATA REPORT >IP", client_sockfd, 3, external_ip);
    CMiscUtil::WriteLog("COMMAND METADATA REPORT >NAME", client_sockfd, 1, &meta_health);

    MYSQL_RES* res_ptr;
    MYSQL_ROW retrow;

    char query[512];

    pthread_mutex_lock(&g_AccessDBLock);
    sprintf(query, "Select id from ne_server where uuid='%s' and failcnt <= %d and deleted != 1", uuid, FAIL_COUNT_LIMIT);
    CMiscUtil::WriteLog("DB Statements from ne_server", client_sockfd, 0);
    if( !CDatabase::QueryResult(query, &res_ptr)){
        pthread_mutex_unlock(&g_AccessDBLock);
        return FALSE;
    }


    int id = 0;
    if( (retrow = mysql_fetch_row(res_ptr)) )
    {
        sscanf(retrow[0], "%d", &id);
        sprintf(query, "Update ne_server SET updated_at=NOW(),external_ip='%s', failcnt=0, meta_health=%d WHERE id=%d", external_ip,meta_health,id);
        CMiscUtil::WriteLog("Update to new_server existed host", client_sockfd, 0);
        if( !CDatabase::ExecuteSQL(query)){
            pthread_mutex_unlock(&g_AccessDBLock);
            return FALSE;
        }
    }
    pthread_mutex_unlock(&g_AccessDBLock);


    if( !send_packet.WriteInt(ERR_NONE))
        return FALSE;
    if( !send_packet.SendPacket())
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
    char metaname[128];
    char uuid[128];

    if( !packet.ReadString(metaname, 128))
        return FALSE;
    if( !packet.ReadString(uuid, 128))
        return FALSE;
    if( !packet.IsEOD())
        return FALSE;

    CMiscUtil::WriteLog("COMMAND ALIVE REPORT", client_sockfd, 3, metaname);

    //MYSQL_RES* res_ptr;
    //MYSQL_ROW retrow;

    char query[512];

    CPacket send_packet;
    send_packet.InitPacket(CAT_RESPONSE, RSP_ALIVE_STATUS, client_sockfd);

    pthread_mutex_lock(&g_AccessDBLock);
    sprintf(query,"Insert into alive_status (hostname, cpu_resource, network_latency, reported_at, uuid) values ('%s', 0, 0, NOW(), '%s') ", metaname, uuid);
    if( !CDatabase::ExecuteSQL(query)){
        pthread_mutex_unlock(&g_AccessDBLock);
        return FALSE;
    }
    pthread_mutex_unlock(&g_AccessDBLock);

    sprintf(query, "Update ne_server SET alive_status=1,updated_at=NOW(),failcnt=0,alertcnt=0 WHERE uuid='%s' and hostname='%s' and deleted != 1 and deleted_at is NULL", uuid,metaname);
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
    char metaname[128];
    char host[128];
    char uuid[128];
    char client_ip[128];
    char availability_zone[32];
    char external_ip[32];
    int type = 0;

    int size;
    struct sockaddr_in sock;
    char* ip;
    size = sizeof(sock);
    memset(&sock, 0x00, size);
    getpeername(client_sockfd, (struct sockaddr *)&sock, (socklen_t *)&size);
    ip = inet_ntoa(sock.sin_addr);
    CMiscUtil::WriteLog("COMMAND CLIENT ADDRESS", client_sockfd, 3, ip);

    if( !packet.ReadString(metaname, 128))
        return FALSE;
    if( !packet.ReadString(host, 128))
        return FALSE;
    if( !packet.ReadInt(type))
        return FALSE;
    if( !packet.ReadString(uuid, 128))
        return FALSE;
    if( !packet.ReadString(client_ip, 128))
        return FALSE;
    if( !packet.ReadString(availability_zone, 32))
        return FALSE;
    if( !packet.ReadString(external_ip, 32))
        return FALSE;
    if( !packet.IsEOD())
        return FALSE;

    CMiscUtil::WriteLog("COMMAND REFRESH INFO REPORT", client_sockfd, 3, metaname);

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
        sprintf(query, "Update ne_server SET alive_status=1, ip='%s',updated_at=NOW(),availability_zone='%s',external_ip='%s',hostname='%s',host='%s', failcnt=0 WHERE id=%d", client_ip,availability_zone,external_ip,metaname,host,id);
        CMiscUtil::WriteLog("Update to ne_server existed host", client_sockfd, 0);
        if( !CDatabase::ExecuteSQL(query)){
            pthread_mutex_unlock(&g_AccessDBLock);
            return FALSE;
        }
    }
    else
    {
        sprintf(query, "Insert into ne_server (host, type, created_at, alive_status, uuid, ip,external_ip,availability_zone,hostname) values ('%s', %d, NOW(), 1, '%s', '%s','%s','%s','%s') ", host, type, uuid, client_ip,external_ip,availability_zone,metaname);
        CMiscUtil::WriteLog("Insert into ne_server new host", client_sockfd, 3, metaname);
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

