#include "batch_processor.h"
#include <unistd.h>
#include <fstream>

#define DEFAULT_MAX_THREADS 8
#define DEFAULT_MAX_QUEUE_SIZE 256
#define DEFAULT_BUNDLE_SIZE 100000

using namespace std;

batch_processor::batch_processor(int nfiles, char *files[])
{
    for(int i = 0; i < nfiles; i++) {
        if(access(files[i], R_OK))
            throw(bad_filename);

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

void batch_processor::insert_action(const action &ac)
{
    ac_list.push_back(ac);
}

typedef list<string> work_bundle_t;
typedef list<work_bundle_t *> work_queue_t;

typedef struct {
    pthread_mutex_t *queue_mutex;
    work_queue_t *work_queue;

    list<action> *ac_list;
    bool quit;
} thread_data_t;

void batch_processor::run()
{
    pthread_t *threads = new pthread_t[max_threads];
    thread_data_t *thread_data = new thread_data_t[max_threads];

    pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
    work_queue_t work_queue;

    for(int i = 0; i < max_threads; i++) {
        thread_data[i].queue_mutex = &queue_mutex;
        thread_data[i].work_queue = &work_queue;

        thread_data[i].ac_list = &ac_list;
        thread_data[i].quit = false;
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
    }
    
    if(!current_bundle->empty()) {
        pthread_mutex_lock(&queue_mutex);
        work_queue.push_back(current_bundle);
        pthread_mutex_unlock(&queue_mutex);
    }
}

void *batch_processor::thread_func(void *vdata)
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

        for(list<action>::iterator ac_it = data->ac_list->begin();
                ac_it != data->ac_list->end();
                ac_it++) {
            ac_it->consume(my_bundle->begin(), my_bundle->end());
        }
        delete my_bundle;
        pthread_mutex_lock(data->queue_mutex);
    }
    pthread_mutex_unlock(data->queue_mutex);
}
