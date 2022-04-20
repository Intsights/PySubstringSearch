import typing


class Writer:
    def __init__(
        self,
        index_file_path: str,
        max_chunk_len: typing.Optional[int] = None,
    ) -> None: ...

    def add_entries_from_file_lines(
        self,
        input_file_path: str,
    ) -> None: ...

    def add_entry(
        self,
        text: str,
    ) -> None: ...

    def dump_data(
        self,
    ) -> None: ...

    def finalize(
        self,
    ) -> None: ...


class Reader:
    def __init__(
        self,
        index_file_path: str,
    ) -> None: ...

    def search(
        self,
        substring: str,
    ) -> typing.List[str]: ...

    def search_multiple(
        self,
        substrings: typing.List[str],
    ) -> typing.List[str]: ...
