## Local usage (single machine - AF_LOCAL sockets)
Compile a target defining LOCAL macro of the makefile

    $ make LOCAL=1 <target>
 
Then use the script `runExec_local.sh`. For example compiling the translator example (tanslator.cpp in the example folder) and executing with 2 workers:

    $ make LOCAL=1 examples/translator
    $ ./scripts/runExec_local.sh examples/translator 2

## Cluster usage (using AF_INET sockets)
Just compile with make file (the default compilation use AF_INET type of sockets):

    $ make <target>
   And execute through the script `runExec.sh`. For example the same translator example with two remote worker:

    $ make examples/translator
    $ ./scripts/runExec.sh examples/translator 2
Please note that the script is designed to work only on *openhpc2* cluster (IP Address: 131.114.142.232).
