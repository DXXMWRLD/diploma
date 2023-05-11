
#include <fstream>
#include <iostream>
#include <numeric>
#include <unistd.h>
#include <vector>

std::vector<size_t> get_cpu_times() {
  std::ifstream proc_stat("/proc/stat");
  proc_stat.ignore(5, ' '); // Skip the 'cpu' prefix.
  std::vector<size_t> times;
  for (size_t time; proc_stat >> time; times.push_back(time))
    ;
  return times;
}

bool get_cpu_times(size_t& idle_time, size_t& total_time) {
  const std::vector<size_t> cpu_times = get_cpu_times();
  if (cpu_times.size() < 4)
    return false;
  idle_time  = cpu_times[3];
  total_time = std::accumulate(cpu_times.begin(), cpu_times.end(), 0);
  return true;
}

float CPUCheck(size_t& previous_idle_time, size_t& previous_total_time) {
  size_t idle_time(0), total_time(0);
  get_cpu_times(idle_time, total_time);

  const float idle_time_delta  = idle_time - previous_idle_time;
  const float total_time_delta = total_time - previous_total_time;
  const float utilization      = 100.0 * (1.0 - idle_time_delta / total_time_delta);

  // std::cout << utilization << '%' << std::endl;

  previous_idle_time  = idle_time;
  previous_total_time = total_time;

  return utilization;
}


#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "sys/times.h"

static clock_t lastCPU, lastSysCPU, lastUserCPU;
static int numProcessors;

void init() {
  FILE* file;
  struct tms timeSample;
  char line[128];

  lastCPU     = times(&timeSample);
  lastSysCPU  = timeSample.tms_stime;
  lastUserCPU = timeSample.tms_utime;

  file          = fopen("/proc/cpuinfo", "r");
  numProcessors = 0;
  while (fgets(line, 128, file) != NULL) {
    if (strncmp(line, "processor", 9) == 0)
      numProcessors++;
  }
  fclose(file);
}

double getCurrentValue() {
  struct tms timeSample;
  clock_t now;
  double percent;

  now = times(&timeSample);
  if (now <= lastCPU || timeSample.tms_stime < lastSysCPU || timeSample.tms_utime < lastUserCPU) {
    // Overflow detection. Just skip this value.
    percent = -1.0;
  } else {
    percent = (timeSample.tms_stime - lastSysCPU) + (timeSample.tms_utime - lastUserCPU);
    percent /= (now - lastCPU);
    percent /= numProcessors;
    percent *= 100;
  }
  lastCPU     = now;
  lastSysCPU  = timeSample.tms_stime;
  lastUserCPU = timeSample.tms_utime;

  return percent;
}