#ifndef BATCH_PROCESSOR_H
#define BATCH_PROCESSOR_H

#include <list>
#include <string>
#include <vector>
#include <iostream>

// must define 3 functions for each digest: copy, add, merge
class filter {
    private:
        virtual void merge(const void *x)
        {
            std::cerr << "Failed polymorphism!" << __func__ << std::cerr;
        }
    public:
        virtual filter *clone()
        {
            std::cerr << "Failed polymorphism!" << __func__ << std::cerr;
            return NULL;
        }
        virtual void insert(const std::string &x) 
        {
            std::cerr << "Failed polymorphism!" << __func__ << std::cerr;
        }

        friend class batch_processor;
};

class digest {
    private:
        virtual void merge(const void *x)=0;
    public:
        virtual digest * clone()
        {
            std::cerr << "Failed polymorphism!" << __func__ << std::cerr;
            return NULL;
        }
        virtual void insert(const std::string &x) 
        {
            std::cerr << "Failed polymorphism: " << __func__ << std::cerr;
        }
        virtual void insert(const filter *x)
        {
            std::cerr << "Failed polymorphism!" << __func__ << std::cerr;
        }

        friend class batch_processor;
};


template <class filt_type = std::string>
class batch_processor {
    private:
        std::vector<digest *> digest_list;
        filt_type filt;
        std::list<std::string> file_list;

        int max_threads;
        size_t max_queue_size;
        size_t bundle_size;

        int default_max_threads();

    public:
        batch_processor(int nfiles, char *files[]);

        void run();

        void add_digest(digest & dig);
        void add_filter(filter & fil);

        void set_max_threads(int val);
};

#endif
