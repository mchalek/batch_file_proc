#ifndef BATCH_PROCESSOR_H
#define BATCH_PROCESSOR_H

#include <list>
#include <string>
#include <vector>
#include <iostream>

#include <unistd.h>
#include <fstream>
#include <pthread.h>

#ifdef __DO_TIMING__
#include <tictoc.h>
#endif

#include <stdint.h>

#define DEFAULT_MAX_THREADS 8
#define DEFAULT_MAX_QUEUE_SIZE 256
#define DEFAULT_BUNDLE_SIZE 2500

using namespace std;

namespace batch_file_processor {

static class bad_file_ex: public exception
{
    virtual const char* what() const throw()
    {
        return "Bad filename!";
    }
} bad_filename;

// filter type is pure virtual. should be inherited by whatever
// type is used in the batch template to process string
// lines into a form usable by the digests you are using
// It only exists to allow the definition of an insert type in
// the digest class that accepts a processed version of the string
// lines
class filter {
    public:
        void assign(const std::string &x)
        {
            std::cerr << "Failed polymorphism!" << __func__ << "::" << __FILE__ << "::" << __LINE__ << std::endl;
        }

        template <class T> friend class batch;
};

// must define 3 functions for each digest: copy, add, merge
class digest {
    private:
        virtual void merge(const void *x)=0;
    public:
        virtual digest * clone()
        {
            std::cerr << "Failed polymorphism!" << __func__ << "::" << __FILE__ << "::" << __LINE__ << std::endl;
            return NULL;
        }
        virtual void insert(const std::string *x) 
        {
            std::cerr << "Failed polymorphism: " << __func__ <<"::" << __FILE__ << "::" << __LINE__ <<  std::endl;
        }
        virtual void insert(void *x)
        {
            std::cerr << "Failed polymorphism!" << __func__ <<"::" << __FILE__ << "::" << __LINE__ <<  std::endl;
        }

        template <class T> friend class batch;
};

typedef list<string> work_bundle_t;
typedef list<work_bundle_t *> work_queue_t;

typedef struct _thread_data_t{
    pthread_mutex_t *queue_mutex;
    work_queue_t *work_queue;

    vector<digest*> digest_list; // not a list of pointers
    bool quit;
} thread_data_t;

typedef struct {
    int64_t num_jobs;
    int64_t num_waits;
    double idle_time;
    double work_time;
    double run_time;
} thread_summary_stats_t;

template <class filt_type = std::string>
class batch {
    private:
        std::vector<digest *> digest_list;
        std::list<std::string> file_list;

        int max_threads;
        size_t max_queue_size;
        size_t bundle_size;
        int verbosity;
        pthread_mutex_t print_mutex;

        int default_max_threads()
        {
            // TODO: should determine based on environment, machine, or something else
            return DEFAULT_MAX_THREADS;
        }

        static void *thread_func(void *vdata)
        {
            thread_data_t *data = (thread_data_t *) vdata;

            thread_summary_stats_t *stats = new thread_summary_stats_t;
            stats->num_jobs = 0;
            stats->num_waits = 0;
            stats->idle_time = 0.0;
            stats->run_time = 0.0;

#ifdef __DO_TIMING__
            tictoc clock;

            clock.tic(); // outer timer for total run-time
#endif

            pthread_mutex_lock(data->queue_mutex);
            while(!data->quit || !data->work_queue->empty()) {
#ifdef __DO_TIMING__
                clock.tic();
#endif
                while(data->work_queue->empty() && !data->quit) {
                    pthread_mutex_unlock(data->queue_mutex);
                    stats->num_waits++;
                    usleep(1000);
                    pthread_mutex_lock(data->queue_mutex);
                }
                if(data->work_queue->empty())  // instructed to quit and no work remains
                    break;

#ifdef __DO_TIMING__
                stats->idle_time += clock.toc();
#endif
                stats->num_jobs++;

                work_bundle_t *my_bundle = data->work_queue->back();
                data->work_queue->pop_back();
                pthread_mutex_unlock(data->queue_mutex);

                for(work_bundle_t::iterator bun_it = my_bundle->begin();
                        bun_it != my_bundle->end(); bun_it++) {
                    for(size_t j = 0; j < data->digest_list.size(); j++) {
                            filt_type filt(*bun_it);
                            data->digest_list[j]->insert(& filt);
                    }
                }
                delete my_bundle;

                pthread_mutex_lock(data->queue_mutex);
            }
            pthread_mutex_unlock(data->queue_mutex);

#ifdef __DO_TIMING__
            stats->run_time = clock.toc();
#endif

            pthread_exit(stats);
            return NULL;
        }

    public:

        batch(int nfiles, char *files[])
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
            verbosity = 0;
            pthread_mutex_init(&print_mutex, NULL);
        }

        void run()
        {
            pthread_t *threads = new pthread_t[max_threads];
            thread_data_t *thread_data = new thread_data_t[max_threads];

            pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
            work_queue_t work_queue;

            tictoc clock;

            for(int i = 0; i < max_threads; i++) {
                thread_data[i].queue_mutex = &queue_mutex;
                thread_data[i].work_queue = &work_queue;

                for(vector<digest *>::iterator dig_it = digest_list.begin();
                        dig_it != digest_list.end();
                        dig_it++) {
                    thread_data[i].digest_list.push_back((*dig_it)->clone()); // "deep" copy
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
                if(verbosity) {
                    pthread_mutex_lock(&print_mutex);
                    cerr << "Working on file " << *file_it << "...";
                    pthread_mutex_unlock(&print_mutex);
                }
                ifstream f(file_it->c_str());

                int64_t num_waits = 0;
                size_t queue_length_sum = 0;
                int64_t queue_length_counts = 0;
                int64_t num_lines = 0;
                
#ifdef __DO_TIMING__
                clock.tic();
#endif

                while(getline(f, line)) {
                    num_lines++;
                    current_bundle->push_back(line);

                    if(current_bundle->size() == bundle_size) {
                        pthread_mutex_lock(&queue_mutex);
                        work_queue.push_back(current_bundle);

                        size_t queue_size = work_queue.size();
                        queue_length_sum += queue_size;
                        queue_length_counts++;

                        while(queue_size > max_queue_size) {
                            pthread_mutex_unlock(&queue_mutex);
                            num_waits++;
                            usleep(1000);
                            pthread_mutex_lock(&queue_mutex);
                            queue_size = work_queue.size();
                        }
                        pthread_mutex_unlock(&queue_mutex);

                        current_bundle = new work_bundle_t;
                    }
                }

#ifdef __DO_TIMING__
                double file_time = clock.toc();
#endif

                if(verbosity) {
                    pthread_mutex_lock(&print_mutex);
#ifdef __DO_TIMING__
                    cerr << "Done. wait cycles: " << num_waits
                        << "; mean queue length: " << ((double) queue_length_sum) / queue_length_counts
                        << "; lines pulled / sec: " << ((double) num_lines) / file_time << endl;
#else
                    cerr << "Done." << endl;
#endif
                    pthread_mutex_unlock(&print_mutex);
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
                thread_summary_stats_t *stats;
                pthread_join(threads[i], (void **) &stats);

#ifdef __DO_TIMING__
                if(verbosity) {
                    cerr << "Thread " << i << " summary:" << endl;
                    cerr << "\t" << stats->num_jobs << " jobs completed in " 
                        << stats->run_time << " seconds => " 
                        << stats->run_time / stats->num_jobs << " seconds per job" << endl;

                    if(stats->num_waits)
                        cerr << "\t" << stats->num_waits << " wait cycles for " 
                            << stats->idle_time << " seconds" << endl;

                    cerr << "\tCPU utilization: " 
                        << 100*(stats->run_time - stats->idle_time) / stats->run_time << "%" << endl;
                }
#endif

                delete stats;
                
                for(size_t j = 0; j < digest_list.size(); j++) {
                    if(verbosity) {
                        pthread_mutex_lock(&print_mutex);
                        cerr << "Merging thread " << i << "'s data..." << endl;
                        pthread_mutex_unlock(&print_mutex);
                    }
                    digest_list[j]->merge(thread_data[i].digest_list[j]);
                    delete thread_data[i].digest_list[j];
                }
            }

            delete [] thread_data;
            delete [] threads;
        }

        void add_digest(digest &dig)
        {
            digest_list.push_back(&dig);
        }

        void set_max_threads(int val)
        {
            max_threads = val;
        }

        void set_verbosity(int val)
        {
            verbosity = val;
        }
};

}


#endif
