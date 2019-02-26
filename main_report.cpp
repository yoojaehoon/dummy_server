/////////////////////////////////////////////////////////////////
//
// Report Thread
//
/////////////////////////////////////////////////////////////////

void check_alive_servers();
void send_alert(const char* message);

void *report_thread(void *arg)
{
    
    mysql_thread_init();

    while (1)
    {

        check_alive_servers();

        sleep(ALIVE_CHECK_INTERVAL);
    }
    
    mysql_thread_end();
    pthread_exit(NULL);
}

void check_metadata_alive_servers()
{
    char query[1024];

    int  id;
    char host[128];
    char uuid[128];
    char external_ip[32];
    char availability_zone[32];

    MYSQL_RES *res_ptr;
    MYSQL_ROW sqlrow;

    sprintf(query,"SELECT id, host,uuid,external_ip,availability_zone FROM ne_server WHERE meta_health=0 and deleted != 1 and deleted_at IS NULL AND updated_at < DATE_SUB(NOW(), INTERVAL 1 MINUTE) AND alerted_at < DATE_SUB(NOW(), INTERVAL 5 MINUTE)");
    pthread_mutex_lock(&g_AccessDBLock);
    if( !CDatabase::QueryResult(query, &res_ptr))
    {
        pthread_mutex_unlock(&g_AccessDBLock);
        return;
    }

    while( (sqlrow = mysql_fetch_row(res_ptr)))
    {
        sscanf(sqlrow[0], "%d", &id);
        sprintf(host, "%s", sqlrow[1]);
        sprintf(uuid, "%s", sqlrow[2]);
        sprintf(external_ip, "%s", sqlrow[3]);
        sprintf(availability_zone, "%s", sqlrow[4]);

        char message[1024];
        sprintf(message,"\"--------MetadataProxy unreachable!-----\nHostname : %s\nUUID : %s\nExternal_IP : %s\nAvailability_zone : %s\"", host, uuid, external_ip, availability_zone);
        send_alert(message);
    }
}

void check_alive_servers()
{
    char query[1024];
    char query_sub[1024];
    int id;
    int fail_cnt;
    int alert_cnt;
    char hostname[128];
    char uuid[128];
    char external_ip[32];
    char availability_zone[32];

    MYSQL_RES *res_ptr;
    MYSQL_ROW sqlrow;

    sprintf(query,"SELECT id, failcnt,alertcnt,host,uuid,external_ip,availability_zone FROM ne_server WHERE deleted != 1 and deleted_at IS NULL AND updated_at < DATE_SUB(NOW(), INTERVAL %d SECOND)", ALIVE_CHECK_INTERVAL);

    pthread_mutex_lock(&g_AccessDBLock);
    if( !CDatabase::QueryResult(query, &res_ptr))
    {
        pthread_mutex_unlock(&g_AccessDBLock);
        return;
    }
    pthread_mutex_unlock(&g_AccessDBLock);

    while( (sqlrow = mysql_fetch_row(res_ptr)))
    {
        sscanf(sqlrow[0], "%d", &id);
        sscanf(sqlrow[1], "%d", &fail_cnt);
        sscanf(sqlrow[2], "%d", &alert_cnt);
        sprintf(hostname,"%s",sqlrow[3]);
        sprintf(uuid,"%s",sqlrow[4]);
        sprintf(external_ip,"%s",sqlrow[5]);
        sprintf(availability_zone,"%s",sqlrow[6]);

        if(fail_cnt < FAIL_COUNT_LIMIT)
        {
            pthread_mutex_lock(&g_AccessDBLock);
            sprintf(query_sub, "UPDATE ne_server SET failcnt=%d WHERE id=%d", ++fail_cnt, id);
            CDatabase::ExecuteSQL(query_sub);
            pthread_mutex_unlock(&g_AccessDBLock);
            CMiscUtil::WriteLog("Increase ne_server fail count limit", -1, 0);
        }
        else{
            if(alert_cnt < ALERT_COUNT)
            {
                pthread_mutex_lock(&g_AccessDBLock);
                sprintf(query_sub, "UPDATE ne_server SET failcnt=0,alertcnt=%d,alerted_at=NOW(),alerted_reason='Client report failed. exceed failure count limit' WHERE id=%d", ++alert_cnt, id);
                CDatabase::ExecuteSQL(query_sub);
                pthread_mutex_unlock(&g_AccessDBLock);
                CMiscUtil::WriteLog("Increase ne_server alarm count", -1, 0);

                char message[1024];
                sprintf(message, "\"Hostname : %s\nUUID : %s\nExternal_IP : %s\nAvailability_zone : %s\"", hostname, uuid, external_ip, availability_zone);
                send_alert(message);
            }
            else{
                pthread_mutex_lock(&g_AccessDBLock);
                sprintf(query_sub, "UPDATE ne_server SET deleted=1,deleted_at=NOW() WHERE id=%d", id);
                CDatabase::ExecuteSQL(query_sub);
                pthread_mutex_unlock(&g_AccessDBLock);
                CMiscUtil::WriteLog("Client : %s , %s is exceed alert count's. set deleted", -1, 4, hostname, uuid);
            }
        }
    }

}

void send_alert(const char* body)
{
    char command[1024];

    sprintf(command,"python /opt/inno_srv2/send_alert.py %s", body);
    system(command);
}


void *report_perf_thread(void *arg)
{
    while (1)
    {
        sleep(60);   
        /* CPerfInfo::InsertDB(); */
    }
    
    mysql_thread_end();
    pthread_exit(NULL);
}


