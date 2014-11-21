all: fogsim

CC = g++
RFLAGS = -O2
CFLAGS = -c -g -Wall 
LFLAGS = -g -Wall
TFLAGS = -c -g
HEADERS = dgflySimulator.h switchModule.h flitModule.h gModule.h generatorModule.h buffer.h localArbiter.h globalArbiter.h configurationFile.h global.h creditFlit.h pbFlit.h pbState.h dimemas.h event.h trace.h communicator.h

fogsim: dgflySimulator.o switchModule.o flitModule.o gModule.o generatorModule.o buffer.o localArbiter.o globalArbiter.o configurationFile.o global.o creditFlit.o pbFlit.o pbState.o event.o trace.o communicator.o
	$(CC) $(LFLAGS) dgflySimulator.o flitModule.o generatorModule.o switchModule.o gModule.o buffer.o localArbiter.o globalArbiter.o configurationFile.o global.o creditFlit.o pbFlit.o pbState.o event.o trace.o communicator.o -o fogsim
	rm -f *.o

dgflySimulator.o: dgflySimulator.cc $(HEADERS)
	$(CC) $(CFLAGS) dgflySimulator.cc

switchModule.o:  switchModule.cc $(HEADERS)
	$(CC) $(CFLAGS) switchModule.cc

flitModule.o: flitModule.cc $(HEADERS)
	$(CC) $(CFLAGS) flitModule.cc

gModule.o: gModule.cc $(HEADERS)
	$(CC) $(CFLAGS) gModule.cc
	
generatorModule.o: generatorModule.cc  $(HEADERS)
	$(CC) $(CFLAGS) generatorModule.cc 
	
buffer.o: buffer.cc $(HEADERS)
	$(CC) $(CFLAGS) buffer.cc
	
localArbiter.o: localArbiter.cc $(HEADERS)
	$(CC) $(CFLAGS) localArbiter.cc
	
globalArbiter.o: globalArbiter.cc $(HEADERS)
	$(CC) $(CFLAGS) globalArbiter.cc
	
configurationFile.o: configurationFile.cc $(HEADERS)
	$(CC) $(CFLAGS) configurationFile.cc

global.o: global.cc $(HEADERS)
	$(CC) $(CFLAGS) global.cc
	
creditFlit.o: creditFlit.cc $(HEADERS)
	$(CC) $(CFLAGS) creditFlit.cc
	
pbFlit.o: pbFlit.cc $(HEADERS)
	$(CC) $(CFLAGS) pbFlit.cc
	
pbState.o: pbState.cc $(HEADERS)
	$(CC) $(CFLAGS) pbState.cc
	
trace.o: trace.cc $(HEADERS)
	$(CC) $(TFLAGS) trace.cc
	
event.o: event.cc $(HEADERS)
	$(CC) $(CFLAGS) event.cc	
	
communicator.o: communicator.cc $(HEADERS)
	$(CC) $(CFLAGS) communicator.cc
		
clean:
	rm -f *.o fogsim
