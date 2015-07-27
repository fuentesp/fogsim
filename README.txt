FOGSim, simulator for interconnection networks.
http://fuentesp.github.io/fogsim/
Copyright (C) 2015 University of Cantabria

To compile FOGSim, it requires GNU make (version 4.7.2 preferred).
Just typing 'make' in the shell prompt should do the trick.

This network simulator requires a configuration file with the
input parameters of the simulation. A sample file is provided
by the title of 'test_parameters.txt'. More info about the 
input parameters that can be used is given in the wiki in the
project page indicated above.

Execution must follow this procedure:

./fogsim A_Configuration_File

  - A_Configuration_File: as example, 'test_parameters.txt' is
	given.
 Any other parameters apart from the configuration file name
	can be provided throughout the configuration file
	specifications or through the command line at users
	election. For the command line, specifications must be
	carried like in the configuration file, specifying the
	name of the parameter plus the "=" symbol and the
	value. An example of typically command line introduced
	parameters is shown below:

./fogsim A_Configuration_File [OutputFileName=An_Output_Filename] [Probability=Injection_Probability] [Seed=Seed_Value]

 where parameters correspond to:
  - An_Output_Filename: provide the desired output filename. 
	FOGSim will output multiple files with different file
	extensions, each for a set of statistics.
  - Injection_Probability: probability of a packet injection
	from any switch at a given cycle. This value is scaled
	as a percentage of maximum injection bandwith, and
	ranges between 0 (no injection) and 100 (1 phit injected
	per cycle).
  - Seed_Value: an integer number to be supplied for execution
	randomization, useful when running multiple instances 
	of a given simulation.

