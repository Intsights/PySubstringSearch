#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <fstream>
#include <vector>
#include <unordered_set>
#include <cstring>
#include <thread>
#include <future>

#include "msufsort.hpp"


class subifstream {
    public:
        subifstream(
            std::string file_path,
            std::uint64_t start,
            std::uint64_t size
        ) {
            this->start = start;
            this->size = size;
            this->end = start + size;

            this->file_stream = std::make_unique<std::ifstream>(
                file_path,
                std::ios_base::in | std::ios_base::binary
            );
            this->file_stream->seekg(this->start, std::ios_base::beg);
            this->current_absolute_position = this->start;
        }

        void seekg(
            std::uint64_t position
        ) {
            this->file_stream->clear();
            if (position > this->size) {
                this->file_stream->seekg(this->end, std::ios_base::beg);
                this->current_absolute_position = this->end;
            } else {
                this->file_stream->seekg(this->start + position, std::ios_base::beg);
                this->current_absolute_position = this->start + position;
            }
        }

        void read(
            char * buffer,
            std::uint64_t size
        ) {
            if (this->current_absolute_position + size > this->end) {
                this->file_stream->read(buffer, this->end - this->current_absolute_position);
                this->current_absolute_position = this->end;
            } else {
                this->file_stream->read(buffer, size);
                this->current_absolute_position += this->size;
            }
        }

        std::unique_ptr<std::ifstream> file_stream;
        std::uint64_t current_absolute_position;
        std::uint64_t start;
        std::uint64_t end;
        std::uint64_t size;
};


class Writer {
    public:
        Writer(
            std::string index_file_path
        ) {
            this->index_file_stream = std::ofstream(
                index_file_path,
                std::ios::binary
            );
        }

        ~Writer() {
            this->finalize();
        }

        void add_entry(
            const std::string & text
        ) {
            for (const char & text_char : text) {
                this->text_stream.push_back(text_char);
            }
            this->text_stream.push_back('\0');

            if (this->text_stream.size() > this->max_entries_bytes_size) {
                this->dump_data();
            }
        }

        void dump_data() {
            if (this->text_stream.size() == 0) {
                return;
            }

            std::uint32_t text_stream_size = this->text_stream.size();
            this->index_file_stream.write(
                (const char *)&text_stream_size,
                sizeof(text_stream_size)
            );
            index_file_stream.write(
                text_stream.data(),
                text_stream.size()
            );

            std::vector<std::int32_t> suffix_array = maniscalco::make_suffix_array(
                this->text_stream.begin(),
                this->text_stream.end(),
                std::thread::hardware_concurrency()
            );
            std::uint32_t suffix_array_bytes_size = suffix_array.size() * sizeof(std::int32_t);
            this->index_file_stream.write(
                (const char *)&suffix_array_bytes_size,
                sizeof(suffix_array_bytes_size)
            );
            this->index_file_stream.write(
                (const char *)suffix_array.data(),
                suffix_array_bytes_size
            );

            this->text_stream.clear();
        }

        void finalize() {
            if (this->text_stream.size() != 0) {
                this->dump_data();
            }

            this->index_file_stream.close();
            this->text_stream.clear();
        }

        std::ofstream index_file_stream;
        std::vector<char> text_stream;
        std::uint32_t max_entries_bytes_size = 512 * 1024 * 1024;
};


class Reader {
    public:
        Reader(
            std::string index_file_path
        ) {
            std::ifstream input_file_stream(
                index_file_path,
                std::ios_base::in | std::ios_base::binary | std::ios_base::ate
            );
            if (!input_file_stream.good()) {
                throw std::runtime_error("Could not find index file path : " + index_file_path);
            }

            std::streamoff file_size = input_file_stream.tellg();
            input_file_stream.seekg(0, std::ios_base::beg);

            while (file_size != input_file_stream.tellg()) {
                std::uint32_t text_stream_size;
                input_file_stream.read(
                    (char *)&text_stream_size,
                    sizeof(text_stream_size)
                );
                std::vector<char> text_vector(text_stream_size);
                input_file_stream.read(
                    text_vector.data(),
                    text_stream_size
                );

                std::uint32_t suffix_array_bytes_size;
                input_file_stream.read(
                    (char *)&suffix_array_bytes_size,
                    sizeof(suffix_array_bytes_size)
                );
                std::shared_ptr<subifstream> suffix_array_file_stream = std::make_shared<subifstream>(
                    index_file_path,
                    input_file_stream.tellg(),
                    suffix_array_bytes_size
                );
                input_file_stream.seekg(suffix_array_bytes_size, std::ios_base::cur);

                this->files.push_back(
                    std::make_pair(
                        std::move(suffix_array_file_stream),
                        std::move(text_vector)
                    )
                );
            }
        }

        ~Reader() {}

        std::vector<std::string> search_parallel(
            const std::string & substring
        ) {
            std::vector<std::string> results;
            std::vector<std::future<std::vector<std::string>>> futures;

            for (std::uint32_t file_index = 0; file_index < this->files.size(); ++file_index) {
                auto future = std::async(
                    &Reader::search_specific_file,
                    this,
                    substring,
                    file_index
                );
                if (this->files.size() == 1) {
                    return future.get();
                } else {
                    futures.push_back(std::move(future));
                }
            }

            for (auto & future : futures) {
                auto result = future.get();
                results.insert(results.end(), result.begin(), result.end());
            }

            return results;
        }

        std::vector<std::string> search_sequential(
            const std::string & substring
        ) {
            std::vector<std::string> results;
            std::unordered_set<std::int32_t> results_indices;

            for (std::uint32_t file_index = 0; file_index < this->files.size(); ++file_index) {
                auto result = this->search_specific_file(
                    substring,
                    file_index
                );
                if (this->files.size() == 1) {
                    return result;
                } else {
                    results.insert(results.end(), result.begin(), result.end());
                }
            }

            return results;
        }

        inline std::vector<std::string> search_specific_file(
            const std::string & substring,
            std::uint32_t file_index
        ) {
            std::vector<std::string> results;
            std::unordered_set<std::int32_t> results_indices;

            const auto & [suffix_array_file_stream, text_vector] = this->files[file_index];
            std::int32_t index;

            std::uint64_t left_anchor = 4;
            std::uint64_t right_anchor = suffix_array_file_stream->size;
            std::uint64_t first_suffix_array_index = std::numeric_limits<std::uint64_t>::max();

            while (left_anchor <= right_anchor) {
                std::uint64_t middle_anchor = left_anchor + ((right_anchor - left_anchor) / 4 / 2 * 4);

                suffix_array_file_stream->seekg(middle_anchor);
                suffix_array_file_stream->read((char *)&index, sizeof(index));

                std::string_view suffix(&text_vector[index], substring.size());

                auto distance = std::memcmp(
                    substring.c_str(),
                    suffix.data(),
                    substring.size()
                );
                if (distance < 0) {
                    right_anchor = middle_anchor - 4;
                } else if (distance > 0) {
                    left_anchor = middle_anchor + 4;
                } else {
                    first_suffix_array_index = middle_anchor;
                    right_anchor = middle_anchor - 4;
                }
            }
            if (first_suffix_array_index == std::numeric_limits<std::uint64_t>::max()) {
                return results;
            }

            std::uint64_t current_index_position = first_suffix_array_index;
            while (current_index_position < suffix_array_file_stream->size) {
                std::vector<std::int32_t> suffixes_vector(100, -1);
                suffix_array_file_stream->seekg(current_index_position);
                suffix_array_file_stream->read((char *)suffixes_vector.data(), sizeof(std::int32_t) * 100);

                bool finished = false;
                for (std::int32_t suffix_index : suffixes_vector) {
                    if (suffix_index == -1) {
                        break;
                    }
                    auto distance = std::memcmp(
                        substring.c_str(),
                        &text_vector[suffix_index],
                        substring.size()
                    );
                    if (distance != 0) {
                        finished = true;
                        break;
                    }

                    for (; suffix_index > 0; suffix_index -= 1) {
                        if (text_vector[suffix_index - 1] == '\0') {
                            break;
                        }
                    }
                    const auto & [iterator, inserted] = results_indices.emplace(suffix_index);
                    if (inserted == true) {
                        results.push_back(std::string(&text_vector[suffix_index]));
                    }
                }
                if (finished) {
                    break;
                }

                current_index_position += sizeof(std::int32_t) * 100;
            }

            return results;
        }

        std::vector<std::pair<std::shared_ptr<subifstream>, std::vector<char>>> files;
};

PYBIND11_MODULE(pysubstringsearch, m) {
    pybind11::class_<Reader>(m, "Reader")
        .def(
            pybind11::init<std::string>(),
            "Reader object that handles searches over an index file",
            pybind11::arg("index_file_path")
        )
        .def(
            "search_parallel",
            &Reader::search_parallel,
            "search over an index file for a substring",
            pybind11::arg("substring")
        )
        .def(
            "search_sequential",
            &Reader::search_sequential,
            "search over an index file for a substring",
            pybind11::arg("substring")
        );
    pybind11::class_<Writer>(m, "Writer")
        .def(
            pybind11::init<std::string>(),
            "Writer object that handles the creation of an index file",
            pybind11::arg("index_file_path")
        )
        .def(
            "add_entry",
            &Writer::add_entry,
            "add a specific substring into the index",
            pybind11::arg("text")
        )
        .def(
            "finalize",
            &Writer::finalize,
            "finalize the index file, writes the content into the file and closes it"
        );
}
