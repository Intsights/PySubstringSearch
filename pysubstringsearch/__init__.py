import typing

from . import pysubstringsearch


class Writer:
    def __init__(
        self,
        index_file_path: str,
        max_chunk_len: typing.Optional[int] = None,
    ) -> None:
        self.writer = pysubstringsearch.Writer(
            index_file_path=index_file_path,
            max_chunk_len=max_chunk_len,
        )

    def add_entries_from_file_lines(
        self,
        input_file_path: str,
    ) -> None:
        self.writer.add_entries_from_file_lines(
            input_file_path=input_file_path,
        )

    def add_entry(
        self,
        text: str,
    ) -> None:
        self.writer.add_entry(
            text=text,
        )

    def dump_data(
        self,
    ) -> None:
        self.writer.dump_data()

    def finalize(
        self,
    ) -> None:
        self.writer.finalize()


class Reader:
    def __init__(
        self,
        index_file_path: str,
    ) -> None:
        self.reader = pysubstringsearch.Reader(
            index_file_path=index_file_path,
        )

    def search(
        self,
        substring: str,
    ) -> typing.List[str]:
        return self.reader.search(
            substring=substring,
        )

    def search_multiple(
        self,
        substrings: typing.List[str],
    ) -> typing.List[str]:
        results = []
        for substring in substrings:
            results.extend(
                self.search(
                    substring=substring,
                ),
            )

        return results
