#include "cmds.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <cerrno>
#include <algorithm>

using std::cout;
using std::cerr;
using std::endl;

namespace ss {

map<string, func_ptr> commands;

map<string, func_ptr1> commands_wo_args;

vector<process> ps;

void cmd_initialize () {

	// commands with arguments
	commands.insert(pair<string, func_ptr>("ls", ss::ls));
	commands.insert(pair<string, func_ptr>("cd", ss::cd));
	commands.insert(pair<string, func_ptr>("query", ss::query));

	//commands without arguments
	commands_wo_args.insert(pair<string, func_ptr1>("show", ss::show_pids));
	commands_wo_args.insert(pair<string, func_ptr1>("clear", ss::clear_screen));

}

void ch_handler (int signum) { // reap child process when SIGCHLD arrives

	pid_t child;
	int status;

	while ((child = waitpid(-1, &status, WNOHANG)) > 0) { // reap terminated child's status
		
		if(WIFEXITED(status)) { // child terminated normally
			
			for(vector<process>::iterator iter = ps.begin(); iter != ps.end(); iter++) {

				if (iter -> pid == child) {
					iter -> reaped = true;
					iter -> state = 0;
					break;			
				}
			}
			
			continue;
		}

		if(WIFSIGNALED(status)) { // child terminated by signal
			
			for(vector<process>::iterator iter = ps.begin(); iter != ps.end(); iter++) {
			
				if (iter -> pid == child) {
					iter -> reaped = true;
					iter -> state = WTERMSIG(status);
					break;			
				}
			}

			if(WTERMSIG(status) == SIGSEGV || WTERMSIG(status) == SIGKILL) {
				// do nothing
			} else { // restart child process

				vector<string> m_tokens;
				bool redir = false;
				bool was_run_in_fg = true;

				for(vector<process>::iterator iter = ps.begin(); iter != ps.end(); iter++) {
					
					if (iter -> pid == child) {
						if (!iter -> run_in_fg) {
							was_run_in_fg = false;
							cout << "About to restart child " << child << endl;
							m_tokens = iter -> tokens;
						}
					}

				}

				if (!was_run_in_fg) { // only restart when the process was launched in the background
					
					vector<string>::iterator redir_opt;

					redir_opt = std::find(m_tokens.begin(), m_tokens.end(), ">");

					if (redir_opt != m_tokens.end()) {
						redir = true;
					}

					recover(m_tokens, true, redir);
				}

			}
		}

		
	}

	// all children are reaped
	return;	

}

void recover (vector<string>& tokens, bool run_in_fg, bool redir) {

	if(commands.find(tokens[0]) != commands.end()) {
		commands[tokens[0]](tokens, run_in_fg, redir);
	} else {
		commands_wo_args[tokens[0]](run_in_fg, redir);
	}

}

void ls (vector<string>& tokens, bool run_in_fg, bool redir) {
	
	// set up the pipe to send child pid to parent, in order for
	// the parent to push the process structure reprensenting the child
	// onto the vector

	// child cannot push itself onto the vector as it's in 
	// different address space than its parent

	int pipefd[2];
	if (pipe(pipefd) == -1) {
		perror("Error creating pipe in ls.");
	}

	pid_t p;
	p = fork();

	if (p == 0) { // child
		
		close(pipefd[0]); // close read end
	
		pid_t child = getpid();

		if (write(pipefd[1], &child, sizeof(pid_t)) == -1) {
			perror("Error in writing to pipe in ls.");
		}

		close(pipefd[1]); // close pipe

		if (redir) {

			vector<string>::iterator redir_opt;
			redir_opt = std::find(tokens.begin(), tokens.end(), ">");			

			if (redir_opt + 1 != tokens.end()) { // ensure that a pathname follows the redirection operator

				int fildes = open(tokens.back().c_str(), O_RDWR | O_CREAT | O_APPEND, S_IRWXU); // write output to file

				if (fildes != -1) {
					dup2(fildes, STDOUT_FILENO);
					dup2(fildes, STDERR_FILENO);
					close(fildes);
				} else {
					perror("Error opening redirection file in ls");
				}

			} else {
				cerr << "Cannot redirect output. No destination specified.\n";
			}

			tokens.erase(redir_opt, tokens.end()); // get rid of the redirection suffix		
		}

		// synchronize access to the global vector
		// want the signal handler to be invoked
		// after the parent has pushed the child struct onto the vector
		sem_t* consume(0); 
		consume = sem_open((string("/p") + std::to_string(child)).c_str(), 
							O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0);

		if (consume == nullptr) {
			perror("Error opening semaphore in ls child");
			exit(EXIT_FAILURE);
		}

		sem_wait(consume);
		sem_close(consume);
		sem_unlink((string("/p") + std::to_string(child)).c_str());

		char** argv = new char*[tokens.size() + 1]; // no freeing, but exec is going to replace the child's image

		for(vector<string>::const_iterator iter = tokens.cbegin(); iter != tokens.cend(); iter++) { 
			argv[iter - tokens.cbegin()] = const_cast<char*>((*iter).c_str()); // prepare argument array
		}

		argv[tokens.size()] = nullptr;

		//sleep(10); // used for testing and demo
		execvp(argv[0], argv);

		perror("Exec in ls failed"); // successful exec should not return
		delete[] argv;
		exit(EXIT_FAILURE);

	} else if (p > 0) { // parent
		
		pid_t waitchild; // used for waitpid only
		pid_t child;
		int status;

		close(pipefd[1]); // close the write end
		
		read(pipefd[0], &child, sizeof(pid_t));

		close(pipefd[0]); // close pipe

		cout << "ls parent reads child pid = " << child << endl; // used for testing and demo
		
		// semaphore to synchronize access to the vector storing process info
		
		sem_t* consume(0);
		consume = sem_open((string("/p") + std::to_string(child)).c_str(), 
							O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0);

		if (consume == nullptr) {
			perror("Error opening semaphore in ls parent");
			exit(EXIT_FAILURE);
		}
		
		// create the structure
		process child_process;
		child_process.pid = child;
		child_process.reaped = false;
		child_process.tokens = tokens;
		child_process.state = -1;
		child_process.run_in_fg = run_in_fg;
		ps.push_back(child_process);

		
		sem_post(consume);
		sem_close(consume);

		if(run_in_fg) { // fg: wait immediately

			while((waitchild = waitpid(child, &status, WNOHANG)) >= 0) {
				
				if(waitchild > 0) {

					for(vector<process>::iterator iter = ps.begin(); iter != ps.end(); iter++) {

						if (iter -> pid == child) {
							iter -> reaped = true;
							if(WIFEXITED(status)) {
								iter -> state = 0;
							}
							if(WIFSIGNALED(status)) {
								iter -> state = WTERMSIG(status);					
							}
							break;	
						}

					}
				}
			
			}

		}
		
	} else { // fork error
		perror("Fork in ls failed");
		exit(EXIT_FAILURE);
	}

}

void cd (vector<string>& tokens, bool run_in_fg, bool redir) {


	if (tokens.size() != 2) {
		cerr << "Passed in a wrong number of arguments.\n";
		cerr << "Correct form: cd <pathname>\n";
		return;
	}

	int ret = chdir(tokens[1].c_str());
	
	if (ret != 0) {
		switch (errno) {
			case ENOENT:
				cerr << "No such file or directory.\n";
				break;
			case ENOTDIR:
				cerr << "The component of the path is not a directory.\n";
				break;
			case EACCES:
				cerr << "Permission denied.\n";
				break;
			default:
				cerr << "Error changing directory.\n";
		}
	}
	
}

void query (vector<string>& tokens, bool run_in_fg, bool redir) {

	// set up the pipe to send child pid to parent, in order for
	// the parent to push the process structure reprensenting the child
	// onto the vector

	// child cannot push itself onto the vector as it's in 
	// different address space than its parent

	int pipefd[2];
	if (pipe(pipefd) == -1) {
		perror("Error creating pipe in query");
	}

	pid_t p;
	p = fork();

	if (p == 0) { // child

		close(pipefd[0]); // close read end
	
		pid_t child = getpid();

		if (write(pipefd[1], &child, sizeof(pid_t)) == -1) {
			perror("Error in query writing to pipe");
		}

		close(pipefd[1]); // close pipe
		
		if (redir) {

			vector<string>::iterator redir_opt;
			redir_opt = std::find(tokens.begin(), tokens.end(), ">");			

			if (redir_opt + 1 != tokens.end()) {

				int fildes = open(tokens.back().c_str(), O_RDWR | O_CREAT | O_APPEND, S_IRWXU); // write output to file

				if (fildes != -1) {
					dup2(fildes, STDOUT_FILENO);
					dup2(fildes, STDERR_FILENO);
					if (close(fildes) == -1) {
						perror(nullptr);
					}
				} else {
					perror("Cannot redirect output. Error opening the destination file.");
				}

			} else {
				cerr << "Cannot redirect output. No destination specified.\n";
			}

			tokens.erase(redir_opt, tokens.end()); // get rid of the redirection suffix, if any		
		}

		// synchronize access to the global vector
		// want the signal handler to be invoked
		// after the parent has pushed the child struct onto the vector
		sem_t* consume(0); 
		consume = sem_open((string("/p") + std::to_string(child)).c_str(), 
							O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0);

		if (consume == nullptr) {
			perror("Error opening semaphore in ls child");
			exit(EXIT_FAILURE);
		}

		sem_wait(consume);
		sem_close(consume);
		sem_unlink((string("/p") + std::to_string(child)).c_str());

		// query processes run in simple shell
		if (tokens.size() != 2) {
			cerr << "Wrong number of arguments! \n";
			cerr << "Correct form: query <pid>\n";
			exit(EXIT_FAILURE);
		}

		string t_pid = tokens[1];
		pid_t target = std::stoi(t_pid, nullptr);

		bool found = false;		
		
		for (vector<process>::const_iterator iter = ps.cbegin(); iter != ps.cend(); iter++) {
			if (iter -> pid == target) {

				found = true;
				cout << "Pid: " << target << endl;
				cout << "Reaped: " << iter -> reaped << endl;

				if (iter -> state == 0) {
					cout << "State: " << "Terminated normally by calling exit.\n";
				} else {

					if (iter -> state == -1) { // process still active, read from /proc
						string line;
						std::fstream fs;
						fs.open(("/proc/" + tokens[1] + "/status").c_str(), std::fstream::in);

						if (fs.is_open()) { ;

							for(int i = 0; i < 2; ++i) { // just want the name and the state of the process
								getline(fs, line);
								cout << line << endl;
							}

							fs.close();

						} else {
							perror("Unable to read process status. Check if pid is correct.");
						}

					} else {
						cout << "State: " << "Terminated by signal " << iter -> state << endl;
					}
				}
			}
		}

		if (!found) {
			cerr << "Simple Shell has not run a process of the specified pid\n";
		}
		
		exit(EXIT_SUCCESS);

	} else if (p > 0) { // parent

		pid_t waitchild; // used for waitpid only
		pid_t child;
		int status;

		close(pipefd[1]); // close the write end
		
		read(pipefd[0], &child, sizeof(pid_t));

		close(pipefd[0]); // close pipe

		cout << "query parent reads child pid = " << child << endl; // used for testing and demo
		
		// semaphore to synchronize the access to the vector storing process info
		
		sem_t* consume(0);
		consume = sem_open((string("/p") + std::to_string(child)).c_str(), 
							O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0);

		if (consume == nullptr) {
			perror("Error opening semaphore in query parent process.");
			exit(EXIT_FAILURE);
		}
		
		process child_process;
		child_process.pid = child;
		child_process.reaped = false;
		child_process.tokens = tokens;
		child_process.state = -1;
		child_process.run_in_fg = run_in_fg;
		ps.push_back(child_process);

		sem_post(consume);
		sem_close(consume);

		if(run_in_fg) { // fg: wait immediately
			
			while((waitchild = waitpid(child, &status, WNOHANG)) >= 0) {
				
				if(waitchild > 0) {

					for(vector<process>::iterator iter = ps.begin(); iter != ps.end(); iter++) {
						if (iter -> pid == child) {
							iter -> reaped = true;
							if(WIFEXITED(status)) {
								iter -> state = 0;
							}
							if(WIFSIGNALED(status)) {
								iter -> state = WTERMSIG(status);					
							}	
						}
					}
				}
			
			}

		} 
		
	} else { // fork error
		perror(nullptr);
		exit(EXIT_FAILURE);
	}
}

void show_pids (bool run_in_fg, bool redir) {

	if(!ps.empty()) {
		cout << "Pids of processes that simple shell has run in this session: \n\n";
	} else {
		cout << "Simple shell has not run any processes yet. \n";
	}

	for(vector<process>::const_iterator iter = ps.cbegin(); iter != ps.cend(); iter++) {
		cout << "Pid: " << iter -> pid << "\nReaped: " << iter -> reaped  << "\nStatus: " << iter -> state << endl;
		cout << "+++++++++\n\n";
	}

}

void clear_screen (bool run_in_fg, bool redir) {

	system("clear");

}

}
