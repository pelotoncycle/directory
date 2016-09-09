import os.path
from unittest import TestCase

from directory import Directory


PATH = os.path.join(os.path.dirname(__file__), 'data')


class TestDirectory(TestCase):
    maxDiff = None

    def test_directory_path(self):
        self.assertEquals(Directory(PATH).path(), PATH)

    def test_open_with_path(self):
        directory = Directory(PATH)
        directory_iter = iter(directory)
        directory_elements = list(directory_iter)
        self.assertItemsEqual(
            [e.d_name() for e in directory_elements],
            ['.', '..', 'd2', 'd1', 'README'])

    def test_directory_entries_open_new_directory(self):
        elements = {e.d_name(): e for e in Directory(PATH)}
        self.assertIsNone(elements['README'].directory())
        self.assertItemsEqual(
            [e.d_name() for e in elements['d2'].directory()],
            ['.', '..', '2.txt', '3.txt'])

    def test_directory_open(self):
        self.assertEquals(
            Directory(PATH).open('README').read(),
            open(os.path.join(PATH, 'README')).read())

    def test_entry_path(self):
        elements = {e.d_name(): e for e in Directory(PATH)}
        self.assertIsNotNone(elements['d2'].directory())
        for e in elements['d2'].directory():
            self.assertEquals(e.path(), os.path.join(PATH, "d2", e.d_name()))

    def test_entry_open(self):
        elements = {e.d_name(): e for e in Directory(PATH)}
        elements = {e.d_name(): e for e in elements['d1'].directory()}
        self.assertEquals(
            elements['1.txt'].open().read(),
            open(os.path.join(PATH, 'd1', '1.txt')).read())
        
