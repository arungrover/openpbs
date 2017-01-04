/*
 * Copyright (C) 1994-2016 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *  
 * This file is part of the PBS Professional ("PBS Pro") software.
 * 
 * Open Source License Information:
 *  
 * PBS Pro is free software. You can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free 
 * Software Foundation, either version 3 of the License, or (at your option) any 
 * later version.
 *  
 * PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.
 *  
 * You should have received a copy of the GNU Affero General Public License along 
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 * Commercial License Information: 
 * 
 * The PBS Pro software is licensed under the terms of the GNU Affero General 
 * Public License agreement ("AGPL"), except where a separate commercial license 
 * agreement for PBS Pro version 14 or later has been executed in writing with Altair.
 *  
 * Altair’s dual-license business model allows companies, individuals, and 
 * organizations to create proprietary derivative works of PBS Pro and distribute 
 * them - whether embedded or bundled with other software - under a commercial 
 * license agreement.
 * 
 * Use of Altair’s trademarks, including but not limited to "PBS™", 
 * "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's 
 * trademark licensing policies.
 *
 */
/*
 * The entry point function for pbs_daemon.
 *
 * Included public functions re:
 *
 *	main	initialization and main loop of pbs_daemon
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <sys/stat.h>

#include "pbs_ifl.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <io.h>
#include <windows.h>
#include "win.h"

#include "list_link.h"
#include "work_task.h"
#include "log.h"
#include "server_limits.h"
#include "attribute.h"
#include "job.h"
#include "reservation.h"
#include "credential.h"
#include "ticket.h"
#include "queue.h"
#include "server.h"
#include "libpbs.h"
#include "net_connect.h"
#include "batch_request.h"
#include "pbs_nodes.h"
#include "svrfunc.h"
#include "tracking.h"
#include "acct.h"
#include "sched_cmds.h"
#include "rpp.h"
#include "dis.h"
#include "dis_init.h"
#include "pbs_ifl.h"
#include "pbs_license.h"
#include "resource.h"
#include "pbs_version.h"
#include "hook.h"
#include "pbs_python.h"
#include "provision.h"
#include "pbs_db.h"



/* Global Data Items */
char	       *acct_file = (char *)0;
char	       *acctlog_spacechar = (char *)0;
int		license_cpus;
char	       *log_file  = (char *)0;
char	       *path_acct;
char	        path_log[MAXPATHLEN+1];
char	       *path_priv = NULL;
char	       *path_jobs = NULL;
char	       *path_rescdef = NULL;
char	       *path_users = NULL;
char	       *path_resvs = NULL;
char	       *path_queues = NULL;
char	       *path_spool = NULL;
char	       *path_svrdb = NULL;
char	       *path_svrdb_new = NULL;
char	       *path_scheddb = NULL;
char	       *path_scheddb_new = NULL;
char 	       *path_track = NULL;
char	       *path_nodes = NULL;
char	       *path_nodes_new = NULL;
char	       *path_nodestate = NULL;
char	       *path_hooks;
char	       *path_hooks_rescdef;
char	       *path_hooks_tracking;
char	       *path_hooks_workdir;
char           *path_secondaryact;
char	       *pbs_o_host = "PBS_O_HOST";
pbs_net_t	pbs_mom_addr = 0;
unsigned int	pbs_mom_port = 0;
unsigned int	pbs_rm_port = 0;
pbs_net_t	pbs_scheduler_addr = 0;
unsigned int	pbs_scheduler_port = 0;
pbs_net_t	pbs_server_addr = 0;
unsigned int	pbs_server_port_dis = 0;
struct server	server;		/* the server structure */
struct sched	scheduler;	/* the sched structure */
char	        server_host[PBS_MAXHOSTNAME+1];	/* host_name  */
char	        primary_host[PBS_MAXHOSTNAME+1]; /* host_name of primary */
int		shutdown_who;		/* see req_shutdown() */
char	       *mom_host = server_host;
long		new_log_event_mask = 0;
int	 	server_init_type = RECOV_WARM;
char	        server_name[PBS_MAXSERVERNAME+1]; /* host_name[:service|port] */
int		svr_delay_entry = 0;
int		svr_do_schedule = SCH_SCHEDULE_NULL;
int		svr_do_sched_high = SCH_SCHEDULE_NULL;
int		svr_total_cpus = 0;		/* total number of cpus on nodes   */
int		have_blue_gene_nodes = 0;
int		svr_ping_rate = 300;	/* time between sets of node pings */
int		svr_unsent_qrun_req = 0;
long		svr_history_enable = 0;
long		svr_history_duration = 0;
pbs_db_conn_t	*svr_db_conn;
char		*path_svrlive;
char		*pbs_server_name;
char		*pbs_server_id; /* pbs server database id */

struct license_block licenses;
struct license_used  usedlicenses;
struct resc_sum *svr_resc_sum;
attribute      *pbs_float_lic;
pbs_list_head	svr_queues;            /* list of queues                   */
pbs_list_head	svr_alljobs;           /* list of all jobs in server       */
pbs_list_head	svr_newjobs;           /* list of incomming new jobs       */
pbs_list_head	svr_allresvs;          /* all reservations in server */
pbs_list_head	svr_newresvs;          /* temporary list for new resv jobs */
pbs_list_head	svr_allhooks;	       /* list of all hooks in server       */
pbs_list_head	svr_queuejob_hooks;
pbs_list_head	svr_modifyjob_hooks;
pbs_list_head	svr_resvsub_hooks;
pbs_list_head	svr_movejob_hooks;
pbs_list_head	svr_runjob_hooks;
pbs_list_head	svr_periodic_hooks;
pbs_list_head	svr_provision_hooks;
pbs_list_head	svr_execjob_begin_hooks;
pbs_list_head	svr_execjob_prologue_hooks;
pbs_list_head	svr_execjob_launch_hooks;
pbs_list_head	svr_execjob_epilogue_hooks;
pbs_list_head	svr_execjob_end_hooks;
pbs_list_head	svr_execjob_preterm_hooks;
pbs_list_head	svr_exechost_periodic_hooks;
pbs_list_head	svr_exechost_startup_hooks;
pbs_list_head	svr_execjob_attach_hooks;
pbs_list_head	task_list_immed;
pbs_list_head	task_list_timed;
pbs_list_head	task_list_event;
pbs_list_head   	svr_deferred_req;
pbs_list_head   	svr_unlicensedjobs;	/* list of jobs to license */
time_t		time_now;
time_t		jan1_yr2038;
time_t          secondary_delay = 30;
int		do_sync_mom_hookfiles;
struct batch_request    *saved_takeover_req;
struct python_interpreter_data  svr_interp_data;
extern void	*AVL_jctx = NULL;
/**
 * @file
 * 	Used only by the TPP layer, to ping nodes only if the connection to the
 * 	local router to the server is up.
 * 	Initially set the connection to up, so that first time ping happens
 * 	by default.
 */
int tpp_network_up = 0;

/**
 * @brief
 * 	just a dummy entry for pbs_close_stdfiles since needed by failover.obj 
 */
void
pbs_close_stdfiles(void)
{
	return;
}
/**
 * @brief
 * 	needed by failover.obj 
 */
void
make_server_auto_restart(confirm)
int	confirm;
{
	return;
}

/**
 * @brief
 * needed by svr_chk_owner.obj and user_func.obj 
 */
int
decrypt_pwd(char *crypted, size_t len, char **passwd)
{
	return (0);
}

/**
 * @brief
 * 	needed by *_recov_db.obj 
 *
 */
void
panic_stop_db(char *txt)
{
	return;
}

/**
 * @brief
 *		Executes server periodic hook script.
 *
 * @par Functionality:
 *      This function initializes python environment and runs python top level
 *		script..
 *
 * @param[in]	phook	-	pointer to periodic hook
 *
 * @return	int
 * @retval	>0	: error code as returned by periodic hook script
 * @retval	0	: success if running periodic hook
 * @retval	-1	: failure
 *
 */

int
execute_python_periodic_hook(hook  *phook)
{
	int 			rc = 255;
	int			exit_code=255;
#ifdef PYTHON
	unsigned int		hook_event;
	char 			*emsg = NULL;
	char			username[MAXPATHLEN];
	int			len;
	hook_input_param_t	req_ptr;

	if (!phook)
		return rc;

	hook_event = HOOK_EVENT_PERIODIC;

	if (phook->user != HOOK_PBSADMIN)
		return rc;

	if (GetUserName((LPSTR) &username, &len) == 0) {
		log_event(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER, LOG_INFO,
			__func__, "unable to fetch the user name");
		return rc;
	}

	rc = pbs_python_event_set(hook_event, username,
		"server", &req_ptr);
	if (rc == -1) { /* internal server code failure */
		log_event(PBSEVENT_DEBUG2,
			PBS_EVENTCLASS_HOOK, LOG_ERR, __func__,
			"Failed to set event; request accepted by default");
		return (-1);
	}

	/* hook_name changes for each hook */
	/* This sets Python event object's hook_name value */
	rc = pbs_python_event_set_attrval(PY_EVENT_HOOK_NAME,
		phook->hook_name);

	if (rc == -1) {
		log_event(PBSEVENT_DEBUG2, PBS_EVENTCLASS_HOOK,
			LOG_ERR, phook->hook_name,
			"Failed to set event 'hook_name'.");
		return (-1);
	}

	/* hook_type needed for internal processing; */
	/* hook_type changes for each hook.	     */
	/* This sets Python event object's hook_type value */
	rc = pbs_python_event_set_attrval(PY_EVENT_HOOK_TYPE,
		hook_type_as_string(phook->type));

	if (rc == -1) {
		log_event(PBSEVENT_DEBUG2, PBS_EVENTCLASS_HOOK,
			LOG_ERR, phook->hook_name,
			"Failed to set event 'hook_type'.");
		return (-1);
	}

	log_event(PBSEVENT_DEBUG3, PBS_EVENTCLASS_HOOK,
		LOG_INFO, phook->hook_name, "started");

	pbs_python_set_mode(PY_MODE); /* hook script mode */

	/* let rc pass through */
	rc=pbs_python_run_code_in_namespace(&svr_interp_data,
		phook->script,
		&exit_code);

	/* go back to server's private directory */
	if (chdir(path_priv) != 0) {
		log_event(PBSEVENT_DEBUG2,
			PBS_EVENTCLASS_HOOK, LOG_WARNING, phook->hook_name,
			"unable to go back server private directory");
	}

	pbs_python_set_mode(C_MODE);  /* PBS C mode - flexible */
	log_event(PBSEVENT_DEBUG3, PBS_EVENTCLASS_HOOK,
		LOG_INFO, phook->hook_name, "finished");

	switch (rc) {
		case 0:
			/* reject if hook script rejects */
			if (pbs_python_event_get_accept_flag() == FALSE) { /* a reject occurred */
				snprintf(log_buffer, LOG_BUF_SIZE-1,
					"%s request rejected by '%s'",
					hook_event_as_string(hook_event),
					phook->hook_name);
				log_event(PBSEVENT_DEBUG3, PBS_EVENTCLASS_HOOK,
					LOG_ERR, phook->hook_name, log_buffer);
				if ((emsg=pbs_python_event_get_reject_msg()) != NULL) {
					snprintf(log_buffer, LOG_BUF_SIZE-1, "%s", emsg);
					/* log also the custom reject message */
					log_event(PBSEVENT_DEBUG3, PBS_EVENTCLASS_HOOK,
						LOG_ERR, phook->hook_name, log_buffer);
				}

			}
			return (exit_code);

		case -1:	/* internal error */
			log_event(PBSEVENT_DEBUG2, PBS_EVENTCLASS_HOOK,
				LOG_ERR, phook->hook_name,
				"Internal server error encountered. Skipping hook.");
			return (rc); /* should not happen */

		case -2:	/* unhandled exception */
			pbs_python_event_reject(NULL);
			pbs_python_event_param_mod_disallow();

			snprintf(log_buffer, LOG_BUF_SIZE-1,
				"%s hook '%s' encountered an exception, "
				"request rejected",
				hook_event_as_string(hook_event), phook->hook_name);
			log_event(PBSEVENT_DEBUG2, PBS_EVENTCLASS_HOOK,
				LOG_ERR, phook->hook_name, log_buffer);
			return (rc);
	}
#endif
	return rc;
}

/**
 * @brief
 * 	main - the initialization and main loop of svr_periodic_hook binary
 */
int
main(int argc, char *argv[])
{
	char	*id = "pbs_run_periodic_hook";
	int	i;
	int		rc;
	hook	*phook;
	char	output_path[MAXPATHLEN + 1];
	int	ret_val;

	/* python externs */
	extern void pbs_python_svr_initialize_interpreter_data(
		struct python_interpreter_data *interp_data);
	extern void pbs_python_svr_destroy_interpreter_data(
		struct python_interpreter_data *interp_data);

	if(set_msgdaemonname("pbs_run_periodic_hook")) {
		fprintf(stderr, "Out of memory\n");
		return 1;
	}

	/* set python interp data */
	svr_interp_data.data_initialized = 0;
	svr_interp_data.init_interpreter_data =
		pbs_python_svr_initialize_interpreter_data;
	svr_interp_data.destroy_interpreter_data =
		pbs_python_svr_destroy_interpreter_data;

	if (argc != 3) {
		log_err(PBSE_INTERNAL, id, "pbs_run_periodic_hook"
			" <hook_name> <server home path>");
		exit(2);
	}

	pbs_loadconf(0);
	/* initialize the pointers in the resource_def array */

	(void)snprintf(path_log, MAXPATHLEN, "%s/%s", pbs_conf.pbs_home_path, PBS_LOGFILES);
	ret_val = log_open_main(log_file, path_log, 0); /* NO silent open */
	fprintf (stdout, " path_log is %s, ret val of log_open_main is %d\n", path_log, ret_val);
	fflush(stdout);

	for (i = 0; i < (svr_resc_size - 1); ++i)
		svr_resc_def[i].rs_next = &svr_resc_def[i+1];

	pbs_python_ext_start_interpreter(&svr_interp_data);

	/* Find the periodic hook info */
	phook = (hook *)malloc(sizeof(hook));
	if (phook == (hook *)0) {
		log_err(errno, id, "no memory");
		exit(2);
	}
	(void)memset((char *)phook, (int)0, (size_t)sizeof(hook));

	phook->hook_name = strdup(argv[1]);
	path_priv = strdup(argv[2]);
	if ((phook->hook_name == NULL) || (path_priv == NULL)) {
		log_err(errno, argv[1], "strdup failed!");
		exit(2);
	}
	phook->type = HOOK_TYPE_DEFAULT;
	phook->user = HOOK_PBSADMIN;
	phook->event = HOOK_EVENT_PERIODIC;
	phook->script = (struct python_script *)NULL;
	/* get script */
	snprintf(output_path, MAXPATHLEN,
		"%s/hooks/%s%s", path_priv, phook->hook_name, ".PY");

	if (pbs_python_ext_alloc_python_script(output_path,
		(struct python_script **)&phook->script) == -1) {
		log_err(errno, argv[1], "Periodic hook script allocation failed!");
		exit(2);
	}


	if ((rc = pbs_python_check_and_compile_script(&svr_interp_data,
		phook->script)) != 0) {
		DBPRT(("%s: Recompilation failed\n", id))
		log_event(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER, LOG_INFO,
			argv[1], "Periodic hook script recompilation failed");
		exit(2);
	}

	rc = execute_python_periodic_hook(phook);
	pbs_python_ext_shutdown_interpreter(&svr_interp_data);

	exit(rc);
}
