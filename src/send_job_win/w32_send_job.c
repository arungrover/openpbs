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
/**
 * @file	w32_send_job.c
 * @brief
 * The entry point function for pbs_daemon.
 *
 * @par Included public functions are:
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
#include "pbs_client_thread.h"
#include "pbs_ecl.h"
#include "pbs_db.h"

#define RETRY 3
/* External functions called */

/* External data items */
extern	int	should_retry_route(int err);
extern int move_job_file(int conn, job *pjob, enum job_file which, int rpp);
extern int                      resc_access_perm;
extern resource_def *svr_resc_def;

/* Local Private Functions */
static int chk_save_file(char *filename);

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
char	       *path_spool = NULL;
char 	       *path_track = NULL;
char		path_hooks[MAXPATHLEN+1];
char		*path_hooks_workdir = NULL;
char		path_hooks_tracking[MAXPATHLEN+1];
char		path_hooks_rescdef[MAXPATHLEN+1];
int		do_sync_mom_hookfiles;
char           *path_secondaryact;
char	       *pbs_o_host = "PBS_O_HOST";
pbs_net_t	pbs_mom_addr = 0;
unsigned int	pbs_mom_port = 0;
unsigned int	pbs_rm_port = 0;
pbs_net_t	pbs_scheduler_addr = 0;
unsigned int	pbs_scheduler_port = 0;
pbs_net_t	pbs_server_addr = 0;
unsigned int	pbs_server_port_dis = 0;
int		queue_rank = 0;
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
/* The following are defined to resolve external reference errors in windows build */
char		*path_svrlive;		/* the svrlive file used for monitoring during failover */
char		*pbs_server_name; /* pbs server name */
char		*pbs_server_id; /* pbs server database id */
char		*path_svrdb;  /* path to server db */
char		*path_nodestate; /* path to node state file */
char		*path_nodes; /* path to nodes file */
char		*path_resvs; /* path to resvs directory */
extern void	*AVL_jctx = NULL; /* used for the jobs AVL tree */
/*
 * Used only by the TPP layer, to ping nodes only if the connection to the
 * local router to the server is up.
 * Initially set the connection to up, so that first time ping happens
 * by default.
 */
int		tpp_network_up = 0;

int		svr_unsent_qrun_req = 0;
/*
 * For history jobs:
 *	To check whether server is configured for history job
 */
long		svr_history_enable = 0;
long		svr_history_duration = 0;

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
pbs_list_head	svr_execjob_preterm_hooks;
pbs_list_head	svr_execjob_end_hooks;
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
struct batch_request    *saved_takeover_req;
struct python_interpreter_data  svr_interp_data;
pbs_db_conn_t *svr_db_conn;

/* just a dummy entry for pbs_close_stdfiles since needed by failover.obj */
void
pbs_close_stdfiles(void)
{
	return;
}
/**
 * @brief
 *	needed by failover.obj 
 */
void
make_server_auto_restart( int confirm)
{
	return;
}

/**
 * @brief
 * 	needed by svr_chk_owner.obj and user_func.obj 
 */
int
decrypt_pwd(char *crypted, size_t len, char **passwd)
{
	return (0);
}

/**
 * @brief
 * 	needed by *_recov_db.obj 
 */
void
panic_stop_db(char *txt)
{
	return;
}

/**
 * @brief
 * 	main - the initialization and main loop of pbs_daemon
 */
int
main(int argc, char *argv[])
{
	char		jobfile[MAXPATHLEN+1];
	char		jobfile_full[MAXPATHLEN+1];
	pbs_net_t	hostaddr = 0;
	int			port = -1;
	int			move_type = -1;
	pbs_list_head	attrl;
	enum conn_type  cntype = ToServerDIS;
	int		con = -1;
	char		*destin;
	int			encode_type;
	int			i;
	char		*id = "pbs_send_job";
	job			*jobp;
	char		 job_id[PBS_MAXSVRJOBID+1];
	attribute	*pattr;
	struct attropl  *pqjatr;      /* list (single) of attropl for quejob */
	char		 script_name[MAXPATHLEN+1];
	int			in_server = -1;
	char		*param_name, *param_val;
	char		buf[4096];
	struct  hostent *hp;
	struct in_addr  addr;
	char            *credbuf = NULL;
	size_t		credlen = 0;
	int 		prot = PROT_TCP;
	/*the real deal or output version and exit?*/

	execution_mode(argc, argv);

	/* If we are not run with real and effective uid of 0, forget it */

	pbs_loadconf(0);

	if (!isAdminPrivilege(getlogin())) {
		fprintf(stderr, "%s: Must be run by root\n", argv[0]);
		exit(SEND_JOB_FATAL);
	}

	/* initialize the pointers in the resource_def array */
	for (i = 0; i < (svr_resc_size - 1); ++i)
		svr_resc_def[i].rs_next = &svr_resc_def[i+1];
	/* last entry is left with null pointer */
	/* set single threaded mode */
	pbs_client_thread_set_single_threaded_mode();
	/* disable attribute verification */
	set_no_attribute_verification();

	/* initialize the thread context */
	if (pbs_client_thread_init_thread_context() != 0) {
		fprintf(stderr, "%s: Unable to initialize thread context\n",
			argv[0]);
		exit(SEND_JOB_FATAL);
	}

	if(set_msgdaemonname("PBS_send_job")) {
		fprintf(stderr, "Out of memory\n");
		return 1;
	}

	winsock_init();

	connection_init();

	while (fgets(buf, sizeof(buf), stdin) != NULL) {
		buf[strlen(buf)-1] = '\0';	/* gets rid of newline */

		param_name = buf;
		param_val = strchr(buf, '=');
		if (param_val) {
			*param_val = '\0';
			param_val++;
		} else {	/* bad param_val -- skipping */
			break;
		}

		if (strcmp(param_name, "jobfile") == 0) {
			jobfile[0] = '\0';
			strncpy(jobfile, param_val, MAXPATHLEN);
		} else if (strcmp(param_name, "destaddr") == 0) {
			hostaddr = atol(param_val);
		} else if (strcmp(param_name, "destport") == 0) {
			port = atoi(param_val);
		} else if (strcmp(param_name, "move_type") == 0) {
			move_type = atoi(param_val);
		} else if (strcmp(param_name, "in_server") == 0) {
			in_server = atoi(param_val);
		} else if (strcmp(param_name, "server_name") == 0) {
			server_name[0] = '\0';
			strncpy(server_name, param_val, PBS_MAXSERVERNAME);
		} else if (strcmp(param_name, "server_host") == 0) {
			server_host[0] = '\0';
			strncpy(server_host, param_val, (sizeof(server_host) - 1));
		} else if (strcmp(param_name, "server_addr") == 0) {
			pbs_server_addr = atol(param_val);
		} else if (strcmp(param_name, "server_port") == 0) {
			pbs_server_port_dis = atoi(param_val);
		} else if (strcmp(param_name, "log_file") == 0) {
			log_file = strdup(param_val);
		} else if (strcmp(param_name, "path_log") == 0) {
			path_log[0] = '\0';
			strncpy(path_log, param_val, MAXPATHLEN);
		} else if (strcmp(param_name, "path_jobs") == 0) {
			path_jobs = strdup(param_val);
		} else if (strcmp(param_name, "path_spool") == 0) {
			path_spool = strdup(param_val);
		} else if (strcmp(param_name, "path_rescdef") == 0) {
			path_rescdef = strdup(param_val);
		} else if (strcmp(param_name, "path_users") == 0) {
			path_users = strdup(param_val);
		} else if (strcmp(param_name, "path_hooks_workdir") == 0) {
			path_hooks_workdir = strdup(param_val);
			if (path_hooks_workdir == NULL)
				exit(SEND_JOB_FATAL);
		} else if (strcmp(param_name, "svr_history_enable") == 0) {
			svr_history_enable = atol(param_val);
		} else if (strcmp(param_name, "svr_history_duration") == 0) {
			svr_history_duration = atol(param_val);
		} else if (strcmp(param_name, "single_signon_password_enable") == 0) {
			if (decode_b(&server.sv_attr[(int)SRV_ATR_ssignon_enable],
				NULL, NULL, param_val) != 0) {
				fprintf(stderr, "%s: failed to set ssignon_password_enable\n", argv[0]);
				exit(SEND_JOB_FATAL);
			}
		} else if (strcmp(param_name, "script_name") == 0) {
			strncpy(script_name, param_val, MAXPATHLEN + 1);
		} else
			break;
	}

	time(&time_now);

	(void)log_open_main(log_file, path_log, 1); /* silent open */

	if (setup_resc(1) == -1) {
		/* log_buffer set in setup_resc */
		log_err(-1, "pbsd_send_job(setup_resc)", log_buffer);
		return (-1);
	}

	if( strlen(jobfile) == 0 || hostaddr == 0 || port == 0 ||  move_type == -1 || \
			in_server == -1 || strlen(server_name) == 0 || strlen(server_host) == 0 || \
			pbs_server_addr == 0 || pbs_server_port_dis == 0 || \
			strlen(path_log) == 0 || path_jobs == NULL || \
			path_spool == NULL || path_users == NULL ) {
		log_err(-1, "pbs_send_job", "error on one of the parameters");
		log_close(0);	/* silent close */
		exit(SEND_JOB_FATAL);
	}

	CLEAR_HEAD(task_list_immed);
	CLEAR_HEAD(task_list_timed);
	CLEAR_HEAD(task_list_event);
	CLEAR_HEAD(svr_queues);
	CLEAR_HEAD(svr_alljobs);
	CLEAR_HEAD(svr_newjobs);
	CLEAR_HEAD(svr_allresvs);
	CLEAR_HEAD(svr_newresvs);
	CLEAR_HEAD(svr_deferred_req);
	CLEAR_HEAD(svr_unlicensedjobs);

	strcpy(jobfile_full, path_jobs);
	strcat(jobfile_full, jobfile);

	if (chk_save_file(jobfile_full) != 0) {
		sprintf(log_buffer, "Error opening jobfile=%s", jobfile);
		log_err(-1, id, log_buffer);
		goto fatal_exit;
	}

	if ((jobp=job_recov_fs(jobfile, RECOV_SUBJOB)) == NULL) {
		sprintf(log_buffer, "Failed to recreate job in jobfile=%s", jobfile);
		log_err(-1, id, log_buffer);
		goto fatal_exit;
	}

	/* now delete the temp job file that was created by job_save_fs in server code
	 * jobs are in database now, no need to keep in filesystem
	 */
	unlink(jobfile_full);

	if (in_server)
		append_link(&svr_alljobs, &jobp->ji_alljobs, jobp);


	/* select attributes/resources to send based on move type */

	if (move_type == MOVE_TYPE_Exec) {
		resc_access_perm = ATR_DFLAG_MOM;
		encode_type = ATR_ENCODE_MOM;
		cntype = ToServerDIS;
	} else {
		resc_access_perm = ATR_DFLAG_USWR | ATR_DFLAG_OPWR |
			ATR_DFLAG_MGWR | ATR_DFLAG_SvRD;
		encode_type = ATR_ENCODE_SVR;
		svr_dequejob(jobp);
	}

	CLEAR_HEAD(attrl);
	pattr = jobp->ji_wattr;
	for (i=0; i < (int)JOB_ATR_LAST; i++) {
		if ((job_attr_def+i)->at_flags & resc_access_perm) {
			(void)(job_attr_def+i)->at_encode(pattr+i, &attrl,
				(job_attr_def+i)->at_name, (char *)0,
				encode_type, NULL);
		}
	}
	attrl_fixlink(&attrl);

	/* script name is passed from parent */

	/* get host name */
	pbs_loadconf(0);
	addr.s_addr = htonl(hostaddr);
	hp = gethostbyaddr((void *)&addr, sizeof(struct in_addr), AF_INET);
	if (hp == NULL) {
		sprintf(log_buffer, "%s: h_errno=%d",
			inet_ntoa(addr), h_errno);
		log_err(-1, id, log_buffer);
	}
	else {
		/* read any credential file */
		(void)get_credential(hp->h_name, jobp,  PBS_GC_BATREQ,
			&credbuf, &credlen);
	}
	/* save the job id for when after we purge the job */

	(void)strcpy(job_id, jobp->ji_qs.ji_jobid);

	con = -1;

	DIS_tcparray_init();

	for (i=0; i<RETRY; i++) {

		pbs_errno = 0;
		/* connect to receiving server with retries */

		if (i > 0) {	/* recycle after an error */
			if (con >= 0)
				svr_disconnect(con);
			if (should_retry_route(pbs_errno) == -1) {
				goto fatal_exit;	/* fatal error, don't retry */
			}
			sleep(1<<i);
		}
		if ((con = svr_connect(hostaddr, port, 0, cntype, prot)) ==
			PBS_NET_RC_FATAL) {
			(void)sprintf(log_buffer, "send_job failed to %lx port %d",
				hostaddr, port);
			log_err(pbs_errno, id, log_buffer);
			goto fatal_exit;
		} else if (con == PBS_NET_RC_RETRY) {
			pbs_errno = WSAECONNREFUSED;	/* should retry */
			continue;
		}
		/*
		 * if the job is substate JOB_SUBSTATE_TRNOUTCM which means
		 * we are recovering after being down or a late failure, we
		 * just want to send the "read-to-commit/commit"
		 */


		if (jobp->ji_qs.ji_substate != JOB_SUBSTATE_TRNOUTCM) {

			if (jobp->ji_qs.ji_substate != JOB_SUBSTATE_TRNOUT) {
				jobp->ji_qs.ji_substate = JOB_SUBSTATE_TRNOUT;
			}

			pqjatr = &((svrattrl *)GET_NEXT(attrl))->al_atopl;
			destin = jobp->ji_qs.ji_destin;

			if (PBSD_queuejob(con, jobp->ji_qs.ji_jobid, destin, pqjatr, (char *)0, prot, NULL)== 0) {
				if (pbs_errno == PBSE_JOBEXIST && move_type == MOVE_TYPE_Exec) {
					/* already running, mark it so */
					log_event(PBSEVENT_ERROR,
						PBS_EVENTCLASS_JOB, LOG_INFO,
						jobp->ji_qs.ji_jobid, "Mom reports job already running");
					goto ok_exit;

				} else if ((pbs_errno == PBSE_HOOKERROR) ||
					(pbs_errno == PBSE_HOOK_REJECT)  ||
					(pbs_errno == PBSE_HOOK_REJECT_RERUNJOB)  ||
					(pbs_errno == PBSE_HOOK_REJECT_DELETEJOB)) {
					char		name_buf[MAXPATHLEN+1];
					int		rfd;
					int		len;
					char		*reject_msg;
					int		err;

					err = pbs_errno;

					reject_msg = pbs_geterrmsg(con);
					(void)snprintf(log_buffer, sizeof(log_buffer),
						"send of job to %s failed error = %d reject_msg=%s",
						destin, err,
						reject_msg?reject_msg:"");
					log_event(PBSEVENT_JOB, PBS_EVENTCLASS_JOB,
						LOG_INFO, jobp->ji_qs.ji_jobid,
						log_buffer);

					(void)strcpy(name_buf, path_hooks_workdir);
					(void)strcat(name_buf, jobp->ji_qs.ji_jobid);
					(void)strcat(name_buf, HOOK_REJECT_SUFFIX);

					if ((reject_msg != NULL) &&
						(reject_msg[0] != '\0')) {

						if ((rfd = open(name_buf,
							O_RDWR|O_CREAT|O_TRUNC, 0600)) == -1) {
							snprintf(log_buffer,
								sizeof(log_buffer),
								"open of reject file %s failed: errno %d",
								name_buf, errno);
							log_event(PBSEVENT_JOB, PBS_EVENTCLASS_JOB, LOG_INFO, jobp->ji_qs.ji_jobid, log_buffer);
						} else {
							secure_file(name_buf, "Administrators",
								READS_MASK|WRITES_MASK|STANDARD_RIGHTS_REQUIRED);
							setmode(rfd, O_BINARY);
							len = strlen(reject_msg)+1;
							/* write also trailing null char */
							if (write(rfd, reject_msg, len) != len) {
								snprintf(log_buffer,
									sizeof(log_buffer),
									"write to file %s incomplete: errno %d", name_buf, errno);
								log_event(PBSEVENT_JOB, PBS_EVENTCLASS_JOB, LOG_INFO, jobp->ji_qs.ji_jobid, log_buffer);
							}
							close(rfd);
						}
					}

					if (err == PBSE_HOOKERROR)
						exit(SEND_JOB_HOOKERR);
					if (err == PBSE_HOOK_REJECT)
						exit(SEND_JOB_HOOK_REJECT);
					if (err == PBSE_HOOK_REJECT_RERUNJOB)
						exit(SEND_JOB_HOOK_REJECT_RERUNJOB);
					if (err == PBSE_HOOK_REJECT_DELETEJOB)
						exit(SEND_JOB_HOOK_REJECT_DELETEJOB);
				} else {
					(void)sprintf(log_buffer, "send of job to %s failed error = %d", destin, pbs_errno);
					log_event(PBSEVENT_JOB, PBS_EVENTCLASS_JOB, LOG_INFO, jobp->ji_qs.ji_jobid, log_buffer);
					continue;
				}
			}

			if (jobp->ji_qs.ji_svrflags & JOB_SVFLG_SCRIPT) {
				if (PBSD_jscript(con, script_name, prot, NULL) != 0)
					continue;
			}

			if (credlen > 0) {
				int     ret;

				ret = PBSD_jcred(con,
					jobp->ji_extended.ji_ext.ji_credtype,
					credbuf, credlen, prot, NULL);
				if ((ret == 0) || (i == (RETRY - 1)))
					free(credbuf);	/* free credbuf if credbuf is sent successfully OR */
				/* at the end of all retry attempts */
				if (ret != 0)
					continue;
			}

			if ((move_type == MOVE_TYPE_Exec) &&
				(jobp->ji_qs.ji_svrflags & JOB_SVFLG_HASRUN) &&
				(hostaddr !=  pbs_server_addr)) {
				/* send files created on prior run */
				if ((move_job_file(con, jobp, StdOut, prot) != 0) ||
					(move_job_file(con, jobp, StdErr, prot) != 0) ||
					(move_job_file(con, jobp, Chkpt, prot) != 0))
					continue;
			}

			jobp->ji_qs.ji_substate = JOB_SUBSTATE_TRNOUTCM;
		}

		if (PBSD_rdytocmt(con, job_id, prot, NULL) != 0)
			continue;

		if (PBSD_commit(con, job_id, prot, NULL) != 0)
			goto fatal_exit;
		goto ok_exit;	/* This child process is all done */
	}
	if (con >= 0)
		svr_disconnect(con);
	/*
	 * If connection is actively refused by the execution node(or mother superior) OR
	 * the execution node(or mother superior) is rejecting request with error
	 * PBSE_BADHOST(failing to authorize server host), the node should be marked down.
	 */
	if ((move_type == MOVE_TYPE_Exec) && (pbs_errno == WSAECONNREFUSED || pbs_errno == PBSE_BADHOST)) {
		i = SEND_JOB_NODEDW;
	} else if (should_retry_route(pbs_errno) == -1) {
		i = SEND_JOB_FATAL;
	} else {
		i = SEND_JOB_RETRY;
	}
	(void)sprintf(log_buffer, "send_job failed with error %d", pbs_errno);
	log_event(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB, LOG_NOTICE,
		jobp->ji_qs.ji_jobid, log_buffer);
	log_close(0);
	net_close(-1);
	unlink(script_name);
	exit(i);

fatal_exit:
	if (con >= 0)
		svr_disconnect(con);
	log_close(0);
	net_close(-1);
	unlink(script_name);
	exit(SEND_JOB_FATAL);

ok_exit:
	if (con >= 0)
		svr_disconnect(con);
	log_close(0);
	net_close(-1);
	unlink(script_name);
	exit(SEND_JOB_OK);
}


/**
 * @brief
 *	checks whether input file is saved by checking its status info.
 *
 * @param[in] filename - filename 
 */
static int
chk_save_file(char *filename)
{
	struct stat sb;

	if (stat(filename, &sb) == -1)
		return (errno);

	if (S_ISREG(sb.st_mode))
		return (0);
	return (-1);
}
