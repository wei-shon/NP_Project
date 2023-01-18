#include<iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include<sys/wait.h>
#include <pwd.h> 
#include <string.h>
#include <string>
#include <vector>
#include<algorithm>
#include <fcntl.h>


using namespace std;
#define MAX_ARGV_SIZE 1024
int const stdout_copy = dup(STDOUT_FILENO);
int const stdin_copy = dup(STDIN_FILENO);
int const stderror_copy = dup(STDERR_FILENO);

//struct this to remember pipe number
struct pipefd{
    int number_to_pipe;
    int fdin;//1 go into pipe
    int fdout;//0 go out of pipe
};

//
// vector //
//
vector <string> cmd;//to store command line in every input and then we will clear when we have done the command line
vector <int> all_pipe_direction_position ;// remember all the special command's position in cmd. special command : !, |, >, number pipe, number stderr
vector<pipefd> pipeMap;

// 
// function //
//
//about file and process
bool check_file_exist( const char* file);
void close_file(); // To close the number : 0 pipe
void signal_handler(int sig); // to handle the signal of process

//about environment
void print_env( int index);
void set_env( int index);

//split command or path
void split_command(string input_command);
vector<string> split_path(vector <string> all_path , string original_path);

//find coomand
int find_last_special_command(int index);
int find_pipe_num(int num); // To find whether there exist the num's pipe

//deal with pipe order
void pipe_number_order_decrease();
void pipe_number_order_increase();

//deal with command
void normalpipe(int pipe_position);
void numberpipe(int pipe_position , int pipe_number);
void normalstderror(int stderror_position);
void numberstderror(int stderror_position , int pipe_number) ;
void last_cmd(int pipe_position );
void redirection_cmd( int redirection_index );
void run_command();

//
// main //
//
int main(){
	string input_command;
	setenv("PATH","bin:.",1);
	while(1)
	{
		cout<<"% ";
		getline(cin,input_command);
		if(input_command=="exit"){ exit(0); }
		split_command(input_command);
		//run command
		run_command();
		//clear cmd, special command position
		cmd.clear();
		cmd.shrink_to_fit();
		all_pipe_direction_position.clear();
	}
}

void set_env( int index){
	/*
		Parameter:
			index : the setenv's index in cmd
		Work:
			setenv env
	*/
	setenv(cmd[index+1].c_str(),cmd[index+2].c_str(),1);
	return ;
}

void print_env( int index){
	/*
		Parameter:
			index : the printenv's index in cmd
		Work:
			print env
	*/
	char* env = getenv(cmd[index+1].c_str());
	if( env != NULL){ printf("%s\n",env); }
	return ;
}

int find_pipe_num(int num){
	/*
		Parameter:
			num : the pipe's number
		Work:
			We can find whether the num's pipe exist or not
			if Yes : return the number in the pipeMap
			if No  : return -1
		Reason:
			because the pipeMap is vector that is no order, 
			We need to use this function to find the num's pipe in which order in pipeMap
		E.q.:
			pipeMap : 1號管, 3號管, 7號管,....
			find_pipe_num(3) -> return 1
			find_pipe_num(7) -> return 2

	*/
    for(int i = 0 ; i < pipeMap.size() ; i++)
    {
        if(pipeMap[i].number_to_pipe==num)
        {
            return i;
        }
    }
    return -1;
}

void close_file(){
	/*
		Parameter:
			num : the pipe's number
		Work:
			We can find whether the num's pipe exist or not
		Reason:
			In order not to close wrong file, We use this function to close 0號管.
			Before closing, We will check 0號管's reading side (fd[0]) and writing side (fd[1]) is using or not.
			Because we will close writing side (fd[1]), we need double check
	*/
	int fd[2];
	int it = find_pipe_num(0);
	if(it!=-1)
	{
		fd[0] = pipeMap[it].fdout;
        fd[1] = pipeMap[it].fdin;
        pipeMap.erase(pipeMap.begin()+it);

		// 預防關到正在開的管子 因為我會再 '｜'的地方去關它讀取的號碼，所以會開到原北關閉的館子的號碼，導致我在這邊會關到那個號碼
		// e.g. fd[0] = 5 , fd[1]=6 , 我會關閉fd[1]=6，這樣下一個人開pipe，會使用到 6 這個數字，所以必面我誤關管子號碼，所以在這邊特別去寫這個迴圈確認。
		for( int i = 0 ; i < pipeMap.size() ; i++)
		{
			if(fd[1] != STDOUT_FILENO && fd[1]!=pipeMap[i].fdin && fd[1]!=pipeMap[i].fdout && pipeMap[i].number_to_pipe!=0){ close(fd[1]); }
			if(fd[0] != STDIN_FILENO && fd[0]!=pipeMap[i].fdin && fd[0]!=pipeMap[i].fdout && pipeMap[i].number_to_pipe!=0) { close(fd[0]); }
		}
	}
	return ;
}

void signal_handler(int sig){ 
	/*
			Parameter : 
				-1 : any child process all can receive
				status : is any status , we also can use NULL
				WNOHANG : means if there no any child process complete and then return right away
			Return Value:
				>0 : child process's status
				=0 : unkown(I don't know)
				-1 : unsuccessful waitpid

			Work:
				this function will receive any child process to avoid the zombie process occurs.
			Reason:
				affraid the zombie process occur
	*/
	int status; 
	while(waitpid(-1,&status,WNOHANG) > 0){ }; 
}

void split_command(string input_command){
	int index =0 ;
	string single_command="";
	for(int i = 0 ; i <= input_command.length() ; i++)
	{
		if(input_command[i]==' ' || input_command[i]=='\n')
		{
			if(single_command!="")
			{
				cmd.push_back(single_command);
				if(single_command[0] == '|' || single_command[0] == '>' || single_command[0] == '!')
				{
					all_pipe_direction_position.push_back(index);
				}
				index++;
				single_command="";
			}
		}
		else if(i == input_command.length())
		{
			if(single_command!="")
			{
				cmd.push_back(single_command);
				index++;
			}
		}
		else
		{
			single_command+=input_command[i];
		}
	}
}

vector<string> split_path(vector <string> all_path , string original_path){
	/*
		Parameter:
			all_path : Store the path that we divide. 
			original_path : the path that We getenv
		Return:
			the completed splited path
		Work:
			split original_path
		Reason:
			Because if We have two path (bin , .) and then all_path is bin:. 
			Path will be seperated by ':'
			So We need this to divide the path!
	*/
    string temp="";
    for (int i = 0 ; i < original_path.length() ; i++)
    {
        if(original_path[i]==':')
        {
            all_path.push_back(temp);
            temp="";
        }
        else if(i == original_path.length()-1)
        {
            temp+=original_path[i];
            all_path.push_back(temp);
        }
        else{ temp+=original_path[i]; }
    }
    return all_path;
}

bool check_file_exist( const char* need_to_be_checked_file){
	/*
		Parameter:
			need_to_be_checked_file :  the file needed be checked
		Return:
			True : the file exist in path
			False : the file doesn't exist in path
		Reason:
			Before We execvp the command, We need to check command exist in bin or not
	*/
	char* path = getenv("PATH");//store original path
    string string_path = path;
    vector <string> all_path;

    string f = need_to_be_checked_file;

    all_path = split_path(all_path , string_path);
    for(int i = 0 ; i < all_path.size() ; i++)
    {
        string temp = all_path[i];
        temp+='/';
        temp+=f;
		if(access(temp.c_str() , F_OK)!=-1) return true;
    }
	return false;
}

int find_last_special_command(int index){
	/*
		Parameter:
			index :  the index that in the cmd
		Return:
			>0 : the '|' or '!' oe '>' 's position in cmd(the splited cmd set)
			-1 : there no any '|' '!' before this index, it means this index is first command in all_pipe_direction_position
		Reason:
			We need split command with '|' '!', so that we need to find '|' '!' position in cmd.
			And then We can catch the command to execvp
	*/
	for (int i = 0 ; i < all_pipe_direction_position.size() ; i++)
	{
		if(all_pipe_direction_position[i]==index && i!=0) 
		{
			return all_pipe_direction_position[i-1];
		}
	}
	if(index==cmd.size()-1)//deal with last_cmd
	{ 
		//多加empty是為了預防最後一個cmd是 number pipe (|7) or number stderr (!4)
		if(!all_pipe_direction_position.empty()){ return all_pipe_direction_position[all_pipe_direction_position.size()-1]; }
	}	
	return -1;
}

void pipe_number_order_decrease(){
	/*
		Work :
			After We finish a command, We nned to make number pipe to decrease.
			Becasue We will get 0號管 to execvp, so that if the number pipe(!5 or|4) occur,
			We need to count this.
	*/
    for( int i = 0 ; i < pipeMap.size() ; i++){ pipeMap[i].number_to_pipe -=1; }
    return;
}

void pipe_number_order_increase(){
	/*
		Work :
			After We finish a command, We nned to make number pipe to increase.
			Becasue We will get 0號管 to execvp, and then We set normal pipe is -1
			We need to +1. that it will get into 0號管, so that it will be execvp.
	*/
	for( int i = 0 ; i < pipeMap.size() ; i++)
    {
        if(pipeMap[i].number_to_pipe == -1)
		{
			pipeMap[i].number_to_pipe+=1;
		}
    }
    return;
}

void normalpipe(int pipe_position){
	/*
		Parameter:
			pipe_postion : the index that pipe(|) in the cmd
		Work:
			the will deal with the normal pipe
	*/

	// find the index that we will use in findind command
	int this_pipe_position = pipe_position;
	int last_pipe_position = find_last_special_command(pipe_position);

	// this is to finad command that we need to execvp
	int index = 0;
	char* argv[MAX_ARGV_SIZE];
	for(int i = last_pipe_position+1 ; i < this_pipe_position;i++)
	{
		argv[index] = strdup(cmd[i].c_str());
		index++;
	}
	argv[index] = NULL;

	int fd_to_write[2];
	if(pipe(fd_to_write) < 0 ){ perror("pipe 1 get wrong!"); }
	//確認是否有0號管
	int it = find_pipe_num(0);
	//若是存在，就要讀取0號管，再執行execvp 
    if(it != -1)
    {
		//initial pipe
		int fd_to_read[2];
        fd_to_read[0] = pipeMap[it].fdout;
        fd_to_read[1] = pipeMap[it].fdin;
        // pipeMap.erase(pipeMap.begin()+it); // no need to do it because the close_file() will do this 

		pid_t pid;
		signal(SIGCHLD,signal_handler);//wait child
		pid = fork();
		
		if(pid == 0)
		{
			//dup file
			if(fd_to_write[1] != STDOUT_FILENO){  dup2(fd_to_write[1],STDOUT_FILENO); }
			if(fd_to_read[0] != STDIN_FILENO)  {  dup2(fd_to_read[0],STDIN_FILENO);   }
			//close file
			if(fd_to_write[1] != STDOUT_FILENO){  close(fd_to_write[1]); }
			if(fd_to_write[0] != STDIN_FILENO) {  close(fd_to_write[0]); }
			if(fd_to_read[1] != STDOUT_FILENO) {  close(fd_to_read[1]);  }
			if(fd_to_read[0] != STDIN_FILENO)  {  close(fd_to_read[0]);  }
			//execvp command
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ execvp(argv[0],argv); }
			exit(0);
		}
		else
		{
			//close this because the fd_to_read no need to write!!
			if(fd_to_read[1] != STDOUT_FILENO){ close(fd_to_read[1]); }
			//dup back
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stdin_copy,STDIN_FILENO);
			//store pipe
			pipefd temp = {-1,fd_to_write[1],fd_to_write[0]};
			pipeMap.push_back(temp);
			//signal handler 要在 父程序結束前 才有用
			//use this to let process to exe first so that the signal handler can be used
			sleep(1);

		}
    }
	//不存在0號管，直接執行execvp
    else{
		pid_t pid;
		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid = fork();
		
		if(pid == 0)
		{
			//dup file
			if(fd_to_write[1] != STDOUT_FILENO){ dup2(fd_to_write[1],STDOUT_FILENO); }
			//close file
			if(fd_to_write[1] != STDOUT_FILENO){ close(fd_to_write[1]); }
			if(fd_to_write[0] != STDIN_FILENO) { close(fd_to_write[0]); }
			//execvp command
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ execvp(argv[0],argv); }
			exit(0);
		}
		else
		{
			//dup back
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stdin_copy,STDIN_FILENO);
			// store pipe
			pipefd temp = { -1 , fd_to_write[1] , fd_to_write[0] };
			pipeMap.push_back(temp);
		}
    }
	return;
}

void numberpipe(int pipe_position , int pipe_number){
	/*
		Parameter:
			pipe_postion : the index that pipe(|1 or |2...) in the cmd
			pipe_number : the number this pipe should be store （要被存在幾號管）
		Work:
			the will deal with the normal pipe
	*/
	// find the index that we will use in findind command
	int this_pipe_position = pipe_position;
	int last_pipe_position = find_last_special_command(pipe_position);
	// cout<<this_pipe_position<<" "<<last_pipe_position<<endl;

	// this is to finad command that we need to execvp
	int index = 0;
	char* argv[MAX_ARGV_SIZE];
	for(int i = last_pipe_position+1 ; i < this_pipe_position;i++)
	{
		argv[index] = strdup(cmd[i].c_str());
		index++;
	}
	argv[index] = NULL;
	
	// check fd_to_write exist or not
	int fd_to_write[2];
    int it = find_pipe_num(pipe_number);
    if(it != -1)
    {
        fd_to_write[0] = pipeMap[it].fdout;
        fd_to_write[1] = pipeMap[it].fdin;
        pipeMap.erase(pipeMap.begin()+it); // need to erase because no one can deal with it, so that we need to deal with
    }
    else
	{
        if(pipe(fd_to_write) < 0 ){ perror("pipe 1 get wrong!"); }
    }

	//we need to see whether the 0號管 exist？
	int data_to_read = find_pipe_num(0);
	// 若是存在，就要stderror_number讀取0號管，再執行execvp 
	if(data_to_read !=-1)
	{
		int fd_to_read[2];
		fd_to_read[0] = pipeMap[data_to_read].fdout;
        fd_to_read[1] = pipeMap[data_to_read].fdin;
        // pipeMap.erase(pipeMap.begin()+data_to_read);// no need to do it because the close_file() will do this 

		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid_t pid;
		pid = fork();

		if (pid<0){ perror("fuck! numberpipe: "); }
		else if(pid == 0)
		{
			//dup file
			if(fd_to_read[0] != STDIN_FILENO)  { dup2(fd_to_read[0],STDIN_FILENO);   }
			if(fd_to_write[1] != STDOUT_FILENO){ dup2(fd_to_write[1],STDOUT_FILENO); }	
			//close file
			if(fd_to_read[0] != STDIN_FILENO)  { close(fd_to_read[0]);  }
			if(fd_to_read[1] != STDOUT_FILENO) { close(fd_to_read[1]);  }
			if(fd_to_write[1] != STDOUT_FILENO){ close(fd_to_write[1]); }
			if(fd_to_write[0] != STDIN_FILENO) { close(fd_to_write[0]); }
			//execvp
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ execvp(argv[0],argv); }
			exit(0);
		}
		else
		{	
			//dip back
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stdin_copy,STDIN_FILENO);
			//store pipe
			pipefd temp = {pipe_number,fd_to_write[1],fd_to_write[0]};
			pipeMap.push_back(temp);
			sleep(0.1);			
		}
	}
	// 若是不存在，就直接執行Execvp並存在管子裡面
	else{
		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid_t pid;
		pid = fork();

		if(pid == 0)
		{
			//dup file
			if(fd_to_write[1] != STDOUT_FILENO){ dup2(fd_to_write[1],STDOUT_FILENO); }
			//close file
			if(fd_to_write[1] != STDOUT_FILENO){ close(fd_to_write[1]);	}
			if(fd_to_write[0] != STDIN_FILENO) { close(fd_to_write[0]);	}
			//check the command exist or not
			if(!check_file_exist(argv[0]))//if not then dup the stdout to original and output wrong message
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ execvp(argv[0],argv); }
			exit(0);
		}
		else
		{
			//dup back
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stdin_copy,STDIN_FILENO);
			//store pipe
			pipefd temp = {pipe_number,fd_to_write[1],fd_to_write[0]};
			pipeMap.push_back(temp);
			sleep(0.1);
		}
	}
	return;
}

void normalstderror(int stderror_position) {
	/*
		Parameter:
			stderror_position : the index that stderr(!1 or !2...) in the cmd
			pipe_number : the number this stderr should be store （要被存在幾號管）
		Work:
			the will deal with the normal pipe
	*/
	// find the index that we will use in findind command
	int this_pipe_position = stderror_position;
	int last_pipe_position = find_last_special_command(stderror_position);
	// this is to finad command that we need to execvp
	int index = 0;
	char* argv[MAX_ARGV_SIZE];
	for(int i = last_pipe_position+1 ; i < this_pipe_position;i++)//last_pipe_position will return -1 or (last_pipe or last! last > )position ,so if we need to catch command the numbe rneed +1
	{
		argv[index] = strdup(cmd[i].c_str());
		index++;
	}
	argv[index] = NULL;

	// check fd_to_write exist or not
	int fd_to_write[2];
	if(pipe(fd_to_write) < 0 ){ perror("pipe 1 get wrong!"); }


	int data_to_read = find_pipe_num(0);//we need to see whether the 0號管 exist？
	// 若是存在，就要讀取0號管，再執行execvp 
	if(data_to_read !=-1)
	{
		int fd_to_read[2];
		fd_to_read[0] = pipeMap[data_to_read].fdout;
        fd_to_read[1] = pipeMap[data_to_read].fdin;
        // pipeMap.erase(pipeMap.begin()+data_to_read);

		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid_t pid;
		pid = fork();
	
		if(pid == 0)
		{
			//dup file
			if(fd_to_read[0] != STDIN_FILENO)  { dup2(fd_to_read[0],STDIN_FILENO);   }
			if(fd_to_write[1] != STDERR_FILENO){ dup2(fd_to_write[1],STDERR_FILENO); }
			if(fd_to_write[1] != STDOUT_FILENO){ dup2(fd_to_write[1],STDOUT_FILENO); }
			//close file
			if(fd_to_read[0] != STDIN_FILENO) { close(fd_to_read[0]);  }
			if(fd_to_read[1] != STDOUT_FILENO){ close(fd_to_read[1]);  }
			if(fd_to_write[0] != STDIN_FILENO){ close(fd_to_write[0]); }
			if(fd_to_write[1] != STDOUT_FILENO && fd_to_write[1] != STDERR_FILENO){ close(fd_to_write[1]); }
			//execvp command
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ execvp(argv[0],argv); }
			exit(0);
		}
		else
		{
			//close this because the fd_to_read no need to write!!
			if(fd_to_read[1] != STDOUT_FILENO){ close(fd_to_read[1]); }
			//dup back
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stderror_copy,2);
			dup2(stdin_copy,STDIN_FILENO);
			//store pipe
			pipefd temp = {-1,fd_to_write[1],fd_to_write[0]};
			pipeMap.push_back(temp);
			sleep(1);
		}
	}
	// 若是不存在，就直接執行Execvp並存在管子裡面
	else{
		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid_t pid;
		pid = fork();
		
		if(pid == 0)
		{
			//dup file
			if(fd_to_write[1] != STDOUT_FILENO){ dup2(fd_to_write[1],STDOUT_FILENO); }
			if(fd_to_write[1] != STDERR_FILENO){ dup2(fd_to_write[1],STDERR_FILENO); }
			//close file
			if(fd_to_write[1] != STDOUT_FILENO && fd_to_write[1] != STDERR_FILENO){ close(fd_to_write[1]); }
			if(fd_to_write[0] != STDIN_FILENO){ close(fd_to_write[0]); }
			//execvp command
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ execvp(argv[0],argv);	}
			exit(0);
		}
		else
		{
			//dup back
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stderror_copy,2);
			dup2(stdin_copy,STDIN_FILENO);
			//store pipe
			pipefd temp = {-1,fd_to_write[1],fd_to_write[0]};
			pipeMap.push_back(temp);
		}
	}
	return;	
}

void numberstderror(int stderror_position , int pipe_number) {
	/*
		Parameter:
			stderror_position : the index that stderr(!1 or !2...) in the cmd
			pipe_number : the number this stderr should be store （要被存在幾號管）
		Work:
			the will deal with the normal pipe
	*/
	// find the index that we will use in findind command
	int this_pipe_position = stderror_position;
	int last_pipe_position = find_last_special_command(stderror_position);
	// this is to finad command that we need to execvp
	int index = 0;
	char* argv[MAX_ARGV_SIZE];
	for(int i = last_pipe_position+1 ; i < this_pipe_position;i++)//last_pipe_position will return -1 or (last_pipe or last! last > )position ,so if we need to catch command the numbe rneed +1
	{
		argv[index] = strdup(cmd[i].c_str());
		index++;
	}
	argv[index] = NULL;

	// check fd_to_write exist or not
	int fd_to_write[2];
    int it = find_pipe_num(pipe_number);
    if(it != -1)
    {
        fd_to_write[0] = pipeMap[it].fdout;
        fd_to_write[1] = pipeMap[it].fdin;
        pipeMap.erase(pipeMap.begin()+it);
		// cout<<fd[0]<<" "<<fd[1]<<endl;
    }
    else{
        if(pipe(fd_to_write) < 0 ){ perror("pipe 1 get wrong!"); }

    }

	int data_to_read = find_pipe_num(0);//we need to see whether the 0號管 exist？
	// 若是存在，就要讀取0號管，再執行execvp 
	if(data_to_read !=-1)
	{
		int fd_to_read[2];
		fd_to_read[0] = pipeMap[data_to_read].fdout;
        fd_to_read[1] = pipeMap[data_to_read].fdin;
        // pipeMap.erase(pipeMap.begin()+data_to_read);

		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid_t pid;
		pid = fork();
	
		if(pid == 0)
		{
			//dup file
			if(fd_to_read[0] != STDIN_FILENO)  { dup2(fd_to_read[0],STDIN_FILENO);   }
			if(fd_to_write[1] != STDERR_FILENO){ dup2(fd_to_write[1],STDERR_FILENO); }
			if(fd_to_write[1] != STDOUT_FILENO){ dup2(fd_to_write[1],STDOUT_FILENO); }
			//close file
			if(fd_to_read[0] != STDIN_FILENO) { close(fd_to_read[0]);  }
			if(fd_to_read[1] != STDOUT_FILENO){ close(fd_to_read[1]);  }
			if(fd_to_write[0] != STDIN_FILENO){ close(fd_to_write[0]); }
			if(fd_to_write[1] != STDOUT_FILENO && fd_to_write[1] != STDERR_FILENO){ close(fd_to_write[1]); }
			//execvp command
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ execvp(argv[0],argv); }
			exit(0);
		}
		else
		{
			//dup back
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stderror_copy,2);
			dup2(stdin_copy,STDIN_FILENO);
			//store pipe
			pipefd temp = {pipe_number,fd_to_write[1],fd_to_write[0]};
			pipeMap.push_back(temp);
			sleep(0.1);
		}
	}
	// 若是不存在，就直接執行Execvp並存在管子裡面
	else{
		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid_t pid;
		pid = fork();
		
		if(pid == 0)
		{
			//dup file
			if(fd_to_write[1] != STDOUT_FILENO){ dup2(fd_to_write[1],STDOUT_FILENO); }
			if(fd_to_write[1] != STDERR_FILENO){ dup2(fd_to_write[1],STDERR_FILENO); }
			//close file
			if(fd_to_write[1] != STDOUT_FILENO && fd_to_write[1] != STDERR_FILENO){ close(fd_to_write[1]); }
			if(fd_to_write[0] != STDIN_FILENO){ close(fd_to_write[0]); }
			//execvp command
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ execvp(argv[0],argv);	}
			exit(0);
		}
		else
		{
			//dup back
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stderror_copy,2);
			dup2(stdin_copy,STDIN_FILENO);
			//store pipe
			pipefd temp = {pipe_number,fd_to_write[1],fd_to_write[0]};
			pipeMap.push_back(temp);
			sleep(0.1);
		}
	}
	return;	
}

void redirection_cmd( int redirection_index ){

	//initial the file to write
    int file_number = open(cmd[redirection_index+1].c_str(),O_RDWR|O_APPEND |O_CREAT|O_TRUNC , S_IWUSR|S_IRUSR);

	// this is to finad command that we need to execvp
    int index = 0;
    char* argv[MAX_ARGV_SIZE];
	// 處理direction 的指令要抓到哪裡
	int head = find_last_special_command(redirection_index);	
    for(int i = head+1 ; i < redirection_index;i++)//這裡的redirection file index要想怎抓
    {
        argv[index] = strdup(cmd[i].c_str());
		// cout<<argv[index]<<endl;
        index++;
    }
    argv[index] = NULL;

	int data_to_read = find_pipe_num(0);//we need to see whether the 0號管 exist？
	// 若是存在，就要讀取0號管，再執行execvp 
	if(data_to_read !=-1)
	{
		int fd_to_read[2];
		fd_to_read[0] = pipeMap[data_to_read].fdout;
        fd_to_read[1] = pipeMap[data_to_read].fdin;

		pid_t pid;
		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid = fork();
		if(pid==0)
		{
			
			//dup file
			dup2(file_number,STDOUT_FILENO);
			if(fd_to_read[0]!=STDIN_FILENO){ dup2(fd_to_read[0],STDIN_FILENO); }
			//close file
			if(fd_to_read[0]!=STDIN_FILENO) { close(fd_to_read[0]); }
			if(fd_to_read[1]!=STDOUT_FILENO){ close(fd_to_read[1]); }
			//execvp command
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ execvp(argv[0],argv);	}
			exit(0);
		}
		else{
			//dup back
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stdin_copy,STDIN_FILENO);
			//close file
			close(file_number);
			if(fd_to_read[1] != STDOUT_FILENO){ close(fd_to_read[1]); }
			if(fd_to_read[0] != STDIN_FILENO) { close(fd_to_read[0]); }
			int status;
			waitpid(pid , &status ,0);
		}
	}
	else
	{
		pid_t pid;
		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid = fork();

		if(pid==0)
		{
			dup2(file_number,STDOUT_FILENO);
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ execvp(argv[0],argv);	}
			exit(0);
		}
		else{
			//dup back
			dup2(stdout_copy,STDOUT_FILENO);
			//close file
			close(file_number);
			int status;
			waitpid(pid , &status ,0);
		}
	}
    return ;
}

void append_redirection(int redirection_index )
{
	//initial the file to write
    int file_number = open(cmd[redirection_index+1].c_str(),O_RDWR|O_APPEND|O_CREAT , S_IWUSR|S_IRUSR);

	// this is to finad command that we need to execvp
    int index = 0;
    char* argv[MAX_ARGV_SIZE];
	// 處理direction 的指令要抓到哪裡
	int head = find_last_special_command(redirection_index);	
    for(int i = head+1 ; i < redirection_index;i++)//這裡的redirection file index要想怎抓
    {
        argv[index] = strdup(cmd[i].c_str());
		// cout<<argv[index]<<endl;
        index++;
    }
    argv[index] = NULL;

	int data_to_read = find_pipe_num(0);//we need to see whether the 0號管 exist？
	// 若是存在，就要讀取0號管，再執行execvp 
	if(data_to_read !=-1)
	{
		int fd_to_read[2];
		fd_to_read[0] = pipeMap[data_to_read].fdout;
        fd_to_read[1] = pipeMap[data_to_read].fdin;

		pid_t pid;
		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid = fork();
		if(pid==0)
		{
			
			//dup file
			dup2(file_number,STDOUT_FILENO);
			if(fd_to_read[0]!=STDIN_FILENO){ dup2(fd_to_read[0],STDIN_FILENO); }
			//close file
			if(fd_to_read[0]!=STDIN_FILENO) { close(fd_to_read[0]); }
			if(fd_to_read[1]!=STDOUT_FILENO){ close(fd_to_read[1]); }
			//execvp command
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ execvp(argv[0],argv);	}
			exit(0);
		}
		else{
			//dup back
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stdin_copy,STDIN_FILENO);
			//close file
			close(file_number);
			if(fd_to_read[1] != STDOUT_FILENO){ close(fd_to_read[1]); }
			if(fd_to_read[0] != STDIN_FILENO) { close(fd_to_read[0]); }
			int status;
			waitpid(pid , &status ,0);
		}
	}
	else
	{
		pid_t pid;
		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid = fork();

		if(pid==0)
		{
			dup2(file_number,STDOUT_FILENO);
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ execvp(argv[0],argv);	}
			exit(0);
		}
		else{
			//dup back
			dup2(stdout_copy,STDOUT_FILENO);
			//close file
			close(file_number);
			int status;
			waitpid(pid , &status ,0);
		}
	}
    return ;
}

void last_cmd(int cmd_position ){
	/*
		Parameter: 
			cmd_position : this will be the last command!!!!
		Work: 
			exe final command
	*/
	
	int it = find_pipe_num(0);//find 0號管 有沒有存在
	//this means we need to do the final pipe
	if(it != -1){

		//initial pipe
        int fd_to_read[2];
        fd_to_read[0] = pipeMap[it].fdout;
        fd_to_read[1] = pipeMap[it].fdin;
        // pipeMap.erase(pipeMap.begin()+it);
        
		// find the index that we will use in findind command
		int this_pipe_position = cmd_position;
		int last_pipe_position = find_last_special_command(cmd_position);

		// this is to finad command that we need to execvp
		int index = 0;
		char* argv[MAX_ARGV_SIZE];
		for(int i = last_pipe_position+1 ; i <= this_pipe_position;i++)
		{
			argv[index] = strdup(cmd[i].c_str());
			index++;
		}
		argv[index] = NULL;

		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid_t pid;
		pid=fork();

		if (pid<0){ perror("fuck! normal fork");  }
		else if(pid == 0)
		{
			//dup file
			if(fd_to_read[0] != STDIN_FILENO){ dup2(fd_to_read[0],STDIN_FILENO); }
			//close file
            if(fd_to_read[1]!= STDOUT_FILENO){ close(fd_to_read[1]); }
            if(fd_to_read[0]!= STDIN_FILENO) { close(fd_to_read[0]); }
            //execvp command
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ execvp(argv[0],argv); }
			exit(0);
		}
		else
		{
			//close the file that no need to read!
			if(fd_to_read[1] != STDOUT_FILENO){ close(fd_to_read[1]); }
			//wait child process
            int status;
			waitpid(pid , &status ,0);
		}
	}
	else{
		// this is to finad command that we need to execvp
		//it will start to 0, because there is no other command!
		int index = 0;
		char* argv[MAX_ARGV_SIZE];
		for(int i = 0 ; i < cmd.size();i++)
		{
			argv[index] = strdup(cmd[i].c_str());
			index++;
		}
		argv[index] = NULL;

		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid_t pid;
		pid=fork();

		if (pid == -1){ perror("fork"); }
		else if(pid ==0)
		{
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ execvp(argv[0],argv);	}
			exit(0);
		}
		else
		{
            int status;
			waitpid(pid , &status ,0);
		}		
	}
	return ;
}

void run_command(){

	for (int i = 0 ; i < cmd.size() ; i++)
	{
		if(cmd[i][0]=='|' && cmd[i][1]>=49)
		{
			//catch the number we need to store
            string number="";
            for(int j = 1 ; j < cmd[i].length();j++){ number+=cmd[i][j]; }
            int num = stoi(number);

			numberpipe(i , num);
			close_file();
            pipe_number_order_decrease();
		}
        else if(cmd[i]=="|")
		{
			normalpipe(i);
			close_file();
			pipe_number_order_increase();
		}
		else if(cmd[i][0]=='!' && cmd[i][1]>=49)
		{
			//catch the number we need to store
            string number="";
            for(int j = 1 ; j < cmd[i].length();j++){ number+=cmd[i][j]; }
            int num = stoi(number);

			numberstderror(i , num);
			close_file();
            pipe_number_order_decrease();
		}
        else if(cmd[i]=="!")
		{
			normalstderror(i);
			close_file();
			pipe_number_order_increase();
		}
		else if(cmd[i]==">>")
		{
			append_redirection(i);
			close_file();
			pipe_number_order_decrease();
			break;
		}
		else if(cmd[i]==">")
		{
			redirection_cmd(i);
			close_file();
			pipe_number_order_decrease();
			break;
		}
		else if (cmd[i]=="printenv")
		{
			print_env(i);
			break;
		}
		else if (cmd[i]=="setenv")
		{
			set_env(i);
			break;
		}
		else if(i == cmd.size()-1 )//to deal with the pipe final case and normal case
		{
			last_cmd(i);
			close_file();
        	pipe_number_order_decrease();//let the pipe_number -1
		}
	}
	return ;
}

