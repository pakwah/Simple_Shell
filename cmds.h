#ifndef _CMDS_H_
#define _CMDS_H_

#include <string>
#include <vector>
#include <map>

using std::vector;
using std::string;
using std::map;
using std::pair;

/**
* A namespace containing all the command functions
* and structures used by the simple shell
*/
namespace ss {

/**
* The structure that is used to hold information about a process
* that has been run in the simple shell
*/
struct process {
	pid_t pid;
	bool reaped;
	vector<string> tokens;
	int state;
	bool run_in_fg;
};

/**
* Define the function pointer type for the functions supporting commands with arguments
*/
typedef void (*func_ptr) (vector<string>&, bool, bool);

/**
* Define the function pointer type for the functions supporting commands without arguments
*/
typedef void (*func_ptr1) (bool, bool);

/**
* Pairs of supported commands that take arguments and their corresponding functions
*/
extern map<string, func_ptr> commands; 

/**
* Pairs of supported commands that do not take arguments and their corresponding functions
*/
extern map<string, func_ptr1> commands_wo_args;

/**
* A list of all the pid's of processes run in the simple shell
*/
extern vector<process> ps;

/**
* Initialize the map of <command, func_ptr> pairs
*/
void cmd_initialize ();

/**
* Signal handler that captures the arrival of SIGCHLD
* @param signum The signal number that arrived
*/
void ch_handler (int signum);

/**
* Function used to recover a background process
* @param tokens A list of the command name and its arguments
* @bool run_in_fg Specifier of whether the process runs in the foreground or not
* @bool redir Specifier of whether the output should be redirected or not
*/
void recover (vector<string>& tokens, bool run_in_fg, bool redir);

/**
* Command that lists the content of the working directory
* @param tokens A list of the command name and its arguments
* @bool run_in_fg Specifier of whether the process runs in the foreground or not
* @bool redir Specifier of whether the output should be redirected or not
*/
void ls (vector<string>& tokens, bool run_in_fg, bool redir = false);

/**
* Command that changes the working directory
* @param tokens A list of the command name and its arguments
* @bool run_in_fg Specifier of whether the process runs in the foreground or not
* @bool redir Specifier of whether the output should be redirected or not
*/
void cd (vector<string>& tokens, bool run_in_fg, bool redir = false);

/**
* Command that queries the state of a process specified by its pid
* and prints it onto the screen or reports an error if no process of the provided pid exists
* @param tokens A list of the command name and its arguments
* @bool run_in_fg Specifier of whether the process runs in the foreground or not
* @bool redir Specifier of whether the output should be redirected or not
*/
void query (vector<string>& tokens, bool run_in_fg, bool redir = false);

/**
* Show the list of all the pid's of processes run in the simple shell
* @bool run_in_fg Specifier of whether the process runs in the foreground or not
* @bool redir Specifier of whether the output should be redirected or not
*/
void show_pids (bool run_in_fg = true, bool redir = false);

/**
* Clear the terminal
* @bool run_in_fg Specifier of whether the process runs in the foreground or not
* @bool redir Specifier of whether the output should be redirected or not
*/
void clear_screen (bool run_in_fg = true, bool redir = false);

}

#endif
