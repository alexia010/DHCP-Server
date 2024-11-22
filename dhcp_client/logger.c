#include "logger.h"

#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

static int log_fd=-1;

const char *get_type(log_level lvl)
{
    switch (lvl)
    {
    case INFO:
        return "INFO";
        break;
    case WARNING:
        return "WARNING";
        break;
    case ERROR:
        return "ERROR";
        break;
    }
}

void log_init(const char* filename)
{
    log_fd=open(filename,O_WRONLY|O_CREAT|O_APPEND,0644);

    if(log_fd==-1)
    {
        perror("Error opening log file");
    }
}

void log_msg(log_level lvl,const char*function_name,const char*msg)
{
    if(log_fd==-1)
    {
        return;
    }

    time_t current_time=time(NULL);
    struct tm *tm_info=localtime(&current_time);

    char buffer[20];        //"YYYY-MM-DD HH:MM:SS"
    strftime(buffer,sizeof(buffer),"%Y-%m-%d %H:%M:%S", tm_info);

    char log_msg[512];
    snprintf(log_msg,sizeof(log_msg),"[%s] [%s] (%s) %s\n",buffer,get_type(lvl),function_name,msg);

    write(log_fd,log_msg,strlen(log_msg));

}

void close_logger()
{
    if(log_fd!=-1)
    {
        close(log_fd);
        log_fd=-1;
    }
}