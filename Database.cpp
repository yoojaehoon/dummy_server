#include <mysql.h>

#include "header.h"
#include "Database.h"
//#include "PerfInfo.h"

char CDatabase::m_strHost[256] = "";
char CDatabase::m_strUser[128] = "";
char CDatabase::m_strPassword[128] = "";
char CDatabase::m_strDatabase[128] = "";
int CDatabase::m_nPoolSize = 0;

conn_deque CDatabase::m_listConnection;
pthread_mutex_t CDatabase::m_ConnectionLock;
sem_t CDatabase::m_ConnectionSemaphore;

int CDatabase::Init(char *host, char *user, char *password, char *database, int poolsize)
{
    strcpy(m_strHost, host);
    strcpy(m_strUser, user);
    strcpy(m_strPassword, password);
    strcpy(m_strDatabase, database);
    m_nPoolSize = poolsize;
    
    int result = sem_init(&m_ConnectionSemaphore, 0, m_nPoolSize);
    
    if (result)
    {
        CMiscUtil::WriteErrorLog("CDatabase::Init", -1, 3, "sem_init");

        return FALSE;
    }
    
    result = pthread_mutex_init(&m_ConnectionLock, NULL);
    
    if (result)
    {
        CMiscUtil::WriteErrorLog("CDatabase::Init", -1, 3, "pthread_mutex_init");

        return FALSE;
    }

    return InitConnectionPool();
}

void CDatabase::Destroy()
{
    DestroyConnectionPool();
    CMiscUtil::WriteLog("Destroy Mysql Pools", -1, 0);
    sem_destroy(&m_ConnectionSemaphore);
    pthread_mutex_destroy(&m_ConnectionLock);
}

int CDatabase::ExecuteSQL(char *query, int nolog)
{
    int result;
    struct timeval start_time, end_time;
    
    MYSQL *conn = GetConnection();

    if (conn == NULL)
        return FALSE;

    for (int i=0; i<3; i++)
    {
        gettimeofday(&start_time, NULL);
        result = mysql_query(conn, query);
        gettimeofday(&end_time, NULL);

        if( end_time.tv_usec < start_time.tv_usec )
        {
            end_time.tv_sec--;
            end_time.tv_usec += 1000000;
        }

        //long long interval = (end_time.tv_sec - start_time.tv_sec)*1000 + (end_time.tv_usec - start_time.tv_usec)/1000;
        //CPerfInfo::AddDBTime(interval);

        if (!result)
            break;
            
        CMiscUtil::WriteErrorLog("CDatabase::ExecuteSQL", -1, 3, query);
            
        usleep(1000);
    }

    PutConnection(conn);

    if (!nolog)
        CMiscUtil::WriteLog("CDatabase::ExecuteSQL", -1, 3, query);

    if (result)
        return FALSE;
    else
        return TRUE;    
}

int CDatabase::ExecuteSQL_GetInt(char *query, int &intvalue, int nolog)
{
    int result;
    struct timeval start_time, end_time;

    MYSQL_RES *res_ptr;
    MYSQL_ROW sqlrow;
    
    MYSQL *conn = GetConnection();

    if (conn == NULL)
        return FALSE;

    for (int i=0; i<3; i++)
    {
        gettimeofday(&start_time, NULL);
        result = mysql_query(conn, query);
        gettimeofday(&end_time, NULL);

        if( end_time.tv_usec < start_time.tv_usec )
        {
            end_time.tv_sec--;
            end_time.tv_usec += 1000000;
        }

        //long long interval = (end_time.tv_sec - start_time.tv_sec)*1000 + (end_time.tv_usec - start_time.tv_usec)/1000;
        //CPerfInfo::AddDBTime(interval);

        if (!result)
            break;
            
        CMiscUtil::WriteErrorLog("CDatabase::ExecuteSQL_GetInt", -1, 3, query);
            
        usleep(1000);
    }

    if (!nolog)
        CMiscUtil::WriteLog("CDatabase::ExecuteSQL_GetInt", -1, 3, query);

    sprintf(query, "SELECT LAST_INSERT_ID()");
    
    for (int i=0; i<3; i++)
    {
        gettimeofday(&start_time, NULL);
        result = mysql_query(conn, query);
        gettimeofday(&end_time, NULL);

        if( end_time.tv_usec < start_time.tv_usec )
        {
            end_time.tv_sec--;
            end_time.tv_usec += 1000000;
        }

        //long long interval = (end_time.tv_sec - start_time.tv_sec)*1000 + (end_time.tv_usec - start_time.tv_usec)/1000;
        //CPerfInfo::AddDBTime(interval);

        if (!result)
            break;
    
        CMiscUtil::WriteErrorLog("CDatabase::ExecuteSQL_GetInt", -1, 3, query);
            
        usleep(1000);
    }
    
    if (!result)
    {
        res_ptr = mysql_store_result(conn);

        if (!res_ptr)
            result = 1;
        else
        {
            if ((sqlrow = mysql_fetch_row(res_ptr)))
                sscanf(sqlrow[0], "%d", &intvalue);
            else
                result = 1;         
        }
        
        mysql_free_result(res_ptr);
    }
    
    PutConnection(conn);

    if (result)
        return FALSE;
    else
        return TRUE;    
}

int CDatabase::ExecuteSQL_GetLong(char *query, long long &longvalue, int nolog)
{
    int result;
    struct timeval start_time, end_time;

    MYSQL_RES *res_ptr;
    MYSQL_ROW sqlrow;
    
    MYSQL *conn = GetConnection();

    if (conn == NULL)
        return FALSE;

    for (int i=0; i<3; i++)
    {
        gettimeofday(&start_time, NULL);
        result = mysql_query(conn, query);
        gettimeofday(&end_time, NULL);

        if( end_time.tv_usec < start_time.tv_usec )
        {
            end_time.tv_sec--;
            end_time.tv_usec += 1000000;
        }

        //long long interval = (end_time.tv_sec - start_time.tv_sec)*1000 + (end_time.tv_usec - start_time.tv_usec)/1000;
        //CPerfInfo::AddDBTime(interval);

        if (!result)
            break;
            
        CMiscUtil::WriteErrorLog("CDatabase::ExecuteSQL_GetLong", -1, 3, query);
            
        usleep(1000);
    }

    if (!nolog)
        CMiscUtil::WriteLog("CDatabase::ExecuteSQL_GetLong", -1, 3, query);

    sprintf(query, "SELECT LAST_INSERT_ID()");
    
    for (int i=0; i<3; i++)
    {
        gettimeofday(&start_time, NULL);
        result = mysql_query(conn, query);
        gettimeofday(&end_time, NULL);

        if( end_time.tv_usec < start_time.tv_usec )
        {
            end_time.tv_sec--;
            end_time.tv_usec += 1000000;
        }

        //long long interval = (end_time.tv_sec - start_time.tv_sec)*1000 + (end_time.tv_usec - start_time.tv_usec)/1000;
        //CPerfInfo::AddDBTime(interval);

        if (!result)
            break;
    
        CMiscUtil::WriteErrorLog("CDatabase::ExecuteSQL_GetLong", -1, 3, query);
            
        usleep(1000);
    }
    
    if (!result)
    {
        res_ptr = mysql_store_result(conn);

        if (!res_ptr)
            result = 1;
        else
        {
            if ((sqlrow = mysql_fetch_row(res_ptr)))
                sscanf(sqlrow[0], "%lld", &longvalue);
            else
                result = 1;         
        }
        
        mysql_free_result(res_ptr);
    }
    
    PutConnection(conn);

    if (result)
        return FALSE;
    else
        return TRUE;    
}


int CDatabase::QueryResult(char *query, MYSQL_RES **res_ptr, int nolog)
{
    int result;
    struct timeval start_time, end_time;
    
    MYSQL *conn = GetConnection();

    if (conn == NULL)
        return FALSE;
        
    for (int i=0; i<3; i++)
    {
        gettimeofday(&start_time, NULL);
        result = mysql_query(conn, query);
        gettimeofday(&end_time, NULL);

        if( end_time.tv_usec < start_time.tv_usec )
        {
            end_time.tv_sec--;
            end_time.tv_usec += 1000000;
        }

        // long long interval = (end_time.tv_sec - start_time.tv_sec)*1000 + (end_time.tv_usec - start_time.tv_usec)/1000;
        // CPerfInfo::AddDBTime(interval);

        if (!result)
            break;
    
        CMiscUtil::WriteErrorLog("CDatabase::QueryResult", -1, 3, query);
            
        usleep(1000);
    }
    
    if (!result)
    {
        *res_ptr = mysql_store_result(conn);

        if (!(*res_ptr))
            result = 1;
    }

    PutConnection(conn);

    if (!nolog)
        CMiscUtil::WriteLog("CDatabase::QueryResult", -1, 3, query);
    
    if (result)
        return FALSE;
    else
        return TRUE;
}

int CDatabase::InitConnectionPool()
{
    for (int i=0; i<m_nPoolSize; i++)
    {
        MYSQL *conn = new MYSQL;
                
        if (!mysql_init(conn))
        {
            CMiscUtil::WriteErrorLog("CDatabase::InitConnectionPool", -1, 3, "mysql_init()");
            delete conn;
            
            return FALSE;
        }

        if (!mysql_real_connect(conn, m_strHost, m_strUser, m_strPassword, m_strDatabase, 3306, (char*)NULL, 0))
        {
            CMiscUtil::WriteErrorLog("CDatabase::InitConnectionPool", -1, 3, "mysql_real_connect()");
            delete conn;
            
            return FALSE;
        }

        mysql_query(conn, "SET collation_connection = utf8_general_ci");
        mysql_query(conn, "SET NAMES 'utf8'");

        m_listConnection.push_back(conn);
    }
    
    return TRUE;
}

void CDatabase::DestroyConnectionPool()
{
    pthread_mutex_lock(&m_ConnectionLock);
    
    for (conn_deque::iterator iter=m_listConnection.begin(); iter!=m_listConnection.end(); iter++)
    {
        MYSQL *conn = *iter;

        mysql_close(conn);
        delete conn;
    }
    
    m_listConnection.clear();
    
    pthread_mutex_unlock(&m_ConnectionLock);
}

int CDatabase::ReconnectConnection(MYSQL *conn)
{
    if (!mysql_real_connect(conn, m_strHost, m_strUser, m_strPassword, m_strDatabase, 3306, (char*)NULL, 0))
    {
        CMiscUtil::WriteErrorLog("CDatabase::ReconnectConnection", -1, 3, "mysql_real_connect()");
        return FALSE;
    }
    
    mysql_query(conn, "SET collation_connection = utf8_general_ci");
    mysql_query(conn, "SET NAMES 'utf8'");
    
    return true;
}

MYSQL* CDatabase::GetConnection()
{
    MYSQL *conn;

    sem_wait(&m_ConnectionSemaphore);
    pthread_mutex_lock(&m_ConnectionLock);
    
    if (m_listConnection.empty())
    {
        pthread_mutex_unlock(&m_ConnectionLock);
        sem_post(&m_ConnectionSemaphore);
        
        return NULL;
    }

    conn = m_listConnection.front();
    
    if (mysql_ping(conn))
    {
        if (ReconnectConnection(conn))
            m_listConnection.pop_front();
        else
        {
            pthread_mutex_unlock(&m_ConnectionLock);
            sem_post(&m_ConnectionSemaphore);
            
            return NULL;
        }
    }
    else
        m_listConnection.pop_front();

    pthread_mutex_unlock(&m_ConnectionLock);

    return conn;
}

void CDatabase::PutConnection(MYSQL *conn)
{
    pthread_mutex_lock(&m_ConnectionLock);

    m_listConnection.push_back(conn);
    
    pthread_mutex_unlock(&m_ConnectionLock);
    sem_post(&m_ConnectionSemaphore);
}

void CDatabase::MakeDBField(char *buffer)
{
    int length = strlen(buffer);
    int pos = 0;
    
    char *cbuffer = new char[length * 2 + 1];
    
    for (int i=0; i<length; i++)
    {
        if (buffer[i] == '\'')
        {
            cbuffer[pos++] = '\'';
            cbuffer[pos++] = '\'';
        }           
        else if (buffer[i] == '\\')
        {
            cbuffer[pos++] = '\\';
            cbuffer[pos++] = '\\';
        }           
        else
            cbuffer[pos++] = buffer[i];
    }
    
    cbuffer[pos] = 0;
    
    memcpy(buffer, cbuffer, pos+1);
    
    delete [] cbuffer;
    
    return;
}


