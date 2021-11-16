use ar::{Builder, Archive, Header};
use bstr::ByteSlice;
use bstr::io::BufReadExt;
use byteorder::{ReadBytesExt, LittleEndian};
use pyo3::exceptions;
use pyo3::prelude::*;
use rayon::prelude::*;
use std::fs::File;
use std::io::{BufReader, Read, Seek, SeekFrom};
use std::sync::{Arc, Mutex};

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("pysubstringsearch/src/msufsort.h");
        unsafe fn calculate_suffix_array(
            input: &[u8],
            output: &mut [i32]
        );
    }
}

#[pyclass]
struct Writer {
    index_file: Builder<File>,
    buffer: Vec<u8>,
    max_chunk_len: usize,
    number_of_index_files: usize,
}

#[pymethods]
impl Writer {
    #[new]
    fn new(
        index_file_path: &str,
        max_chunk_len: Option<usize>,
    ) -> PyResult<Self> {
        let index_file = File::create(index_file_path)?;
        let max_chunk_len = max_chunk_len.unwrap_or(512 * 1024 * 1024);

        Ok(
            Writer {
                index_file: Builder::new(index_file),
                buffer: Vec::with_capacity(max_chunk_len),
                max_chunk_len,
                number_of_index_files: 0,
            }
        )
    }

    fn add_entries_from_file_lines(
        &mut self,
        input_file_path: &str,
    ) -> PyResult<()> {
        let input_file = File::open(input_file_path)?;
        let input_file_reader = BufReader::new(input_file);
        input_file_reader.for_byte_line(
            |line| {
                if self.buffer.len() + line.len() + 1 > self.max_chunk_len {
                    self.dump_data()?;
                }
                self.buffer.extend_from_slice(line);
                self.buffer.push(b'\n');

                Ok(true)
            }
        )?;

        Ok(())
    }

    fn add_entry(
        &mut self,
        text: &str,
    ) -> PyResult<()> {
        if text.len() > self.max_chunk_len {
            return Err(exceptions::PyValueError::new_err("entry is too big"));
        }

        if self.buffer.len() + text.len() + 1 > self.max_chunk_len {
            self.dump_data()?;
        }
        self.buffer.extend_from_slice(text.as_bytes());
        self.buffer.push(b'\n');

        Ok(())
    }

    fn dump_data(
        &mut self,
    ) -> PyResult<()> {
        if self.buffer.is_empty() {
            return Ok(());
        }

        let text_file_name = format!("text_{}", self.number_of_index_files);
        self.index_file.append(
            &Header::new(
                text_file_name.as_bytes().to_vec(),
                self.buffer.len() as u64,
            ),
            self.buffer.as_slice(),
        )?;


        let mut suffix_array: Vec<i32> = Vec::with_capacity(self.buffer.len() + 1);
        unsafe {
            suffix_array.set_len(self.buffer.len() + 1);
            ffi::calculate_suffix_array(
                self.buffer.as_slice(),
                suffix_array.as_mut_slice(),
            );
        }

        let suffix_file_name = format!("suffix_array_{}", self.number_of_index_files);
        unsafe {
            self.index_file.append(
                &Header::new(
                    suffix_file_name.as_bytes().to_vec(),
                    ((self.buffer.len() + 1) * std::mem::size_of::<i32>()) as u64,
                ),
                std::slice::from_raw_parts(
                    suffix_array.as_ptr() as *const u8,
                    suffix_array.len() * std::mem::size_of::<i32>(),
                ),
            )?;
        }

        self.number_of_index_files += 1;
        self.buffer.clear();

        Ok(())
    }

    fn finalize(
        &mut self,
    ) -> PyResult<()> {
        if !self.buffer.is_empty() {
            self.dump_data()?;
        }

        Ok(())
    }
}

impl Drop for Writer {
    fn drop(
        &mut self,
    ) {
        self.finalize().unwrap();
    }
}

struct SubIndex {
    data: Vec<u8>,
    archive_file: Archive<File>,
    suffixes_file_index: usize,
}

#[pyclass]
struct Reader {
    sub_indexes: Vec<SubIndex>,
}

#[pymethods]
impl Reader {
    #[new]
    fn new(
        index_file_path: &str,
    ) -> PyResult<Self> {
        let mut sub_indexes = Vec::new();

        let mut archive_file = Archive::new(File::open(index_file_path)?);
        let number_of_files = archive_file.count_entries()? / 2;

        for i in 0..number_of_files {
            let mut archive_file = Archive::new(File::open(index_file_path)?);
            let mut data_file = archive_file.jump_to_entry(i * 2)?;
            let mut data = Vec::new();
            data_file.read_to_end(&mut data)?;

            sub_indexes.push(
                SubIndex {
                    data,
                    archive_file: Archive::new(File::open(index_file_path)?),
                    suffixes_file_index: i + 1,
                }
            );
        }

        Ok(Reader { sub_indexes })
    }

    fn search(
        &mut self,
        substring: &str,
    ) -> PyResult<Vec<String>> {
        let results = Arc::new(Mutex::new(Vec::new()));

        self.sub_indexes.par_iter_mut().for_each(
            |sub_index| {
                let mut current_suffix_array_index = None;

                let mut suffixes_file = sub_index.archive_file.jump_to_entry(sub_index.suffixes_file_index).unwrap();
                let mut left_anchor: usize = 0;
                let mut right_anchor: usize = suffixes_file.header().size() as usize;
                while left_anchor <= right_anchor {
                    let middle_anchor = left_anchor + ((right_anchor - left_anchor) / 4 / 2 * 4);
                    suffixes_file.seek(SeekFrom::Start(middle_anchor as u64)).unwrap();
                    let data_index = suffixes_file.read_i32::<LittleEndian>().unwrap();

                    let newline_position = match sub_index.data[data_index as usize..].find(b"\n") {
                        Some(newline_position) => data_index as usize + newline_position,
                        None => sub_index.data.len() - 1,
                    };
                    let line = &sub_index.data[data_index as usize..newline_position];

                    if line.starts_with(substring.as_bytes()) {
                        current_suffix_array_index = Some(middle_anchor);
                        right_anchor = middle_anchor - 4;
                    } else {
                        match substring.as_bytes().cmp(line) {
                            std::cmp::Ordering::Less => right_anchor = middle_anchor - 4,
                            std::cmp::Ordering::Greater => left_anchor = middle_anchor + 4,
                            std::cmp::Ordering::Equal => {
                                current_suffix_array_index = Some(middle_anchor);
                                right_anchor = middle_anchor - 4;
                            },
                        };
                    }
                }

                let mut matches_ranges = Vec::new();
                if let Some(current_index) = current_suffix_array_index {
                    let mut current_index = current_index;
                    let suffixes_file_len = suffixes_file.seek(SeekFrom::End(0)).unwrap() as usize;
                    loop {
                        suffixes_file.seek(SeekFrom::Start(current_index as u64)).unwrap();
                        let data_index = suffixes_file.read_i32::<LittleEndian>().unwrap();
                        let line_head = match sub_index.data[data_index as usize..].find(b"\n") {
                            Some(next_nl_pos) => data_index as usize + next_nl_pos,
                            None => sub_index.data.len() - 1,
                        };
                        let line_tail = match sub_index.data[..data_index as usize].rfind(b"\n") {
                            Some(previous_nl_pos) => previous_nl_pos + 1,
                            None => 0,
                        };
                        let line = &sub_index.data[line_tail..line_head];
                        if !line.contains_str(substring.as_bytes()) {
                            break;
                        }
                        if !matches_ranges.contains(&(line_tail, line_head)) {
                            results.lock().unwrap().push(line.to_str_lossy().to_string());
                            matches_ranges.push((line_tail, line_head));
                        }
                        current_index += 4;
                        if current_index >= suffixes_file_len {
                            break;
                        }
                    }
                }
            }
        );

        let results = results.lock().unwrap().to_vec();

        Ok(results)
    }
}

#[pymodule]
fn pysubstringsearch(
    _py: Python,
    m: &PyModule,
) -> PyResult<()> {
    m.add_class::<Writer>()?;
    m.add_class::<Reader>()?;

    Ok(())
}
