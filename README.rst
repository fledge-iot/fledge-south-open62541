========================================================================
OPC UA C/C++ South plugin 
========================================================================

A simple asynchronous OPC UA plugin that registers for change events on
OPC UA objects.

NOTE:

This plugin assumes the open62541 is available at a specified location
in the file system, see below.

Configuration
-------------

This configuration of this plugin requires 3 parameters to be set

asset
  An asset name prefix that is added to the OPC UA variables retrieved from the OPC UA server

url
  The URL used to connect the server, of the form opc.tcp://<hostname>:<port>/...

subscribeById
  A toggle that determines of the subscriptions are to be treated as
  OPC UA node names or as browse names.


subscriptions
  An array of OPC UA node names that will control the subscription to
  variables in the OPC UA server.

  If the subscribeById option is set then this is an array of node
  Id's. Each node Id should be of the form ns=..;s=... Where ns is a
  namespace index and s is the node id string identifier.
 
  If the subscribeById option is not set then the array is an array of
  browse names. The array may be empty, in which case all variables are
  subscribed to in the server and will create assets in Fledge. Although
  simple subscribing to everything will return a lot of data that may
  not be of use. Alternatively a set of string may be give, the format
  of the strings is <namespace>:<name>. If the namespace is not requied
  then the name can simply be given. The plugin will traverse the node
  tree of the server and subscribe to all variables that live below the
  named nodes in the subscriptions array.
  
  Configuration examples:

.. code-block:: console

    {"subscriptions":["5:Simulation","2:MyLevel"]}
    {"subscriptions":["5:Sinusoid1","2:MyLevel","5:Sawtooth1"]}
    {"subscriptions":["2:Random.Double","2:Random.Boolean"]}

In the above examples
 - 5:Simulation is a node name under ObjectsNode
 - 5:Sinusoid1 and 5:Sawtooth1 are variables under ObjectsNode/Simulation 
 - 2:MyLevel is a variable under ObjectsNode/MyObjects/MyDevice
 - Random.Double and Random.Boolean are variables under ObjectsNode/Demo
 - 5 and 2 are the NamespaceIndex values of a node or a variable

It's also possible to specify an empty subscription array:

.. code-block:: console

    {"subscriptions":[]}

Note: depending on OPC UA server configuration (number of objects, number of variables)
this empty configuration might take a while to be loaded.

Object names, variable names and NamespaceIndexes can be easily retrieved
browsing the given OPC UA server using OPC UA clients, such as UaExpert

https://www.unified-automation.com/downloads/opc-ua-clients.html


As an example the UA client shows:

.. code-block:: console

    Node:

    NodeId ns=5;s=85/0:Simulation
    NodeClass [Object]
    BrowseName 5:Simulation

    Variables:

    NodeId ns=5;s=Sinusoid1
    NodeClass [Variable]
    BrowseName 5:Sinusoid1

    NodeId ns=2;s=MyLevel
    NodeClass [Variable]
    BrowseName 2:MyLevel

Most examples come from Object in ProSys OPC UA simulation server:

https://www.prosysopc.com/products/opc-ua-simulation-server/

Building open62541
------------------

Run the script requirements.sh to automate this and place a copy of the open62541
project in your home directory.

.. code-block:: console

  requirements.sh

If you require to place the open62541 code elsewhere you may pass the requirements.sh script an argument of a directory name to use.

.. code-block:: console

  requirements.sh ~/projects

Build
-----

To build the opcua plugin run the commands:

.. code-block:: console

  $ mkdir build
  $ cd build
  $ cmake ..
  $ make

- By default the Fledge develop package header files and libraries
  are expected to be located in /usr/include/fledge and /usr/lib/fledge
- If **FLEDGE_ROOT** env var is set and no -D options are set,
  the header files and libraries paths are pulled from the ones under the
  FLEDGE_ROOT directory.
  Please note that you must first run 'make' in the FLEDGE_ROOT directory.

You may also pass one or more of the following options to cmake to override 
this default behaviour:

- **FLEDGE_SRC** sets the path of a Fledge source tree
- **FLEDGE_INCLUDE** sets the path to Fledge header files
- **FLEDGE_LIB sets** the path to Fledge libraries
- **FLEDGE_INSTALL** sets the installation path of Random plugin

NOTE:
 - The **FLEDGE_INCLUDE** option should point to a location where all the Fledge 
   header files have been installed in a single directory.
 - The **FLEDGE_LIB** option should point to a location where all the Fledge
   libraries have been installed in a single directory.
 - 'make install' target is defined only when **FLEDGE_INSTALL** is set

Examples:

- no options

  $ cmake ..

- no options and FLEDGE_ROOT set

  $ export FLEDGE_ROOT=/some_fledge_setup

  $ cmake ..

- set FLEDGE_SRC

  $ cmake -DFLEDGE_SRC=/home/source/develop/Fledge  ..

- set FLEDGE_INCLUDE

  $ cmake -DFLEDGE_INCLUDE=/dev-package/include ..
- set FLEDGE_LIB

  $ cmake -DFLEDGE_LIB=/home/dev/package/lib ..
- set FLEDGE_INSTALL

  $ cmake -DFLEDGE_INSTALL=/home/source/develop/Fledge ..

  $ cmake -DFLEDGE_INSTALL=/usr/local/fledge ..
