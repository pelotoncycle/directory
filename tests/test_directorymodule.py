import os.path
from unittest import TestCase

from directory import Directory


PATH = os.path.join(os.path.dirname(__file__), 'data')


class TestDirectory(TestCase):
    maxDiff = None
    def test_open_with_path(self):
        directory = Directory(PATH)
        directory_iter = iter(directory) 
        directory_elements =  list(directory_iter)
        
        self.assertItemsEqual(
            [e.d_name() for e in directory_elements],
            ['.', '..', 'd2', 'd1', 'README'])

    def test_directory_entries_open_new_directory(self):
        elements = {e.d_name(): e for e in Directory(PATH)}
        self.assertIsNone(elements['README'].directory());
        self.assertItemsEqual(
            [e.d_name() for e in elements['d2'].directory()],
            ['.', '..', '2.txt', '3.txt'])


        
        
