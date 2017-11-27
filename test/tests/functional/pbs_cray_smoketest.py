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


@tags('cray,smoke')
class TestCraySmokeTest(TestFunctional):

    """
    """

    def setUp(self):
        if not self.du.get_platform().startswith('cray'):
            self.skipTest("Test suite only meant to run on a Cray")
        TestFunctional.setUp(self)

    @tags('cray,smoke')
    def test_cray_apstat(self):
        """
        Check output of apstat -rn on Cray.
        The compute node summary should show that there are no nodes in use,
        held, or down.  Check the that the number of up nodes is the same as
        the number of available nodes.
        """
	cu = CrayUtils()
	self.assertTrue(cu.count_node_in_state('resv') is 0,
		        "No compute node should be having ALPS reservation")
	self.assertTrue(cu.count_node_in_state('use') is 0,
		        "No compute node should be in use")
	nodes_up_apstat = cu.count_node_in_state('up')
	nodes = self.server.status(NODE, {'resources_available.vntype' : 'cray_compute'})
	self.assertEqual(nodes_up_apstat, len(nodes))



    @tags('cray,smoke')
    def test_cray_pbsnodes(self):
        """
        Check pbsnodes -av output on a Cray system.
        pbsnodes output should show that nodes are free and resources are
        available.
        """
	nodes = self.server.status(NODE)
	for node in nodes:
	    self.assertEqual(node['state'], 'free')
	    self.assertEqual(node['resources_assigned.ncpus'], '0')
	    self.assertEqual(node['resources_assigned.mem'], '0kb')

    @tags('cray,smoke')
    def test_cray_login_job(self):
        """
        Submit a simple sleep job that requests to run on a login node
	and expect that job to go in running state on a login node.
        """
	j1 = Job(TEST_USER, {ATTR_l+'.vntype' : 'login'})
	# specifically create a job script because PTL framework
	# will create a job with aprun on cray but login nodes may not
	# have this command on them
	j1.create_script('sleep 10\n')
	jid1 = self.server.submit(j1)
	self.server.expect(JOB, {ATTR_state: 'R'}, id=jid1)
	# fetch node name where the job is running and check that the
	# node is a login node
        qstat = self.server.status(JOB, 'exec_vnode', id=jid1)
        vname = qstat[0]['exec_vnode'].partition(':')[0].strip('(')
        self.server.expect(NODE, {'resources_available.vntype': 'login'},
		           id=vname)

	cu = CrayUtils()
	#Check if number of compute nodes in use are 0
	self.assertEqual(cu.count_node_in_state('use'),0)

    @tags('cray,smoke')
    def test_cray_compute_job(self):
        """
	Submit a simple sleep job that runs on a compute node and 
	expect the job to go in running state on a compute node.
        """
	j1 = Job(TEST_USER)
	jid1 = self.server.submit(j1)
	self.server.expect(JOB, {ATTR_state: 'R'}, id=jid1)
	# fetch node name where the job is running and check that the
	# node is a compute node
        qstat = self.server.status(JOB, 'exec_vnode', id=jid1)
        vname = qstat[0]['exec_vnode'].partition(':')[0].strip('(')
        self.server.expect(NODE, {'resources_available.vntype': 'cray_compute'},
		           id=vname)

	cu = CrayUtils()
	#Check if number of compute nodes in use are 1
	self.assertEqual(cu.count_node_in_state('use'),1)
