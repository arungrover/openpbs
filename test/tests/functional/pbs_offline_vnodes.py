# coding: utf-8

# Copyright (C) 1994-2018 Altair Engineering, Inc.
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
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.
# See the GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Commercial License Information:
#
# For a copy of the commercial license terms and conditions,
# go to: (http://www.pbspro.com/UserArea/agreement.html)
# or contact the Altair Legal Department.
#
# Altair’s dual-license business model allows companies, individuals, and
# organizations to create proprietary derivative works of PBS Pro and
# distribute them - whether embedded or bundled with other software -
# under a commercial license agreement.
#
# Use of Altair’s trademarks, including but not limited to "PBS™",
# "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's
# trademark licensing policies.

from tests.functional import *


class TestOfflineVnodeOnHookFailure(TestFunctional):
    """
    Tests if vnodes are marked offline if a hook fails
    """
    def setUp(self):
        if len(self.moms.values()) != 2:
            self.skipTest("Provide 2 moms while invoking test")
	if not self.du.get_platform().startswith('cray'):
	    self.is_cray = False
	else:
	    self.is_cray = True

	# The moms provided to the test may have unwanted vnodedef files.
        if self.moms.values()[0].has_vnode_defs():
            self.moms.values()[0].delete_vnode_defs()
        if self.moms.values()[1].has_vnode_defs():
            self.moms.values()[1].delete_vnode_defs()
	TestFunctional.setUp(self)

    def tearDown(self):
	self.server.manager(MGR_CMD_DELETE, NODE, id='@default')
	TestFunctional.tearDown(self)

    def create_mom_hook(self):
	name = "h1"
	body = ("import pbs\n"
		"import time\n"
		"time.sleep(60)\n"
		"pbs.event.accept()")
	attr = {'event':'execjob_begin', 'fail_action':'offline_vnodes', 'alarm':'3', 'enabled' : 'true'}
	self.server.create_import_hook(name, attr, body)

    def create_multi_vnodes(self, num_moms, opt):
	if self.is_cray is True:
	    return
	vn_attrs = {ATTR_rescavail + '.ncpus': 1,
                    ATTR_rescavail + '.mem': '1024mb'}
	for i in range(num_moms):
	    self.server.create_vnodes('vnode', vn_attrs, 3, self.moms.values()[i], usenatvnode=True, delall=opt, expect=False)


    def test_single_mom_hook_failure_affects_vnode(self):
	"""
	Run an execjob_begin hook that sleep for sometime,
	at the same time set an alarm value so less that
	the hook alarms out and server executes the fail_action
	After this check if vnodes are marked offline.
	In case of a single mom reporting vnodes, it should mark
	all the vnodes and mom as offline.
	"""
	self.create_multi_vnodes(1, True)
	self.create_mom_hook()
	self.server.manager(MGR_CMD_DELETE, NODE, id=self.moms.values()[1].shortname)

	j1 = Job(TEST_USER)
	self.server.expect(NODE, {ATTR_NODE_state: 'free'}, id=self.moms.values()[0].shortname)
	j1.set_sleep_time(1000)
	jid = self.server.submit(j1)
	self.server.manager(MGR_CMD_SET, SERVER, {'scheduling': '0'})
	self.server.expect(JOB, {ATTR_state: 'Q'}, id=jid)

	self.server.expect(NODE, {ATTR_NODE_state: 'offline'}, id=self.moms.values()[0].shortname)
	if self.is_cray is True:
	    vlist = []
	    vnl = self.server.filter(
			VNODE, {'resources_available.vntype': 'cray_compute'})
	    vlist = vnl["resources_available.vntype=cray_compute"]
	    # Loop through each compute vnode in the list and check if state = offline
	    for v1 in vlist:
		# Check that the vnode is in offline state
		self.server.expect(VNODE, {'state': 'offline'}, id=v1, max_attempts=3, interval=2)

	else: 
	    self.server.expect(NODE, {ATTR_NODE_state: 'offline'}, id='vnode[0]')
	    self.server.expect(NODE, {ATTR_NODE_state: 'offline'}, id='vnode[1]')

    def test_multi_mom_hook_failure_affects_vnode(self):
	"""
	Run an execjob_begin hook that sleep for sometime,
	at the same time set an alarm value so less that
	the hook alarms out and server executes the fail_action
	After this check if vnodes are marked offline.
	In case of a multiple mom reporting same set of vnodes, 
	it should not mark the vnodes and mom as offline because 
	there are other moms active and reporting same vnodes.
	"""
	self.create_multi_vnodes(2, False)
	self.create_mom_hook()

	# set one  natural node to have higher ncpus than the other one
	self.server.manager(MGR_CMD_SET, NODE, {ATTR_rescavail + '.ncpus':'256'}, id=self.moms.values()[0].shortname)
	self.server.manager(MGR_CMD_SET, NODE, {ATTR_rescavail + '.ncpus':'1'}, id=self.moms.values()[1].shortname)

	j1 = Job(TEST_USER)
	self.server.expect(NODE, {ATTR_NODE_state: 'free'}, id=self.moms.values()[0].shortname)
	self.server.expect(NODE, {ATTR_NODE_state: 'free'}, id=self.moms.values()[1].shortname)
        
	if self.is_cray is True:
	    # on a cray, make sure job runs on login node with higher number of ncpus
            j1.set_attributes({ATTR_l + '.select': '1:ncpus=256:vntype=cray_login'})
        else:
	    j1.set_attributes({ATTR_l + '.select': '1:ncpus=256'})

	jid = self.server.submit(j1)
	self.server.expect(JOB, {ATTR_state: 'Q'}, id=jid)
	
	self.server.expect(NODE, {ATTR_NODE_state: 'offline'}, id=self.moms.values()[0].shortname)
	self.server.expect(NODE, {ATTR_NODE_state: 'free'}, id=self.moms.values()[1].shortname)
	if self.is_cray is True:
	    vlist = []
	    vnl = self.server.filter(
			VNODE, {'resources_available.vntype': 'cray_compute'})
	    vlist = vnl["resources_available.vntype=cray_compute"]
	    # Loop through each compute vnode in the list and check if state = free
	    for v1 in vlist:
		# Check that the node is in free state
		self.server.expect(VNODE, {'state': 'free'}, id=v1, max_attempts=3, interval=2)

	else:
	    self.server.expect(NODE, {ATTR_NODE_state: 'free'}, id='vnode[0]')
	    self.server.expect(NODE, {ATTR_NODE_state: 'free'}, id='vnode[1]')

	pbsnodes_reset = os.path.join(self.server.pbs_conf['PBS_EXEC'], 'bin', 'pbsnodes') + ' -r ' + self.moms.values()[0].shortname
        self.du.run_cmd(self.server.hostname, cmd=pbsnodes_reset, sudo=True)
	self.server.expect(NODE, {ATTR_NODE_state: 'free'}, id=self.moms.values()[0].shortname)
	self.server.manager(MGR_CMD_SET, HOOK, {'enabled':'False'},id="h1")
	self.server.manager(MGR_CMD_SET, SCHED, {'scheduling':'True'})
	self.server.expect(JOB, {ATTR_state: 'R'}, id=jid)
