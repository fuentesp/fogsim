FOGSim, simulator for interconnection networks.
https://code.google.com/p/fogsim/
Copyright (C) 2014 University of Cantabria

To compile FOGSim, it requires GNU make (version ??? preferred).
Just typing 'make' in the shell prompt should do the trick.

This network simulator requires a configuration file with the
input parameters of the simulation. A sample file is provided
by the title of 'test_parameters.txt'. More info about the 
input parameters that can be used is given in the wiki in the
project page indicated above.

Execution must follow this procedure:

./fogsim A_Configuration_File An_Output_Filename Local_Queues_Size Global_Queues_Size Bubble Injection_Probability Seed 

  - A_Configuration_File: as example, 'test_parameters.txt' is
	given.
  - An_Output_Filename: provide the desired output filename. 
	FOGSim will output multiple files with different file
	extensions, each for a set of statistics.
  - Local_Queues_Size: size of the buffers employed for local
	transit queues. An example value of 32 is suggested.
  - Global_Queues_Size: size of the buffers employed for global
	transit queues. An example value of 256 is suggested.
  - Bubble: bubble size to be employed when needed. An example
	value of 1 is suggested.
  - Injection_Probability: probability of a packet injection
	from any switch at a given cycle. This value is scaled
	as a percentage of maximum injection bandwith, and
	ranges between 0 (no injection) and 100 (1 phit injected
	per cycle).
  - Seed: an integer number to be supplied for execution
	randomization, useful when running multiple instances 
	of a given simulation.

