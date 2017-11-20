# coding: utf-8

# Copyright (C) 1994-2016 Altair Engineering, Inc.
# For more information, contact Altair at www.altair.com.
#
# This file is part of the PBS Professional ("PBS Pro") software.
#
# Open Source License Information:
#
# PBS Pro is free software. You can redistribute it and/or modify it under the
# terms of the GNU Affero General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
# details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# Commercial License Information:
#
# The PBS Pro software is licensed under the terms of the GNU Affero General
# Public License agreement ("AGPL"), except where a separate commercial license
# agreement for PBS Pro version 14 or later has been executed in writing with
# Altair.
#
# Altair’s dual-license business model allows companies, individuals, and
# organizations to create proprietary derivative works of PBS Pro and
# distribute them - whether embedded or bundled with other software - under
# a commercial license agreement.
#
# Use of Altair’s trademarks, including but not limited to "PBS™",
# "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's
# trademark licensing policies.

from tests.functional import *
from ptl.utils.pbs_crayutils import CrayUtils


@tags('cray')
class TestCraySmokeTest(TestFunctional):

    """
    """

    def setUp(self):
        if not self.du.get_platform().startswith('cray'):
            self.skipTest("Test suite only meant to run on a Cray")
        TestFunctional.setUp(self)
    @tags('craysmoke')
    def test_cray_qstat(self):
        """
        Check if qstat -Bf reports the right data on Cray
        """

    @tags('craysmoke')
    def test_cray_apstat(self):
        """
        Check output of apstat -rn on Cray.
        The compute node summary should show that there are no nodes in use,
        held, or down.  Check the that the number of up nodes is the same as
        the number of available nodes.
        """
	cu = CrayUtils()
	self.assertTrue(cu.count_node_in_state('resv') is '0',
		        "No compute node should be in use")



    @tags('craysmoke')
    def test_cray_pbsnodes(self):
        """
        Check pbsnodes -av output on a Cray system.
        pbsnodes output should show that nodes are free and resources are
        available.
        """

    @tags('craysmoke')
    def test_cray_simple_job(self):
        """
        Submit a simple sleep job and expect that to go in running state.
        """

    @tags('craysmoke')
    def test_cray_multiple_MPI_jobs(self):
        """
        Test multiple MPI jobs that run one after another.
        """
