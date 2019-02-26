/////////////////////////////////////////////////////////////////
//
// Clean °ü¸® Thread
//
/////////////////////////////////////////////////////////////////

void ClearOldColumn()
{
    char query[1024];

    //sprintf(query,"DELETE FROM alive_status WHERE reported_at < DATE_SUB( NOW(), INTERVAL 1 DAY)");
    sprintf(query,"DELETE FROM alive_status WHERE reported_at < DATE_SUB( NOW(), INTERVAL 6 HOUR)");
    CDatabase::ExecuteSQL(query);

    sprintf(query,"DELETE FROM ne_server WHERE deleted = 1 or updated_at < DATE_SUB( NOW(), INTERVAL 6 HOUR)");
    CDatabase::ExecuteSQL(query);
}

void *clean_thread(void *arg)
{
    while (1)
    {
        CMiscUtil::ClearOldLog();
        ClearOldColumn();
        sleep(3600);
    }
    
    pthread_exit(NULL);
}

