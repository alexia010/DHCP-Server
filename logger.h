#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>
#include <string.h>

typedef enum 
{
    INFO,
    WARNING,
    ERROR
}log_level;

const char *get_type(log_level lvl);

void log_init(const char* filename);
void log_msg(log_level lvl,const char*function_name,const char*msg);
void close_logger();

#endif