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

using namespace std;
#define MAX_ARGV_SIZE 1024
#define MAX_USERS 30
int const stdout_copy = dup(STDOUT_FILENO);
int const stdin_copy = dup(STDIN_FILENO);
int const stderror_copy = dup(STDERR_FILENO);
int devNull = open("/dev/null", O_WRONLY);

//struct this to remember pipe number
struct pipefd{
    int number_to_pipe;
    int fdin;//1 go into pipe
    int fdout;//0 go out of pipe
};
struct userpipe{
    int givewhoUserID;
    int fdin;
    int fdout;
};

struct user{
	int userid;
	string nickname="(no name)";
    struct sockaddr_in clnt_addr;
    int client_scoketfd_number;
	vector <string> env; //PATH
	vector <string> envpath; //bin:.
    vector<pipefd> pipeMap;
    vector<userpipe> userpipeMap ;
};

user* whoIsUseServer;


////////////
// vector //
////////////
vector <string> cmd;//to store command line in every input and then we will clear when we have done the command line
vector <int> all_pipe_direction_position ;// remember all the special command's position in cmd. special command : !, |, >, number pipe, number stderr
vector<user>UserMap;

////////////// 
// function //
//////////////
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
void run_command(int socketfd);

//deal with TCP,socket
int connectTCP(int port);
string buffer_spilit(char* buf);
void who(int clnt);
void boardcastLogin(int clnt_sock);
void boardcastLogout(int clnt_sock);
void welcomeMessage(int clnt_sock);
void nameUserName(int clnt_sock);
void tell(int clnt_sock, int friends);
void yell(int clnt_sock);
void userpipeTake(int pipe_position , int userpipe_number );
void userpipeGive(int pipe_position , int userpipe_number);
void userpipeTake_with_another_commands(int pipe_position , int userpipe_number , bool last_pipe_position);
void Erase_userAllInformation(int Userid);
void sortUserMap();

//////////
// main //
//////////
int main(int argc, char* argv[]){
   
	string input_command;
	
    //build master socket
	int msock = connectTCP(atoi(argv[1]));
    // cout<<"hello";
    //創建讀的文件
    fd_set readfds;
    fd_set activefds;
    FD_ZERO(&activefds);
    FD_SET(msock, &activefds);
	FD_ZERO(&readfds);
    FD_SET(msock, &activefds);
    

    //接收客户端请求
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    int maxfd = msock; //since in this way, there is no connection, The msock is the max
    while (true) {
        readfds = activefds; //memcpy(&readfds,&activefds,sizeof(readfds));
        // cout<<"hello"<<endl;
        int readyfd = select(maxfd+1,&readfds,NULL,NULL,NULL);
        // cout<<"listen"<<endl;
        // cout<<readyfd<<endl;
        if(readyfd<0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("select");
        }
        if(FD_ISSET(msock,&readfds))
        {
            int clnt_sock = accept(msock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
            if (clnt_sock < 0) {
                cout << "Error: accept" << endl;
            }
            FD_SET(clnt_sock,&activefds);
            // cout<<"welcom";
            //store user information
           int max_uesrid = 1;
            bool ThisIDisUsed = true;
            while(ThisIDisUsed)
            {
                ThisIDisUsed = false;
                for(int i = 0 ; i < UserMap.size() ; i++)
                {
                    if(UserMap[i].userid == max_uesrid)
                    {
                        max_uesrid+=1;
                        ThisIDisUsed=true;
                        break;
                    }
                }
            }
            if(UserMap.size()<MAX_USERS)
            {
                // setenv("PATH","bin:.",1);
                struct user temp ;
                temp.userid = max_uesrid;
                temp.clnt_addr = clnt_addr;
                temp.env.push_back("PATH");
                temp.envpath.push_back("bin:.");
                temp.client_scoketfd_number = clnt_sock;
                UserMap.push_back(temp);
            }
            if(clnt_sock > maxfd) maxfd = clnt_sock;
            welcomeMessage(clnt_sock);
            boardcastLogin(clnt_sock);
            sortUserMap();
            write(clnt_sock,"% ",2);
            readyfd --;
            if(readyfd == 0)continue;  //如果只有msock被監控成功，那麼就重新select
        }

        char str[1024] ;

        //處理lfd之外監控成功的文件描述符，進行輪詢
        for(int i=0;i<=UserMap.size();i++)
        {
            if(FD_ISSET(UserMap[i].client_scoketfd_number,&readfds) && UserMap[i].client_scoketfd_number!=msock)   //在client數組中尋找是否有被監控成功的文件描述符
            {
                //此時說明client[i]對於的文件描述符監控成功，有消息發來，直接讀取即可
                memset(str, '\0', sizeof(str));
                int readcount = recv(UserMap[i].client_scoketfd_number,str,sizeof(str),0);
                if(readcount == -1)
                {
                    cout<<"read fault : "<<strerror(errno)<<endl;
                    continue;
                }
                else
                {
                    input_command = buffer_spilit(str);
                    // cout<<input_command<<endl;
                    if(input_command=="exit"){  
                        // cout<<"in"<<endl;
                        boardcastLogout(UserMap[i].client_scoketfd_number);
                        close(UserMap[i].client_scoketfd_number);  //關閉描述符
                        FD_CLR(UserMap[i].client_scoketfd_number,&activefds);   //將該描述符從描述符集合中去除
						Erase_userAllInformation(UserMap[i].userid);
                        UserMap.erase(UserMap.begin()+ i); 
                        continue;
                    }
                    split_command(input_command);


                    //run command
                    whoIsUseServer = &UserMap[i];
                    //set user env path
                    for(int k = 0 ; k < whoIsUseServer->env.size() ; k++)
                    {
	                    setenv(whoIsUseServer->env[k].c_str(),whoIsUseServer->envpath[k].c_str(),1);
                    }
                    //run command
					sortUserMap();
                    run_command(UserMap[i].client_scoketfd_number);

                    //clear cmd, special command position			
                    cmd.clear();
                    cmd.shrink_to_fit();
                    all_pipe_direction_position.clear();
                    write(UserMap[i].client_scoketfd_number, "% ", 2);

            //         cout<<"(From "<<inet_ntoa(clit_info[i].sin_addr)<<":"<<ntohs(clit_info[i].sin_port)<<")";
		    // for(int j=0;j<readcount;j++)cout<<str[j];
		    // cout<<endl;
            //         for(int j=0;j<readcount;j++)str[j] = toupper(str[j]);
            //         write(client[i],str,readcount);
                }
                readyfd--;
                if(readyfd == 0)break;
            }
        }
        // write(clnt_sock, str, sizeof(str));
    }

	//关闭套接字
    close(msock);
    
}

void set_env( int index){
	/*
		Parameter:
			index : the setenv's index in cmd
		Work:
			setenv env
	*/
    for(int i = 0 ; i < whoIsUseServer->env.size() ; i++)
    {
        if(whoIsUseServer->env[i] == cmd[index+1])
        {
            whoIsUseServer->env.erase(whoIsUseServer->env.begin()+i);
            whoIsUseServer->envpath.erase(whoIsUseServer->envpath.begin()+i);
        }
    }
    whoIsUseServer->env.push_back(cmd[index+1]);
    whoIsUseServer->envpath.push_back(cmd[index+2]);
	setenv(cmd[index+1].c_str(),cmd[index+2].c_str(),1);
    // whoIsUseServer->envpath
	return ;
}

void print_env( int index){
	/*
		Parameter:
			index : the printenv's index in cmd
		Work:
			print env
	*/
    string env="";
    for(int i = 0 ; i < whoIsUseServer->env.size() ; i++)
    {
        if( whoIsUseServer->env[i]==cmd[index+1])
        {
            cout<<whoIsUseServer->envpath[i]<<endl;
        }
    } 
	// char* env = getenv(cmd[index+1].c_str());
	// if( env != NULL){ printf("%s\n",env); }
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
    for(int i = 0 ; i < whoIsUseServer->pipeMap.size() ; i++)
    {
        if(whoIsUseServer->pipeMap[i].number_to_pipe==num)
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
		fd[0] = whoIsUseServer->pipeMap[it].fdout;
        fd[1] = whoIsUseServer->pipeMap[it].fdin;
        whoIsUseServer->pipeMap.erase(whoIsUseServer->pipeMap.begin()+it);

		// 預防關到正在開的管子 因為我會再 '｜'的地方去關它讀取的號碼，所以會開到原北關閉的館子的號碼，導致我在這邊會關到那個號碼
		// e.g. fd[0] = 5 , fd[1]=6 , 我會關閉fd[1]=6，這樣下一個人開pipe，會使用到 6 這個數字，所以必面我誤關管子號碼，所以在這邊特別去寫這個迴圈確認。
		for( int i = 0 ; i < whoIsUseServer->pipeMap.size() ; i++)
		{
			if(fd[1] != STDOUT_FILENO && fd[1]!=whoIsUseServer->pipeMap[i].fdin && fd[1]!=whoIsUseServer->pipeMap[i].fdout && whoIsUseServer->pipeMap[i].number_to_pipe!=0){ close(fd[1]); }
			if(fd[0] != STDIN_FILENO && fd[0]!=whoIsUseServer->pipeMap[i].fdin && fd[0]!=whoIsUseServer->pipeMap[i].fdout && whoIsUseServer->pipeMap[i].number_to_pipe!=0) { close(fd[0]); }
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
    for( int i = 0 ; i < whoIsUseServer->pipeMap.size() ; i++){ 
        whoIsUseServer->pipeMap[i].number_to_pipe -=1; 
    }
    return;
}

void pipe_number_order_increase(int number){
	/*
		Work :
			After We finish a command, We nned to make number pipe to increase.
			Becasue We will get 0號管 to execvp, and then We set normal pipe is -1
			We need to +1. that it will get into 0號管, so that it will be execvp.
		Change:
			we add int num to become parameter because we may need to cat <2 |4 pipe number so that we will use this to let number pipe can pipe 4
	*/
	for( int i = 0 ; i < whoIsUseServer->pipeMap.size() ; i++)
    {
        if(whoIsUseServer->pipeMap[i].number_to_pipe == -1 || whoIsUseServer->pipeMap[i].number_to_pipe == 0)
		{
			whoIsUseServer->pipeMap[i].number_to_pipe+=number;
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
        // cout<<argv[index]<<endl;
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
        fd_to_read[0] = whoIsUseServer->pipeMap[it].fdout;
        fd_to_read[1] = whoIsUseServer->pipeMap[it].fdin;
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
			if(fd_to_read[1] != STDOUT_FILENO) {  close(fd_to_read[1]);  }
			if(fd_to_read[0] != STDIN_FILENO)  {  close(fd_to_read[0]);  }
			//execvp command
            // dup2(stdout_copy,STDOUT_FILENO);
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
			// dup2(stdout_copy,STDOUT_FILENO);
			// dup2(stdin_copy,STDIN_FILENO);
			//store pipe
			pipefd temp = {-1,fd_to_write[1],fd_to_write[0]};
			whoIsUseServer->pipeMap.push_back(temp);
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
			whoIsUseServer->pipeMap.push_back(temp);
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
    if(cmd[pipe_position-1][0]=='<')
    {
        pipe_number_order_increase(pipe_number);
        return;
    }
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
        fd_to_write[0] = whoIsUseServer->pipeMap[it].fdout;
        fd_to_write[1] = whoIsUseServer->pipeMap[it].fdin;
        whoIsUseServer->pipeMap.erase(whoIsUseServer->pipeMap.begin()+it); // need to erase because no one can deal with it, so that we need to deal with
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
		fd_to_read[0] = whoIsUseServer->pipeMap[data_to_read].fdout;
        fd_to_read[1] = whoIsUseServer->pipeMap[data_to_read].fdin;
        whoIsUseServer->pipeMap.erase(whoIsUseServer->pipeMap.begin()+data_to_read);
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
			whoIsUseServer->pipeMap.push_back(temp);
			sleep(0.1);			
		}
	}
	// 若是不存在，就直接執行Execvp並存在管子裡面
	else{
		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid_t pid;
		pid = fork();
        // cout<<fd_to_write[1]<<" : "<<fd_to_write[0]<<endl;
		if(pid == 0)
		{
			//dup file
			if(fd_to_write[1] != STDOUT_FILENO){ dup2(fd_to_write[1],STDOUT_FILENO); }
			//close file
			if(fd_to_write[1] != STDOUT_FILENO){ close(fd_to_write[1]);	}
			if(fd_to_write[0] != STDIN_FILENO) { close(fd_to_write[0]);	}
			//check the command exist or not
            // dup2(stdout_copy,STDOUT_FILENO);
			if(!check_file_exist(argv[0]))//if not then dup the stdout to original and output wrong message
			{
				cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
			}
			else{  execvp(argv[0],argv); }
            
			exit(0);
		}
		else
		{
			//dup back
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stdin_copy,STDIN_FILENO);
			//store pipe
			pipefd temp = {pipe_number,fd_to_write[1],fd_to_write[0]};
			whoIsUseServer->pipeMap.push_back(temp);
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

	if(cmd[stderror_position-1][0]=='<')
    {
        pipe_number_order_decrease();
        return;
    }
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
		fd_to_read[0] = whoIsUseServer->pipeMap[data_to_read].fdout;
        fd_to_read[1] = whoIsUseServer->pipeMap[data_to_read].fdin;
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
			whoIsUseServer->pipeMap.push_back(temp);
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
			whoIsUseServer->pipeMap.push_back(temp);
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

	if(cmd[stderror_position-1][0]=='<')
    {
        pipe_number_order_increase(pipe_number);
        return;
    }
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
        fd_to_write[0] = whoIsUseServer->pipeMap[it].fdout;
        fd_to_write[1] = whoIsUseServer->pipeMap[it].fdin;
        whoIsUseServer->pipeMap.erase(whoIsUseServer->pipeMap.begin()+it);
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
		fd_to_read[0] = whoIsUseServer->pipeMap[data_to_read].fdout;
        fd_to_read[1] = whoIsUseServer->pipeMap[data_to_read].fdin;
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
			whoIsUseServer->pipeMap.push_back(temp);
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
			whoIsUseServer->pipeMap.push_back(temp);
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
        if(cmd[i][0]=='<')continue;
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
		fd_to_read[0] = whoIsUseServer->pipeMap[data_to_read].fdout;
        fd_to_read[1] = whoIsUseServer->pipeMap[data_to_read].fdin;
		close(fd_to_read[1]);
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
		fd_to_read[0] = whoIsUseServer->pipeMap[data_to_read].fdout;
        fd_to_read[1] = whoIsUseServer->pipeMap[data_to_read].fdin;
		close(fd_to_read[1]);
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
        fd_to_read[0] = whoIsUseServer->pipeMap[it].fdout;
        fd_to_read[1] = whoIsUseServer->pipeMap[it].fdin;
        close(fd_to_read[1]);
        // pipeMap.erase(pipeMap.begin()+it);
        // cout<<"fd_to_read[1] :"<<fd_to_read[1]<<"fd_to_read[0]"<<fd_to_read[0]<<endl;
        
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
            // dup2(stdout_copy,STDOUT_FILENO);
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
            if(fd_to_read[0]!= STDIN_FILENO) { close(fd_to_read[0]); }  
			//wait child process
            int status;
			waitpid(pid , &status ,0); //放這個結果出錯了
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

void run_command(int socketfd){

    bool userpipe_should_be_Cat_first = false;
    bool _last_command_is_done = false;
	for (int i = 0 ; i < cmd.size() ; i++)
	{
        dup2(socketfd,STDOUT_FILENO);
        dup2(socketfd,STDERR_FILENO);
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
        else if(cmd[i][0]=='>' && cmd[i][1]>=49)
		{
            if(i != cmd.size()-1 &&cmd[i+1][0]=='<' && cmd[i+1][1]>=49)
            {
				//如果後面有 < 代表我們該先接受 user pipe ，所以我們用userpipe_should_be_Cat_first 來去當作signal!! 告訴 userpipeTake_with_another_commands 要記得回頭做 > !!!
                userpipe_should_be_Cat_first = true;
                continue;
            }
            //catch the number we need to store
            string number="";
            for(int j = 1 ; j < cmd[i].length();j++){ number+=cmd[i][j]; }
            int num = stoi(number);

			userpipeGive(i , num);
			close_file();
			pipe_number_order_decrease();
			// break;
		}
        else if(cmd[i][0]=='<' && cmd[i][1]>=49)
		{
            if(i != cmd.size()-1 || userpipe_should_be_Cat_first)
            {
                //it means there still another command behind <2
				// or we need deal with userpipe_should_be_Cat_first it means we need pipe to another user
                string number="";
                for(int j = 1 ; j < cmd[i].length();j++){ number+=cmd[i][j]; }
                int num = stoi(number);

                userpipeTake_with_another_commands(i , num , userpipe_should_be_Cat_first );
                close_file();
                pipe_number_order_increase(1);
            }
            else{
				// into this because behind < we have no others command so we need output on client's monitor
                //catch the number we need to store
                string number="";
                for(int j = 1 ; j < cmd[i].length();j++){ number+=cmd[i][j]; }
                int num = stoi(number);

                userpipeTake(i , num);
                close_file();
                pipe_number_order_decrease();
                _last_command_is_done = true;
            }
            if(userpipe_should_be_Cat_first)
            {
				// into this because we need deal with > after we catch user pipe from <
                //catch the number we need to store
                string number="";
                for(int j = 1 ; j < cmd[i-1].length();j++){ number+=cmd[i-1][j]; }
                int num = stoi(number);

                userpipeGive(i-1 , num);
                close_file();
                pipe_number_order_decrease();
                userpipe_should_be_Cat_first = false;

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
            who(socketfd);
            break;
        }
        else if(cmd[i]=="name")
        {
            nameUserName(socketfd);
            break;
        }
        else if(cmd[i]=="tell")
        {
            tell(socketfd , stoi(cmd[1]));
            break;
        }
        else if(cmd[i]=="yell")
        {
            yell(socketfd);
            break;
        }
		else if(i == cmd.size()-1 && !_last_command_is_done)//to deal with the pipe final case and normal case
		{
			last_cmd(i);
			close_file();
        	pipe_number_order_decrease();//let the pipe_number -1
		}
	}
	return ;
}

string buffer_spilit(char* buf){
    string word="";
    for(int i = 0 ; i < MAX_ARGV_SIZE ; i++)
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

void boardcastLogin(int clnt_sock){
    for(int i = 0 ; i < UserMap.size() ; i++)
    {
        if(UserMap[i].client_scoketfd_number==clnt_sock)
        {
            for(int j = 0 ; j < UserMap.size() ; j++)
            {
                dup2(UserMap[j].client_scoketfd_number ,STDOUT_FILENO);
                cout<<"*** User '"<<UserMap[i].nickname<<"' entered from "<<inet_ntoa(UserMap[i].clnt_addr.sin_addr)<<':'<<htons(UserMap[i].clnt_addr.sin_port)<<". ***"<<endl;
            }
        }
    }
    dup2(stdout_copy,STDOUT_FILENO);
    return;
}

void boardcastLogout(int clnt_sock){
    for(int i = 0 ; i < UserMap.size() ; i++)
    {
        if(UserMap[i].client_scoketfd_number==clnt_sock)
        {
            for(int j = 0 ; j < UserMap.size() ; j++)
            {
                dup2(UserMap[j].client_scoketfd_number ,STDOUT_FILENO);
                cout<<"*** User '"<<UserMap[i].nickname<<"' left. ***"<<endl;
            }
        }
    }
    dup2(stdout_copy,STDOUT_FILENO);
    return;
}

void who(int clnt)
{
    dup2(clnt,STDOUT_FILENO);
    // in order to print name orderly from ID1 to the last
    cout<<"<ID>\t<nickname>\t<IP:port>\t<indicate me>"<<endl;
    for(int i = 0 ; i < UserMap.size() ; i++)
    {
        if(UserMap[i].client_scoketfd_number==clnt)
        {
            cout<<UserMap[i].userid<<'\t'<<UserMap[i].nickname<<'\t'<<inet_ntoa(UserMap[i].clnt_addr.sin_addr)<<':'<<htons(UserMap[i].clnt_addr.sin_port)<<"\t<-me"<<endl;
        }
        else{
            cout<<UserMap[i].userid<<'\t'<<UserMap[i].nickname<<'\t'<<inet_ntoa(UserMap[i].clnt_addr.sin_addr)<<':'<<htons(UserMap[i].clnt_addr.sin_port)<<endl;
        }
    }
    return;
}
void welcomeMessage(int clnt_sock)
{
    for(int i = 0 ; i < UserMap.size() ; i++)
    {
        if(UserMap[i].client_scoketfd_number==clnt_sock)
        {
            dup2(UserMap[i].client_scoketfd_number ,STDOUT_FILENO);
            cout<<"****************************************"<<endl;
            cout<<"** Welcome to the information server. **"<<endl;
            cout<<"****************************************"<<endl;
        }
    }
    dup2(stdout_copy,STDOUT_FILENO);
    return;
}

void nameUserName(int clnt_sock)
{
    //check the name is used or not
    for(int i = 0 ; i < UserMap.size() ; i++)
    {
        if (UserMap[i].nickname==cmd[1])
        {
            dup2(clnt_sock ,STDOUT_FILENO);
            cout<<"*** User '"<<cmd[1]<<"' already exists. ***"<<endl;
            dup2(stdout_copy,STDOUT_FILENO);
            return;
        }
    }
    // if not we name it
    for(int i = 0 ; i < UserMap.size() ; i++)
    {
        if(UserMap[i].client_scoketfd_number==clnt_sock)
        {
            UserMap[i].nickname=cmd[1];
            for (int j = 0 ; j < UserMap.size() ; j++)
            {
                dup2(UserMap[j].client_scoketfd_number ,STDOUT_FILENO);
                cout<<"*** User from "<<inet_ntoa(UserMap[i].clnt_addr.sin_addr)<<':'<<htons(UserMap[i].clnt_addr.sin_port)<<" is named '"<<UserMap[i].nickname<<"'. ***"<<endl;
            }
        }
    }
    dup2(stdout_copy,STDOUT_FILENO);
    return;
}

void tell(int clnt_sock, int friends)
{
    //check the user that be telled exist or not
    bool findFriends = false;
    for(int i = 0 ; i < UserMap.size() ; i++)
    {
        if (UserMap[i].userid==friends)
        {
            findFriends = true;
            dup2(UserMap[i].client_scoketfd_number ,STDOUT_FILENO);
        }
    }
    if(!findFriends)
    {
        dup2(clnt_sock ,STDOUT_FILENO);
        cout<<"*** Error: user #"<<friends<<" does not exist yet. ***"<<endl;
    }
    else{
        // input message to friends
        for(int i = 0 ; i < UserMap.size() ; i++)
        {
            if (UserMap[i].client_scoketfd_number==clnt_sock)
            {
                cout<<"*** "<<UserMap[i].nickname<<" told you ***: ";
            }
        }
        for(int i = 2 ; i < cmd.size() ; i++)
        {
            if (i==cmd.size()-1)
            {
                cout<<cmd[i]<<endl;
            }
            else{
                cout<<cmd[i]<<" ";
            }
        }
    }
    dup2(stdout_copy,STDOUT_FILENO);
    return;

}

void yell(int clnt_sock)
{
    for(int i= 0 ; i < UserMap.size() ; i++)
    {
        if (UserMap[i].client_scoketfd_number==clnt_sock)
        {
            for (int j = 0 ; j < UserMap.size() ; j++)
            {
                dup2(UserMap[j].client_scoketfd_number ,STDOUT_FILENO);
                cout<<"*** "<<UserMap[i].nickname<<" yelled ***: ";
                for(int k = 1 ; k < cmd.size() ; k++)
                {
                    if (k==cmd.size()-1)
                    {
                        cout<<cmd[k]<<endl;
                    }
                    else{
                        cout<<cmd[k]<<" ";
                    }
                }
            }
        }
    }
    
    dup2(stdout_copy,STDOUT_FILENO);
    return;
}

void userpipeGive(int pipe_position , int userpipe_number)
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

	// find the index that we will use in findind command
	int this_pipe_position = pipe_position;
	int last_pipe_position = find_last_special_command(pipe_position);
	// cout<<this_pipe_position<<" "<<last_pipe_position<<endl;

	// this is to finad command that we need to execvp
	int index = 0;
	char* argv[MAX_ARGV_SIZE];
	for(int i = last_pipe_position+1 ; i < this_pipe_position;i++)
	{
        
        if(cmd[i][0]=='<') continue;
		argv[index] = strdup(cmd[i].c_str());
        // cout<<argv[index]<<endl;
		index++;
	}
	argv[index] = NULL;

	// check user is life
    bool life_or_not = false; // cheack user is live or not
    bool the_user_exit = true;// is same with life_or_not to check user live or not
    for(int i = 0 ; i < UserMap.size() ; i++)
    {
        if(UserMap[i].userid == userpipe_number)
        {
            life_or_not= true;
        }
    }
    if(!life_or_not)
    {
        the_user_exit=false;
        dup2(whoIsUseServer->client_scoketfd_number ,STDOUT_FILENO);
        cout<<"*** Error: user #"<< userpipe_number <<" does not exist yet. ***"<<endl;
        dup2(stdout_copy,STDOUT_FILENO);
		if(!check_file_exist(argv[0]))
		{
			cerr<<"Unknown command: ["<<argv[0]<<"]."<<endl;
		}
		return ;
    }
    //cheack Userpipe exist or not
    bool cheack_exist_userpipe = false;
    for(int i = 0 ; i < whoIsUseServer->userpipeMap.size() ; i++)
    {
        if(whoIsUseServer->userpipeMap[i].givewhoUserID == userpipe_number)
        {
            cheack_exist_userpipe = true;
        }

    }
    if(cheack_exist_userpipe)
    {
        dup2(whoIsUseServer->client_scoketfd_number ,STDOUT_FILENO);
        cout<<"*** Error: the pipe #"<<whoIsUseServer->userid<<"->#"<<userpipe_number<<" already exists. ***"<<endl;
        dup2(stdout_copy,STDOUT_FILENO);
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
	int fd_to_write[2];
    if(pipe(fd_to_write) < 0 ){ perror("pipe write get wrong!"); }

	
    //boardcast send message
    for (int i = 0 ; i < UserMap.size() ; i++)
    {
        if(UserMap[i].userid == userpipe_number)
        {
            // cout<<UserMap[i].userid<<endl;
            for (int j = 0 ; j < UserMap.size() ; j++){
                dup2(UserMap[j].client_scoketfd_number ,STDOUT_FILENO);
                cout<<"*** "<<whoIsUseServer->nickname<<" (#"<<whoIsUseServer->userid<<") just piped '"<<command<<"' to "<<UserMap[i].nickname<<" (#"<<UserMap[i].userid<<")  ***"<<endl;
                dup2(stdout_copy,STDOUT_FILENO);
            }

        }
    }
	// 若是存在，就要stderror_number讀取0號管，再執行execvp 
	int data_to_read = find_pipe_num(0);
	if(data_to_read !=-1)
	{
		int fd_to_read[2];
		fd_to_read[0] = whoIsUseServer->pipeMap[data_to_read].fdout;
        fd_to_read[1] = whoIsUseServer->pipeMap[data_to_read].fdin;
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
                if(fd_to_write[1] != STDOUT_FILENO){ dup2(fd_to_write[1],STDOUT_FILENO); }	
                if(fd_to_write[1] != STDOUT_FILENO){ close(fd_to_write[1]); }
                if(fd_to_write[0] != STDIN_FILENO) { close(fd_to_write[0]); }
            }
            else{
                dup2(whoIsUseServer->client_scoketfd_number,STDOUT_FILENO); 
            }
            // dup2(stdout_copy,STDOUT_FILENO);
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
			//dip back
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stdin_copy,STDIN_FILENO);
			//store pipe
            if(the_user_exit)
            {
                // if(fd_to_write[1] != STDOUT_FILENO){ close(fd_to_write[1]);	}
                userpipe temp = {userpipe_number,fd_to_write[1],fd_to_write[0]};
			    whoIsUseServer->userpipeMap.push_back(temp);
            }
			// sleep(1);
            // int status;
			// waitpid(pid , &status ,0);		
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
                if(fd_to_write[1] != STDOUT_FILENO){ dup2(fd_to_write[1],STDOUT_FILENO); }	
                if(fd_to_write[1] != STDOUT_FILENO){ close(fd_to_write[1]); }
                if(fd_to_write[0] != STDIN_FILENO) { close(fd_to_write[0]); }
            }
            else{
                dup2(whoIsUseServer->client_scoketfd_number,STDOUT_FILENO); 
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
			//dup back
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stdin_copy,STDIN_FILENO);
			//store pipe
			if(the_user_exit)
            {
                // if(fd_to_write[1] != STDOUT_FILENO){ close(fd_to_write[1]);	}
                userpipe temp = {userpipe_number,fd_to_write[1],fd_to_write[0]};
			    whoIsUseServer->userpipeMap.push_back(temp);
            }
			// sleep(3);
            // int status;
			// waitpid(pid , &status ,0);
		}
    }
	return;
}

void userpipeTake(int pipe_position , int userpipe_number )
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
    int writefdNumber;
    // check user is life
    bool life_or_not = false;
    for(int i = 0 ; i < UserMap.size() ; i++)
    {
        if(UserMap[i].userid == userpipe_number)
        {
            life_or_not= true;
            writefdNumber = UserMap[i].client_scoketfd_number;
        }
    }
    if(!life_or_not)
    {
        dup2(whoIsUseServer->client_scoketfd_number ,STDOUT_FILENO);
        cout<<"*** Error: user #"<< userpipe_number <<" does not exist yet. ***"<<endl;
        dup2(stdout_copy,STDOUT_FILENO);
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
	
	//we need to see whether the 0號管 exist？
	bool data_to_read = false;
    int fd_to_read[2];
    for(int i = 0 ; i < UserMap.size() ; i++)
    {
        if(UserMap[i].userid == userpipe_number)
        {
            for(int j = 0 ; j < UserMap[i].userpipeMap.size() ; j++)
            {
                if (UserMap[i].userpipeMap[j].givewhoUserID == whoIsUseServer->userid)
                {
                    data_to_read = true;
                    fd_to_read[0] = UserMap[i].userpipeMap[j].fdout;
                    fd_to_read[1] = UserMap[i].userpipeMap[j].fdin;
                    close(fd_to_read[1]);
                    UserMap[i].userpipeMap.erase(UserMap[i].userpipeMap.begin()+j);
                }
            }
        }
    }
	// 若是存在，就要stderror_number讀取0號管，再執行execvp 
	if(data_to_read)
	{
        // pipeMap.erase(pipeMap.begin()+data_to_read);// no need to do it because the close_file() will do this 

		signal(SIGCHLD,signal_handler);//to wait the child to come back
		pid_t pid;
		pid = fork();

		if (pid<0){ perror("fuck! numberpipe: "); }
		else if(pid == 0)
		{
            //boardcast recieve message
            for (int i = 0 ; i < UserMap.size() ; i++)
            {
                if(UserMap[i].userid == userpipe_number)
                {
                    for (int j = 0 ; j < UserMap.size() ; j++){
                        dup2(UserMap[j].client_scoketfd_number ,STDOUT_FILENO);
                        cout<<"*** "<<whoIsUseServer->nickname<<" (#"<<whoIsUseServer->userid<<") just received from "<<UserMap[i].nickname<<" (#"<<UserMap[i].userid<<") by '"<<command<<"' ***"<<endl;
                        // dup2(stdout_copy,STDOUT_FILENO);
                    }

                }
            }
            dup2(whoIsUseServer->client_scoketfd_number , STDOUT_FILENO);
			//dup file
			if(fd_to_read[0] != STDIN_FILENO)  { dup2(fd_to_read[0],STDIN_FILENO);   }
			//close file
			if(fd_to_read[0] != STDIN_FILENO)  { close(fd_to_read[0]);  }
			if(fd_to_read[1] != STDOUT_FILENO) { close(fd_to_read[1]);  }
			
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
                // cout<<"哭阿2"<<endl;
                execvp(argv[0],argv); 
            }
			exit(0);
		}
		else
		{	
			//dip back
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stdin_copy,STDIN_FILENO);
			
			sleep(1);		
            int status;
			waitpid(pid , &status ,0);
            	
		}
	}
	// 若是不存在，就直接執行Execvp並存在管子裡面
	else{

        dup2(whoIsUseServer->client_scoketfd_number,STDOUT_FILENO);
        cout<<"*** Error: the pipe #"<<userpipe_number<<"->#"<<whoIsUseServer->userid<<" does not exist yet. ***"<<endl;
        dup2(stdout_copy,STDOUT_FILENO);
    }
	return;
}

void userpipeTake_with_another_commands(int pipe_position , int userpipe_number , bool userpipe_should_be_Cat_first)
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
    // check user is life
    bool life_or_not = false;
    for(int i = 0 ; i < UserMap.size() ; i++)
    {
        if(UserMap[i].userid == userpipe_number)
        {
            life_or_not= true;
        }
    }
    if(!life_or_not)
    {
        dup2(whoIsUseServer->client_scoketfd_number ,STDOUT_FILENO);
        cout<<"*** Error: user #"<< userpipe_number <<" does not exist yet. ***"<<endl;
        dup2(stdout_copy,STDOUT_FILENO);
        return ;
    }

	// find the index that we will use in findind command
	int this_pipe_position = pipe_position;
	int last_pipe_position = find_last_special_command(pipe_position);
	// cout<<this_pipe_position<<" "<<last_pipe_position<<endl;

	// this is to finad command that we need to execvp
	int index = 0;
	char* argv[MAX_ARGV_SIZE];
    
	//if we meet userpipe_should_be_Cat_first we need to cat further pipe_position
	//because we not to do it the last_pipe_position will be '>' position
	//we can't get any command
	//e.g.  
	//  number >2 <3
	//  we need cat <3 and then number >2, but last_pipe_position is 1 and this_pipe_position is 2, therefore, we can't get any command.
	//  so we should find 更前面的 pipe position, last_pipe_position will be -1, we can get the number!!
	//  after get command we need to cahge it to be "cat" since number 是給 '>' 用的 
    if(userpipe_should_be_Cat_first)
    {
        last_pipe_position = find_last_special_command(last_pipe_position);
    }

	for(int i = last_pipe_position+1 ; i < this_pipe_position;i++)
	{
        if(cmd[i][0]=='>') continue;
		argv[index] = strdup(cmd[i].c_str());
		index++;
	}
	if(pipe_position!=cmd.size()-1 && cmd[pipe_position+1][0]=='>' )
	{
		string a = "cat";
    	argv[0]=strdup(a.c_str());
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

	//we need to see whether the 0號管 exist？
	bool data_to_read = false;
    int fd_to_read[2];
    for(int i = 0 ; i < UserMap.size() ; i++)
    {
        if(UserMap[i].userid == userpipe_number)
        {
            for(int j = 0 ; j < UserMap[i].userpipeMap.size() ; j++)
            {
                if (UserMap[i].userpipeMap[j].givewhoUserID == whoIsUseServer->userid)
                {
                    data_to_read = true;
                    fd_to_read[0] = UserMap[i].userpipeMap[j].fdout;
                    fd_to_read[1] = UserMap[i].userpipeMap[j].fdin;
                    close(fd_to_read[1]);
                    UserMap[i].userpipeMap.erase(UserMap[i].userpipeMap.begin()+j);
                }
            }
        }
    }

    
	// 若是存在，就要stderror_number讀取0號管，再執行execvp 
	if(data_to_read)
	{
        // pipeMap.erase(pipeMap.begin()+data_to_read);// no need to do it because the close_file() will do this 
        //boardcast recieve message
        for (int i = 0 ; i < UserMap.size() ; i++)
        {
            if(UserMap[i].userid == userpipe_number)
            {
                for (int j = 0 ; j < UserMap.size() ; j++){
                    dup2(UserMap[j].client_scoketfd_number ,STDOUT_FILENO);
                    cout<<"*** "<<whoIsUseServer->nickname<<" (#"<<whoIsUseServer->userid<<") just received from "<<UserMap[i].nickname<<" (#"<<UserMap[i].userid<<") by '"<<command<<"' ***"<<endl;
                }

            }
        }
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
			// if(fd_to_read[1] != STDOUT_FILENO) { close(fd_to_read[1]);  }
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
			//dip back
			// if(fd_to_write[1] != STDOUT_FILENO){ close(fd_to_write[1]); }
            // if(fd_to_read[0] != STDIN_FILENO)  { close(fd_to_read[0]);  }
			// if(fd_to_read[1] != STDOUT_FILENO) { close(fd_to_read[1]);  }
			dup2(stdout_copy,STDOUT_FILENO);
			dup2(stdin_copy,STDIN_FILENO);
			//store pipe
			pipefd temp = {-1,fd_to_write[1],fd_to_write[0]};
			whoIsUseServer->pipeMap.push_back(temp);
			// sleep(1);	
            // int status;
			// waitpid(pid , &status ,0);
					
		}
	}
	// 若是不存在，就直接執行Execvp並存在管子裡面
	else{

        dup2(whoIsUseServer->client_scoketfd_number,STDOUT_FILENO);
        cout<<"*** Error: the pipe #"<<userpipe_number<<"->#"<<whoIsUseServer->userid<<" does not exist yet. ***"<<endl;
        dup2(stdout_copy,STDOUT_FILENO);
    }
	return;
}

void Erase_userAllInformation(int Userid)
{
	//To erase the exit user's userpipe
	//because we save userpipe in sender, so that we should run all user's userpipe to find the leaving user and delete it 
	for(int i = 0 ; i < UserMap.size() ; i++ )
	{
		for(int j = 0 ; j < UserMap[i].userpipeMap.size() ; j++)
		{
			if(UserMap[i].userpipeMap[j].givewhoUserID==Userid)
			{
				UserMap[i].userpipeMap.erase(UserMap[i].userpipeMap.begin()+j);
			}
		}
	}
}

void sortUserMap()
{
	// we will sort UserMap to let "who" command can easy to output
	for(int i = 0 ; i < UserMap.size() ; i++)
    {
        for(int j = 0 ; j < UserMap.size() ; j++)
        {
			if(UserMap[i].userid < UserMap[j].userid)
			{
				struct user temp = UserMap[i];
				UserMap[i]=UserMap[j];
				UserMap[j] = temp;
			}
		}
	}
}