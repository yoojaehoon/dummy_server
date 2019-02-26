#include "header.h"
#include "MiscUtil.h"

char CMiscUtil::m_strLogPath[256] = "";
pthread_mutex_t CMiscUtil::m_LogLock = PTHREAD_MUTEX_INITIALIZER;

void CMiscUtil::InitLog(const char *logpath)
{
    strcpy(m_strLogPath, logpath);
    
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;
    char newpath[256];
    
    if ((dp = opendir(m_strLogPath)) == NULL)
        return;
   
    time_t the_time;
    (void)time(&the_time);
      
    while ((entry = readdir(dp)) != NULL)
    {
        sprintf(newpath, "%s/%s", m_strLogPath, entry->d_name);
        
        if (lstat(newpath, &statbuf) == -1)
            return;

        if (S_ISREG(statbuf.st_mode))
        {
            if (strcmp(".", entry->d_name) == 0 ||
                strcmp("..", entry->d_name) == 0)
                continue;
            
            if (statbuf.st_mtime < the_time - 30 * 24 * 3600)
                unlink(newpath);
        }
    }
}

void CMiscUtil::ClearOldLog()
{
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;
    char newpath[256];
    
    if ((dp = opendir(m_strLogPath)) == NULL)
        return;
   
    time_t the_time;
    (void)time(&the_time);
      
    while ((entry = readdir(dp)) != NULL)
    {
        sprintf(newpath, "%s/%s", m_strLogPath, entry->d_name);
        
        if (lstat(newpath, &statbuf) == -1)
            return;

        if (S_ISREG(statbuf.st_mode))
        {
            if (strcmp(".", entry->d_name) == 0 ||
                strcmp("..", entry->d_name) == 0)
                continue;
            
            if (statbuf.st_mtime < the_time - 1 * 24 * 3600)
            //if (statbuf.st_mtime < the_time - 7 * 24 * 3600)
                unlink(newpath);
        }
    }
        
    closedir(dp);       
}

void CMiscUtil::WriteLog(const char *message, int client_sockfd, int logtype1, const void *param1, const void *param2)
{
    FILE *file;
    struct tm tm_ptr;
    time_t the_time;
    char logpath[256];

//  pthread_mutex_lock(&m_LogLock);

    (void)time(&the_time);
    localtime_r(&the_time, &tm_ptr);

    sprintf(logpath, "%sINFO_%04d-%02d-%02d_%02d", m_strLogPath, tm_ptr.tm_year+1900, tm_ptr.tm_mon+1, tm_ptr.tm_mday, tm_ptr.tm_hour);

    file = (FILE *)fopen(logpath, "a+");

    if (file == NULL)
    {
//      pthread_mutex_unlock(&m_LogLock);
        return;
    }

    struct timeval tv = {0, 0};
    gettimeofday(&tv, 0);
    
    fprintf(file, "[%02d:%02d:%02d %06d] %s", tm_ptr.tm_hour, tm_ptr.tm_min, tm_ptr.tm_sec, (int)tv.tv_usec, message);
    
    if (client_sockfd != -1)
        fprintf(file, ", SOCKET - %d", client_sockfd);
        
    switch (logtype1)
    {
        case 0:
            fprintf(file, "\n");
            break;
            
        case 1:
            fprintf(file, ", P1 - %d\n", *(int *)param1);
            break;
            
        case 2:
            fprintf(file, ", P1 - %lld\n", *(long long *)param1);
            break;
            
        case 3:
            fprintf(file, ", P1 - %.300s\n", (const char *)param1);
            break;
            
        case 4:
            fprintf(file, ", P1 - %.300s, P2 - %.300s\n", (const char *)param1, (const char *)param2);
            break;
    }
                
    fclose(file);

//  pthread_mutex_unlock(&m_LogLock);   
}

void CMiscUtil::WriteErrorLog(const char *message, int client_sockfd, int logtype1, const void *param1, const void *param2)
{
    FILE *file;
    struct tm tm_ptr;
    time_t the_time;
    char logpath[256];

//  pthread_mutex_lock(&m_LogLock);

    (void)time(&the_time);
    localtime_r(&the_time, &tm_ptr);

    sprintf(logpath, "%sERROR_%04d-%02d-%02d", m_strLogPath, tm_ptr.tm_year+1900, tm_ptr.tm_mon+1, tm_ptr.tm_mday);

    file = (FILE *)fopen(logpath, "a+");

    if (file == NULL)
    {
//      pthread_mutex_unlock(&m_LogLock);
        return;
    }
    
    struct timeval tv = {0, 0};
    gettimeofday(&tv, 0);

    fprintf(file, "[%02d:%02d:%02d %06d] %s", tm_ptr.tm_hour, tm_ptr.tm_min, tm_ptr.tm_sec, (int)tv.tv_usec, message);
    
    if (client_sockfd != -1)
        fprintf(file, ", SOCKET - %d", client_sockfd);
    
    switch (logtype1)
    {
        case 0:
            fprintf(file, "\n");
            break;
            
        case 1:
            fprintf(file, ", P1 - %d\n", *(int *)param1);
            break;
            
        case 2:
            fprintf(file, ", P1 - %lld\n", *(long long *)param1);
            break;
            
        case 3:
            fprintf(file, ", P1 - %.300s\n", (const char *)param1);
            break;
            
        case 4:
            fprintf(file, ", P1 - %.300s, P2 - %.300s\n", (const char *)param1, (const char *)param2);
            break;
    }
                
    fclose(file);

//  pthread_mutex_unlock(&m_LogLock);   
}

int CMiscUtil::SendFileBuffer(int client_sockfd, const char *buffer, int length)
{
    int totalsend = 0;
    int nsend;
    
    while (length > totalsend)
    {
        if (length - totalsend > PACKET_SIZE)
        {
            nsend = write(client_sockfd, buffer + totalsend, PACKET_SIZE);
            
            if (nsend < 1)
            {
                WriteErrorLog("SendFileBuffer()", client_sockfd, 0);
                return FALSE;
            }
        }
        else
        {
            nsend = write(client_sockfd, buffer + totalsend, length - totalsend);
            
            if (nsend < 1)
            {
                WriteErrorLog("SendFileBuffer()", client_sockfd, 0);
                return FALSE;
            }
        }
        
        totalsend += nsend;
    }   

//  WriteLog("SendFileBuffer()", client_sockfd, 1, &length);    
        
    return TRUE;
}


void CMiscUtil::ClearDirectory(const char *path)
{
/*    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;
    char newpath[500];
    
    if ((dp = opendir(path)) == NULL)
        return;
        
    while ((entry = readdir(dp)) != NULL)
    {
        sprintf(newpath, "%s/%s", path, entry->d_name);
        
        if (lstat(newpath, &statbuf) == -1)
            return;

        if (S_ISREG(statbuf.st_mode))
        {
            if (strcmp(".", entry->d_name) == 0 ||
                strcmp("..", entry->d_name) == 0)
                continue;
            
            unlink(newpath);
        }
        
        if (S_ISDIR(statbuf.st_mode))
        {
            if (strcmp(".", entry->d_name) == 0 ||
                strcmp("..", entry->d_name) == 0)
                continue;

            ClearDirectory(newpath);
            rmdir(newpath);
        }
    }
    
    closedir(dp);
*/
}

void CMiscUtil::GetDiskSize(const char *path, long long &disksize, long long &disksize_free)
{
    struct statfs status_fs;
    
    disksize = 0;
    disksize_free = 0;
        
    if (statfs(path, (struct statfs *)(&status_fs)) == 0)
    {
        disksize = status_fs.f_blocks * status_fs.f_bsize;
        disksize_free = status_fs.f_bfree * status_fs.f_bsize;
    }
        
    return;
}

int CMiscUtil::MoveFile(const char *sourcepath, const char *targetpath)
{
    if (rename(sourcepath, targetpath) == -1)
    {
        char buffer[FILE_BUFFER_SIZE];
        FILE *file1, *file2;
        int nread = 1;
        
        file1 = (FILE *)fopen64(sourcepath, "r");
        
        if (file1 == NULL)
        {
            CMiscUtil::WriteErrorLog("fopen64()", -1, 3, sourcepath);
            
            return FALSE;
        }
    
        file2 = (FILE *)fopen64(targetpath, "w");
    
        if (file2 == NULL)
        {
            CMiscUtil::WriteErrorLog("fopen64()", -1, 3, targetpath);
            
            fclose(file1);
            return FALSE;
        }
        
        while (nread > 0)
        {
            nread = (int)fread((void *)buffer, sizeof(char), sizeof(buffer), file1);
            
            if (nread > 0)
            {
                if (fwrite(buffer, nread, 1, file2) != 1)
                {
                    CMiscUtil::WriteErrorLog("fwrite()", -1, 3, targetpath);
                    
                    fclose(file2);
                    fclose(file1);
                    
                    unlink(targetpath);
    
                    return FALSE;
                }
            }
        }
        
        fclose(file2);
        fclose(file1);  
    
        unlink(sourcepath);
    }
    
    return TRUE;
}

