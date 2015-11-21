#include "network_wrapper.h"
#include "network_package.h"
#include "utilities.h"
#include "system_variables.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <sstream>
#include <thread>
#include "model_interface.h"

#ifdef OS_WIN
#include <Windows.h>
#include <conio.h>
#endif


#ifdef OS_LINUX
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include<sys/wait.h>
#include <errno.h>
#include <signal.h>
#endif

using namespace std;



extern "C"
{
	void MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_INITIALISE(int *, int *, int *, int *, int *);
	void MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_PUT_FILE(int *, int *, int *, char *);
	void MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_GET_FILE(int *, int *, int *, char *);
	void MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_STORE_INSTRUCTION_SET(int *);
	void MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_PROCESS_TEMPLATE_FILES(int *, int *, char *);
	void MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_DELETE_OUTPUT_FILES(int *, char *);
	void MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_WRITE_MODEL_INPUT_FILES(int *, int *, char *, double *);
	void MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_READ_MODEL_OUTPUT_FILES(int *, int *, char *, double *, char *);
	void MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_FINALISE(int *);
	void MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_GET_STATUS(int *, int *);
	void MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_GET_DIMENSIONS(int *, int *);
	void MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_GET_MESSAGE_STRING(int *, char *);

}



#ifdef OS_WIN
PROCESS_INFORMATION start(string &cmd_string)
{
	char* cmd_line = _strdup(cmd_string.c_str());
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	if (!CreateProcess(NULL, cmd_line, NULL, NULL, false, 0, NULL, NULL, &si, &pi))
	{
		std::string cmd_string(cmd_line);
		throw std::runtime_error("CreateProcess() failed for command: " + cmd_string);
	}
	return pi;
}
#endif


#ifdef OS_LINUX
int start(string &cmd_string)
{
	//split cmd_string on whitespaces
	stringstream cmd_ss(cmd_string);
	string cmd;
	vector<string> cmds;
	while (getline(cmd_ss, cmd))
	{
		cmds.push_back(cmd);
	}
	//create the strurture for execv
	vector<char const*> arg_v;
	for (size_t icmd = 0; icmd<cmds.size(); ++icmd)
	{
		arg_v.push_back(cmds[icmd].data());
	}
	//char * const*argv = new char* [cmds.size()+1];
	//for (size_t icmd=0; icmd<cmds.size(); ++icmd)
	//{
	//  argv[icmd] = cmds[icmd].data();
	//}
	//argv[cmds.size() + 1] = NULL; //last arg must be NULL
	arg_v.push_back(NULL);

	pid_t pid = fork();
	if (pid == 0)
	{
		setpgid(0, 0);
		int success = execv(arg_v[0], const_cast<char* const*>(&(arg_v[0])));
		if (success == -1)
		{
			throw std::runtime_error("execv() failed for command: " + cmd_string);
		}
	}
	else
	{
		setpgid(pid, pid);
	}
	return pid;
}

#endif


void ModelInterface::throw_mio_error(string base_message)
{
	int ifail;
	char message[500];
	cout << endl << endl << " MODEL INTERFACE ERROR:" << endl;
	MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_GET_MESSAGE_STRING(&ifail, message);
	throw runtime_error("model input/output error:" + base_message);
}


void ModelInterface::set_files()
{
	//put template files
	int inum = 1;
	int itype = 1;
	for (auto &file : tplfile_vec)
	{
		MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_PUT_FILE(&ifail, &itype, &inum, pest_utils::string_as_fortran_char_ptr(file, 50));
		if (ifail != 0) throw_mio_error("putting template file" + file);
		inum++;
	}

	//put model in files
	inum = 1;
	itype = 2;
	for (auto &file : inpfile_vec)
	{
		MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_PUT_FILE(&ifail, &itype, &inum, pest_utils::string_as_fortran_char_ptr(file, 50));
		if (ifail != 0) throw_mio_error("putting model input file" + file);
		inum++;
	}

	//put instructions files
	inum = 1;
	itype = 3;
	for (auto &file : insfile_vec)
	{
		MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_PUT_FILE(&ifail, &itype, &inum, pest_utils::string_as_fortran_char_ptr(file, 50));
		if (ifail != 0) throw_mio_error("putting instruction file" + file);
		inum++;
	}

	//put model out files
	inum = 1;
	itype = 4;
	for (auto &file : outfile_vec)
	{
		MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_PUT_FILE(&ifail, &itype, &inum, pest_utils::string_as_fortran_char_ptr(file, 50));
		if (ifail != 0) throw_mio_error("putting model output file" + file);
		inum++;
	}
}

ModelInterface::ModelInterface()
{
	initialized = false;
}

ModelInterface::ModelInterface(vector<string> _tplfile_vec, vector<string> _inpfile_vec,
	vector<string> _insfile_vec, vector<string> _outfile_vec, vector<string> _comline_vec)
{
	tplfile_vec = _tplfile_vec;
	inpfile_vec = _inpfile_vec;
	insfile_vec = _insfile_vec;
	outfile_vec = _outfile_vec;
	comline_vec = _comline_vec;

	initialized = false;
}

void ModelInterface::initialize(vector<string> _tplfile_vec, vector<string> _inpfile_vec,
	vector<string> _insfile_vec, vector<string> _outfile_vec, vector<string> _comline_vec,
	vector<string> &_par_name_vec, vector<string> &_obs_name_vec)
{
	tplfile_vec = _tplfile_vec;
	inpfile_vec = _inpfile_vec;
	insfile_vec = _insfile_vec;
	outfile_vec = _outfile_vec;
	comline_vec = _comline_vec;

	initialize(_par_name_vec,_obs_name_vec);
}


void ModelInterface::initialize(vector<string> &_par_name_vec, vector<string> &_obs_name_vec)
{
	par_name_vec = _par_name_vec;
	obs_name_vec = _obs_name_vec;
	int npar = par_name_vec.size();
	int nobs = obs_name_vec.size();
	int ntpl = tplfile_vec.size();
	int nins = insfile_vec.size();

	MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_INITIALISE(&ifail, &ntpl, &nins, &npar, &nobs);
	if (ifail != 0) throw_mio_error("initializing mio module");

	set_files();

	//check template files
	MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_PROCESS_TEMPLATE_FILES(&ifail, &npar, pest_utils::StringvecFortranCharArray(par_name_vec, 50, pest_utils::TO_LOWER).get_prt());
	if (ifail != 0)throw_mio_error("error in template files");

	////build instruction set
	MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_STORE_INSTRUCTION_SET(&ifail);
	if (ifail != 0) throw_mio_error("error building instruction set");

	initialized = true;

}

ModelInterface::~ModelInterface()
{
	MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_FINALISE(&ifail);
	if (ifail != 0) ModelInterface::throw_mio_error("error finalizing model interface");
}

void ModelInterface::run(Parameters* pars, Observations* obs)
{	
	
	pest_utils::thread_flag terminate(false);
	pest_utils::thread_flag finished(false);
	pest_utils::thread_exceptions shared_exceptions;
	
	vector<string> par_names = pars->get_keys();
	vector<string> obs_names = obs->get_keys();
	vector<double> par_vals = pars->get_data_vec(par_names);
	vector<double> obs_vals = obs->get_data_vec(obs_names);

	if (!initialized)
	{
		initialize(par_names,obs_names);
	}


	run(&terminate, &finished, &shared_exceptions, par_names, par_vals, obs_names, obs_vals);
	pars->update(par_names, par_vals);
	obs->update(obs_names, obs_vals);
}


void ModelInterface::run(pest_utils::thread_flag* terminate, pest_utils::thread_flag* finished, pest_utils::thread_exceptions *shared_execptions,
						vector<string> &par_name_vec, vector<double> &par_values,
						vector<string> &obs_name_vec, vector<double> &obs_vec)
{
	
	if (!initialized)
	{
		initialize(par_name_vec,obs_name_vec);
	}

	try
	{
		//first delete any existing input and output files	
		// This outer loop is a work around for a bug in windows.  Window can fail to release a file
		// handle quick enough when the external run executes very quickly
		bool failed_file_op = true;
		int n_tries = 0;
		while (failed_file_op)
		{
			vector<string> failed_file_vec;
			failed_file_op = false;
			for (auto &out_file : outfile_vec)
			{
				if ((pest_utils::check_exist_out(out_file)) && (remove(out_file.c_str()) != 0))
				{
					failed_file_vec.push_back(out_file);
					failed_file_op = true;
				}
			}
			for (auto &in_file : inpfile_vec)
			{
				if ((pest_utils::check_exist_out(in_file)) && (remove(in_file.c_str()) != 0))
				{
					failed_file_vec.push_back(in_file);
					failed_file_op = true;
				}
			}
			if (failed_file_op)
			{
				++n_tries;
				w_sleep(1000);
				if (n_tries > 5)
				{
					ostringstream str;
					str << "model interface error: Cannot delete existing following model files:";
					for (const string &ifile : failed_file_vec)
					{
						str << " " << ifile;
					}
					throw PestError(str.str());
				}
			}

		}

		
		int ifail;
		int ntpl = tplfile_vec.size();
		int npar = par_values.size();

		if (std::any_of(par_values.begin(), par_values.end(), OperSys::double_is_invalid))
		{
			throw PestError("Error running model: invalid parameter value returned");
		}
		
		MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_WRITE_MODEL_INPUT_FILES(&ifail, &npar, pest_utils::StringvecFortranCharArray(par_name_vec, 50, pest_utils::TO_LOWER).get_prt(),
			&par_values[0]);
		if (ifail != 0) throw_mio_error("error writing model input files from template files");

		if (std::any_of(par_values.begin(), par_values.end(), OperSys::double_is_invalid))
		{
			throw PestError("Error running model: invalid parameter value returned");
		}
		if (std::any_of(obs_vec.begin(), obs_vec.end(), OperSys::double_is_invalid))
		{
			throw PestError("Error running model: invalid observation value returned");
		}



#ifdef OS_WIN
		//a flag to track if the run was terminated
		bool term_break = false;
		//create a job object to track child and grandchild process
		HANDLE job = CreateJobObject(NULL, NULL);
		if (job == NULL) throw PestError("could not create job object handle");
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
		jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		if (0 == SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli)))
		{
			throw PestError("could not assign job limit flag to job object");
		}
		for (auto &cmd_string : comline_vec)
		{
			//start the command
			PROCESS_INFORMATION pi;
			try
			{
				pi = start(cmd_string);
			}
			catch (...)
			{
				finished->set(true);
				throw std::runtime_error("start_command() failed for command: " + cmd_string);
			}
			if (0 == AssignProcessToJobObject(job, pi.hProcess))
			{
				throw PestError("could not add process to job object: " + cmd_string);
			}
			DWORD exitcode;
			while (true)
			{
				//sleep		
				std::this_thread::sleep_for(std::chrono::milliseconds(OperSys::thread_sleep_milli_secs));
				//check if process is still active
				GetExitCodeProcess(pi.hProcess, &exitcode);
				//if the process ended, break
				if (exitcode != STILL_ACTIVE)
				{
					break;
				}
				//else cout << exitcode << "...still waiting for command " << cmd_string << endl;
				//check for termination flag
				if (terminate->get())
				{
					std::cout << "received terminate signal" << std::endl;
					//try to kill the process
					bool success = (CloseHandle(job) != 0);
					//bool success = TerminateProcess(pi.hProcess, 0);
					if (!success)
					{
						finished->set(true);
						throw std::runtime_error("unable to terminate process for command: " + cmd_string);
					}
					term_break = true;
					break;
				}
			}
			//jump out of the for loop if terminated
			if (term_break) break;
		}


#endif

#ifdef OS_LINUX
		//a flag to track if the run was terminated
		bool term_break = false;
		for (auto &cmd_string : *commands)
		{
			//start the command
			int command_pid = start(cmd_string);
			while (true)
			{
				//sleep
				std::this_thread::sleep_for(std::chrono::milliseconds(OperSys::thread_sleep_milli_secs));
				//check if process is still active
				int status;
				pid_t exit_code = waitpid(command_pid, &status, WNOHANG);
				//if the process ended, break
				if (exit_code == -1)
				{
					finished->set(true);
					throw std::runtime_error("waitpid() returned error status for command: " + cmd_string);
				}
				else if (exit_code != 0)
				{
					break;
				}
				//check for termination flag
				if (terminate->get())
				{
					std::cout << "received terminate signal" << std::endl;
					//try to kill the process
					errno = 0;
					int success = kill(-command_pid, SIGKILL);
					if (success == -1)
					{
						finished->set(true);
						throw std::runtime_error("unable to terminate process for command: " + cmd_string);
					}
					term_break = true;
					break;
				}
			}
			//jump out of the for loop if terminated
			if (term_break) break;
		}
#endif

		if (term_break) return;

		// process instruction files		
		int nins = insfile_vec.size();
		int nobs = obs_name_vec.size();
		obs_vec.resize(nobs, -9999.00);
		
		MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_READ_MODEL_OUTPUT_FILES(&ifail, &nobs, pest_utils::StringvecFortranCharArray(obs_name_vec, 50, pest_utils::TO_LOWER).get_prt(),
			&obs_vec[0], err_instruct);
		if (ifail != 0) throw_mio_error("error processing model output files: offending instruction line:\n" + string(err_instruct));


		vector<string> invalid;
		for (int i = 0; i != par_name_vec.size(); i++)
		{
			if (OperSys::double_is_invalid(par_values.at(i)))
				invalid.push_back(par_name_vec.at(i));
		}
		if (invalid.size() > 0)
		{
			stringstream ss;
			ss << "invalid parameter values read for the following parameters: ";
			for (auto &i : invalid)
				ss << i << '\n';
			throw PestError(ss.str());
		}

		for (int i = 0; i != obs_name_vec.size(); i++)
		{
			if (OperSys::double_is_invalid(obs_vec.at(i)))
				invalid.push_back(obs_name_vec.at(i));
		}
		if (invalid.size() > 0)
		{
			stringstream ss;
			ss << "invalid observation values read for the following observations: ";
			for (auto &i : invalid)
				ss << i << '\n';
			throw PestError(ss.str());
		}
		//set the finished flag for the listener thread
		finished->set(true);
		//MODEL_INPUT_OUTPUT_INTERFACE_mp_MIO_FINALISE(&ifail);
		//if (ifail != 0) ModelInterface::throw_mio_error("error finalizing model interface");

	}
	catch (...)
	{
		shared_execptions->add(current_exception());
	}
	return;

}


