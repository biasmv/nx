#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <math.h>

void usage(const char* prog_name)
{
  fprintf(stderr, "usage: %s [-hc][-n <times>] command\n", prog_name);
  exit(1);
}


typedef struct  {
  float real_time;;
  float user_time;
  float sys_time;
} CallStat, *CallStatP;

float timeval_to_sec(struct timeval* tv)
{
  return (float)tv->tv_sec+0.000001*(float)tv->tv_usec;
}

typedef enum {
  CSF_CSV,
  CSF_HUMAN,
} CallStatFormat;

float minf(float a, float b)
{
  return a<b ? a : b;
}
float maxf(float a, float b)
{
  return a>b ? a : b;
}

void callstat_print_sep(FILE* fd, CallStatFormat fmt)
{
  if (fmt==CSF_HUMAN){
    fprintf(fd, "[---------|----------|----------|----------]\n");
  }
}
void callstat_print_head(FILE* fd, CallStatFormat fmt)
{
  callstat_print_sep(fd, fmt);
  if (fmt==CSF_HUMAN) {
    fprintf(fd, "| name    | real     | user     | sys      |\n");    
  }
  if (fmt==CSF_CSV) {
    fprintf(fd, "name, real, user, sys\n");
  }
  callstat_print_sep(fd, fmt);
}

void callstat_print(FILE* fd, CallStatP call_stat, const char* name,
                    CallStatFormat fmt)
{
  if (fmt==CSF_CSV) {
    fprintf(fd, "%s, %.3f, %.3f, %.3f\n", name, call_stat->real_time, 
            call_stat->user_time, call_stat->sys_time);
    return;
  }
  if (fmt==CSF_HUMAN) {
    fprintf(fd, "%10s %10.3f %10.3f %10.3f\n", name, call_stat->real_time, 
            call_stat->user_time, call_stat->sys_time);
    return;
  }
}

/*
   global variables
 */

char* g_program_name=NULL;
int g_num_repeats=10;
int g_output_format=CSF_HUMAN;
FILE* g_output_stream=NULL;

int parse_options(int argc, char **argv)
{
  /* 
     if the program name is of the form <num>x, use <num> as the default number 
     of repeats
   */
  char* end_ptr;
  int x=strtol(argv[0], &end_ptr, 10);
  if (*end_ptr=='x' && *(end_ptr+1)=='\0') {
    g_num_repeats=x;
  }
  int opt=0;
  g_output_stream=stdout;
  while ((opt = getopt(argc, argv, "hcn:o:")) != -1) {
    switch(opt) {
      case 'n':
        g_num_repeats=strtol(optarg, &end_ptr, 10);
        if (*end_ptr!='\0') {
          fprintf(stderr, "invalid integer literal for -n parameter.\n");
          usage(g_program_name);
        }
        if (g_num_repeats<=0 || g_num_repeats>10000) {
          fprintf(stderr, "Repeats must be in the range [1-10000].\n");
          usage(g_program_name);
        }
        break;
      case 'h':
       g_output_format=CSF_HUMAN;
       break;
      case 'o':
        if (!(g_output_stream=fopen(optarg, "w+"))) {
          perror("Can't open file");
          usage(g_program_name);
        }
        break;
      case 'c':
        g_output_format=CSF_CSV;
        break;
      case '?':
      default:
        usage(g_program_name);
    }
  }
  if (argc-optind==0) {
    usage(g_program_name); 
  }
  return optind;
}

int main(int argc, char **argv)
{
  g_program_name=argv[0];
  
  int consumed_args=parse_options(argc, argv);
  /* 
     if we get here, all parameters have been parsed correctly. skip used 
     parameters. Everything that is left is the command we would like to 
     execute.
   */
  argc-=consumed_args;
  argv+=consumed_args;
  
  
  int pid=0, status=0;
  struct timeval before, after;
  struct rusage ru;
  CallStatP call_stats=(CallStatP)malloc(sizeof(CallStat)*g_num_repeats);
  callstat_print_head(g_output_stream, g_output_format);
  int i=0;
  for (; i<g_num_repeats; ++i) {
    gettimeofday(&before, NULL);
    switch(pid = vfork()) {
      case -1:
        perror(g_program_name);
        if (g_output_stream!=stdout) {
          fclose(g_output_stream);
        }
        exit(1);
      case 0:
        execvp(*argv, argv);
        perror(*argv);
        _exit((errno == ENOENT) ? 127 : 126);
    }

    while (wait3(&status, 0, &ru) != pid);
    
    gettimeofday(&after, NULL);
    if (!WIFEXITED(status))
      fprintf(stderr, "Command terminated abnormally.\n");
    
    timersub(&after, &before, &after);

    call_stats[i].real_time=timeval_to_sec(&after);
    call_stats[i].user_time=timeval_to_sec(&ru.ru_utime);
    call_stats[i].sys_time=timeval_to_sec(&ru.ru_stime);
    char num_buf[10];
    snprintf(num_buf, 10, "%d", i+1);
    callstat_print(g_output_stream, &call_stats[i], num_buf, g_output_format);
  }
  callstat_print_sep(g_output_stream, g_output_format);
  /* collect statistics */
  CallStat min=call_stats[0], max=call_stats[0], mean=call_stats[0];
  CallStat stddev;
  memset(&stddev, 0, sizeof(CallStat));
  for (i=1; i<g_num_repeats; ++i) {
    min.real_time=minf(min.real_time, call_stats[i].real_time);
    min.user_time=minf(min.user_time, call_stats[i].user_time);
    min.sys_time=minf(min.sys_time, call_stats[i].sys_time);
    
    max.real_time=maxf(max.real_time, call_stats[i].real_time);
    max.user_time=maxf(max.user_time, call_stats[i].user_time);
    max.sys_time=maxf(max.sys_time, call_stats[i].sys_time);
    
    mean.real_time+=call_stats[i].real_time;
    mean.user_time+=call_stats[i].user_time;
    mean.sys_time+=call_stats[i].sys_time;
  }
  
  mean.real_time/=g_num_repeats;
  mean.user_time/=g_num_repeats;
  mean.sys_time/=g_num_repeats;
  
  for (i=0; i<g_num_repeats; ++i) {
    float d1=call_stats[i].real_time-mean.real_time;
    stddev.real_time+=d1*d1;
    float d2=call_stats[i].user_time-mean.user_time;
    stddev.real_time+=d2*d2;
    float d3=call_stats[i].sys_time-mean.sys_time;
    stddev.sys_time+=d3*d3;
  }
  stddev.real_time=sqrt(stddev.real_time/g_num_repeats);
  stddev.user_time=sqrt(stddev.user_time/g_num_repeats);
  stddev.sys_time=sqrt(stddev.sys_time/g_num_repeats);
  callstat_print(g_output_stream, &mean, "mean", g_output_format);
  callstat_print(g_output_stream, &min, "min", g_output_format);
  callstat_print(g_output_stream, &max, "max", g_output_format);
  callstat_print(g_output_stream, &stddev, "stddev", g_output_format);
  callstat_print_sep(g_output_stream, g_output_format);
  if (g_output_stream!=stdout) {
    fclose(g_output_stream);    
  }

  exit (WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE);
}