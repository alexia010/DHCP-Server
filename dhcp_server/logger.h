#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>
#include <string.h>

#define INFO_FLE_BASE_NAME "log_info"
#define WARNING_FLE_BASE_NAME "log_warning"
#define ERROR_FLE_BASE_NAME "log_error"
#define LOG_STATES_FILE "log_states"

#define MAX_FILENAME_LENGTH 20
#define MAX_FILE_SIZE 1048576   // 1 MB


typedef enum 
{
    INFO,
    WARNING,
    ERROR
}log_level;


void log_init();
void log_msg(log_level lvl,const char*function_name,const char*msg);
void close_logger();

#endif