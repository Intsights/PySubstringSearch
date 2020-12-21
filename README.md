<p align="center">
    <a href="https://github.com/Intsights/PySubstringSearch">
        <img src="https://raw.githubusercontent.com/Intsights/PySubstringSearch/master/images/logo.png" alt="Logo">
    </a>
    <h3 align="center">
        Python library for fast substring/pattern search written in C++ leveraging Suffix Array Algorithm
    </h3>
</p>

![license](https://img.shields.io/badge/MIT-License-blue)
![Python](https://img.shields.io/badge/Python-3.6%20%7C%203.7%20%7C%203.8%20%7C%203.9%20%7C%20pypy3-blue)
![Build](https://github.com/Intsights/PySubstringSearch/workflows/Build/badge.svg)
[![PyPi](https://img.shields.io/pypi/v/PySubstringSearch.svg)](https://pypi.org/project/PySubstringSearch/)

## Table of Contents

- [Table of Contents](#table-of-contents)
- [About The Project](#about-the-project)
  - [Built With](#built-with)
  - [Performance](#performance)
    - [500MB File](#500mb-file)
    - [6000MB File](#6000mb-file)
  - [Prerequisites](#prerequisites)
  - [Installation](#installation)
- [Usage](#usage)
- [License](#license)
- [Contact](#contact)


## About The Project

PySubstringSearch is a library intended for searching over an index file for substring patterns. The library is written in C++ to achieve speed and efficiency. The library also uses [Msufsort](https://github.com/michaelmaniscalco/msufsort) suffix array construction library for string indexing. The created index consists of the original text and a 32bit suffix array structs. The library relies on a proprietary container protocol to hold the original text along with the index in chunks of 512mb to evade the limitation of the Suffix Array Construction implementation.

The module implements multiple methods.
`search` - search concurrently for a substring existed in different entries within the index file. As the index file getting bigger with multiple inner chunks, the concurrency effect increases.
`count_entries` - return the number of entries in the index file consisting of the substring.
`count_occurrences` - return the number of occurrences of the substring in all the entries. If the substring exists multiple times in the same entry, each occurrence will be counted.


### Built With

* [Msufsort](https://github.com/michaelmaniscalco/msufsort)


### Performance

#### 500MB File
| Library | Function | Time | #Results | Improvement Factor |
| ------------- | ------------- | ------------- | ------------- | ------------- |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | Ripgrepy('text_one', '500mb').run().as_string.split('\n') | 148ms | 2367 | 1.0x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | reader.search('text_one') | 1.28ms | 2367 | 115.6x |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | Ripgrepy('text_two', '500mb').run().as_string.split('\n') | 116ms | 159 | 1.0x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | reader.search('text_two') | 228µs | 159 | 508.7x |

#### 6000MB File
| Library | Function | Time | #Results | Improvement Factor |
| ------------- | ------------- | ------------- | ------------- | ------------- |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | Ripgrepy('text_one', '6000mb').run().as_string.split('\n') | 2.4s | 59538 | 1.0x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | reader.search_parallel('text_one') | 43.5ms | 59538 | 55.1x |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | Ripgrepy('text_two', '6000mb').run().as_string.split('\n') | 1.5s | 7266 | 1.0x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | reader.search_sequential('text_two') | 4.51ms | 7266 | 332.6x |

### Prerequisites

In order to compile this package you should have GCC & Python development package installed.
* Fedora
```sh
sudo dnf install python3-devel gcc-c++
```
* Ubuntu 18.04
```sh
sudo apt install python3-dev g++-9
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

# lookup for a substring sequentially
reader.search_sequential('short')
>>> ['some short string']

# lookup for a substring sequentially
reader.search_sequential('string')
>>> ['some short string', 'another but now a longer string']

# lookup for a substring concurrently
reader.search_parallel('short')
>>> ['some short string']

# lookup for a substring concurrently
reader.search_parallel('string')
>>> ['some short string', 'another but now a longer string']
```



## License

Distributed under the MIT License. See `LICENSE` for more information.


## Contact

Gal Ben David - gal@intsights.com

Project Link: [https://github.com/Intsights/PySubstringSearch](https://github.com/Intsights/PySubstringSearch)
