LDLIBS=-lpthread -lrt
CXXFLAGS+=-D__DO_TIMING__ -I/home/kevin/code/cxx/tictoc/

BIN=test_batch_processor

all:$(BIN)

#test_batch_processor: batch_processor.o

#batch_processor.o: batch_processor.cpp batch_processor.h

clean: 
	rm -f *.o
	rm -f $(BIN)
