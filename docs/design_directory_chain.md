# Directory Chain Design

This document described how to handle directroy/file in debugfatfs.

## Overview

debugfatfs caches the directroy and file metadata in memory.
These caches are manged by List structure.

The head of the list is "Directory",
and after that elements are "File"/"Directory" under the "Directory".

```mermaid
flowchart TB
  subgraph d1 ["index 0:  /"]
    direction LR
    a(["/"])
    a1["FILE1"]
    a2["DIR1"]
    a3["DIR2"]

    a -->a1
    a1-->a2
    a2-->a3
  end

  subgraph d2 ["index 1:  DIR1"]
    direction LR
    b(["DIR1"])
    b1["FILE2"]
    b2["FILE3"]

    b -->b1
    b1-->b2
  end

  subgraph d3 ["index 2:  DIR2"]
    direction LR
    c(["DIR2"])
    c1["FILE4"]
    c2["FILE5"]

    c -->c1
    c1-->c2
  end

  d1 ~~~ d2 ~~~ d3
```

The key value of the list depends on whether head or otherwise.

* head: First cluster index
* otherwise: Name hash

