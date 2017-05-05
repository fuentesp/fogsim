all: fogsim

CC = g++
RFLAGS = -O2 -std=c++11
CFLAGS = -c -g -Wno-sign-compare -std=c++11
DFLAGS = -g -Wall
ROUTING = routing.h flexibleRouting.h ofar.h par.h pb.h pbAny.h rlm.h ugal.h
ROUTING_FILES = $(addprefix routing/, $(ROUTING))
FLIT = flitModule.h pbFlit.h creditFlit.h caFlit.h
FLIT_FILES = $(addprefix flit/, $(FLIT))
TRAFFIC = steady.h burst.h all2all.h mix.h transient.h
TRAFFIC_FOLDERS = $(addprefix generator/, $(addprefix trafficPattern/, $(TRAFFIC)))
GENERATOR = event.h generatorModule.h trace.h traceGenerator.h burstGenerator.h graph500Generator.h
GENERATOR_FILES = $(addprefix generator/, $(GENERATOR)) $(TRAFFIC_FOLDERS)
SWITCH = switchModule.h ioqSwitchModule.h
ARBITER = arbiter.h cosArbiter.h lrsArbiter.h priorityLrsArbiter.h rrArbiter.h priorityRrArbiter.h ageArbiter.h priorityAgeArbiter.h inputArbiter.h outputArbiter.h
PORT = port.h bufferedPort.h inPort.h outPort.h bufferedOutPort.h dynBufInPort.h dynBufOutPort.h dynBufBufferedOutPort.h
BUFFER = buffer.h
SWITCH_FOLDERS = $(SWITCH) $(ARBITER) $(BUFFER) $(PORT)
SWITCH_FILES = switch/*.h switch/*/*.h

HEADERS = dgflySimulator.h gModule.h configurationFile.h global.h pbState.h caHandler.h communicator.h generator/dimemas.h routing/min.h routing/minCond.h routing/val.h routing/valAny.h routing/olm.h $(FLIT_FILES) $(ROUTING_FILES) $(GENERATOR_FILES) $(SWITCH_FILES)
	
fogsim:
	$(CC) $(RFLAGS) dgflySimulator.cc gModule.cc configurationFile.cc global.cc pbState.cc caHandler.cc communicator.cc $(FLIT_FILES:.h=.cc) $(ROUTING_FILES:.h=.cc) $(SWITCH_FILES:.h=.cc) $(GENERATOR_FILES:.h=.cc) -o fogsim

dev: dgflySimulator.o gModule.o configurationFile.o global.o pbState.o caHandler.o communicator.o $(FLIT:.h=.o) $(ROUTING:.h=.o) $(GENERATOR:.h=.o) $(TRAFFIC:.h=.o) $(SWITCH_FOLDERS:.h=.o) 
	$(CC) $(DFLAGS) dgflySimulator.o gModule.o configurationFile.o global.o pbState.o caHandler.o communicator.o $(FLIT:.h=.o) $(ROUTING:.h=.o) $(GENERATOR:.h=.o) $(TRAFFIC:.h=.o) $(SWITCH_FOLDERS:.h=.o) -o fogsim

dgflySimulator.o: dgflySimulator.cc $(HEADERS)
	$(CC) $(CFLAGS) dgflySimulator.cc

gModule.o: gModule.cc $(HEADERS)
	$(CC) $(CFLAGS) gModule.cc
	
configurationFile.o: configurationFile.cc $(HEADERS)
	$(CC) $(CFLAGS) configurationFile.cc

global.o: global.cc $(HEADERS)
	$(CC) $(CFLAGS) global.cc
	
pbState.o: pbState.cc $(HEADERS)
	$(CC) $(CFLAGS) pbState.cc

caHandler.o: caHandler.cc $(HEADERS)
	$(CC) $(CFLAGS) caHandler.cc
	
communicator.o: communicator.cc $(HEADERS)
	$(CC) $(CFLAGS) communicator.cc

$(ROUTING:.h=.o): %.o: routing/%.cc $(HEADERS)
	$(CC) $(CFLAGS) routing/$(@:.o=.cc)

$(FLIT:.h=.o): %.o: flit/%.cc $(HEADERS)
	$(CC) $(CFLAGS) flit/$(@:.o=.cc)

$(TRAFFIC:.h=.o): %.o: generator/trafficPattern/%.cc $(HEADERS)
	$(CC) $(CFLAGS) generator/trafficPattern/$(@:.o=.cc)

$(GENERATOR:.h=.o): %.o: generator/%.cc $(HEADERS)
	$(CC) $(CFLAGS) generator/$(@:.o=.cc)
		
$(SWITCH:.h=.o): %.o: switch/%.cc $(HEADERS)
	$(CC) $(CFLAGS) switch/$(@:.o=.cc)

$(ARBITER:.h=.o): %.o: switch/arbiter/%.cc $(HEADERS)
	$(CC) $(CFLAGS) switch/arbiter/$(@:.o=.cc)

$(PORT:.h=.o): %.o: switch/port/%.cc $(HEADERS)
	$(CC) $(CFLAGS) switch/port/$(@:.o=.cc)

$(BUFFER:.h=.o): %.o: switch/buffer/%.cc $(HEADERS)
	$(CC) $(CFLAGS) switch/buffer/$(@:.o=.cc)

clean:
	rm -f *.o fogsim
