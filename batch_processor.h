#ifndef BATCH_PROCESSOR_H
#define BATCH_PROCESSOR_H

#include <list>
#include <string>
#include <exception>

class bad_file_ex: public std::exception
{
    virtual const char* what() const throw()
    {
        return "Bad filename!";
    }
} bad_filename;

class Digest {
    public:
        virtual Digest & operator+=(const std::string &x);
        virtual Digest & operator=(const Digest &x);
};

class action {
    private:
        Digest data;
    public:
        action(const Digest &x) : data(x) 
        {}

        bool consume(std::list<std::string>::const_iterator start, 
                     std::list<std::string>::const_iterator stop) {
            while(start != stop) {
                data += *start;
                start++;
            }
        }
};

class batch_processor {
    private:
        std::list<digest> digest_list;
        std::list<std::string> file_list;

        int max_threads;
        int max_queue_size;
        int bundle_size;

        int default_max_threads();

        void *thread_func(void *vdata);
        
    public:
        batch_processor(int nfiles, char *files[]);

        void run();

        void insert_action(const action & ac);

        void set_max_threads(int val);
};

#endif
