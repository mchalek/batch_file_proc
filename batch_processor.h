#ifndef BATCH_PROCESSOR_H
#define BATCH_PROCESSOR_H

#include <list>
#include <string>
#include <vector>
#include <iostream>

// must define 3 functions for each digest: copy, add, merge
class digest {
    public:
        virtual digest & operator+=(const std::string &x){std::cout << "got em\n"; return *this;}
        virtual digest & operator+=(const digest &x){return *this;}
        virtual digest & operator=(const digest &x){return *this;}
};

class batch_processor {
    private:
        std::vector<digest *> digest_list;
        std::list<std::string> file_list;

        int max_threads;
        size_t max_queue_size;
        size_t bundle_size;

        int default_max_threads();

    public:
        batch_processor(int nfiles, char *files[]);

        void run();

        void add_digest(digest & dig);

        void set_max_threads(int val);
};

#endif
