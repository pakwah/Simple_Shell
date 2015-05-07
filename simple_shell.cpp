#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <csignal>
#include <algorithm>

#include "cmds.h"

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::map;
using std::pair;

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
map<string, func_ptr> commands;

/**
* Pairs of supported commands that do not take arguments and their corresponding functions
*/
map<string, func_ptr1> commands_wo_args;

/**
* Initialize the map of <command, func_ptr> pairs
*/
void cmd_initialize () {

	// commands with arguments
	commands.insert(pair<string, func_ptr>("ls", ss::ls));
	commands.insert(pair<string, func_ptr>("cd", ss::cd));
	commands.insert(pair<string, func_ptr>("query", ss::query));

	//commands without arguments
	commands_wo_args.insert(pair<string, func_ptr1>("show", ss::show_pids));
	commands_wo_args.insert(pair<string, func_ptr1>("clear", ss::clear_screen));
}

/**
* Split the command into tokens using a delimiter
*
* @param src The source string
* @param delim The delimiter used to split the source string
* @param dst A vector of tokens resulted from splitting the string
*/
void tokenize (string& src, string delim, std::vector<string>& dst) {
	size_t pos = 0;
	string token;

	while ((pos = src.find(delim)) != std::string::npos) {
		token = src.substr(0, pos);
		if (!token.empty()) { // eliminate spaces before the first token
			dst.push_back(token);
		}
		src.erase(0, pos + delim.length());
	}

	if (!src.empty()) {
		dst.push_back(src);
	}
}

/**
* Take the command and run it, if it exists
* @param tokens A list of tokens from the command string the user entered 
* @param run_in_fg Specification of whether this job should be run in the foreground or not
*/
void run_command (vector<string>& tokens, bool run_in_fg, bool redir) {

	if(commands.find(tokens[0]) != commands.end()) { // command exists
		commands[tokens[0]](tokens, run_in_fg, redir);
	} else if(commands_wo_args.find(tokens[0]) != commands_wo_args.end()) { // command w/o arguments exists
		commands_wo_args[tokens[0]](run_in_fg, redir);
	} else { // command not exists
		cout << "Command not supported." << endl;
	}

}

int main() {

	// buffer for getting the current directory
	char cur_buf[300]; 

	string cur_dir; // current directory string 
	string command; // command string
	string exit = "exit"; // command for exit

	// a list of tokens from the user input
	vector<string> tokens; 

	// set up signal handler
	signal(SIGCHLD, ss::ch_handler);

	// initialize maps of supported commands 
	cmd_initialize();
	ss::cmd_initialize();

	while(1) {	
		
		bool fg_param_present = true; // whether the user explicitly specifies foreground/background mode
		bool run_in_fg = true; // run job in foreground/background mode
		bool redir = false; // whether the user wants the output to be redirected

		cur_dir = getcwd(cur_buf, 300);

		cout << "Simple_Shell:" + cur_dir + "$ "; // prompt
		getline(std::cin, command); // read in user input

		if(command.compare(exit) != 0) {
			
			if(command.empty()) {
				continue; // empty command
			}

			tokenize(command, " ", tokens); // tokenize user input with space character as the delimiter

			/* determine running mode: foreground or background */
			if(tokens[0] == "bg") { 
				run_in_fg = false;
			} else {
				if (tokens[0] != "fg") {
					fg_param_present = false;
				}
			}

			/* discard fg/bg specifier */
			if (fg_param_present) {
				tokens.erase(tokens.begin()); 
			}
			
			/* determine if redirection is requested */
			vector<string>::iterator iter;

			iter = std::find(tokens.begin(), tokens.end(), ">");
			if (iter != tokens.end()) {
				redir = true;
			}

			/* run the command */
			run_command(tokens, run_in_fg, redir); 
			
			/* clear token vector for the next command */
			tokens.clear(); 
		
		} else {
			break; // user entered "exit"
		}
		
	}

	cout << "Exited simple shell." << endl;

	return 0;
}
