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


// inherited template class
template <class Dest>
class action {
    private:
        Dest *data;
    public:
        action() {
            data = new Dest;
        }
        virtual bool consume(std::list<std::string>::const_iterator start, 
                             std::list<std::string>::const_iterator stop);

        ~action() {
            delete data;
        }
};

template <class Dest>
class batch_processor {
    private:
        std::list< action<Dest> > ac_list;
        std::list<std::string> file_list;

        int max_threads;
        int max_queue_size;

        int default_max_threads();

        void *thread_func(void *vdata);
        
    public:
        batch_processor(int nfiles, char *files[]);

        void run();

        void insert(const action<Dest> & ac);

        void set_max_threads(int val);
};

#endif
