
We are creating the Service Controller (SC) for the Solar Director (SD) Ecosystem

The purpose of SC is to manage and monitor the sd agents

The agents are by default in the SOLARD_BINDIR directory.  Every agent should implement the -I (info) argument which details the role (inverter/battery/etc) roles are defined in the sd lib at ~/src/sd/lib/sd/common.h

This is a utility, not an agent and therefore should instiantiate an instance of client vs agent.

The core of this program should be in C and take advange of the C-Based libsd functions.

The logic of the program should in JS including the management of the agents, the monitoring (which should be done every read interval), and any other tasks that might change or be updated.

you should create a web based user interface which will show a dashboard on the current status of all agents, manage agents (stop/stop/enable/disable/etc). when adding agents in the ui the user should be able to specify a path for the agent. the default action in the ui when adding agents is to provide a list of agents in SOLARD_BINDIR which have not yet been added and provide a button/link to add via a path.

the executer should create multiple claude sub agents to write the code and create the ui

the ui must be started and all endpoints tested by tester and verifier

if in the UI the SOLARD_BINDIR env variable is not defined, it should Look for configfile in ~/.sd.conf, and /etc/sd.conf, /usr/local/etc/sd.conf.  look at ~/src/sd/lib/sd/common.c solard_common_init() for more info on how this file is processed.

UI should include backend api with the ability to add/remove/start/stop agents.  VERIFIER should make sure those functions work.

UI should also include a logs button to view the logfile of a specific agent
