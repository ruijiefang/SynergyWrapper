# OpenMPI benchmark collection

Currently there are:
  - Distributed insertion sort (Not very effective)
  - Distributed matrix multiplication (Also not very effective)
  - Distributed addition (kind-a effective)
  
To-be-implemented:
  - Distributed Strassen
  - Distributed Quick/mergesort
  - Distributed DFS over large trees
  - Distributed PI
  - Distributed solution to SAT/TSP problems (very inefficient)


Let's hope we don't run out of time yet...
## To use:
```
mpirun -np [num of workers per host] --hostfile [path-to-hostfile] ./MPI_Test_Programs [Test|Matrix|Addition|TestConcept|Sort] <Optional args per selection>
```


### Arguments for `Matrix`:
 - `--size <SIZE>` specifies the size of the rectangular matrix A, B="SIZE*SIZE"
 - `-G <G>` specifies the number of rows distributed to each host. The partition algorithm we use guarantees a minimal fairness of one row per host but may encounter errors when granularity is very, very large.

### Arguments for `Addition`:
 - `<Target>` The number we're going to reach.

### Arguments for `Sort`:


...More to be added.
