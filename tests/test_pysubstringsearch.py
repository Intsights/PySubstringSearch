import os
import tempfile
import unittest

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
        try:
            with tempfile.TemporaryDirectory() as tmp_directory:
                index_file_path = f'{tmp_directory}/output.idx'
                writer = pysubstringsearch.Writer(
                    index_file_path=index_file_path,
                )
                for string in strings:
                    writer.add_entry(
                        text=string,
                    )
                writer.finalize()

                reader = pysubstringsearch.Reader(
                    index_file_path=index_file_path,
                )
                self.assertCountEqual(
                    first=reader.search(
                        substring=substring,
                    ),
                    second=expected_results,
                )

                try:
                    os.unlink(
                        path=index_file_path,
                    )
                except Exception:
                    pass
        except PermissionError:
            pass

    def test_file_not_found(
        self,
    ):
        with self.assertRaises(
            expected_exception=FileNotFoundError,
        ):
            pysubstringsearch.Reader(
                index_file_path='missing_index_file_path',
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
            'رجعوني عنيك لأيامي اللي راحوا',
            'علموني أندم على الماضي وجراحه',
            'اللي شفته قبل ما تشوفك عنيه',
            'عمر ضايع يحسبوه إزاي عليّ',
            'انت عمري اللي ابتدي بنورك صباحه',
            'قد ايه من عمري قبلك راح وعدّى',
            'يا حبيبي قد ايه من عمري راح',
            'ولا شاف القلب قبلك فرحة واحدة',
            'ولا داق في الدنيا غير طعم الجراح',
            'ابتديت دلوقت بس أحب عمري',
            'ابتديت دلوقت اخاف لا العمر يجري',
            'كل فرحه اشتاقها من قبلك خيالي',
            'التقاها في نور عنيك قلبي وفكري',
            'يا حياة قلبي يا أغلى من حياتي',
            'ليه ما قابلتش هواك يا حبيبي بدري',
            'اللي شفته قبل ما تشوفك عنيه',
            'عمر ضايع يحسبوه إزاي عليّ',
            'انت عمري اللي ابتدي بنورك صباحه',
            'الليالي الحلوه والشوق والمحبة',
            'من زمان والقلب شايلهم عشانك',
            'دوق معايا الحب دوق حبه بحبه',
            'من حنان قلبي اللي طال شوقه لحنانك',
            'هات عنيك تسرح في دنيتهم عنيه',
            'هات ايديك ترتاح للمستهم ايديه',
        ]

        self.assert_substring_search(
            strings=strings,
            substring='زمان',
            expected_results=[
                'من زمان والقلب شايلهم عشانك',
            ],
        )

        self.assert_substring_search(
            strings=strings,
            substring='في',
            expected_results=[
                'هات عنيك تسرح في دنيتهم عنيه',
                'التقاها في نور عنيك قلبي وفكري',
                'ولا داق في الدنيا غير طعم الجراح',
            ],
        )

        self.assert_substring_search(
            strings=strings,
            substring='حنان',
            expected_results=[
                'من حنان قلبي اللي طال شوقه لحنانك',
            ],
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

    def test_short_string(
        self,
    ):
        strings = [
            'ab',
        ]
        self.assert_substring_search(
            strings=strings,
            substring='a',
            expected_results=[
                'ab',
            ],
        )
