Triangles Counting in CUDA GPU
=========

CUDA implementation of parallel algorithm for counting triangles.

## CSR format storage
Suppose our graph has m edges and n nodes.
we use sparse matrix (CSR) to store the graph. Two arrays are required, (n+1) length array
(row array) and (m) length array storing node index. We only store upper trianglar matrix.

For example, suppose our graph has (0,1),(1,2),(1,3),(2,3),(2,4),(3,4),(3,5),(4,5).
n=6,m=8.
Storing in CSR Upper Triangular Format is equivalent to:
`[1,2,3,3,4,4,5,5]`(m) and `[0,1,3,5,7,8,8]`(n+1).

```Python
from scipy import sparse
a = sparse.csr_matrix(([1]*8,[1,2,3,3,4,4,5,5], [0,1,3,5,7,8,8]), shape=(6,6))
print(a)
```

## Counting Triangles
The parallelism comes from the triangles containing each edge is indepedent from each other.
 