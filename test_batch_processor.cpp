#include "batch_file_processor.hpp"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <stdint.h>

using namespace std;

class histogram : public batch_file_processor::digest
{
    private:
        int *counts;
        int min;
        int max;
        int nbins;
        int delta;

        // make void merge private to ensure type correctness
        void merge(const void * vx)
        {
            const histogram *x = (const histogram *) vx;
            for(int i = 0; i < nbins; i++)
                counts[i] += x->counts[i];
        }

    public:
        histogram(int mn, int mx, int nb) {
            min = mn;
            max = mx;
            nbins = nb;
            delta = (max-min)/nbins;

            counts = new int[nbins];
            memset(counts, 0, nbins*sizeof(int));
        }
        void insert(const std::string * x)
        {
            int64_t val = strtol(x->c_str(), NULL, 10);
            int64_t bin = (val-min)/delta;
            if(bin < 0 || bin > nbins) {
                return;
            }
            counts[bin]++;
        }
        
        histogram * clone()
        {
            histogram *x = new histogram(min, max, nbins);

            memcpy(x->counts, counts, nbins*sizeof(int));

            return x;
        }

        void print() {
            for(int i = 0; i < nbins; i++) {
                cout << "[" << min+i*delta << "]: " << counts[i] << endl;
            }
        }

        ~histogram()
        {
            delete [] counts;
        }
};

int main(int argc, char *argv[])
{
    if(argc < 2) {
        cerr << "must provide filename" << endl;
        exit(0);
    }

    histogram h(0,10,10);
    batch_file_processor::batch<> bp(argc-1, argv+1);
    bp.add_digest(h);
    bp.run();

    h.print();
}
