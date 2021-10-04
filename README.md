## Introduction

The DiRe-CFD (Dispersion Resampling - Computational Fluid Dynamics) library implements a new numerical scheme for simulating fluid flow. The method - unlike previous ones - is not restricted spatially, can be computed with varying local precision/detail across time and space, while having O(k*n) computational complexity (where "n" is the number of simulated "nodes", "k" is a constant factor).

## Contents

The DiRe-CFD library is implemented in the "dire_cfd" folder.
A demo, showing it's usage is implemented in the "dire_cfd_demo" folder.
The underlying math's and algorithm's detailed description is found in the "study" folder.

## License

This work is dual-licensed under either NPOSL-3.0 or MIT. See the LICENSE files in each subrepository.
