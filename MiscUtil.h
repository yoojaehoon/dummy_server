#ifndef _MISCUTIL_H
#define _MISCUTIL_H

class CMiscUtil
{
    public:
        // ·α×°ü¼östatic void InitLog(char *logpath);
        
        //static void WriteLog(char *message, int client_sockfd, int logtype1, const void *param1 = NULL, const void *param2 = NULL);
        //static void WriteErrorLog(char *message, int client_sockfd, int logtype1, const void *param1 = NULL, const void *param2 = NULL);

        static void InitLog(const char *logpath);
        static void ClearOldLog();
        static void WriteLog(const char *message, int client_sockfd, int logtype1, const void *param1 = NULL, const void *param2 = NULL);
        static void WriteErrorLog(const char *message, int client_sockfd, int logtype1, const void *param1 = NULL, const void *param2 = NULL);
        
        static int SendFileBuffer(int client_sockfd, const char *buffer, int length);
        
        // ÆÀ½ýºÅ °ü¼östatic void ClearDirectory(char *path);
        static void ClearDirectory(const char *path);
        static void GetDiskSize(const char *path, long long &disksize, long long &disksize_free);
        static int MoveFile(const char *sourcepath, const char *targetpath);
        
    private:
        // ·α×°ü¼östatic char m_strLogPath[256];
        static char m_strLogPath[256];
        static pthread_mutex_t m_LogLock;
};

#endif //_MISCUTIL_H

