#include <common/c_job.h>
#include <stdio.h>
#include <stdlib.h>
#include <zpl/zpl.h>

ZPL_RING_DECLARE(extern, jobs_ring_, job_t);
ZPL_RING_DEFINE(jobs_ring_, job_t);

typedef struct thread_data_t {
  unsigned idx;
  job_system_t *sys;

  enum {
    THREAD_BUSY,
    THREAD_WAIT,
  } state;
} thread_data_t;

typedef struct job_system_t {
  zpl_semaphore job_count;
  zpl_atomic32 job_count_value;

  zpl_mutex job_mutex;

  jobs_ring_job_t jobs;
  zpl_thread threads[32];
  thread_data_t data[32];

  unsigned exiting;
  unsigned thread_count;
} job_system_t;

long C_JobEntryPoint(struct zpl_thread *thread) {
  thread_data_t *data = thread->user_data;
  unsigned thread_idx = data->idx;
  job_system_t *sys = data->sys;

  for (;;) {
    zpl_semaphore_wait(&sys->job_count);

    if (sys->exiting) {
      return 0;
    }

    zpl_mutex_lock(&sys->job_mutex);
    job_t *the_job = jobs_ring_get(&sys->jobs);
    zpl_mutex_unlock(&sys->job_mutex);

    data->state = THREAD_BUSY;
    the_job->proc(the_job->data, thread_idx);
    data->state = THREAD_WAIT;

    zpl_atomic32_fetch_add(&sys->job_count_value, -1);
  }

  return 0;
}

job_system_t *C_JobSystemCreate(unsigned max_threads) {
  if (max_threads > 32) {
    printf("[ERROR] A maximum of 32 threads per job system.\n");
    return NULL;
  }
  job_system_t *system = calloc(1, sizeof(job_system_t));

  zpl_atomic32_store(&system->job_count_value, 0);
  zpl_semaphore_init(&system->job_count);
  zpl_mutex_init(&system->job_mutex);
  jobs_ring_init(&system->jobs, zpl_heap_allocator(), 10000);

  system->thread_count = max_threads;

  for (unsigned i = 0; i < max_threads; i++) {
    zpl_thread_init(&system->threads[i]);

    system->data[i].sys = system;
    system->data[i].idx = i;

    zpl_thread_start(&system->threads[i], C_JobEntryPoint, &system->data[i]);
  }

  return system;
}

void C_JobSystemEnqueue(job_system_t *job_system, job_t job) {
  zpl_mutex_lock(&job_system->job_mutex);
  jobs_ring_append(&job_system->jobs, job);
  zpl_mutex_unlock(&job_system->job_mutex);
  zpl_atomic32_fetch_add(&job_system->job_count_value, 1);
  zpl_semaphore_post(&job_system->job_count, 1);
}

unsigned C_JobSystemQueueEmpty(job_system_t *job_system) {
  return zpl_atomic32_load(&job_system->job_count_value) == 0;
}

unsigned C_JobSystemAllDone(job_system_t *job_system) {
  for (unsigned i = 0; i < job_system->thread_count; i++) {
    if (job_system->data[i].state == THREAD_BUSY) {
      // printf("not all thread waiting\n");
      return false;
    }
  }

  unsigned queue_is_empty = C_JobSystemQueueEmpty(job_system);

  // printf("all thread waiting && queue_is_empty == %d\n", queue_is_empty);

  return queue_is_empty;
}

void C_JobSystemDestroy(job_system_t *job_system) {
  job_system->exiting = true;
  zpl_semaphore_post(&job_system->job_count, job_system->thread_count);

  for (unsigned i = 0; i < job_system->thread_count; i++) {
    zpl_thread_join(&job_system->threads[i]);

    zpl_thread_destroy(&job_system->threads[i]);
  }

  jobs_ring_free(&job_system->jobs);

  free(job_system);
}
