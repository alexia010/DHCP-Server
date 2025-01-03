#include "logger.h"

#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>


static int log_fds[3]={-1,-1,-1};
static int log_counters[3]={1,1,1};
static char log_filename_bases[3][MAX_FILENAME_LENGTH];

const char *get_log_type(log_level lvl)
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

void open_new_log_file(log_level lvl)
{
    if(log_fds[lvl]!=-1)
    {
        close(log_fds[lvl]);
    }

    char log_file[MAX_FILENAME_LENGTH];

  snprintf(log_file,MAX_FILENAME_LENGTH,"%s%d%s",log_filename_bases[lvl],log_counters[lvl],".txt");

    log_fds[lvl]=open(log_file,O_WRONLY|O_CREAT|O_APPEND,0644);

    if(log_fds[lvl]==-1)
    {
        perror("Error opening log file");
    }
}

void load_log_states()
{
    int fd=open(LOG_STATES_FILE, O_RDONLY);

    if(fd==-1)
    {
        perror("Error opening state log file");
        
        log_counters[INFO]=1;
        log_counters[WARNING]=1;
        log_counters[ERROR]=1;
        return;
    }

    char buffer[256];
    ssize_t br=read(fd,buffer,sizeof(buffer)-1);

    if(br>0)
    {
        buffer[br]='\0';

        sscanf(buffer, "INFO %d\nWARNING %d\nERROR %d",
                &log_counters[INFO],&log_counters[WARNING],&log_counters[ERROR]);

    }

    close(fd);

}

void log_init()
{
    strncpy(log_filename_bases[INFO],INFO_FLE_BASE_NAME,strlen(INFO_FLE_BASE_NAME));
    strncpy(log_filename_bases[WARNING],WARNING_FLE_BASE_NAME,strlen(WARNING_FLE_BASE_NAME));
    strncpy(log_filename_bases[ERROR],ERROR_FLE_BASE_NAME,strlen(ERROR_FLE_BASE_NAME));

    load_log_states(LOG_STATES_FILE);

    open_new_log_file(INFO);
    open_new_log_file(WARNING);
    open_new_log_file(ERROR);
}   


void log_msg(log_level lvl,const char*function_name,const char*msg)
{
    if(log_fds[lvl]==-1)
    {
        return;
    }

    time_t current_time=time(NULL);
    struct tm *tm_info=localtime(&current_time);

    char timestamp[20];        //"YYYY-MM-DD HH:MM:SS"
    strftime(timestamp,sizeof(timestamp),"%Y-%m-%d %H:%M:%S", tm_info);

    char log_msg[512];
    ssize_t msg_length=snprintf(log_msg,sizeof(log_msg),"[%s] [%s] (%s) %s\n",timestamp,get_log_type(lvl),function_name,msg);

    off_t current_size=lseek(log_fds[lvl],0,SEEK_END);

        if (current_size + msg_length >= MAX_FILE_SIZE) {
        log_counters[lvl]++;
        open_new_log_file(lvl);
    }

    if(write(log_fds[lvl],log_msg,strlen(log_msg))==-1)
    {
        perror("Error writing to log file.");
    }

}

void save_log_state()
{
    int fd=open(LOG_STATES_FILE,O_WRONLY|O_CREAT|O_TRUNC,0644);

    if(fd==-1)
    {
        perror("Error opening log state file for writing");
        return;
    }

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "INFO %d\nWARNING %d\nERROR %d\n", 
             log_counters[INFO], log_counters[WARNING], log_counters[ERROR]);

    write(fd, buffer, strlen(buffer));

    close(fd);
}

void close_logger()
{
    save_log_state();

    for(int i=INFO;i<=ERROR;i++)
    {
        if(log_fds[i]!=-1)
        {
            close(log_fds[i]);
            log_fds[i]=-1;
        }
    }

}

