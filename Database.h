#ifndef _DATABASE_H
#define _DATABASE_H

#include <deque>

typedef std::deque<MYSQL *> conn_deque;

class CDatabase
{
    public:
        static int Init(char *host, char *user, char *password, char *database, int poolsize);
        static int ExecuteSQL(char *query, int nolog = 1);
        static int ExecuteSQL_GetInt(char *query, int &intvalue, int nolog=1);
        static int ExecuteSQL_GetLong(char *query, long long &longvalue, int nolog=1);
        static int QueryResult(char *query, MYSQL_RES **res_ptr, int nolog=1);
        static void Destroy();
        static void MakeDBField(char *buffer);

    protected:
        static int InitConnectionPool();
        static void DestroyConnectionPool();
        static int ReconnectConnection(MYSQL *conn);

        static MYSQL* GetConnection();
        static void PutConnection(MYSQL *conn);
        
    private:
        static char m_strHost[256];
        static char m_strUser[128];
        static char m_strPassword[128];
        static char m_strDatabase[128];
        static int  m_nPoolSize;
        
        static conn_deque m_listConnection;
        static pthread_mutex_t m_ConnectionLock;
        static sem_t m_ConnectionSemaphore;
};

#endif //_DATABASE_H


