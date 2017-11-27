# coding: utf-8

# Copyright (C) 1994-2017 Altair Engineering, Inc.
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
import os

class CrayUtils:

    """
    Cray specific utility class
    """
    node_status = []
    node_summary = {}
    def __init__(self):
        (self.node_status, self.node_summary) = self.parse_apstat()

    def parse_apstat(self, options='-rn'):
        cmd_run = os.popen('apstat' +" " + options)
        cmd_result = cmd_run.read()
        keys = cmd_result.split('\n')[0].split()
        status = []
        summary = {}
        count = 0
        cmd_iter = iter(cmd_result.split('\n'))
        for line in cmd_iter:
            if count == 0:
                count += 1
                continue
            if "Compute node summary" in line:
	        summary_line = next(cmd_iter)
                summary_keys = summary_line.split()
                summary_data = next(cmd_iter).split()
                sum_index=0
                for a in summary_keys:
                    summary[a] = summary_data[sum_index]
                    sum_index += 1
                break
            index = 0
            obj = {}
            for val in line.split():
                obj[keys[index]] = val
            status.append(obj)
        return (status, summary)

    def count_node_in_state(self, state='up'):
        return int(self.node_summary[state])
