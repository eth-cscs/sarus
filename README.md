# Sarus - An OCI-compatible container engine for HPC

[![License: BSD 3-Clause](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause)
[![Documentation](https://readthedocs.org/projects/sarus/badge/?version=latest)](https://sarus.readthedocs.io/en/latest/?badge=latest)
[![Integration](https://github.com/eth-cscs/sarus/actions/workflows/integration.yml/badge.svg)](https://github.com/eth-cscs/sarus/actions/workflows/integration.yml)

Sarus is a software to run Linux containers on High Performance Computing
environments. Its development has been driven by the specific requirements of
HPC systems, while leveraging open standards and technologies to encourage
vendor and community involvement.

Key features:

* Spawning of isolated software environments (*containers*), built by users to
  fit the deployment of a specific application
* Security oriented to HPC systems
* Extensible runtime by means of OCI hooks to allow current and future
  support of custom hardware while achieving native performance
* Creation of container filesystems tailored for diskless nodes and parallel
  filesystems
* Compatibility with the presence of a workload manager
* Compatibility with the Open Container Initiative (OCI) standards:

    - Can pull images from registries adopting the OCI Distribution
      Specification or the Docker Registry HTTP API V2 protocol
    - Can import and convert images adopting the OCI Image Format
    - Sets up a container bundle complying to the OCI Runtime Specification
    - Uses an OCI-compliant runtime to spawn the container process


## Accessing the documentation

The full documentation is available on [Read the Docs](https://sarus.readthedocs.io).

If you wish to generate the documentation yourself, the sources are located in the `doc`
directory and can be built using Python 3 and Sphinx:

```
cd doc
python3 -m venv ./venv
source venv/bin/activate
pip3 install -r requirements.txt
make html
```


## Communications

If you think you've identified a security issue in the project, please *DO NOT*
report the issue publicly via the Github issue tracker. Instead, send an
email with as many details as possible to the following address:
`sarus@cscs.ch`.
This is the business address to the core Sarus maintainers.

To report bugs and request new features, you can use GitHub [issues](https://github.com/eth-cscs/sarus/issues).


## Contributing

Contributions to Sarus are welcome and greatly appreciated. Please refer to the `CONTRIBUTING.md` file for more detailed
information about contributing.


## Publications

* Benedicic, L., Cruz, F.A., Madonna, A. and Mariotti, K., 2019, June. Sarus: Highly Scalable Docker Containers for HPC Systems. In *International Conference on High Performance Computing* (pp. 46-60). Springer, Cham.

  https://doi.org/10.1007/978-3-030-34356-9_5
