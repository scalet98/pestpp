#include "RunManagerAbstract.h"

#ifndef RUNMANAGER_FORTRAN_WRAP_H_
#define RUNMANAGER_FORTRAN_WRAP_H_

extern "C"
{
int RMIF_CREATE_SERIAL(char *f_comline, int  *comline_str_len, int *comline_array_len,
	char *f_storfile, int *storfile_len,
	char *f_rundir, int *rundir_len, int *n_max_fail);

int RMIF_CREATE_PANTHER(char *f_comline, int  *comline_str_len, int *comline_array_len,
	char *f_storfile, int *storfile_len,
	char *f_port, int *f_port_len,
	char *f_info_filename, int *info_filename_len, int *n_max_fail,
	double *overdue_reched_fac, double *overdue_giveup_fac);

int RMIF_ADD_RUN(double *parameter_data, int *npar, int *model_exe_index, int *id);

int RMIF_ADD_RUN_WITH_INFO(double *parameter_data, int *npar, int *model_exe_index, int *id,
	char *f_info_txt, int  *info_txt_len, double *info_value);

int RMIF_INITIALIZE(char *f_pname, int  *pname_str_len, int *pname_array_len,
				 char *f_oname, int  *oname_str_len, int *oname_array_len);

int RMIF_INITIALIZE_RESTART(char *f_storfile, int *storfile_len);

int RMIF_REINITIALIZE();

int RMIF_RUN();

int RMIF_RUN_UNTIL(int *condition, int *no_ops, double *time_sec, int *return_cond);

int RMIF_CANCEL_RUN(int *run_id);

int RMIF_GET_RUN(int *run_id, double *parameter_data, int *npar, double *obs_data, int *nobs);

int RMIF_GET_RUN_WITH_INFO(int *run_id, double *parameter_data, int *npar, double *obs_data, int *nobs,
	char *f_info_txt, int  *info_txt_len, double *info_value);

int RMIF_GET_RUN_STATUS_INFO(int *run_id, int* run_status, double *max_runtime, int *n_concurrent_runs);

int RMIF_GET_NUM_FAILED_RUNS(int *nfail);

int RMIF_GET_FAILED_RUN_IDS(int *run_id_array, int *len_run_id_array);

int RMIF_GET_NUM_TOTAL_RUNS(int *nruns);

int RMFI_DELETE();

					 
}
#endif //RUNMANAGER_FORTRAN_WRAP_H_