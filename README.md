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
  - [Prerequisites](#prerequisites)
  - [Installation](#installation)
- [Usage](#usage)
- [License](#license)
- [Contact](#contact)


## About The Project

PySubstringSearch is a library intended for searching over an index file for substring patterns. The library is written in C++ to achieve speed and efficiency. The library also uses [Msufsort](https://github.com/michaelmaniscalco/msufsort) suffix array construction library for string indexing. The created index consists of the original text and a 32bit suffix array structs. The library relies on a proprietary container protocol to hold the original text along with the index in chunks of 512mb to evade the limitation of the Suffix Array Construction implementation.

The module implements two methods, search_sequential & search_parallel. search_sequential searches through the inner chunks one by one where search_parallel searches concurrently. When dealing with big indices, bigger than 1gb for example, search_parallel would function faster. I advice to check them both with the resulted index to find which one fits better.


### Built With

* [Msufsort](https://github.com/michaelmaniscalco/msufsort)


### Performance

| Library  | Text Size | Function | Time | #Results | Improvement Factor |
| ------------- | ------------- | ------------- | ------------- | ------------- | ------------- |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | 500mb | Ripgrepy('text_one', '500mb').run().as_string.split('\n') | 127 ms ± 694 µs per loop | 12553 | 1.0x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | 500mb | reader.search_sequential('text_one') | 2.48 ms ± 53.4 µs per loop | 12553 | 51.2x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | 500mb | reader.search_parallel('text_one') | 3.78 ms ± 350 µs per loop | 12553 | 33.6x |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | 500mb | Ripgrepy('text_two', '500mb').run().as_string.split('\n') | 127 ms ± 623 µs per loop | 769 | 1.0x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | 500mb | reader.search_sequential('text_two') | 156 µs ± 916 ns per loop | 769 | 814.0x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | 500mb | reader.search_parallel('text_two') | 251 µs ± 80.2 µs per loop | 769 | 506.0x |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | 6gb | Ripgrepy('text_one', '6gb').run().as_string.split('\n') | 1.38 s ± 3.82 ms | 206884 | 1.0x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | 6gb | reader.search_sequential('text_one') | 93.7 ms ± 2.16 ms per loop | 206884 | 15.3x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | 6gb | reader.search_parallel('text_one') | 34.3 ms ± 321 µs per loop | 206884 | 40.5x |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | 6gb | Ripgrepy('text_two', '6gb').run().as_string.split('\n') | 1.61 s ± 37.2 ms per loop | 6921 | 1.0x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | 6gb | reader.search_sequential('text_two') | 2.22 ms ± 79.3 µs per loop | 6921 | 725.2x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | 6gb | reader.search_parallel('text_two') | 1.38 ms ± 26 µs per loop | 6921 | 1166.6x |

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
