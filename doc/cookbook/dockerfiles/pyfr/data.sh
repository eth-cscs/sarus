#!/bin/bash

# From the container:
## copy the example data to the scratch directory
mkdir $SCRATCH/pyfr/
cd PyFR-1.8.0/examples/
cp -r euler_vortex_2d/ $SCRATCH/pyfr/euler_vortex_2d

## Convert mesh data to PyFR format
cd $SCRATCH/pyfr/euler_vortex_2d
pyfr import euler_vortex_2d.msh euler_vortex_2d.pyfrm

## Partition the mesh and exit the container
pyfr partition 4 euler_vortex_2d.pyfrm .
