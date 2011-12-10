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

int main(int argc, char ** argv)
{
  int repeats=10;
  char* end_ptr;
  int x=strtol(argv[0], &end_ptr, 10);
  if (*end_ptr=='x' && *(end_ptr+1)=='\0') {
    repeats=x;
  }
  int pid=0;

  int status=0;
  struct timeval before, after;
  struct rusage ru;
  int opt=0;
  CallStatFormat fmt=CSF_HUMAN;
  const char* program_name=argv[0];
  FILE* fd=stdout;
  while ((opt = getopt(argc, argv, "hcn:o:")) != -1) {
    switch(opt) {
    case 'n':
      repeats=strtol(optarg, &end_ptr, 10);
      if (*end_ptr!='\0') {
        fprintf(stderr, "invalid integer literal for -n parameter.\n");
        usage(program_name);
      }
      if (repeats<=0 || repeats>10000) {
        fprintf(stderr, "invalid value for -n parameter. Must be in the range [1-10000].\n");
        usage(program_name);
      }
      break;
    case 'h':
     fmt=CSF_HUMAN;
     break;
    case 'o':
      if (!(fd=fopen(optarg, "w+"))) {
        perror("Can't open file for writing");
        usage(program_name);
      }
      break;
    case 'c':
      fmt=CSF_CSV;
      break;
    case '?':
    default:
      usage(program_name);
    }
  }
  if (!(argc -= optind)) {
    usage(program_name); 
  }
  /* skip all processed parameters. */
  argv += optind;

  int repeat=0;
  CallStatP call_stats=(CallStatP)malloc(sizeof(CallStat)*repeats);
  callstat_print_head(fd, fmt);

  for (; repeat<repeats; ++repeat) {
    gettimeofday(&before, NULL);
    switch(pid = vfork()) {
      case -1:
        perror(program_name);
        if (fd!=stdout) {
          fclose(fd);
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

    call_stats[repeat].real_time=timeval_to_sec(&after);
    call_stats[repeat].user_time=timeval_to_sec(&ru.ru_utime);
    call_stats[repeat].sys_time=timeval_to_sec(&ru.ru_stime);
    char num_buf[10];
    snprintf(num_buf, 10, "%d", repeat+1);
    callstat_print(fd, &call_stats[repeat], num_buf, fmt);
  }
  callstat_print_sep(fd, fmt);
  /* collect statistics */
  CallStat min=call_stats[0], max=call_stats[0], mean=call_stats[0];
  CallStat stddev;
  memset(&stddev, 0, sizeof(CallStat));
  int i;
  for (i=1; i<repeats; ++i) {
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
  
  mean.real_time/=repeats;
  mean.user_time/=repeats;
  mean.sys_time/=repeats;
  
  for (i=0; i<repeats; ++i) {
    float d1=call_stats[i].real_time-mean.real_time;
    stddev.real_time+=d1*d1;
    float d2=call_stats[i].user_time-mean.user_time;
    stddev.real_time+=d2*d2;
    float d3=call_stats[i].sys_time-mean.sys_time;
    stddev.sys_time+=d3*d3;
  }
  stddev.real_time=sqrt(stddev.real_time/repeats);
  stddev.user_time=sqrt(stddev.user_time/repeats);
  stddev.sys_time=sqrt(stddev.sys_time/repeats);
  callstat_print(fd, &mean, "mean", fmt);
  callstat_print(fd, &min, "min", fmt);
  callstat_print(fd, &max, "max", fmt);
  callstat_print(fd, &stddev, "stddev", fmt);
  callstat_print_sep(fd, fmt);
  if (fd!=stdout) {
    fclose(fd);    
  }

  exit (WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE);
}