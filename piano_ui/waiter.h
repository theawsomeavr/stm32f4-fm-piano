#ifndef WAITER_H
#define WAITER_H

#ifdef _WIN32
#include <windows.h>

class Waiter {
public:
    void init(){
        timeBeginPeriod(1);
        _mutex = CreateMutex(nullptr, false, nullptr);
        _event = CreateEventA(nullptr, false, false, nullptr);
    }

    void end(){
        timeEndPeriod(1);
        CloseHandle(_mutex);
        CloseHandle(_event);
    }

    void lock(){ WaitForSingleObject(_mutex, INFINITE); }
    void unlock(){ ReleaseMutex(_mutex); }

    void wait_for(uint32_t millis){ WaitForSingleObject(_event, millis); }
    void wait(){ WaitForSingleObject(_event, INFINITE); }
    void cancel(){ SetEvent(_event); }
private:
    HANDLE _mutex;
    HANDLE _event;
};
#elif __unix__
#include <pthread.h>

class Waiter {
public:
    void init(){
        pthread_condattr_t attr;
        pthread_condattr_init( &attr);
        pthread_condattr_setclock( &attr, CLOCK_MONOTONIC);
        pthread_mutex_init(&_mutex, NULL);
        pthread_mutex_init(&_event_mutex, NULL);
        pthread_cond_init(&_event, &attr);
    }

    void end(){
        pthread_mutex_destroy(&_mutex);
        pthread_mutex_destroy(&_event_mutex);
        pthread_cond_destroy(&_event);
    }

    void lock(){ pthread_mutex_lock(&_mutex); }
    void unlock(){ pthread_mutex_unlock(&_mutex); }

    void wait_for(uint64_t millis){
        timespec ev_time;
        clock_gettime(CLOCK_MONOTONIC, &ev_time);
        uint64_t new_nsec = 1000000000L * ev_time.tv_sec + ev_time.tv_nsec + 1000000L * millis;
        ev_time.tv_sec = new_nsec / 1000000000L;
        ev_time.tv_nsec = new_nsec % 1000000000L;

        pthread_mutex_lock(&_event_mutex);
        pthread_cond_timedwait(&_event, &_event_mutex, &ev_time);
        pthread_mutex_unlock(&_event_mutex);
    }
    void wait(){ 
        pthread_mutex_lock(&_event_mutex);
        pthread_cond_wait(&_event, &_event_mutex);
        pthread_mutex_unlock(&_event_mutex);
    }
    void cancel(){
        pthread_mutex_lock(&_event_mutex);
        pthread_cond_signal(&_event);
        pthread_mutex_unlock(&_event_mutex);
    }
private:
    pthread_mutex_t _mutex, _event_mutex;
    pthread_cond_t _event;
};
#endif

#endif
