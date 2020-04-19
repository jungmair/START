# START - Self-Tuning Adaptive Radix Tree
The basis of this project is an efficient but extendible [ART](https://db.in.tum.de/~leis/papers/ART.pdf) implementation. Adding new multilevel nodes based on rewiring and a self-tuning component results in a self-tuning adaptive radix tree as described in  [our Paper](http://db.in.tum.de/~fent/papers/Self%20Tuning%20Art.pdf).

### Configurable ART implementation
Current implementations of adaptive radix trees are either implemented efficiently or flexible. Using hard-coded switch-case statements to handle the different node types permits many compiler optimizations and avoids the performance impacts of virtual functions. However, adding a new node type comes with a lot of effort.

Our implementation of an adaptive radix tree in [art_impl.tcc](art_impl.tcc) handles arbitrary node types (also multilevel nodes) at compile time. All config is injected with a C++ trait as template parameter:

* Which keys are indexed? 32-bit integer, 64-bit integer, strings?
* Which node types should be used?
* How are the keys stored? In a simple vector?
* Do we need statistics? e.g. memory consumption ...
* ...

With these informations, an efficient ART implementation is constructed at compile time. For this purpose, we use our own implementation of a switch-case statement generated at compile-time using a mixture of macro and template magic: [util/compile-time-switch.tcc](util/compile-time-switch.tcc).
### Nodes
The `nodes` directory contains all code related to implementing different node types. The base class for each node type can be found in [nodes/node.tcc](nodes/node.tcc).

Since START is an extension to the adaptive radix tree, all standard ART Nodes (4/16/48/256) are implemented efficiently in [nodes/standard/](nodes/standard/). The two rewired node types, `Rewired64K` and `Rewired16M` can be found at [nodes/rewired/](nodes/rewired/), `MultiNode4` at [nodes/simple/](nodes/simple/).

When a node overflows, or during the tuning phase, transitions from one node type to another occur. Then, all node entries must be migrated from one node instance to a newly created of another type. For many node types this implemented generically. However, for some migrations, e.g. to rewired nodes, between 4/16/48, efficient specializations have been implemented. All migration-related code is located in [nodes/migrate/](nodes/migrate/).


### Measurements
For a decent costmodel, we measure the cost of a single lookup in different node types for three cases: Node is cached, node header is cached, the node is not cached at all. These finegrained measurements are possible by combining three assembler instructions:

* `mfence` as memory barrier, to separate the code to be measured
* `rdtsc` for measuring cycles exactly.
* `clflush` for flushing single caches from the caches  

To get reliable numbers, single measurements are repeated and the median is computed. The complete implementation can be found in [measurements/measurements.cpp](measurements/measurements.cpp)
### Tuning
The tuning component is of course one main component of a self-tuning adaptive radix tree. All tuning-related code can be found in `tuning`, [tuning/tuning.tcc] contains the main tuning procedure including dynamic programming.

During the tuning phase, we attach tags to the tree nodes by misusing the prefix field. After tuning, we restore the prefix fields. This allows for optimization without permant memory overhead. Have a look at [tuning/util/tag_manager.tcc](tuning/util/tag_manager.tcc) for more details.

The tuning process can be split into three phases:

1. **Cache estimation**: First, we iterate through the tree and estimate which nodes will most likely reside in L1/L2/L3 or no cache using [tuning/util/cache_estimator.tcc](tuning/util/cache_estimator.tcc). This estimation is then stored in the node's tag. 
2. **Dynamic programming**: Next, we apply dynamic programming to optimize the replacement decisions based on the [cost model](tuning/util/costmodel.tcc) The tree is visited bottom up, relevant DP data is accumulated using the [dp_data](tuning/util/dp_data.tcc) type.
3. **Replacing** Finally, the decisions made during dynamic programming are realized.

### Rewiring
As noted before, two multilevel nodes, `Rewired64K` and `Rewired16M` use a technique called rewiring that was proposed by [Schuhknecht et. al.](http://www.vldb.org/pvldb/vol9/p768-schuhknecht.pdf) in 2016. Rewiring allows mapping multiple virtual pages to the same physical pages, which is exploited in our rewired node implementations.

The rewiring implementation proposed in 2016 uses main memory files and mmap cales. Thus, no preparation is required, a recent Linux kernel is enough. Still, this way of implementing rewiring is far from perfect, causing many costly system calls and page faults as well as a significant kernel memory overhead. These drawbacks can be avoided by inserting a [kernel module for rewiring](https://github.com/jungmair/rewiring-lkm). Both approaches are implemented in [util/rewiring/](util/rewiring/). mmap-based rewiring is the default.

### Test
This project can be tested by running a google test suite located under `test/`. These black-box tests use various datasets to verify the functionality: dense32, dense64, random32, random64, zipf64, random string, url
### SOSD integration
For a simple integration of our implementation into the [SOSD benchmark](https://github.com/learnedsystems/sosd), we provide two wrapper classes:
* **`sosd-competitor-adapter-ART.h`**: no START features: only standard ART nodes, no tuning
* **`sosd-competitor-adapter-START.h`**: also uses the three START nodes (Rewired64K, Rewired16M, MultiNode4) and the self-tuning component

Follow this instructions to integrate our ART/START implementation into SOSD:
1. Checkout the SOSD benchmark
2. change to the `competitor` directory and checkout this repository
3. add to `benchmark.cc`: 
```cpp
#include "competitors/START/sosd-competitor-adapter-ART.h"
#include "competitors/START/sosd-competitor-adapter-START.h"
[...]
benchmark.Run<ART_NEW<uint32_t>>();
benchmark.Run<START<uint32_t>>();
[...]
benchmark.Run<ART_NEW<uint64_t>>();
benchmark.Run<START<uint64_t>>();
```
4. follow the SOSD README to install all dependencies, build and run the benchmark


### Build
Before building make sure that both, `boost hana` and `boost preprocessor` are installed.