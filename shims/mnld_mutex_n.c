/*
 * LD_PRELOAD for mnld (init.m681.gps.rc). libmnl.so (Android N) destroys
 * one of its mutexes twice when the MNL thread ends; bionic since P
 * aborts on that ("FORTIFY: pthread_mutex_destroy called on a destroyed
 * mutex"). In bionic pthread_mutex_destroy() frees nothing, it only marks
 * the mutex destroyed, so a no-op keeps every later lock/init valid.
 */
#include <pthread.h>

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	(void)mutex;
	return 0;
}
