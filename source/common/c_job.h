#pragma once

typedef struct job_system_t job_system_t;

typedef void (*jobs_proc_t)(void *data, unsigned thread_idx);

typedef struct job_t {
  jobs_proc_t proc;
  void *data;
} job_t;

job_system_t *C_JobSystemCreate(unsigned max_threads);
void C_JobSystemEnqueue(job_system_t* job_system, job_t job);
unsigned C_JobSystemAllDone(job_system_t* job_system);
void C_JobSystemDestroy(job_system_t* job_system);
