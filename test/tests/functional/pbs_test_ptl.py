
from tests.functional import *


class Test_ptl(TestFunctional):
    """
    Test suite to test run_count attribute of a job.
    """

    def test_run_count_overflow(self):
        """
        Submit a job that requests run count exceeding 64 bit integer limit
        and see that such a job gets rejected.
        """
	print self.servers
