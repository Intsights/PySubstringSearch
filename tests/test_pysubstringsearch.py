import unittest
import tempfile

import pysubstringsearch


class PySubstringSearchTestCase(
    unittest.TestCase,
):
    def assert_substring_search(
        self,
        strings,
        substring,
        expected_results,
    ):
        with tempfile.TemporaryDirectory() as tmp_directory:
            writer = pysubstringsearch.Writer(
                index_file_path=f'{tmp_directory}/output.idx',
            )
            for string in strings:
                writer.add_entry(
                    text=string,
                )
            writer.finalize()

            reader = pysubstringsearch.Reader(
                index_file_path=f'{tmp_directory}/output.idx',
            )
            self.assertCountEqual(
                first=reader.search_parallel(
                    substring=substring,
                ),
                second=expected_results,
            )
            self.assertCountEqual(
                first=reader.search_sequential(
                    substring=substring,
                ),
                second=expected_results,
            )

    def test_file_not_found(
        self,
    ):
        with self.assertRaises(
            expected_exception=RuntimeError,
        ):
            pysubstringsearch.Reader(
                index_file_path=f'missing_index_file_path',
            )

    def test_sanity(
        self,
    ):
        strings = [
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

        self.assert_substring_search(
            strings=strings,
            substring='four',
            expected_results=[
                'four',
            ],
        )

        self.assert_substring_search(
            strings=strings,
            substring='f',
            expected_results=[
                'four',
                'five',
            ],
        )

        self.assert_substring_search(
            strings=strings,
            substring='our',
            expected_results=[
                'four',
            ],
        )

        self.assert_substring_search(
            strings=strings,
            substring='aaa',
            expected_results=[],
        )

    def test_edgecases(
        self,
    ):
        strings = [
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

        self.assert_substring_search(
            strings=strings,
            substring='none',
            expected_results=[],
        )

        self.assert_substring_search(
            strings=strings,
            substring='one',
            expected_results=[
                'one',
            ],
        )

        self.assert_substring_search(
            strings=strings,
            substring='onet',
            expected_results=[],
        )

        self.assert_substring_search(
            strings=strings,
            substring='ten',
            expected_results=[
                    'ten',
                    'tenten',
                ],
        )

    def test_unicode(
        self,
    ):
        strings = [
            '诶比西',
        ]

        self.assert_substring_search(
            strings=strings,
            substring='诶',
            expected_results=[
                '诶比西',
            ],
        )

        self.assert_substring_search(
            strings=strings,
            substring='诶比',
            expected_results=[
                '诶比西',
            ],
        )

        self.assert_substring_search(
            strings=strings,
            substring='比诶',
            expected_results=[],
        )

        self.assert_substring_search(
            strings=strings,
            substring='none',
            expected_results=[],
        )

    def test_multiple_words_string(
        self,
    ):
        strings = [
            'some short string',
            'another but now a longer string',
            'more text to add',
        ]

        self.assert_substring_search(
            strings=strings,
            substring='short',
            expected_results=[
                'some short string',
            ],
        )
