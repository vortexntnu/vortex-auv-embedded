import unittest


class TestFullstackSmoke(unittest.TestCase):
	def test_import(self):
		# Import should not execute the pipeline.
		import fullstack_prototype  # noqa: F401


if __name__ == '__main__':
	unittest.main()