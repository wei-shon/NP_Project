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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<sys/ipc.h>
#include<sys/shm.h>

using namespace std;
#define MAX_ARGV_SIZE 15000
#define MAX_USER 30
#define Client_Message 1025
#define Client_name 30
int const stdout_copy = dup(STDOUT_FILENO);
int const stdin_copy = dup(STDIN_FILENO);
int const stderror_copy = dup(STDERR_FILENO);
int devNull = open("/dev/null", O_WRONLY);
int User_shmid ; //
int Message_shmid;
int FileName_shmid;
int Group_shmid;

#define SIGUSR3 20 

struct user{
	int userid=0;
    pid_t pid;
	char nickname [Client_name]="(no name)";
    char client_addr[INET_ADDRSTRLEN] ;
    int port;
	bool userpipe_exist[MAX_USER];
};

struct Room{
	char RoomName[Client_name];
	int group_id [30];
};


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
vector<Room> RoomList;
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
void pipe_number_order_increase(int number);

//deal with command
void normalpipe(int pipe_position);
void numberpipe(int pipe_position , int pipe_number);
void normalstderror(int stderror_position);
void numberstderror(int stderror_position , int pipe_number) ;
void last_cmd(int pipe_position );
void redirection_cmd( int redirection_index );
void run_command(int socketfd , int id);



string buffer_spilit(string buf);
int connectTCP(int port);

//shared memory
static int CommShm(int size,int flags , string path);
int DestroyShm(int shmid);
void signal_share_memory_handler(int sig);
void signal_user_pipe_handler(int sig , int sender , int reciever);

void boardcastLogin(int id);
void boardcastLogout(int id);
void who(int id);
void welcomeMessage();
void nameUserName(int id);
void tell( int id , int friends);
void yell(int id);
void userpipeGive(int pipe_position , int userpipe_number , int id);
void userpipeTake(int pipe_position , int userpipe_number , int id);
void userpipeTake_with_another_commands(int pipe_position , int userpipe_number , int id);
void userpipe_GiveAndTake(int id);
void signal_open_file(int sig);
void signal_open_write_file(int sig);
void signal_exit_server(int sig);


void grouptell(int id);
void group(int id);
//
// main //
//
int main(int argc, char* argv[]){
	// signal(SIGUSR1,signal_open_file);
	signal(SIGINT,signal_exit_server);

	string input_command;
	setenv("PATH","bin:.",1);
	//创建套接字
	int msock = connectTCP(atoi(argv[1]));
	
	//build share memory
    User_shmid = CommShm(30*sizeof(user),IPC_CREAT | 0666 ,"/");
	Message_shmid = CommShm(Client_Message,IPC_CREAT | 0666 , "/tmp");
	FileName_shmid = CommShm(Client_Message,IPC_CREAT | 0666 , "/etc/tmpfiles.d");
	Group_shmid = CommShm(sizeof(Room),IPC_CREAT | 0666 , ".");
	cout<< User_shmid <<endl;
	cout << Message_shmid <<endl;
	cout << FileName_shmid <<endl;
	cout << Group_shmid <<endl;
	char* Message_share_memory_addr = (char*) shmat(Message_shmid,NULL,0);
	memset(Message_share_memory_addr,'\0', Client_Message);
	shmdt(Message_share_memory_addr);

	//reset User_share_memory_addr and Message_share_memory_addr content
	Room* Group_share_memory_addr = (Room*) shmat(Group_shmid,NULL,0);
	memset(Group_share_memory_addr->RoomName , '\0' , Client_name*sizeof(char));
	memset(Group_share_memory_addr->group_id , 0 , 30*sizeof(int));
	shmdt(Group_share_memory_addr);
	//reset User_share_memory_addr and Message_share_memory_addr content
	user* User_share_memory_addr = (user*) shmat(User_shmid,NULL,0);
	for(int j = 0 ; j < MAX_USER ; j++)
	{
		User_share_memory_addr[j].userid=0;
		memset(User_share_memory_addr[j].nickname , '\0' , Client_name*sizeof(char));
		memset(User_share_memory_addr[j].client_addr , '\0' , INET_ADDRSTRLEN*sizeof(char));
		User_share_memory_addr[j].port = 0;
		User_share_memory_addr[j].pid=0;
		memset(User_share_memory_addr[j].userpipe_exist , false , INET_ADDRSTRLEN*sizeof(bool));
	}
	// cout<<"done!"<<endl;
	shmdt(User_share_memory_addr);
    //接收客户端请求
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    while (true) {
        // std::cout << "...listening" << std::endl;
        int clnt_sock = accept(msock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if (clnt_sock < 0) {
            continue;
        }

		pid_t pid;

		pid = fork();
		if(pid==0){
			close(msock);
            pid_t pid2 = getpid();

			user* User_share_memory_addr = (user*) shmat(User_shmid,NULL,0);
			// cout<<User_share_memory_addr<<endl;
			int UsrId=0;
			for(int j = 0 ; j < MAX_USER ; j++)
			{
				if( User_share_memory_addr[j].userid == 0 )
				{
					UsrId=j+1;
					break;
				}
			}
			User_share_memory_addr[UsrId-1].userid = UsrId;
            User_share_memory_addr[UsrId-1].pid = pid2;
			User_share_memory_addr[UsrId-1].port = (int) htons(clnt_addr.sin_port);
			string name = "(no name)";
			strncpy(User_share_memory_addr[UsrId-1].nickname, name.c_str() , Client_name);
			strncpy(User_share_memory_addr[UsrId-1].client_addr,inet_ntoa(clnt_addr.sin_addr) , INET_ADDRSTRLEN);
			shmdt(User_share_memory_addr);

			// shmdt(User_share_memory_addr);
			dup2(clnt_sock,STDOUT_FILENO);
			dup2(clnt_sock,STDERR_FILENO);
			dup2(clnt_sock,STDIN_FILENO);
			welcomeMessage();
			// cout << User_share_memory_addr[UsrId-1].port<<endl;
			signal(SIGUSR3,signal_share_memory_handler);
			signal(SIGUSR2,signal_open_file);
			signal(SIGUSR1,signal_open_write_file);
            boardcastLogin(UsrId);

			while (true) {
                
				cout<<"% ";
				getline(cin,input_command);
				input_command = buffer_spilit(input_command);

				// cout<<input_command<<endl;
				if(input_command=="exit"){ 
					cout<<UsrId<<endl;
					signal(SIGUSR3,signal_share_memory_handler);
					boardcastLogout(UsrId);
                    exit(0);
                }
				split_command(input_command);

				//run command
				run_command(clnt_sock , UsrId);

				//clear cmd, special command position			
				cmd.clear();
				cmd.shrink_to_fit();
				all_pipe_direction_position.clear();
				
			}	
		}
		else{
			close(clnt_sock);
		}
		
    }

    DestroyShm(User_shmid);
    DestroyShm(Message_shmid);
    DestroyShm(FileName_shmid);
	//关闭套接字
    close(msock);
	return 0;
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

void signal_share_memory_handler(int sig){
	char* Message_share_memory_addr = (char*) shmat(Message_shmid,NULL,0);
	if(Message_share_memory_addr == (char*)-1)
	{
		cerr<<"Message_share_memory_addr : singal handler"<<endl;
	}
	cout<< Message_share_memory_addr <<endl;

	shmdt(Message_share_memory_addr);
    return;
}
void signal_open_file(int sig){
	char* FileName_share_memory_addr = (char*) shmat(FileName_shmid,NULL,0);
	if(FileName_share_memory_addr == (char*)-1)
	{
		cerr<<"FileName_share_memory_addr : singal handler"<<endl;
	}
	// cout<< "in" <<endl;

	const char* myfifo = FileName_share_memory_addr;
	int fd = open(myfifo , O_RDONLY , S_IRUSR);
	// sleep(1);
	// close(fd);
	return;
}

void signal_open_write_file(int sig){
	char* FileName_share_memory_addr = (char*) shmat(FileName_shmid,NULL,0);
	if(FileName_share_memory_addr == (char*)-1)
	{
		cerr<<"FileName_share_memory_addr : singal handler"<<endl;
	}
	// cout<< "in" <<endl;

	const char* myfifo = FileName_share_memory_addr;
	int fd = open(myfifo , O_WRONLY , S_IRUSR);
	// sleep(1);
	close(fd);
	return;
}

void signal_exit_server(int sig){
	DestroyShm(User_shmid);
    DestroyShm(Message_shmid);
    DestroyShm(FileName_shmid);
	exit(0);
	return;
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

void pipe_number_order_increase(int number){
	/*
		Work :
			After We finish a command, We nned to make number pipe to increase.
			Becasue We will get 0號管 to execvp, and then We set normal pipe is -1
			We need to +1. that it will get into 0號管, so that it will be execvp.
	*/
	for( int i = 0 ; i < pipeMap.size() ; i++)
    {
        if(pipeMap[i].number_to_pipe == -1 || pipeMap[i].number_to_pipe == 0) 
		{
			pipeMap[i].number_to_pipe+=number;
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
	 if(cmd[pipe_position-1][0]=='<')
    {
        pipe_number_order_decrease();
        return;
    }
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
		close(fd_to_read[1]);
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
			// if(fd_to_read[1] != STDOUT_FILENO) {  close(fd_to_read[1]);  }
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
			// if(fd_to_read[1] != STDOUT_FILENO){ close(fd_to_read[1]); }
			//dup back
			// dup2(stdout_copy,STDOUT_FILENO);
			// dup2(stdin_copy,STDIN_FILENO);
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
			if(execvp(argv[0],argv) < 0)
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			exit(0);
		}
		else
		{
			//dup back
			// dup2(stdout_copy,STDOUT_FILENO);
			// dup2(stdin_copy,STDIN_FILENO);
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
	if(cmd[pipe_position-1][0]=='<')
    {
        pipe_number_order_increase(pipe_number);
        return;
    }
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
		close(fd_to_read[1]);
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
			if(execvp(argv[0],argv) < 0)
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			exit(0);
		}
		else
		{	
			//dip back
			// dup2(stdout_copy,STDOUT_FILENO);
			// dup2(stdin_copy,STDIN_FILENO);
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
			if(execvp(argv[0],argv) < 0)
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			exit(0);
		}
		else
		{
			//dup back
			// dup2(stdout_copy,STDOUT_FILENO);
			// dup2(stdin_copy,STDIN_FILENO);
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
		close(fd_to_read[1]);
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
			if(execvp(argv[0],argv) < 0)
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			exit(0);
		}
		else
		{
			//close this because the fd_to_read no need to write!!
			if(fd_to_read[1] != STDOUT_FILENO){ close(fd_to_read[1]); }
			//dup back
			// dup2(stdout_copy,STDOUT_FILENO);
			// dup2(stderror_copy,2);
			// dup2(stdin_copy,STDIN_FILENO);
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
			if(execvp(argv[0],argv) < 0)
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			exit(0);
		}
		else
		{
			//dup back
			// dup2(stdout_copy,STDOUT_FILENO);
			// dup2(stderror_copy,2);
			// dup2(stdin_copy,STDIN_FILENO);
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
		close(fd_to_read[1]);

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
			if(execvp(argv[0],argv) < 0)
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			exit(0);
		}
		else
		{
			//dup back
			// dup2(stdout_copy,STDOUT_FILENO);
			// dup2(stderror_copy,2);
			// dup2(stdin_copy,STDIN_FILENO);
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
			if(execvp(argv[0],argv) < 0)
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			exit(0);
		}
		else
		{
			//dup back
			// dup2(stdout_copy,STDOUT_FILENO);
			// dup2(stderror_copy,2);
			// dup2(stdin_copy,STDIN_FILENO);
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
			if(execvp(argv[0],argv) < 0)
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			exit(0);
		}
		else{
			//dup back
			// dup2(stdout_copy,STDOUT_FILENO);
			// dup2(stdin_copy,STDIN_FILENO);
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
			if(execvp(argv[0],argv) < 0)
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			exit(0);
		}
		else{
			//dup back
			// dup2(stdout_copy,STDOUT_FILENO);
			//close file
			close(file_number);
			int status;
			waitpid(pid , &status ,0);
		}
	}
    return ;
}

void append_redirection(int redirection_index ){
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
			if(execvp(argv[0],argv) < 0)
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
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
			if(execvp(argv[0],argv) < 0)
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
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
	// cout<<it<<endl;
	if(it != -1){

		//initial pipe
        int fd_to_read[2];
        fd_to_read[0] = pipeMap[it].fdout;
        fd_to_read[1] = pipeMap[it].fdin;
		close(fd_to_read[1]);
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
			if(execvp(argv[0],argv) < 0)
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
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
			if(execvp(argv[0],argv) < 0)
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
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

void run_command(int socketfd , int id){

	for (int i = 0 ; i < cmd.size() ; i++)
	{
		dup2(socketfd,STDOUT_FILENO);
		dup2(socketfd,STDERR_FILENO);
		dup2(socketfd,STDIN_FILENO);
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
			pipe_number_order_increase(1);
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
			pipe_number_order_increase(1);
		}
		else if(cmd[i]==">>")
		{
			append_redirection(i);
			close_file();
			pipe_number_order_decrease();
			break;
		}
		else if( i!=cmd.size()-1 && ( (cmd[i][0]=='>' && cmd[i][1]>=49 && cmd[i+1][0]=='<' && cmd[i+1][1]>=49) || (cmd[i+1][0]=='>' && cmd[i+1][1]>=49 && cmd[i][0]=='<' && cmd[i][1]>=49) )  )
		{
			//in this condition may be :
			// 1. ls >1 <2 
			// 2. ls <2 >1

			signal(SIGUSR1,signal_open_write_file);
			signal(SIGUSR2,signal_open_file);
			signal(SIGUSR3,signal_share_memory_handler);
			userpipe_GiveAndTake(id);
			close_file();
			pipe_number_order_decrease();
			break;
			

		}
		else if(cmd[i][0]=='>' && cmd[i][1]>=49)
		{
            //catch the number we need to store
            string number="";
            for(int j = 1 ; j < cmd[i].length();j++){ number+=cmd[i][j]; }
            int num = stoi(number);

			signal(SIGUSR3,signal_share_memory_handler);
			signal(SIGUSR2,signal_open_file);
			userpipeGive(i , num , id);
			close_file();
			pipe_number_order_decrease();
			break;
		}
        else if(cmd[i][0]=='<' && cmd[i][1]>=49)
		{
            if(i != cmd.size()-1 )
            {
                // //it means there still another command behind <2
				// // or we need deal with userpipe_should_be_Cat_first it means we need pipe to another user
                string number="";
                for(int j = 1 ; j < cmd[i].length();j++){ number+=cmd[i][j]; }
                int num = stoi(number);

				signal(SIGUSR3,signal_share_memory_handler);
				signal(SIGUSR1,signal_open_write_file);
                userpipeTake_with_another_commands(i , num  , id);	
                close_file();
                pipe_number_order_increase(1);
            }
            else{
				// into this because behind < we have no others command so we need output on client's monitor
                //catch the number we need to store
                string number="";
                for(int j = 1 ; j < cmd[i].length();j++){ number+=cmd[i][j]; }
                int num = stoi(number);

				signal(SIGUSR3,signal_share_memory_handler);
				signal(SIGUSR1,signal_open_write_file);
                userpipeTake(i , num , id);
                close_file();
                pipe_number_order_decrease();
            }
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
		else if(cmd[i]=="who")
        {
            who(id);
            break;
        }
        else if(cmd[i]=="name")
        {
			signal(SIGUSR3,signal_share_memory_handler);
            nameUserName(id);
            break;
        }
        else if(cmd[i]=="tell")
        {
			signal(SIGUSR3,signal_share_memory_handler);
            tell(id , stoi(cmd[1]));
            break;
        }
        else if(cmd[i]=="yell")
        {
			signal(SIGUSR3,signal_share_memory_handler);
            yell(id);
            break;
        }
		else if(cmd[i]=="group")
		{
			group(id);
			break;
		}
		else if(cmd[i]=="grouptell")
		{
			signal(SIGUSR3,signal_share_memory_handler);
			grouptell(id);
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

string buffer_spilit(string buf)
{
    string word="";
    for(int i = 0 ; i < buf.size() ; i++)
    {
        if ( 31 < int(buf[i]) && int(buf[i]) < 126)
        {
            word+=buf[i];
        }
    }
    return word;
}

int connectTCP(int port){
	int msock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    //将套接字和IP、端口绑定
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));  //每个字节都用0填充
    serv_addr.sin_family = AF_INET;  //使用IPv4地址
    serv_addr.sin_addr.s_addr = INADDR_ANY;  //具体的IP地址 
    
    serv_addr.sin_port = htons(port);  //端口
	
	int optval = 1;
	setsockopt(msock, SOL_SOCKET, SO_REUSEADDR , &optval, sizeof(optval));

	bind(msock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    //进入监听状态，等待用户发起请求
    listen(msock, 0);
	return msock;
}

static int CommShm(int size,int flags , string path)
{
	key_t key = ftok(path.c_str(),1);
	if(key < 0)
	{
		perror("ftok");
		return -1;
	}
	int shmid = 0;
	if((shmid = shmget(key,size,flags)) < 0)
	{
		perror("shmget");
		return -2;
	}
	return shmid;
}
int DestroyShm(int shmid)
{
	if(shmctl(shmid,IPC_RMID,NULL) < 0)
	{
		perror("shmctl");
		return -1;
	}
	return 0;
}



void boardcastLogin(int id)
{
	
	user* User_share_memory_addr = (user*) shmat(User_shmid,NULL,0);
	if(User_share_memory_addr == (user*)-1)
	{
		cerr<<"User_share_memory_addr : boardcastLogin"<<endl;
	}
	char* Message_share_memory_addr = (char*) shmat(Message_shmid,NULL,0);
	if(Message_share_memory_addr == (char*)-1)
	{
		cerr<<"Message_share_memory_addr : boardcastLogin"<<endl;
	}
    // cout<<Message_share_memory_addr<<endl;
	memset(Message_share_memory_addr,'\0', Client_Message);

	string message="";
	message+="*** User '";
	message+=User_share_memory_addr[id-1].nickname;
	message+="' entered from ";
	message+=User_share_memory_addr[id-1].client_addr;
	message+=':';
	message+=to_string(User_share_memory_addr[id-1].port);
	message+=". ***";
	strncpy(Message_share_memory_addr , message.c_str() , Client_Message);

	// cout<<User_share_memory_addr[id-1].nickname<<endl;

	//signal everyone
	for( int i = 0 ; i < MAX_USER ; i++)
	{
		// cout<<i<<endl;
		if(User_share_memory_addr[i].userid!=0)
		{
			if(kill( User_share_memory_addr[i].pid , SIGUSR3) <0)
			{
				cerr<<"kill problem"<<endl;
			}
		}
	}

	shmdt(Message_share_memory_addr);
	shmdt(User_share_memory_addr);
    return;
}

void boardcastLogout(int id){

	string message="";
	char* Message_share_memory_addr = (char*) shmat(Message_shmid,NULL,0);
	if(Message_share_memory_addr == (char*)-1)
	{
		cerr<<"Message_share_memory_addr : boardcastLogout"<<endl;
	}
	memset(Message_share_memory_addr,'\0', Client_Message);

    user* User_share_memory_addr = (user*) shmat(User_shmid,NULL,0);
	if(User_share_memory_addr == (user*)-1)
	{
		cerr<<"User_share_memory_addr : boardcastLogout"<<endl;
	}
	Room* Group_share_memory_addr = (Room*) shmat(Group_shmid,NULL,0);
	if(Group_share_memory_addr == (Room*)-1)
	{
		cerr<<"Group_share_memory_addr : boardcastLogin"<<endl;
	}
	message+="*** User '";
	message+=User_share_memory_addr[id-1].nickname;
	message+="' left. ***";

	strncpy(Message_share_memory_addr , message.c_str() , Client_Message);
	//signal everyone
	for( int i = 0 ; i < MAX_USER ; i++)
	{
		if(User_share_memory_addr[i].userid!=0)
		{
			kill( User_share_memory_addr[i].pid , SIGUSR3);
		}
	}
	

	for(int j = 0 ; j < 30 ; j++)
	{
		if (Group_share_memory_addr->group_id[j]==id)
		{
			Group_share_memory_addr->group_id[j] = 0;
			break;
		}
	}
		
	User_share_memory_addr[id-1].userid = 0;
	memset(User_share_memory_addr[id-1].nickname , '\0' , Client_name*sizeof(char));
	memset(User_share_memory_addr[id-1].client_addr , '\0' , INET_ADDRSTRLEN*sizeof(char));
	memset(User_share_memory_addr[id-1].userpipe_exist , false , INET_ADDRSTRLEN*sizeof(bool));
	//in order to clean the exit user's exist Map avoid that anyone can access its share memory
	for(int i = 0 ; i < MAX_USER ; i++)
	{
		User_share_memory_addr[i].userpipe_exist[id-1] = false;
	}
	User_share_memory_addr[id-1].port = 0;
	User_share_memory_addr[id-1].pid=0;
	shmdt(Message_share_memory_addr);
	shmdt(User_share_memory_addr);
	shmdt(Group_share_memory_addr);
    return;
}

void who(int id)
{
    // in order to print name orderly from ID1 to the last
	user* User_share_memory_addr = (user*) shmat(User_shmid,NULL,0);
	if(User_share_memory_addr == (user*)-1)
	{
		cerr<<"User_share_memory_addr : boardcastLogin"<<endl;
	}

    cout<<"<ID>\t<nickname>\t<IP:port>\t<indicate me>"<<endl;
    for(int i = 0 ; i < MAX_USER ; i++)
    {
		if(User_share_memory_addr[i].userid != 0 )
		{
			if(User_share_memory_addr[i].userid==id)
			{
				cout<<User_share_memory_addr[i].userid<<'\t'<<User_share_memory_addr[i].nickname<<'\t'<<User_share_memory_addr[i].client_addr<<':'<<User_share_memory_addr[i].port<<"\t<-me"<<endl;
			}
			else{
				cout<<User_share_memory_addr[i].userid<<'\t'<<User_share_memory_addr[i].nickname<<'\t'<<User_share_memory_addr[i].client_addr<<':'<<User_share_memory_addr[i].port<<endl;
			}
		}
        
    }
	shmdt(User_share_memory_addr);
    return;
}

void group(int id)
{
	Room* Group_share_memory_addr = (Room*) shmat(Group_shmid,NULL,0);
	if(Group_share_memory_addr == (Room*)-1)
	{
		cerr<<"Group_share_memory_addr : boardcastLogin"<<endl;
	}
	strncpy(Group_share_memory_addr->RoomName, cmd[1].c_str() , Client_name);
	for(int i = 2 ; i < cmd.size() ; i++)
	{
		// cout<<cmd[i]<<endl;
		for(int j = 0 ; j < 30 ; j++)
		{
			if (Group_share_memory_addr->group_id[j]==0)
			{
				Group_share_memory_addr->group_id[j] = stoi(cmd[i]);
				break;
			}
		}
		
	}
	shmdt(Group_share_memory_addr);
	return ;
}
void grouptell(int id)
{
	
	Room* Group_share_memory_addr = (Room*) shmat(Group_shmid,NULL,0);
	if(Group_share_memory_addr == (Room*)-1)
	{
		cerr<<"Group_share_memory_addr : boardcastLogin"<<endl;
	}
	char* Message_share_memory_addr = (char*) shmat(Message_shmid,NULL,0);
	if(Message_share_memory_addr == (char*)-1)
	{
		cerr<<"Message_share_memory_addr : boardcastLogin"<<endl;
	}
	memset(Message_share_memory_addr,'\0', Client_Message);
	//initial User info
	user* User_share_memory_addr = (user*) shmat(User_shmid,NULL,0);
	if(User_share_memory_addr == (user*)-1)
	{
		cerr<<"User_share_memory_addr : boardcastLogin"<<endl;
	}


	string roomName = cmd[1];

	bool ID_exist = false;
	for(int i = 0 ; i < 30 ; i++)
	{
		// cout<<Group_share_memory_addr->group_id[i]<<endl;
		if(id == Group_share_memory_addr->group_id[i])
		{
			ID_exist = true;
		}
	}
	if(!ID_exist)
	{
		cout<<"ERROR : You are not in "<<roomName<<endl;
	}
	else{
		string message = "";
		for(int i = 2 ; i < cmd.size() ; i++)
		{
			message+=cmd[i];
		}
		strncpy(Message_share_memory_addr , message.c_str() , Client_Message);
		for(int j = 0 ; j < 30 ; j++)
		{
			for (int i = 0 ; i < MAX_USER ; i++)
			{
				if(User_share_memory_addr[i].userid!=0 && User_share_memory_addr[i].userid==Group_share_memory_addr->group_id[j])
				{
					kill( User_share_memory_addr[i].pid , SIGUSR3);
				}
			}

		}
	}

	shmdt(Message_share_memory_addr);
	shmdt(Group_share_memory_addr);
	shmdt(User_share_memory_addr);
	return;
}

void welcomeMessage()
{
	cout<<"****************************************"<<endl;
	cout<<"** Welcome to the information server. **"<<endl;
	cout<<"****************************************"<<endl;
    return;
}

void nameUserName(int id)
{
	//initial message
	string message="";
	char* Message_share_memory_addr = (char*) shmat(Message_shmid,NULL,0);
	if(Message_share_memory_addr == (char*)-1)
	{
		cerr<<"Message_share_memory_addr : boardcastLogin"<<endl;
	}
	memset(Message_share_memory_addr,'\0', Client_Message);

	//initial user share memory
	user* User_share_memory_addr = (user*) shmat(User_shmid,NULL,0);
	if(User_share_memory_addr == (user*)-1)
	{
		cerr<<"User_share_memory_addr : boardcastLogin"<<endl;
	}

    //check the name is used or not
    for(int i = 0 ; i < MAX_USER ; i++)
    {
		if(User_share_memory_addr[i].userid==0)
		{
			continue;
		}
        else if (User_share_memory_addr[i].nickname==cmd[1])
        {
            cout<<"*** User '"<<cmd[1]<<"' already exists. ***"<<endl;
            return;
        }
    }
    // if not we name it
    for(int i = 0 ; i < MAX_USER ; i++)
    {
        if(User_share_memory_addr[i].userid==id)
        {
			strncpy(User_share_memory_addr[i].nickname, cmd[1].c_str() , Client_name);
			message+="*** User from ";
			message+=User_share_memory_addr[i].client_addr;
			message+=":";
			message+=to_string(User_share_memory_addr[i].port);
			message+=" is named '";
			message+=User_share_memory_addr[i].nickname;
			message+="'. ***";
			
        }
    }
	strncpy(Message_share_memory_addr , message.c_str() , Client_Message);

	//signal everyone
	for( int i = 0 ; i < MAX_USER ; i++)
	{
		if(User_share_memory_addr[i].userid!=0)
		{
			kill( User_share_memory_addr[i].pid , SIGUSR3);
		}
	}

	// signal(SIGUSR3,signal_share_memory_handler);
	shmdt(Message_share_memory_addr);
	shmdt(User_share_memory_addr);
    return;
}

void tell(int id , int friends)
{
	//initial message
	string message="";
	char* Message_share_memory_addr = (char*) shmat(Message_shmid,NULL,0);
	if(Message_share_memory_addr == (char*)-1)
	{
		cerr<<"Message_share_memory_addr : boardcastLogin"<<endl;
	}
	memset(Message_share_memory_addr,'\0', Client_Message);

	//initial user share memory
	user* User_share_memory_addr = (user*) shmat(User_shmid,NULL,0);
	if(User_share_memory_addr == (user*)-1)
	{
		cerr<<"User_share_memory_addr : boardcastLogin"<<endl;
	}

    //check the user that be telled exist or not
    bool findFriends = false;
    for(int i = 0 ; i < MAX_USER ; i++)
    {
        if (User_share_memory_addr[i].userid==friends)
        {
            findFriends = true;
        }
    }
    if(!findFriends)
    {
        cout<<"*** Error: user #"<<friends<<" does not exist yet. ***"<<endl;
    }
    else{
        // input message to friends
		message+="*** ";
		message+=User_share_memory_addr[id-1].nickname;
		message+=" told you ***: ";
        for(int i = 2 ; i < cmd.size() ; i++)
        {
			message+=cmd[i];
			if (i!=cmd.size()-1)
			{
				message+=" ";
			}
        }
    }

	strncpy(Message_share_memory_addr , message.c_str() , Client_Message);

	for( int i = 0 ; i < MAX_USER ; i++)
	{
		if(User_share_memory_addr[i].userid==friends)
		{
			kill( User_share_memory_addr[i].pid , SIGUSR3);
		}
	}

	shmdt(Message_share_memory_addr);
	shmdt(User_share_memory_addr);
    return;

}

void yell(int id)
{
	//initial message
	string message="";
	char* Message_share_memory_addr = (char*) shmat(Message_shmid,NULL,0);
	if(Message_share_memory_addr == (char*)-1)
	{
		cerr<<"Message_share_memory_addr : boardcastLogin"<<endl;
	}
	memset(Message_share_memory_addr,'\0', Client_Message);

	//initial user share memory
	user* User_share_memory_addr = (user*) shmat(User_shmid,NULL,0);
	if(User_share_memory_addr == (user*)-1)
	{
		cerr<<"User_share_memory_addr : boardcastLogin"<<endl;
	}
	message+="*** ";
	message+=User_share_memory_addr[id-1].nickname;
	message+=" yelled ***: ";
	for(int k = 1 ; k < cmd.size() ; k++)
	{
		message+=cmd[k];
		if (k!=cmd.size()-1)
		{
			message+=" ";
		}
	}
	strncpy(Message_share_memory_addr , message.c_str() , Client_Message);

	//signal everyone
	for( int i = 0 ; i < MAX_USER ; i++)
	{
		if(User_share_memory_addr[i].userid!=0)
		{
			kill( User_share_memory_addr[i].pid , SIGUSR3);
		}
	}

	// signal(SIGUSR3,signal_share_memory_handler);
	shmdt(Message_share_memory_addr);
	shmdt(User_share_memory_addr);
    return;
}

void userpipeGive(int pipe_position , int userpipe_number , int id)
{
	/*
		Parameter:
			pipe_position : where do '>' index in cmd  
			userpipe_number : which userid we should pipe
		Work:
			To pipe something to User. 
			But we will save the pipe in Sender's UserpipeMap.
			If Reciever want to take it, it should find sender first!
	*/
	//initial Message content
	string message="";
	char* Message_share_memory_addr = (char*) shmat(Message_shmid,NULL,0);
	if(Message_share_memory_addr == (char*)-1)
	{
		cerr<<"Message_share_memory_addr : boardcastLogin"<<endl;
	}
	// memset(Message_share_memory_addr,'\0', Client_Message);
	//initial 
	string FileName="";
	char* FileName_share_memory_addr = (char*) shmat(FileName_shmid,NULL,0);
	if(FileName_share_memory_addr == (char*)-1)
	{
		cerr<<"FileName_share_memory_addr : boardcastLogin"<<endl;
	}
	//initial User info
	user* User_share_memory_addr = (user*) shmat(User_shmid,NULL,0);
	if(User_share_memory_addr == (user*)-1)
	{
		cerr<<"User_share_memory_addr : boardcastLogin"<<endl;
	}

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
        // cout<<argv[index]<<endl;
		index++;
	}
	argv[index] = NULL;

	// check user is life
    bool life_or_not = false; // cheack user is live or not
    bool the_user_exit = true;// is same with life_or_not to check user live or not
    for(int i = 0 ; i < MAX_USER ; i++)
    {
        if(User_share_memory_addr[i].userid == userpipe_number)
        {
            life_or_not= true;
        }
    }
    if(!life_or_not)
    {
        the_user_exit=false;
        cout<<"*** Error: user #"<< userpipe_number <<" does not exist yet. ***"<<endl;
		if(!check_file_exist(argv[0]))
		{
			cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
		}
		return;
    }
	// if(life_or_not && !check_file_exist(argv[0]))
	// {
	// 	cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
	// }
    //cheack Userpipe exist or not!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!要記得做
    if(User_share_memory_addr[id-1].userpipe_exist[userpipe_number-1]==true)
    {
        cout<<"*** Error: the pipe #"<<id<<"->#"<<userpipe_number<<" already exists. ***"<<endl;
        return ;
    }

    //scratch the command
    string command="";
    for(int i = 0 ; i < cmd.size();i++)
	{
        if(i == cmd.size()-1)
		{
        	command+=cmd[i]; 
		}
		else{
			command+=cmd[i]; 
        	command+=' ';
		}
	}



	// check fd_to_write exist or not
	int writefd;
	int myfifo=0;
    // FIFO file path
	string filename="/tmp/"+to_string(id)+"to"+to_string(userpipe_number)+"file";
    const char * fifo_name = filename.c_str();
	if (access(fifo_name, F_OK) == -1)
    {
        // 管道文件不存在
        // 创建命名管道
        myfifo = mkfifo(fifo_name,  S_IWUSR|S_IRUSR);
        if (myfifo != 0)
        {
            cerr<<"Could not create fifo :"<<fifo_name<<endl;
            exit(EXIT_FAILURE);
        }
    }
	// pass file name to FileName_share_memory_addr and let the reader open file first in order to let writer can write.
	//Since the File must read and write at the same time!!
	FileName+=filename;
	memset(FileName_share_memory_addr,'\0', Client_Message);
	strncpy(FileName_share_memory_addr , FileName.c_str() , Client_Message);
	kill( User_share_memory_addr[userpipe_number-1].pid , SIGUSR2);
	//check writefd 
	writefd = open(fifo_name, O_WRONLY |O_CREAT   , S_IWUSR|S_IRUSR);
	if( writefd < 0 )
	{
		cerr<<"open write failed"<<endl;
	}
	else{
	    //if write exist and then boardcast send message
		User_share_memory_addr[id-1].userpipe_exist[userpipe_number-1] = true;
		for (int i = 0 ; i < MAX_USER ; i++)
		{
			if(User_share_memory_addr[i].userid == userpipe_number)
			{
				message="";
				memset(Message_share_memory_addr,'\0', Client_Message);
				message+="*** ";
				message+=User_share_memory_addr[id-1].nickname;
				message+=" (#";
				message+=to_string(User_share_memory_addr[id-1].userid);
				message+=") just piped '";
				message+=command;
				message+="' to ";
				message+=User_share_memory_addr[i].nickname;
				message+=" (#";
				message+=to_string(User_share_memory_addr[i].userid);
				message+=")  ***";
				
			}
		}
		strncpy(Message_share_memory_addr , message.c_str() , Client_Message);
		for( int i = 0 ; i < MAX_USER ; i++)
		{
			if(User_share_memory_addr[i].userid!=0)
			{
				kill( User_share_memory_addr[i].pid , SIGUSR3 );
			}
		}
	}

	
	// 若是存在，就要stderror_number讀取0號管，再執行execvp 
	int data_to_read = find_pipe_num(0);
	if(data_to_read !=-1)
	{
		int fd_to_read[2];
		fd_to_read[0] = pipeMap[data_to_read].fdout;
        fd_to_read[1] = pipeMap[data_to_read].fdin;
        close(fd_to_read[1]);

        // pipeMap.erase(pipeMap.begin()+data_to_read);// no need to do it because the close_file() will do this 

		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid_t pid;
		pid = fork();

		if (pid<0){ perror("fuck! numberpipe: "); }
		else if(pid == 0)
		{
            
			//dup file
			if(fd_to_read[0] != STDIN_FILENO)  { dup2(fd_to_read[0],STDIN_FILENO);   }
			if(fd_to_read[0] != STDIN_FILENO)  { close(fd_to_read[0]);  }
			// if(fd_to_read[1] != STDOUT_FILENO) { close(fd_to_read[1]);  }
            //close file
            if(the_user_exit)
            {
                if(writefd != STDOUT_FILENO){ dup2(writefd,STDOUT_FILENO); }	
                if(writefd != STDOUT_FILENO){ close(writefd); }
            }
			//execvp
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ 
        		if(!the_user_exit)
                {
                    dup2(devNull,STDOUT_FILENO);
                }
                execvp(argv[0],argv); 
            }
			exit(0);
		}
		else
		{	
			close(writefd);
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
			//close file
			if(the_user_exit)
            {
                if(writefd != STDOUT_FILENO){ 
					if( dup2(writefd,STDOUT_FILENO)<0)
					{
						cerr<<"fuck! dup2"<<endl;
					} 
				}	
                if(writefd != STDOUT_FILENO){ close(writefd); }
            }
			//check the command exist or not
            // dup2(stdout_copy,STDOUT_FILENO);
			if(!check_file_exist(argv[0]))//if not then dup the stdout to original and output wrong message
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{  
                if(!the_user_exit)
                {
                    dup2(devNull,STDOUT_FILENO);
                }
                execvp(argv[0],argv); 
            }
            
			exit(0);
		}
		else
		{

		}
    }
	close(writefd);
	shmdt(Message_share_memory_addr);
	shmdt(User_share_memory_addr);
	shmdt(FileName_share_memory_addr);
	return;
}

void userpipeTake(int pipe_position , int userpipe_number , int id)
{
	/*
		Parameter:
			pipe_position : where do '>' index in cmd  
			userpipe_number : which userid we should take pipe from
		Work:
			To receieve pipe from User. 
			But we will go to sender's UserpipeMap to take pipe .
			If get it we will delete pipe in Sender's UserpipeMap.
	*/

	//initial Message content
	string message="";
	char* Message_share_memory_addr = (char*) shmat(Message_shmid,NULL,0);
	if(Message_share_memory_addr == (char*)-1)
	{
		cerr<<"Message_share_memory_addr : boardcastLogin"<<endl;
	}
	// memset(Message_share_memory_addr,'\0', Client_Message);
	string FileName="";
	char* FileName_share_memory_addr = (char*) shmat(FileName_shmid,NULL,0);
	if(FileName_share_memory_addr == (char*)-1)
	{
		cerr<<"FileName_share_memory_addr : boardcastLogin"<<endl;
	}
	memset(FileName_share_memory_addr,'\0', Client_Message);
	//initial User info
	user* User_share_memory_addr = (user*) shmat(User_shmid,NULL,0);
	if(User_share_memory_addr == (user*)-1)
	{
		cerr<<"User_share_memory_addr : boardcastLogin"<<endl;
	}


    // check user is life
    bool life_or_not = false;
    for(int i = 0 ; i < MAX_USER ; i++)
    {
        if(User_share_memory_addr[i].userid == userpipe_number)
        {
            life_or_not= true;
        }
    }
    if(!life_or_not)
    {
        cout<<"*** Error: user #"<< userpipe_number <<" does not exist yet. ***"<<endl;
		return;
    }


	// find the index that we will use in findind command
	int this_pipe_position = pipe_position;
	int last_pipe_position = find_last_special_command(pipe_position);
	// cout<<this_pipe_position<<" "<<last_pipe_position<<endl;

	// this is to finad command that we need to execvp
	int index = 0;
	char* argv[MAX_ARGV_SIZE];
	for(int i = last_pipe_position+1 ; i < this_pipe_position;i++)
	{
        if(cmd[i][0]=='>') continue;
		argv[index] = strdup(cmd[i].c_str());
		index++;
	}
    string command="";
    for(int i = 0 ; i < cmd.size();i++)
	{
		if(i == cmd.size()-1)
		{
        	command+=cmd[i]; 
		}
		else{
			command+=cmd[i]; 
        	command+=' ';
		}
        
	}
	argv[index] = NULL;
	
	string filename="/tmp/"+to_string(userpipe_number)+"to"+to_string(id)+"file";
    const char * fifo_name = filename.c_str();
	if(User_share_memory_addr[userpipe_number-1].userpipe_exist[id-1] == false)
	{
        cout<<"*** Error: the pipe #"<<userpipe_number<<"->#"<<User_share_memory_addr[id-1].userid<<" does not exist yet. ***"<<endl;
		return;
	}
	// pass file name to FileName_share_memory_addr and let the reader open file first in order to let writer can write.
	//Since the File must read and write at the same time!!
	FileName+=filename;
	strncpy(FileName_share_memory_addr , FileName.c_str() , Client_Message);
	kill( User_share_memory_addr[userpipe_number-1].pid , SIGUSR1);
	//check readfd
	int readfd;
	readfd = open(fifo_name, O_RDONLY  , S_IRUSR);
	if( readfd < 0 )
	{
		cerr<<"open read failed"<<endl;
	}
	User_share_memory_addr[userpipe_number-1].userpipe_exist[id-1] = false;
	// 若是存在，就要stderror_number讀取file，再執行execvp 
	if(readfd)
	{
		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid_t pid;
		pid = fork();

		if (pid<0){ perror("fuck! numberpipe: "); }
		else if(pid == 0)
		{
			//boardcast send message
			for (int i = 0 ; i < MAX_USER ; i++)
			{
				if(User_share_memory_addr[i].userid == userpipe_number)
				{
					memset(Message_share_memory_addr,'\0', Client_Message);
					message="";
					message+="*** ";
					message+=User_share_memory_addr[id-1].nickname;
					message+=" (#";
					message+=to_string(User_share_memory_addr[id-1].userid);
					message+=") just received from ";
					message+=User_share_memory_addr[i].nickname;
					message+=" (#";
					message+=to_string(User_share_memory_addr[i].userid);
					message+=") by '";
					message+=command;
					message+="' ***";
				}
			}
			strncpy(Message_share_memory_addr , message.c_str() , Client_Message);
			for( int i = 0 ; i < MAX_USER ; i++)
			{
				if(User_share_memory_addr[i].userid!=0)
				{
					kill( User_share_memory_addr[i].pid , SIGUSR3);
				}
			}

			//dup file
			if(readfd != STDIN_FILENO)  { dup2(readfd,STDIN_FILENO);   }
			//close file
			if(readfd != STDIN_FILENO)  { close(readfd);  }
			
			//execvp
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ 
        		if(!life_or_not)
                {
                    dup2(devNull,STDIN_FILENO);
                }
                execvp(argv[0],argv); 
            }
			exit(0);
		}
		else
		{	
			sleep(1);		
            int status;
			waitpid(pid , &status ,0);
            	
		}
	}
	// 若是不存在，就直接執行Execvp並存在管子裡面
	
	close(readfd);
	shmdt(Message_share_memory_addr);
	shmdt(User_share_memory_addr);
	shmdt(FileName_share_memory_addr);
	return;
}

void userpipeTake_with_another_commands(int pipe_position , int userpipe_number , int id)
{
    	/*
		Parameter:
			pipe_position : where do '>' index in cmd  
			userpipe_number : which userid we should take pipe from
			userpipe_should_be_Cat_first : To check we should do > and perpare something!!!!
		Work:
			To receieve pipe from User. 
			But we will go to sender's UserpipeMap to take pipe .
			If get it we will delete pipe in Sender's UserpipeMap.
			Most important, we need to store output in pipe because there another command behind us!!
	*/

	//initial Message content
	string message="";
	char* Message_share_memory_addr = (char*) shmat(Message_shmid,NULL,0);
	if(Message_share_memory_addr == (char*)-1)
	{
		cerr<<"Message_share_memory_addr : boardcastLogin"<<endl;
	}
	// memset(Message_share_memory_addr,'\0', Client_Message);
	string FileName="";
	char* FileName_share_memory_addr = (char*) shmat(FileName_shmid,NULL,0);
	if(FileName_share_memory_addr == (char*)-1)
	{
		cerr<<"FileName_share_memory_addr : boardcastLogin"<<endl;
	}
	memset(FileName_share_memory_addr,'\0', Client_Message);
	//initial User info
	user* User_share_memory_addr = (user*) shmat(User_shmid,NULL,0);
	if(User_share_memory_addr == (user*)-1)
	{
		cerr<<"User_share_memory_addr : boardcastLogin"<<endl;
	}


    // check user is life
    bool life_or_not = false;
    for(int i = 0 ; i < MAX_USER ; i++)
    {
        if(User_share_memory_addr[i].userid == userpipe_number)
        {
            life_or_not= true;
        }
    }
    if(!life_or_not)
    {
        cout<<"*** Error: user #"<< userpipe_number <<" does not exist yet. ***"<<endl;
        return ;
    }

	// find the index that we will use in findind command
	int this_pipe_position = pipe_position;
	int last_pipe_position = find_last_special_command(pipe_position);
	// cout<<this_pipe_position<<" "<<last_pipe_position<<endl;

	// this is to finad command that we need to execvp
	int index = 0;
	char* argv[MAX_ARGV_SIZE];

	for(int i = last_pipe_position+1 ; i < this_pipe_position;i++)
	{
        if(cmd[i][0]=='>') continue;
		argv[index] = strdup(cmd[i].c_str());
		index++;
	}
    string command="";
    for(int i = 0 ; i < cmd.size();i++)
	{
       if(i == cmd.size()-1)
		{
        	command+=cmd[i]; 
		}
		else{
			command+=cmd[i]; 
        	command+=' ';
		}
	}
	argv[index] = NULL;
	
	// check fd_to_write exist or not
	int fd_to_write[2];
    if(pipe(fd_to_write) < 0 ){ perror("pipe 1 get wrong!"); }

	//we need to see whether the file exist？
	string filename="/tmp/"+to_string(userpipe_number)+"to"+to_string(id)+"file";
    const char * fifo_name = filename.c_str();
	if(User_share_memory_addr[userpipe_number-1].userpipe_exist[id-1] == false)
	{
        cerr<<"*** Error: the pipe #"<<userpipe_number<<"->#"<<User_share_memory_addr[id-1].userid<<" does not exist yet. ***"<<endl;
		return;
	}

	// pass file name to FileName_share_memory_addr and let the reader open file first in order to let writer can write.
	//Since the File must read and write at the same time!!
	FileName+=filename;
	strncpy(FileName_share_memory_addr , FileName.c_str() , Client_Message);
	kill( User_share_memory_addr[userpipe_number-1].pid , SIGUSR1);
	int readfd;
	readfd = open(fifo_name, O_RDONLY  , S_IRUSR);
	User_share_memory_addr[userpipe_number-1].userpipe_exist[id-1] = false;
	// 若是存在，就要stderror_number讀取file，再執行execvp 
	if(readfd)
	{
        //if read file exist and then boardcast send message
		for (int i = 0 ; i < MAX_USER ; i++)
		{
			if(User_share_memory_addr[i].userid == userpipe_number)
			{
				memset(Message_share_memory_addr,'\0', Client_Message);
				message="";
				message+="*** ";
				message+=User_share_memory_addr[id-1].nickname;
				message+=" (#";
				message+=to_string(User_share_memory_addr[id-1].userid);
				message+=") just received from ";
				message+=User_share_memory_addr[i].nickname;
				message+=" (#";
				message+=to_string(User_share_memory_addr[i].userid);
				message+=") by '";
				message+=command;
				message+="' ***";
			}
		}
		strncpy(Message_share_memory_addr , message.c_str() , Client_Message);
		for( int i = 0 ; i < MAX_USER ; i++)
		{
			if(User_share_memory_addr[i].userid!=0)
			{
				kill( User_share_memory_addr[i].pid , SIGUSR3);
			}
		}

		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid_t pid;
		pid = fork();

		if (pid<0){ perror("fuck! numberpipe: "); }
		else if(pid == 0)
		{
			//dup file
			if(readfd != STDIN_FILENO)  { dup2(readfd,STDIN_FILENO);   }
			if(fd_to_write[1] != STDOUT_FILENO){ dup2(fd_to_write[1],STDOUT_FILENO); }	
			//close file
			if(readfd != STDIN_FILENO)  { close(readfd);  }
			if(fd_to_write[1] != STDOUT_FILENO){ close(fd_to_write[1]); }
			if(fd_to_write[0] != STDIN_FILENO) { close(fd_to_write[0]); }
			//execvp
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ 
        		if(!life_or_not)
                {
                    dup2(devNull,STDIN_FILENO);
                }
                execvp(argv[0],argv); 
            }
			exit(0);
		}
		else
		{	
			pipefd temp = { -1 , fd_to_write[1] , fd_to_write[0] };
			pipeMap.push_back(temp);
		}
	}
	// 若是不存在，就直接執行Execvp並存在管子裡面
	else{

        cout<<"*** Error: the pipe #"<<userpipe_number<<"->#"<<User_share_memory_addr[id-1].userid<<" does not exist yet. ***"<<endl;
    }
	close(readfd);
	shmdt(Message_share_memory_addr);
	shmdt(User_share_memory_addr);
	shmdt(FileName_share_memory_addr);
	return;
}

void userpipe_GiveAndTake(int id)
{
	string message="";
	char* Message_share_memory_addr = (char*) shmat(Message_shmid,NULL,0);
	if(Message_share_memory_addr == (char*)-1)
	{
		cerr<<"Message_share_memory_addr : boardcastLogin"<<endl;
	}
	// memset(Message_share_memory_addr,'\0', Client_Message);
	string FileName="";
	char* FileName_share_memory_addr = (char*) shmat(FileName_shmid,NULL,0);
	if(FileName_share_memory_addr == (char*)-1)
	{
		cerr<<"FileName_share_memory_addr : boardcastLogin"<<endl;
	}
	memset(FileName_share_memory_addr,'\0', Client_Message);
	//initial User info
	user* User_share_memory_addr = (user*) shmat(User_shmid,NULL,0);
	if(User_share_memory_addr == (user*)-1)
	{
		cerr<<"User_share_memory_addr : boardcastLogin"<<endl;
	}

	// this is to finad command that we need to execvp
	int index = 0;
	char* argv[MAX_ARGV_SIZE];

	for(int i = 0 ; i < cmd.size();i++)
	{
        if(cmd[i][0]=='>') continue;
        else if(cmd[i][0]=='<') continue;
		argv[index] = strdup(cmd[i].c_str());
		index++;
	}
	argv[index] = NULL;

    string command="";
    for(int i = 0 ; i < cmd.size();i++)
	{
       if(i == cmd.size()-1)
		{
        	command+=cmd[i]; 
		}
		else{
			command+=cmd[i]; 
        	command+=' ';
		}
	}

	int write_id; // >2. 2 is write_id
	int read_id; // <3. 3 is read_id
	for(int i = 0 ; i < cmd.size() ; i++)
	{
		if(cmd[i][0]=='>')
		{
			string number="";
			for(int j = 1 ; j < cmd[i].length();j++){ number+=cmd[i][j]; }
			write_id = stoi(number);
		}
		else if(cmd[i][0]=='<'){
			string number="";
			for(int j = 1 ; j < cmd[i].length();j++){ number+=cmd[i][j]; }
			read_id = stoi(number);
		}
	}
	
	
	//'<' command need to check file is exist or not!!
    // '>' command need to check user is life
    bool writeID_live_or_not = false;
    bool readID_live_or_not = false;
    for(int i = 0 ; i < MAX_USER ; i++)
    {
        if(User_share_memory_addr[i].userid == write_id)
        {
            writeID_live_or_not= true;
        }
		if(User_share_memory_addr[i].userid == read_id)
        {
            readID_live_or_not= true;
        }
    }
	//
	//need to deal with read first
	//
	int readfd=-1;
	string filename="/tmp/"+to_string(read_id)+"to"+to_string(id)+"file";
	
	if(!readID_live_or_not)
    {
        cerr<<"*** Error: user #"<< read_id <<" does not exist yet. ***"<<endl;
    }
	else if(User_share_memory_addr[read_id-1].userpipe_exist[id-1] == false)
	{
		cout<<"*** Error: the pipe #"<<read_id<<"->#"<<id<<" does not exist yet. ***"<<endl;
	}
	else
	{
		// pass file name to FileName_share_memory_addr and let the reader open file first in order to let writer can write.
		//Since the File must read and write at the same time!!
		const char * read_fifo_name = filename.c_str();
		FileName+=filename;
		strncpy(FileName_share_memory_addr , FileName.c_str() , Client_Message);
		kill( User_share_memory_addr[read_id-1].pid , SIGUSR1);
		readfd = open(read_fifo_name, O_RDONLY  , S_IRUSR);
		User_share_memory_addr[read_id-1].userpipe_exist[id-1] = false;
		if( readfd < 0 )
		{
			cerr<<"*** Error: the pipe #"<<read_id<<"->#"<<id<<" does not exist yet. ***"<<endl;
		}
	}
	
	//
	//and then is write!!
    //
	int writefd;
	int myfifo;
	filename = "/tmp/"+to_string(id)+"to"+to_string(write_id)+"file";
    
	// memset(Message_share_memory_addr,'\0', Client_Message);
	// message="";
	FileName="";
	memset(FileName_share_memory_addr,'\0', Client_Message);
	if(!writeID_live_or_not)
    {
        cerr<<"*** Error: user #"<< write_id <<" does not exist yet. ***"<<endl;
    }
	else if(!check_file_exist(argv[0]))
	{
		cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
	}
	else if(User_share_memory_addr[id-1].userpipe_exist[write_id-1] == true)
	{
		cout<<"*** Error: the pipe #"<<id<<"->#"<<write_id<<" already exists. ***"<<endl;
		return;
	}
	else{
		const char * write_fifo_name = filename.c_str();
		if (access(write_fifo_name, F_OK) == -1)
		{
			// 管道文件不存在
			// 创建命名管道
			myfifo = mkfifo(write_fifo_name,  S_IWUSR|S_IRUSR);
			if (myfifo != 0)
			{
				cerr<<"Could not create fifo :"<<write_fifo_name<<endl;
				exit(EXIT_FAILURE);
			}
		}
		FileName+=filename;
		strncpy(FileName_share_memory_addr , FileName.c_str() , Client_Message);
		kill( User_share_memory_addr[write_id-1].pid , SIGUSR2);
		// cout<<filename<<endl;
		writefd = open(write_fifo_name, O_WRONLY |O_CREAT   , S_IWUSR|S_IRUSR);
		User_share_memory_addr[id-1].userpipe_exist[write_id-1] = true;
		if(writefd < 0 )
		{
			cerr<<"can't open write file"<<endl;
		}
	}

	

	memset(Message_share_memory_addr,'\0', Client_Message);
	message="";
	if(readfd>=0)
	{
		//boardcast send message
		message+="*** ";
		message+=User_share_memory_addr[id-1].nickname;
		message+=" (#";
		message+=to_string(id);
		message+=") just received from ";
		message+=User_share_memory_addr[read_id-1].nickname;
		message+=" (#";
		message+=to_string(read_id);
		message+=") by '";
		message+=command;
		message+="' ***\n";
	}

	if(writefd>=0){
		
		//boardcast send message
		message+="*** ";
		message+=User_share_memory_addr[id-1].nickname;
		message+=" (#";
		message+=to_string(id);
		message+=") just piped '";
		message+=command;
		message+="' to ";
		message+=User_share_memory_addr[write_id-1].nickname;
		message+=" (#";
		message+=to_string(write_id);
		message+=")  ***";
	}	
	strncpy(Message_share_memory_addr , message.c_str() , Client_Message);
	for( int i = 0 ; i < MAX_USER ; i++)
	{
		if(User_share_memory_addr[i].userid!=0)
		{
			kill( User_share_memory_addr[i].pid , SIGUSR3);
			// int status;
			// waitpid(User_share_memory_addr[i].pid , &status , 0);
		}
	}
	// sleep(1);
	//execvp command
	if(readfd)
	{
		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid_t pid;
		pid = fork();

		if (pid<0){ perror("fuck! numberpipe: "); }
		else if(pid == 0)
		{
            
			//dup file
			if(readfd != STDIN_FILENO)  { dup2(readfd,STDIN_FILENO);   }
			if(readfd != STDIN_FILENO)  { close(readfd);  }
			if(writefd != STDOUT_FILENO){ dup2(writefd,STDOUT_FILENO); }	
			if(writefd != STDOUT_FILENO){ close(writefd); }
			//execvp
			if(!check_file_exist(argv[0]))
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{ 
        		if(!writeID_live_or_not)
                {
                    dup2(devNull,STDOUT_FILENO);
                }
                execvp(argv[0],argv); 
            }
			exit(0);
		}
		else
		{	
			//nothing
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
			//close file
			dup2(devNull,STDIN_FILENO);
			//check the command exist or not
            // dup2(stdout_copy,STDOUT_FILENO);
			if(!check_file_exist(argv[0]))//if not then dup the stdout to original and output wrong message
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{  
                if(!writeID_live_or_not)
                {
                    dup2(devNull,STDOUT_FILENO);
                }
                execvp(argv[0],argv); 
            }
            
			exit(0);
		}
		else
		{
			//nothing
		}
    }
	close(writefd);
	close(readfd);
	shmdt(Message_share_memory_addr);
	shmdt(User_share_memory_addr);
	shmdt(FileName_share_memory_addr);
	return;
}