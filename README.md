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
    - [7500MB File](#7500mb-file)
  - [Installation](#installation)
- [Usage](#usage)
- [License](#license)
- [Contact](#contact)


## About The Project

PySubstringSearch is a library designed to search over an index file for substring patterns. In order to achieve speed and efficiency, the library is written in Rust. For string indexing, the library uses [libsais](https://github.com/IlyaGrebnov/libsais) suffix array construction library. The index created consists of the original text and a 32bit suffix array struct. To get around the limitations of the Suffix Array Construction implementation, the library uses a proprietary container protocol to hold the original text and index in chunks of 512MB.

The module implements a method for searching.
- `search` - Find different entries with the same substring concurrently. Concurrency increases as the index file grows in size with multiple inner chunks.


### Built With

* [libsais](https://github.com/IlyaGrebnov/libsais)


### Performance

#### 500MB File
| Library | Function | Time | #Results | Improvement Factor |
| ------------- | ------------- | ------------- | ------------- | ------------- |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | Ripgrepy('google', '500mb').run().as_string.split('\n') | 47.2ms | 5943 | 1.0x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | reader.search('google') | 1.22ms | 5943 | 38.7x |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | Ripgrepy('text_two', '500mb').run().as_string.split('\n') | 44.7ms | 159 | 1.0x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | reader.search('text_two') | 9.5µs | 159 | 4968x |

#### 7500MB File
| Library | Function | Time | #Results | Improvement Factor |
| ------------- | ------------- | ------------- | ------------- | ------------- |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | Ripgrepy('google', '6000mb').run().as_string.split('\n') | 900ms | 62834 | 1.0x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | reader.search('google') | 10.1ms | 62834 | 89.1x |
| [ripgrepy](https://pypi.org/project/ripgrepy/) | Ripgrepy('text_two', '6000mb').run().as_string.split('\n') | 820ms | 0 | 1.0x |
| [PySubstringSearch](https://github.com/Intsights/PySubstringSearch) | reader.search('text_two') | 200µs | 0 | 4100x |


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

# adding entries from file lines
writer.add_entries_from_file_lines('input_file.txt')

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
