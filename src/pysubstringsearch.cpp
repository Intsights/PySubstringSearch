#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <fstream>
#include <vector>
#include <unordered_set>
#include <cstring>
#include <thread>
#include <future>
#include <mutex>

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

        std::vector<std::string> search(
            const std::string & substring
        ) {
            std::vector<std::string> entries;
            std::vector<std::future<void>> futures;

            for (std::uint32_t file_index = 0; file_index < this->files.size(); ++file_index) {
                auto future = std::async(
                    &Reader::search_entries_in_file,
                    this,
                    std::ref(entries),
                    substring,
                    file_index
                );
                futures.push_back(std::move(future));
            }

            for (auto & future : futures) {
                future.wait();
            }

            return entries;
        }

        std::uint32_t count_occurrences(
            const std::string & substring
        ) {
            auto number_of_occurrences = 0;
            std::vector<std::future<std::optional<std::tuple<std::int32_t, std::int32_t>>>> futures;

            for (std::uint32_t file_index = 0; file_index < this->files.size(); ++file_index) {
                auto future = std::async(
                    &Reader::get_substring_positions,
                    this,
                    substring,
                    file_index
                );
                futures.push_back(std::move(future));
            }

            for (auto & future : futures) {
                auto result = future.get();
                if (result.has_value()) {
                    auto [first_text_index, last_text_index] = result.value();
                    auto number_of_text_indices = ((last_text_index - first_text_index) / 4) + 1;
                    number_of_occurrences += number_of_text_indices;
                }
            }

            return number_of_occurrences;
        }

        std::uint32_t count_entries(
            const std::string & substring
        ) {
            auto number_of_entries = 0;
            std::vector<std::future<std::size_t>> futures;

            for (std::uint32_t file_index = 0; file_index < this->files.size(); ++file_index) {
                auto future = std::async(
                    &Reader::count_entries_in_file,
                    this,
                    substring,
                    file_index
                );
                futures.push_back(std::move(future));
            }

            for (auto & future : futures) {
                auto number_of_entries_in_file = future.get();
                number_of_entries += number_of_entries_in_file;
            }

            return number_of_entries;
        }

        inline std::optional<std::tuple<std::int32_t, std::int32_t>> get_substring_positions(
            const std::string & substring,
            std::uint32_t file_index
        ) {
            const auto & [suffix_array_file_stream, text_vector] = this->files[file_index];

            // The index file first 4 bytes are the size of the index file
            // in a uint32_t format, hence starting from the 5th byte.
            std::uint64_t left_anchor = 4;
            std::uint64_t right_anchor = suffix_array_file_stream->size;

            std::optional<std::uint64_t> first_suffix_array_index = std::nullopt;
            std::optional<std::uint64_t> last_suffix_array_index = std::nullopt;
            while (left_anchor <= right_anchor) {
                std::uint64_t middle_anchor = left_anchor + ((right_anchor - left_anchor) / 4 / 2 * 4);

                std::int32_t text_index;
                suffix_array_file_stream->seekg(middle_anchor);
                suffix_array_file_stream->read((char *)&text_index, sizeof(text_index));

                auto distance = std::memcmp(substring.c_str(), &text_vector[text_index], substring.size());
                if (distance < 0) {
                    right_anchor = middle_anchor - 4;
                } else if (distance > 0) {
                    left_anchor = middle_anchor + 4;
                } else {
                    first_suffix_array_index = middle_anchor;
                    if (!last_suffix_array_index.has_value()) {
                        last_suffix_array_index = middle_anchor;
                    }
                    right_anchor = middle_anchor - 4;
                }
            }
            if (!first_suffix_array_index.has_value()) {
                return std::nullopt;
            }

            left_anchor = last_suffix_array_index.value();
            right_anchor = suffix_array_file_stream->size;
            while (left_anchor <= right_anchor) {
                std::uint64_t middle_anchor = left_anchor + ((right_anchor - left_anchor) / 4 / 2 * 4);

                std::int32_t text_index;
                suffix_array_file_stream->seekg(middle_anchor);
                suffix_array_file_stream->read((char *)&text_index, sizeof(text_index));

                auto distance = std::memcmp(substring.c_str(), &text_vector[text_index], substring.size());
                if (distance < 0) {
                    right_anchor = middle_anchor - 4;
                } else if (distance > 0) {
                    left_anchor = middle_anchor + 4;
                } else {
                    last_suffix_array_index = middle_anchor;
                    left_anchor = middle_anchor + 4;
                }
            }

            return std::make_tuple(
                first_suffix_array_index.value(),
                last_suffix_array_index.value()
            );
        }

        inline void search_entries_in_file(
            std::vector<std::string> & entries,
            const std::string & substring,
            std::uint32_t file_index
        ) {
            auto substring_positions = this->get_substring_positions(
                substring,
                file_index
            );
            if (!substring_positions.has_value()) {
                return;
            }
            auto [first_text_index, last_text_index] = substring_positions.value();

            auto number_of_text_indices = ((last_text_index - first_text_index) / 4) + 1;
            std::vector<std::int32_t> text_indices(number_of_text_indices);

            const auto & [suffix_array_file_stream, text_vector] = this->files[file_index];
            suffix_array_file_stream->seekg(first_text_index);
            suffix_array_file_stream->read(
                (char *)text_indices.data(),
                sizeof(std::int32_t) * number_of_text_indices
            );

            std::unordered_set<std::string> current_entries(number_of_text_indices);
            for (std::int32_t text_index : text_indices) {
                std::int32_t entry_start = text_index;
                for (; entry_start > 0; entry_start -= 1) {
                    if (text_vector[entry_start - 1] == '\0') {
                        break;
                    }
                }
                current_entries.emplace(&text_vector[entry_start]);
            }

            this->entries_lock.lock();
            entries.insert(entries.end(), current_entries.begin(), current_entries.end());
            this->entries_lock.unlock();
        }

        inline std::size_t count_entries_in_file(
            const std::string & substring,
            std::uint32_t file_index
        ) {
            auto substring_positions = this->get_substring_positions(
                substring,
                file_index
            );
            if (!substring_positions.has_value()) {
                return 0;
            }
            auto [first_text_index, last_text_index] = substring_positions.value();

            auto number_of_text_indices = ((last_text_index - first_text_index) / 4) + 1;
            std::vector<std::int32_t> text_indices(number_of_text_indices);

            const auto & [suffix_array_file_stream, text_vector] = this->files[file_index];
            suffix_array_file_stream->seekg(first_text_index);
            suffix_array_file_stream->read(
                (char *)text_indices.data(),
                sizeof(std::int32_t) * number_of_text_indices
            );

            std::unordered_set<std::int32_t> entries_start_indices(number_of_text_indices);
            for (std::int32_t text_index : text_indices) {
                std::int32_t entry_start = text_index;
                for (; entry_start > 0; entry_start -= 1) {
                    if (text_vector[entry_start - 1] == '\0') {
                        break;
                    }
                }
                entries_start_indices.emplace(entry_start);
            }

            return entries_start_indices.size();
        }

        std::vector<std::pair<std::shared_ptr<subifstream>, std::vector<char>>> files;
        std::mutex entries_lock;
};

PYBIND11_MODULE(pysubstringsearch, m) {
    pybind11::class_<Reader>(m, "Reader")
        .def(
            pybind11::init<std::string>(),
            "Reader object that handles searches over an index file",
            pybind11::arg("index_file_path")
        )
        .def(
            "search",
            &Reader::search,
            "search over an index file for a substring, returns list of distinct entries",
            pybind11::arg("substring")
        )
        .def(
            "count_occurrences",
            &Reader::count_occurrences,
            "search over an index file for a substring, returns the number of occurrences",
            pybind11::arg("substring")
        )
        .def(
            "count_entries",
            &Reader::count_entries,
            "search over an index file for a substring, returns the number of entries",
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
