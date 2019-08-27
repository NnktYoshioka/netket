# Spin
Hilbert space composed of spin states.

## Class Constructor [1]
Constructs a new ``Spin`` given a graph and the value of each spin.

|Argument|            Type            |                    Description                    |
|--------|----------------------------|---------------------------------------------------|
|graph   |netket._C_netket.graph.Graph|Graph representation of sites.                     |
|s       |float                       |Spin at each site. Must be integer or half-integer.|

### Examples
Simple spin hilbert space.

```python
>>> from netket.graph import Hypercube
>>> from netket.hilbert import Spin
>>> g = Hypercube(length=10,n_dim=2,pbc=True)
>>> hi = Spin(graph=g, s=0.5)
>>> print(hi.size)
100

```


## Class Constructor [2]
Constructs a new ``Spin`` given a graph and the value of each spin.

|Argument|            Type            |                     Description                     |
|--------|----------------------------|-----------------------------------------------------|
|graph   |netket._C_netket.graph.Graph|Graph representation of sites.                       |
|s       |float                       |Spin at each site. Must be integer or half-integer.  |
|total_sz|float                       |Constrain total spin of system to a particular value.|

### Examples
Simple spin hilbert space.

```python
>>> from netket.graph import Hypercube
>>> from netket.hilbert import Spin
>>> g = Hypercube(length=10,n_dim=2,pbc=True)
>>> hi = Spin(graph=g, s=0.5, total_sz=0)
>>> print(hi.size)
100

```



## Class Methods 
### number_to_state
Returns the visible configuration corresponding to the i-th basis state
for input i. Throws an exception iff the space is not indexable.



### random_vals
Member function generating uniformely distributed local random states.

|Argument|                                                                                  Type                                                                                  |                                      Description                                       |
|--------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------|
|state   |numpy.ndarray[float64[m, 1], flags.writeable]                                                                                                                           |A reference to a visible configuration, in output this contains the random state.       |
|rgen    |std::__1::mersenne_twister_engine<unsigned int, 32ul, 624ul, 397ul, 31ul, 2567483615u, 11ul, 4294967295u, 7ul, 2636928640u, 15ul, 4022730752u, 18ul, 1812433253u> = None|The random number generator. If None, the global NetKet random number generator is used.|

### Examples
Test that a new random state is a possible state for the hilbert
space.

```python
>>> import netket as nk
>>> import numpy as np
>>> hi = nk.hilbert.Boson(n_max=3, graph=nk.graph.Hypercube(length=5, n_dim=1))
>>> rstate = np.zeros(hi.size)
>>> rg = nk.utils.RandomEngine(seed=1234)
>>> hi.random_vals(rstate, rg)
>>> local_states = hi.local_states
>>> print(rstate[0] in local_states)
True

```



### state_to_number
Returns index of the given many-body configuration.
Throws an exception iff the space is not indexable.


### states
Returns an iterator over all valid configurations of the Hilbert space.
Throws an exception iff the space is not indexable.


### update_conf
Member function updating a visible configuration using the information on
where the local changes have been done.

|Argument |                    Type                     |                       Description                        |
|---------|---------------------------------------------|----------------------------------------------------------|
|v        |numpy.ndarray[float64[m, 1], flags.writeable]|The vector of visible units to be modified.               |
|to_change|numpy.ndarray[int32]                         |A list of which quantum numbers will be modified.         |
|new_conf |numpy.ndarray[float64]                       |Contains the value that those quantum numbers should take.|

## Properties

|  Property  |                                                                            Type                                                                            |                                                      Description                                                       |
|------------|------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------|
|graph       |netket.graph.Graph                                                                                                                                          | The Graph used to construct this Hilbert space.                                                                        |
|is_discrete |bool                                                                                                                                                        | Whether the hilbert space is discrete.                                                                                 |
|is_indexable|        We call a Hilbert space indexable if and only if the total Hilbert space        dimension can be represented by an index of type int.        Returns|            bool: Whether the Hilbert space is indexable.                                                               |
|local_size  |int                                                                                                                                                         | Size of the local hilbert space.                                                                                       |
|local_states|list[float]                                                                                                                                                 | List of discreet local quantum numbers.                                                                                |
|n_states    |int                                                                                                                                                         | The total dimension of the many-body Hilbert space.                 Throws an exception iff the space is not indexable.|
|size        |int                                                                                                                                                         | The number of visible units needed to describe the system.                                                             |
