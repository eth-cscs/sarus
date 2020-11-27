# Contributing

Contributions to Sarus are welcome and greatly appreciated. Proper credit will be given to contributors by adding their
names to the `CONTRIBUTORS.md` file. The copyright of Sarus belongs to ETH Zurich, and external contributors are required to
sign a Contributor License Agreement; the development team will privately contact new contributors during their first
content submission.

## Contribution guidelines

### Submission

- When adding new features or fixing bugs not covered by existing tests, please also provide unit and/or integration tests
  for the code.
- Add an entry to the `CHANGELOG.md` file.
- Submit your contributions as GitHub Pull Requests to the main public repository at https://github.com/eth-cscs/sarus.

### Code style

Sarus uses the conventions below for its C++ code. Keep the style consistent with what you see in the files you're
modifying. Beware there may be some inconsistencies in the present code, but avoid introducing yet another style.

- Camel case is used for names of variables, functions and classes. Class names start with a capital letter.
- Indentation style is K&R-derived, with indentation of 4 spaces and no tabs.
