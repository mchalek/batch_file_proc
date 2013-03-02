#include "batch_processor.h"
#include <unistd.h>
#include <fstream>
#include <pthread.h>
#include <iostream>

#define DEFAULT_MAX_THREADS 8
#define DEFAULT_MAX_QUEUE_SIZE 256
#define DEFAULT_BUNDLE_SIZE 100000

using namespace std;

static class bad_file_ex: public exception
{
    virtual const char* what() const throw()
    {
        return "Bad filename!";
    }
} bad_filename;

batch_processor::batch_processor(int nfiles, char *files[])
{
    for(int i = 0; i < nfiles; i++) {
        if(access(files[i], F_OK) < 0) {
            throw(bad_filename);
        }

        string f(files[i]);
        file_list.push_back(f);
    }

    max_threads = default_max_threads();
    max_queue_size = DEFAULT_MAX_QUEUE_SIZE;
    bundle_size = DEFAULT_BUNDLE_SIZE;
}

int batch_processor::default_max_threads()
{
    // TODO: should determine based on environment, machine, or something else
    return DEFAULT_MAX_THREADS;
}

void batch_processor::set_max_threads(int val)
{
    max_threads = val;
}

void batch_processor::add_digest(digest &dig)
{
    digest_list.push_back(&dig);
}

typedef list<string> work_bundle_t;
typedef list<work_bundle_t *> work_queue_t;

typedef struct {
    pthread_mutex_t *queue_mutex;
    work_queue_t *work_queue;

    vector<digest> digest_list; // not a list of pointers
    bool quit;
} thread_data_t;

void *thread_func(void *vdata)
{
    thread_data_t *data = (thread_data_t *) vdata;

    pthread_mutex_lock(data->queue_mutex);
    while(!data->quit || !data->work_queue->empty()) {
        while(data->work_queue->empty() && !data->quit) {
            pthread_mutex_unlock(data->queue_mutex);
            usleep(1000);
            pthread_mutex_lock(data->queue_mutex);
        }
        if(data->work_queue->empty())  // instructed to quit and no work remains
            break;

        work_bundle_t *my_bundle = data->work_queue->back();
        data->work_queue->pop_back();
        pthread_mutex_unlock(data->queue_mutex);

        for(work_bundle_t::iterator bun_it = my_bundle->begin();
                bun_it != my_bundle->end(); bun_it++) {
            cout << "got string: " << *bun_it << endl;
            for(size_t j = 0; j < data->digest_list.size(); j++) {
                    cout << "adding to digest" << endl;
                    data->digest_list[j] += *bun_it;
            }
        }
        delete my_bundle;
        pthread_mutex_lock(data->queue_mutex);
    }
    pthread_mutex_unlock(data->queue_mutex);

    pthread_exit(NULL);
    return NULL;
}

void batch_processor::run()
{
    pthread_t *threads = new pthread_t[max_threads];
    thread_data_t *thread_data = new thread_data_t[max_threads];

    pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
    work_queue_t work_queue;

    for(int i = 0; i < max_threads; i++) {
        thread_data[i].queue_mutex = &queue_mutex;
        thread_data[i].work_queue = &work_queue;

        for(vector<digest *>::iterator dig_it = digest_list.begin();
                dig_it != digest_list.end();
                dig_it++) {
            thread_data[i].digest_list.push_back(*(*dig_it)); // "deep" copy
        }
        thread_data[i].quit = false;

        // launch thread
        pthread_create(threads + i, NULL, thread_func, thread_data + i);
    }

    work_bundle_t *current_bundle = new work_bundle_t;
    for(list<string>::iterator file_it = file_list.begin();
            file_it != file_list.end();
            file_it++) {
        string line;
        ifstream f(file_it->c_str());

        while(getline(f, line)) {
            current_bundle->push_back(line);

            if(current_bundle->size() == bundle_size) {
                pthread_mutex_lock(&queue_mutex);
                work_queue.push_back(current_bundle);
                size_t queue_size = work_queue.size();
                while(queue_size > max_queue_size) {
                    pthread_mutex_unlock(&queue_mutex);
                    usleep(1000);
                    pthread_mutex_lock(&queue_mutex);
                }
                pthread_mutex_unlock(&queue_mutex);

                current_bundle = new work_bundle_t;
            }
        }

        f.close();
    }
    
    if(!current_bundle->empty()) {
        pthread_mutex_lock(&queue_mutex);
        work_queue.push_back(current_bundle);
        pthread_mutex_unlock(&queue_mutex);
    }

    for(int i = 0; i < max_threads; i++)
        thread_data[i].quit = true;

    for(int i = 0; i < max_threads; i++) {
        pthread_join(threads[i], NULL);
        
        for(size_t j = 0; j < digest_list.size(); j++)
            (*digest_list[j]) += thread_data[i].digest_list[j];
    }
}

