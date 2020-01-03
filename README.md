<p align="center">
    <a href="https://github.com/wavenator/PySubstringSearch">
        <img src="https://raw.githubusercontent.com/wavenator/PySubstringSearch/master/images/logo.png" alt="Logo">
    </a>
    <h3 align="center">
        Python library for fast substring/pattern search written in C++ leveraging Suffix Array Algorithm
    </h3>
</p>

![license](https://img.shields.io/badge/MIT-License-blue)
![Python](https://img.shields.io/badge/Python-3.6%20%7C%203.7%20%7C%203.8-blue)
![Build](https://github.com/wavenator/PySubstringSearch/workflows/Build/badge.svg)
[![PyPi](https://img.shields.io/pypi/v/PySubstringSearch.svg)](https://pypi.org/project/PySubstringSearch/)

## Table of Contents

- [Table of Contents](#table-of-contents)
- [About The Project](#about-the-project)
  - [Built With](#built-with)
  - [Performance](#performance)
    - [High number of results](#high-number-of-results)
    - [Low number of results](#low-number-of-results)
  - [Prerequisites](#prerequisites)
  - [Installation](#installation)
- [Usage](#usage)
- [License](#license)
- [Contact](#contact)


## About The Project

PySubstringSearch is a library intended for searching over an index file for substring patterns. The library is written in C++ to achieve speed and efficiency. The library also uses [Msufsort](https://github.com/michaelmaniscalco/msufsort) suffix array construction library for string indexing. The created index consists of the original text and a 32bit suffix array structs. The library relies on a proprietary container protocol to hold the original text along with the index in chunks of 512mb to evade the limitation of the Suffix Array Construction implementation.


### Built With

* [Msufsort](https://github.com/michaelmaniscalco/msufsort)


### Performance

Test was measured on a file containing 500MB of text

#### High number of results
| Library  | Function | Time | #Results | Improvement Factor |
| ------------- | ------------- | ------------- | ------------- | ------------- |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | Ripgrepy('text', '500mb').run().as_string | 82.1 ms ± 1.15 ms per loop | 10737 | 1.0x |
| [PySubstringSearch](https://github.com/wavenator/PySubstringSearch) | reader.search('text') | 2.31 ms ± 142 µs per loop | 10737 | 35.5x |

#### Low number of results
| Library  | Function | Time | #Results | Improvement Factor |
| ------------- | ------------- | ------------- | ------------- | ------------- |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | Ripgrepy('text', '500mb').run().as_string | 101 ms ± 526 µs per loop | 251 | 1.0x |
| [PySubstringSearch](https://github.com/wavenator/PySubstringSearch) | reader.search('text') | 55.9 µs ± 464 ns per loop | 251 | 1803.0x |

### Prerequisites

In order to compile this package you should have GCC & Python development package installed.
* Fedora
```sh
sudo dnf install python3-devel gcc-c++
```
* Ubuntu 18.04
```sh
sudo apt install python3-dev g++-8
```

### Installation

```sh
pip3 install PySubstringSearch
```



## Usage

Create an index
```python
import pysubstringsearch

# creating a new index file
# if a file with this name is already exists, it will be overwritten
writer = pysubstringsearch.Writer(
    index_file_path='output.idx',
)

# adding entries to the new index
writer.add_entry('some short string')
writer.add_entry('another but now a longer string')
writer.add_entry('more text to add')

# making sure the data is dumped to the file
writer.finalize()
```

Search a substring within an index
```python
import pysubstringsearch

# opening an index file for searching
reader = pysubstringsearch.Reader(
    index_file_path='output.idx',
)

# lookup for a substring
reader.search('short')
>>> ['some short string']

# lookup for a substring
reader.search('string')
>>> ['some short string', 'another but now a longer string']
```



## License

Distributed under the MIT License. See `LICENSE` for more information.


## Contact

Gal Ben David - wavenator@gmail.com

Project Link: [https://github.com/wavenator/PySubstringSearch](https://github.com/wavenator/PySubstringSearch)
