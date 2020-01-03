import unittest
import tempfile

import pysubstringsearch


class PySubstringSearchTestCase(
    unittest.TestCase,
):
    def test_sanity(
        self,
    ):
        with tempfile.TemporaryDirectory() as tmp_directory:
            domains = [
                'one',
                'two',
                'three',
                'four',
                'five',
                'six',
                'seven',
                'eight',
                'nine',
                'ten',
            ]

            index_file_path = f'{tmp_directory}/output.idx'
            writer = pysubstringsearch.Writer(
                index_file_path=index_file_path,
            )
            for domain in domains:
                writer.add_entry(
                    text=domain,
                )
            writer.finalize()

            reader = pysubstringsearch.Reader(
                index_file_path=index_file_path,
            )

            results = reader.search(
                substring='four',
            )
            self.assertCountEqual(
                first=results,
                second=[
                    'four',
                ],
            )

            results = reader.search(
                substring='f',
            )
            self.assertCountEqual(
                first=results,
                second=[
                    'four',
                    'five',
                ],
            )

            results = reader.search(
                substring='our',
            )
            self.assertCountEqual(
                first=results,
                second=[
                    'four',
                ],
            )

            results = reader.search(
                substring='aaa',
            )
            self.assertCountEqual(
                first=results,
                second=[],
            )

    def test_edgecases(
        self,
    ):
        with tempfile.TemporaryDirectory() as tmp_directory:
            domains = [
                'one',
                'two',
                'three',
                'four',
                'five',
                'six',
                'seven',
                'eight',
                'nine',
                'ten',
                'tenten',
            ]

            index_file_path = f'{tmp_directory}/output.idx'
            writer = pysubstringsearch.Writer(
                index_file_path=index_file_path,
            )
            for domain in domains:
                writer.add_entry(
                    text=domain,
                )
            writer.finalize()

            reader = pysubstringsearch.Reader(
                index_file_path=index_file_path,
            )

            results = reader.search(
                substring='none',
            )
            self.assertCountEqual(
                first=results,
                second=[],
            )

            results = reader.search(
                substring='one',
            )
            self.assertCountEqual(
                first=results,
                second=[
                    'one',
                ],
            )

            results = reader.search(
                substring='onet',
            )
            self.assertCountEqual(
                first=results,
                second=[],
            )

            results = reader.search(
                substring='ten',
            )
            self.assertCountEqual(
                first=results,
                second=[
                    'ten',
                    'tenten',
                ],
            )

    def test_unicode(
        self,
    ):
        with tempfile.TemporaryDirectory() as tmp_directory:
            domains = [
                '诶比西',
            ]

            index_file_path = f'{tmp_directory}/output.idx'
            writer = pysubstringsearch.Writer(
                index_file_path=index_file_path,
            )
            for domain in domains:
                writer.add_entry(
                    text=domain,
                )
            writer.finalize()

            reader = pysubstringsearch.Reader(
                index_file_path=index_file_path,
            )

            results = reader.search(
                substring='诶',
            )
            self.assertCountEqual(
                first=results,
                second=[
                    '诶比西',
                ],
            )

            results = reader.search(
                substring='诶比',
            )
            self.assertCountEqual(
                first=results,
                second=[
                    '诶比西',
                ],
            )

            results = reader.search(
                substring='比诶',
            )
            self.assertCountEqual(
                first=results,
                second=[],
            )

            results = reader.search(
                substring='none',
            )
            self.assertCountEqual(
                first=results,
                second=[],
            )

    def test_multiple_words_string(
        self,
    ):
        with tempfile.TemporaryDirectory() as tmp_directory:
            index_file_path = f'{tmp_directory}/output.idx'
            writer = pysubstringsearch.Writer(
                index_file_path=index_file_path,
            )
            writer.add_entry('some short string')
            writer.add_entry('another but now a longer string')
            writer.add_entry('more text to add')
            writer.finalize()

            reader = pysubstringsearch.Reader(
                index_file_path=index_file_path,
            )

            results = reader.search(
                substring='short',
            )
            self.assertCountEqual(
                first=results,
                second=[
                    'some short string',
                ],
            )
