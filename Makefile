LDLIBS=-lpthread

BIN=test_batch_processor

test_batch_processor: batch_processor.o

batch_processor.o: batch_processor.cpp batch_processor.h

clean: 
	rm -f *.o
	rm -f $(BIN)
