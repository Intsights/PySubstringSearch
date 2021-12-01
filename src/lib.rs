use ahash::AHashSet;
use bstr::io::BufReadExt;
use byteorder::{ReadBytesExt, WriteBytesExt, ByteOrder, LittleEndian};
use memchr::memmem;
use parking_lot::Mutex;
use pyo3::exceptions;
use pyo3::prelude::*;
use rayon::prelude::*;
use std::fs::File;
use std::io::{BufReader, BufWriter, Read, Seek, SeekFrom, Write};
use std::sync::Arc;
use std::str;

extern "C" {
    pub fn libsais(
        data: *const u8,
        suffix_array: *mut i32,
        data_len: i32,
        suffix_array_extra_space: i32,
        symbol_frequency_table: *mut i32,
    ) -> i32;
}

fn construct_suffix_array(
    buffer: &[u8],
) -> Vec<i32> {
    unsafe {
        let mut suffix_array: Vec<i32> = Vec::with_capacity(buffer.len());
        suffix_array.set_len(buffer.len());

        libsais(
            buffer.as_ptr(),
            suffix_array.as_mut_ptr(),
            buffer.len() as i32,
            0,
            std::ptr::null_mut::<i32>(),
        );

        suffix_array
    }
}

#[pyclass]
struct Writer {
    index_file: BufWriter<File>,
    buffer: Vec<u8>,
}

#[pymethods]
impl Writer {
    #[new]
    fn new(
        index_file_path: &str,
        max_chunk_len: Option<usize>,
    ) -> PyResult<Self> {
        let index_file = File::create(index_file_path)?;
        let index_file = BufWriter::new(index_file);
        let max_chunk_len = max_chunk_len.unwrap_or(512 * 1024 * 1024);

        Ok(
            Writer {
                index_file,
                buffer: Vec::with_capacity(max_chunk_len),
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
                if self.buffer.len() + line.len() + 1 > self.buffer.capacity() {
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
        if text.len() > self.buffer.capacity() {
            return Err(exceptions::PyValueError::new_err("entry is too big"));
        }

        if self.buffer.len() + text.len() + 1 > self.buffer.capacity() {
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

        self.index_file.write_u32::<LittleEndian>(self.buffer.len() as u32)?;
        self.index_file.write_all(&self.buffer)?;

        let suffix_array = construct_suffix_array(&self.buffer);
        self.index_file.write_u32::<LittleEndian>((suffix_array.len() * 4) as u32)?;
        for suffix in suffix_array {
            self.index_file.write_i32::<LittleEndian>(suffix)?;
        }

        self.buffer.clear();

        Ok(())
    }

    fn finalize(
        &mut self,
    ) -> PyResult<()> {
        if !self.buffer.is_empty() {
            self.dump_data()?;
        }
        self.index_file.flush()?;

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
    index_file: BufReader<File>,
    suffixes_file_start: usize,
    suffixes_file_end: usize,
    finder: memmem::Finder<'static>,
    finder_rev: memmem::FinderRev<'static>,
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
        let index_file = File::open(index_file_path)?;
        let mut index_file = BufReader::new(index_file);
        let index_file_metadata = std::fs::metadata(index_file_path)?;
        let index_file_len = index_file_metadata.len();
        let mut bytes_read = 0;

        let mut sub_indexes = Vec::new();

        while bytes_read < index_file_len {
            let data_file_len = index_file.read_u32::<LittleEndian>()?;
            let mut data = Vec::with_capacity(data_file_len as usize);
            unsafe { data.set_len(data_file_len as usize) };
            index_file.read_exact(&mut data)?;

            let suffixes_file_len = index_file.read_u32::<LittleEndian>()? as usize;
            let suffixes_file_start = index_file.seek(SeekFrom::Current(0))? as usize;
            let suffixes_file_end = suffixes_file_start + suffixes_file_len;
            index_file.seek(SeekFrom::Current(suffixes_file_len as i64))?;

            bytes_read += 4 + 4 + data_file_len as u64 + suffixes_file_len as u64;

            sub_indexes.push(
                SubIndex {
                    data,
                    index_file: BufReader::new(File::open(index_file_path)?),
                    suffixes_file_start,
                    suffixes_file_end,
                    finder: memmem::Finder::new(b"\n"),
                    finder_rev: memmem::FinderRev::new(b"\n"),
                }
            );
        }

        Ok(Reader { sub_indexes })
    }

    fn search(
        &mut self,
        substring: &str,
    ) -> PyResult<Vec<&str>> {
        let results = Arc::new(Mutex::new(Vec::new()));

        self.sub_indexes.par_iter_mut().for_each(
            |sub_index| {
                let mut start_of_indices = None;
                let mut end_of_indices = None;

                let mut left_anchor = sub_index.suffixes_file_start;
                let mut right_anchor = sub_index.suffixes_file_end - 4;
                while left_anchor <= right_anchor {
                    let middle_anchor = left_anchor + ((right_anchor - left_anchor) / 4 / 2 * 4);
                    sub_index.index_file.seek(SeekFrom::Start(middle_anchor as u64)).unwrap();
                    let data_index = sub_index.index_file.read_i32::<LittleEndian>().unwrap();

                    let line = &sub_index.data[data_index as usize..];
                    if line.starts_with(substring.as_bytes()) {
                        start_of_indices = Some(middle_anchor);
                        right_anchor = middle_anchor - 4;
                    } else {
                        match substring.as_bytes().cmp(line) {
                            std::cmp::Ordering::Less => right_anchor = middle_anchor - 4,
                            std::cmp::Ordering::Greater => left_anchor = middle_anchor + 4,
                            std::cmp::Ordering::Equal => {},
                        };
                    }
                }
                if start_of_indices.is_none() {
                    return;
                }

                let mut right_anchor = sub_index.suffixes_file_end - 4;
                while left_anchor <= right_anchor {
                    let middle_anchor = left_anchor + ((right_anchor - left_anchor) / 4 / 2 * 4);
                    sub_index.index_file.seek(SeekFrom::Start(middle_anchor as u64)).unwrap();
                    let data_index = sub_index.index_file.read_i32::<LittleEndian>().unwrap();

                    let line = &sub_index.data[data_index as usize..];
                    if line.starts_with(substring.as_bytes()) {
                        end_of_indices = Some(middle_anchor);
                        left_anchor = middle_anchor + 4;
                    } else {
                        match substring.as_bytes().cmp(line) {
                            std::cmp::Ordering::Less => right_anchor = middle_anchor - 4,
                            std::cmp::Ordering::Greater => left_anchor = middle_anchor + 4,
                            std::cmp::Ordering::Equal => {},
                        };
                    }
                }

                let start_of_indices = start_of_indices.unwrap();
                let end_of_indices = end_of_indices.unwrap();

                let mut suffixes = Vec::with_capacity(end_of_indices - start_of_indices + 4);
                unsafe { suffixes.set_len(end_of_indices - start_of_indices + 4) };

                sub_index.index_file.seek(SeekFrom::Start(start_of_indices as u64)).unwrap();
                sub_index.index_file.read_exact(&mut suffixes).unwrap();

                let mut matches_ranges = AHashSet::new();
                let mut local_results = Vec::with_capacity((end_of_indices - start_of_indices + 4) / 4);
                for suffix in suffixes.chunks_mut(4) {
                    let data_index = LittleEndian::read_i32(suffix);
                    let line_head = match sub_index.finder.find(&sub_index.data[data_index as usize..]) {
                        Some(next_nl_pos) => data_index as usize + next_nl_pos,
                        None => sub_index.data.len() - 1,
                    };
                    let line_tail = match sub_index.finder_rev.rfind(&sub_index.data[..data_index as usize]) {
                        Some(previous_nl_pos) => previous_nl_pos + 1,
                        None => 0,
                    };
                    if matches_ranges.insert(line_tail) {
                        let line = unsafe { str::from_utf8_unchecked(&sub_index.data[line_tail..line_head]) };
                        local_results.push(line);
                    }
                }

                results.lock().extend(local_results);
            }
        );

        let results = results.lock().to_vec();

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
