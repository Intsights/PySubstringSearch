<p align="center">
    <a href="https://github.com/Intsights/PySubstringSearch">
        <img src="https://raw.githubusercontent.com/Intsights/PySubstringSearch/master/images/logo.png" alt="Logo">
    </a>
    <h3 align="center">
        A Python library written in Rust that searches for substrings quickly using a Suffix Array
    </h3>
</p>

![license](https://img.shields.io/badge/MIT-License-blue)
![Python](https://img.shields.io/badge/Python-3.7%20%7C%203.8%20%7C%203.9%20%7C%203.10-blue)
![Build](https://github.com/Intsights/PySubstringSearch/workflows/Build/badge.svg)
[![PyPi](https://img.shields.io/pypi/v/PySubstringSearch.svg)](https://pypi.org/project/PySubstringSearch/)

## Table of Contents

- [Table of Contents](#table-of-contents)
- [About The Project](#about-the-project)
  - [Built With](#built-with)
  - [Performance](#performance)
    - [500MB File](#500mb-file)
    - [6000MB File](#6000mb-file)
  - [Installation](#installation)
- [Usage](#usage)
- [License](#license)
- [Contact](#contact)


## About The Project

PySubstringSearch is a library designed to search over an index file for substring patterns. In order to achieve speed and efficiency, the library is written in Rust. For string indexing, the library uses [Msufsort](https://github.com/michaelmaniscalco/msufsort) suffix array construction library. The index created consists of the original text and a 32bit suffix array struct. To get around the limitations of the Suffix Array Construction implementation, the library uses a proprietary container protocol to hold the original text and index in chunks of 512MB.

The module implements a method for searching.
- `search` - Find different entries with the same substring concurrently. Concurrency increases as the index file grows in size with multiple inner chunks.


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
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | reader.search('text_one') | 15.4ms | 59538 | 155.8x |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | Ripgrepy('text_two', '6000mb').run().as_string.split('\n') | 1.5s | 7266 | 1.0x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | reader.search('text_two') | 1.97ms | 7266 | 761.4x |


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

Gal Ben David - gal@intsights.com

Project Link: [https://github.com/Intsights/PySubstringSearch](https://github.com/Intsights/PySubstringSearch)
