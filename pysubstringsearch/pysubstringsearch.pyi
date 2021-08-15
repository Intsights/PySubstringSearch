import typing


class Writer:
    def __init__(
        self,
        index_file_path: str,
    ) -> None: ...

    def from_file_lines(
        self,
        index_file_path: str,
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