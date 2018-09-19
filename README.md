# DynPolAC
## Dynamic Policy-based Access Control for IoT Systems
IoT systems are comprised of many nodes with live data communication among them. Some of their environments are also dynamic and moving such that their nodes require information handshake in <1s intervals. DynaPolAC is an authorization service for data passage between the nodes of dynamic IoT systems.

## Prerequisites
DynPolAC is tested in QNX platform with a custom-made databse. To connect to and run DynPolAC, you'd need to use a posix-based database with message passing IPC quality. DynPolAC needs to be instantiated in the database code and connected to your server.
If you need guidance about how to instantiate DynPolAC, please contact the author at mkarimib at ece dot ubc dot ca.

## Direcotry Structure:
1. **dynPolAC**: Implementation of the policy handling (registration, check and update) with the database.
2. **parsePolicy**: Application for parsing the xml and xacml policy files. The policy files must be parsed at the bootup time or start of the test and be registered with your database. In our case we have a posix compliant key-value database that we register the policy files in it.
```bash
  usage:
        defdp
            [-f <file>] <xml datapoint file>
            [-p <xml policy>] <xml policy file>
            [-P <xacml policy>] <xacml policy file>

    The defdp command allows dynamic creation of
    data points and policy from XML files

    The XML file takes the following form:

    <?xml version="1.0" encoding="utf-8" ?>
    <defdp>
        <point>
            <name>John.Phone.Application</name>
            <type>str</type>
            <format>%s</format>
            <length>12</length>
            <value>xxxxxxxx</value>
            <meta>
                <tag>type:password</tag>
                <tag>location:vancouver</tag>
                <tag>user:jackie</tag>
                <tag>group:technician</tag>
            </meta>
        </point>

        ...

    </defdp>


    The XML ``policy'' file might look something like this:
    <?xml version="1.0" encoding="utf-8"?>
    <policyFile>
        <policy>
            <rule min="140" max="1000">comparator</rule>
            <attributes>
                <type>current</type>
                <location>San Francisco</location>
                <time>2017-02-16T19:20:30</time>
                <user></user>
                <group></group>
            </attributes>
        </policy>

        ...

    </policyFile>
```

3. **discreteEventSimulator**: this directory has the runner for testing DynPolAC.

```bash
    This application will generate random Objects at psuedo-normal distribution
    rate, then queue the up linearly. The queue is M/M/1 which is the most
    commonly used type of queue for single-processor systems.
    The mean service time is calculated hence the service rate (miu)
    based on the calculations traffic intensity lambda/miu can be captured.
    Additionally the probability of n jobs in the system can also be captured.

    usage:
        discreteEventSimulator
              [-m <mean value for random number generator>]
              [-s <sigma is the standard deviation>]
              [-l <lambda is the mean arrival rate>]
              [-E <Number of Epochs>]
              [-o <show output data streams>]
              Sensitivity options:
              [-p <path to save the SteadyStatePerformance file>]
              [-f <sensitivity analysis: fix lambda factor (arrival rate)
                  ie. number of drones at some fix number>]
              [-n <sensitivity analysis: policy file number code>]
                  - THE POLICY FILES ARE --HANDMADE-- AND ARE IN
                    MULTIPLES OF 8 RULES.
              [-q <sensitivity analysis: query size number code>]
                  1 means query size 200B   only
                  2 means query size 500B   only
                  .. increments of 500B ..
                  11 means query size 5KB   only

    Examples:
    discreteEventSimulator -l 0.5 -m 3 -s 2 -E 10000 -p /ubc/Mehdi/steadyState.csv
    discreteEventSimulator -l 0.5 -E 1500 -f 7 -p /ubc/Mehdi/rate7.csv
    discreteEventSimulator -l 0.5 -m 3 -s 2 -E 5000 -n 3 -p /ubc/Mehdi/rule3.csv
    discreteEventSimulator -l 0.5 -m 3 -s 2 -E 5000 -q 1 -p /ubc/Mehdi/queue1.csv
```

4. **samplePolicy**: Some previously used policy files are provided.
5. **sampleDB**: A sample data base is provided. The database is parsed one time by your database application. Once data are ready to be queried, you can use posix message passing mechanism to query the database.
6. **scripts**: Scripts written to automate simulation. Take these scripts as a guideline. You have to come up with your scripts in your system depending on how you run the database. Everytime, to get correct results, you have to kill your database and create a new one.
7. **FormalVerification**: contains scripts to parse the XML policy files and reports if there is any conflict --- Z3 solver is used to see if the constraints are satisfiable.

## Paper
_Link to be added_
DynPolAC: Dynamic Policy-based Access Control for IoT Systems
