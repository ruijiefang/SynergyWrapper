Synergy4
============

Synergy4 is a framework for statistic-multiplexed, resilient and scalable high-performance computing.

## Brief intro

Requirements:

 - `libzmq-4.2,libczmq-3.0+` for networking functionalities (ZeroMQ)

 - `mongodb-dev,libbson` for parser implementation (MongoDB's bson library)

 - `CMake > 3.5` for building

 - POSIX-compliance for shared memory; shared memory size must be greater than 4MB.

 - `pthreads` support

The entire framework is composed of a few parts, listed below:

 - `libzlog,libchash` supporting libraries for logging and a hash table

 - `p2pmd` a peer-to-peer application-level routing overlay using ring topology. This guarantees maximal availability of synergy4 computation and dynamic, runtime scalability.

 - `parser` uses `libbson` to parse an `sng-config.json` file

 - `tsd` is the tuple-space daemon

 - `p2pm_` helpers are utility functions for ZeroMQ

 - `bv.h`, `ts_helper.h`, `shared_memory.h`, `zhelpers.h` are header-file-based simple libraries for expressing complex communication semantics in easier ways

 - `libts` is the client-side header/static library that is linked against the user application during compile time

 - `libcnf-compat` is a compatibility wrapper for old Synergy (Synergy 3.0-) applications. It does not support all API calls, so some modifications on the program is required.
 (Note: `libcnf-compat` is also a header/static library that is linked against user application during compile time).

**To build Synergy4, make a new directory, and type in `cmake ../ && make`. **

## Where are they?

 - `libchash`,`zlog` are under `contrib/`. (we didn't develop them! see their license files and readme's for corresponding authors and copyrights.
 - `zeromq`, `libbzon` are requirements to be satisfied before compilation. They better be on your local machine!
 - `tsd` is located under `src/tsd`, although it uses other dependencies
 - `parser` is located under `src/parser`, `p2pmd` is located under `src/p2pmd`, `libts` is located under `src/ts`...... You get it. They all depend on the `src/*.c src/*.h` functions
 for day-to-day operations.
 - Examples for using the basig ring topology can be found under `src/p2pmd_examples`. You should be able to run them after reading a few lines of code. They get compiled as `tserver,tclient,ttest`.
 - For more information, go read `CMakeLists.txt` yourself. It's pretty direct how different components get compiled and are linked against each other.


## Authors
Synergy4 is authored by Rui-Jie Fang and Justin Y. Shi.

## Contact

Please contact rfang@temple.edu or shi@temple.edu for more information. 

## Acknowledgements

`Synergy4` also benefited from discussions with Y. Celik in our research group.