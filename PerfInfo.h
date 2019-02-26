#ifndef __PERFINFO_H__
#define __PERFINFO_H__

#include <vector>

class CPerfInfo
{
    public:
        static int Init(int serviceidx, char* strIP);
        static void Destroy();

        static void AddCount(int connection, int error, int etc1=0, int etc2=0);
        static void AddTransTime(double time);
        static void AddDBTime(double time);

        static void InsertDB();

    private:
        static int m_connectioncnt;
        static int m_errorcnt;
        static int m_etccnt1;
        static int m_etccnt2;

        static double m_transtime;
        static int m_transcount;
        static double m_dbtime;
        static int m_dbcount;

        static int m_serviceidx;
        static char m_localip[32];

        static pthread_mutex_t m_TransTimeLock;
        static pthread_mutex_t m_DBTimeLock;
        static pthread_mutex_t m_PerfoLock;
};

#endif //__PERFINFO_H__
