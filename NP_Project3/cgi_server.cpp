#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <cstring> 
#include <cstddef>
using namespace std;
using boost::asio::ip::tcp;
using boost::asio::io_service;
using namespace boost::asio;
using namespace boost::asio::ip;

typedef std::shared_ptr<ip::tcp::socket> socket_pointer;
boost::asio::io_context context;
vector <string> all_query_String;
vector <string> host; 
vector <string> port; 
vector <string> file; 


void replace(string &data) {
    boost::replace_all(data, "&", "&amp;");
    boost::replace_all(data, "\"", "&quot;");
    boost::replace_all(data, "\'", "&apos;");
    boost::replace_all(data, "<", "&lt;");
    boost::replace_all(data, ">", "&gt;");
    boost::replace_all(data, "\r", "");
    boost::replace_all(data, "\n", "&NewLine;");
}
string output_command(string session, string content){
    string comm = "";
    replace(content);
    comm+="<script>document.getElementById('";
    comm+=session;
    comm+="').innerHTML += '<b>" ;
    comm+=content;
    comm+="</b>';</script>";
    return comm;
}
 
string output_shell(string session, string content){
    string sh = "";
    replace(content);
    sh+="<script>document.getElementById('";
    sh+=session;
    sh+="').innerHTML += '";
    sh+=content;
    sh+="';</script>";
    return sh;
}


void split_command(string input_command){
	string single_command="";
	for( unsigned int i = 0 ; i <= input_command.length() ; i++)
	{
		if(input_command[i]=='=' || input_command[i]=='&')
		{
			if(single_command!="")
			{
				all_query_String.push_back(single_command);
				single_command="";
			}
		}
		else if(i == input_command.length())
		{
			if(single_command!="")
			{
				all_query_String.push_back(single_command);
			}
		}
		else
		{
			single_command+=input_command[i];
		}
	}
}
string panel_console(){
    string panel ="";
    string test_case_menu ="";
    string host_menu="";
    for(int i=1;i<=10;i++)
        test_case_menu += "<option value=\"t"+to_string(i)+".txt\">t"+to_string(i)+".txt</option>\n";
    for(int i=1;i<=12;i++)
        host_menu += "<option value=\"nplinux"+to_string(i)+".cs.nctu.edu.tw\">nplinux"+to_string(i)+"</option>\n" ;
    panel+="HTTP / 1.1 200 OK\n";
    panel+="Content-type: text/html\r\n\r\n\n";
    panel+="<!DOCTYPE html>\n";
    panel+="<html lang=\"en\">\n";
    panel+="  <head>\n";
    panel+="    <title>NP Project 3 Panel</title>\n";
    panel+="    <link\n";
    panel+="      rel=\"stylesheet\"\n";
    panel+="      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n";
    panel+="      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n";
    panel+="      crossorigin=\"anonymous\"\n";
    panel+="    />\n";
    panel+="    <link\n";
    panel+="      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n";
    panel+="      rel=\"stylesheet\"\n";
    panel+="    />\n";
    panel+="    <link\n";
    panel+="      rel=\"icon\"\n";
    panel+="      type=\"image/png\"\n";
    panel+="      href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"\n";
    panel+="    />\n";
    panel+="    <style>\n";
    panel+="      * {\n";
    panel+="        font-family: 'Source Code Pro', monospace;\n";
    panel+="      }\n";
    panel+="    </style>\n";
    panel+="  </head>\n";
    panel+="  <body class=\"bg-secondary pt-5\">\n";
    panel+="<form action=\"console.cgi\" method=\"GET\">\n" ;
    panel+="  <table class=\"table mx-auto bg-light\" style=\"width: inherit\">\n" ;
    panel+="    <thead class=\"thead-dark\">\n" ;
    panel+="      <tr>\n" ;
    panel+="        <th scope=\"col\">#</th>\n" ;
    panel+="        <th scope=\"col\">Host</th>\n" ;
    panel+="        <th scope=\"col\">Port</th>\n" ;
    panel+="        <th scope=\"col\">Input File</th>\n" ;
    panel+="      </tr>\n" ;
    panel+="    </thead>\n" ;
    panel+="    <tbody>\n" ;
    panel+="<tr>\n";
    for (int i=0;i<5;i++){
        panel+="        <th scope=\"row\" class=\"align-middle\">Session "+to_string(i+1)+"</th>\n";
        panel+="        <td>\n";
        panel+="          <div class=\"input-group\">\n";
        panel+="            <select name=\"h"+to_string(i)+"\" class=\"custom-select\">\n" ;
        panel+="              <option></option>"+host_menu+"\n";
        panel+="            </select>\n";
        panel+="            <div class=\"input-group-append\">\n";
        panel+="              <span class=\"input-group-text\">.cs.nctu.edu.tw</span>\n";
        panel+="            </div>\n";
        panel+="          </div>\n";
        panel+="        </td>\n";
        panel+="        <td>\n";
        panel+="          <input name=\"p"+ to_string(i)+"\" type=\"text\" class=\"form-control\" size=\"5\" />\n";
        panel+="        </td>\n";
        panel+="        <td>\n";
        panel+="          <select name=\"f"+ to_string(i)+"\" class=\"custom-select\">\n" ;
        panel+="            <option></option>\n";
        panel+="            "+test_case_menu+"\n";
        panel+="          </select>\n";
        panel+="        </td>\n";
        panel+="      </tr>\n" ;
    }  
    panel+="              <tr>\n";
    panel+="            <td colspan=\"3\"></td>\n";
    panel+="            <td>\n";
    panel+="              <button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>\n";
    panel+="            </td>\n";
    panel+="          </tr>\n";
    panel+="        </tbody>\n";
    panel+="      </table>\n";
    panel+="    </form>\n";
    panel+="  </body>\n";
    panel+="</html>\n";
    return panel ;

}
string html_background(){    
    string background = "";
    background+="HTTP / 1.1 200 OK\n";
    background+="Content-type: text/html\r\n\r\n\n";
    background+="<!DOCTYPE html>\n";
    background+="<html lang=\"en\">\n";
    background+="<head>\n";
    background+="<meta charset=\"UTF-8\" />\n";
    background+="<title>NP Project 3 Sample Console</title>\n";
    background+="<link\n";
    background+="rel=\"stylesheet\"\n";
    background+="href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n";
    background+="integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n";
    background+="crossorigin=\"anonymous\"\n";
    background+="/>\n";
    background+="<link\n";
    background+="href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n";
    background+="rel=\"stylesheet\"\n";
    background+="/>\n";
    background+="<link\n";
    background+="rel=\"icon\"\n";
    background+="type=\"image/png\"\n";
    background+="href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n";
    background+="/>\n";
    background+="<style>\n";
    background+="* {\n";
    background+="font-family: \'Source Code Pro\', monospace;\n";
    background+="font-size: 1rem !important;\n";
    background+="}\n";
    background+="body {\n";
    background+="background-color: #212529;\n";
    background+="}\n";
    background+="pre {\n";
    background+="color: #cccccc;\n";
    background+="}\n";
    background+="b {\n";
    background+="color: #01b468;\n";
    background+="}\n";
    background+="</style>\n";
    background+="</head>\n";
    background+="<body>\n";
    background+="<table class=\"table table-dark table-bordered\">\n";
    background+="<thead>\n";
    background+="<tr>\n";
    for(unsigned int j = 0 ; j < host.size() ; j++)
    {
        background+="<th scope=\"col\">";
        background+=host[j];
        background+=":";
        background+=port[j];
        background+="</th>\n";
    }
    background+="</tr>\n";
    background+="</thead>\n";
    background+="<tbody>\n";
    background+="<tr>\n";
    for(unsigned int j = 0 ; j < host.size() ; j++)
    {
        background+="<td><pre id=\"s";
        background+=to_string(j);
        background+="\" class=\"mb-0\"></pre></td>\n";
    }
    background+="</tr>\n";
    background+="</tbody>\n";
    background+="</table>\n";
    background+="</body>\n";
    background+="</html>\n";
    return background;
}


class Client : public std::enable_shared_from_this<Client>
{
public:
    Client(string FileName , string session , socket_pointer ptr ,tcp::resolver::query que ):
    session(std::move(session)), ptr(ptr) ,que(std::move(que))
    {
        file.open("test_case/" + FileName,ios::in);
    }
    void start()
    {
        do_resolve();
    }
private:    
    void do_read()
    {
        auto self(shared_from_this());
        memset(data_ , '\0' , max_length);
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
            if (!ec) {           
                string data=string(data_);     
                string sh = output_shell(session, data);
                ptr->async_write_some(buffer(sh.c_str(), sh.length()),
                                      [this, self](boost::system::error_code ec, std::size_t length) {}
                                    );
                if (data.find("% ")!=string::npos) {
                    do_write();
                    do_read() ;   
                }else
                    do_read() ;                  
            }
        });
    }
    void do_write()
    {
        auto self(shared_from_this());
        string command ;
        getline(file, command);
        command += '\n' ;
        async_write(socket_,buffer(command.c_str(),command.length()),
                [this, self](boost::system::error_code ec, std::size_t length ) {
        });
        string comm = output_command(session, command);
        ptr->async_write_some(buffer(comm.c_str(), comm.length()),
                                      [this, self](boost::system::error_code ec, std::size_t length) {}
                                    );
    }
    void do_connect(tcp::resolver::iterator it)
    {
        auto self(shared_from_this());
        socket_.async_connect(*it, [this, self](boost::system::error_code ec) 
        {
            if (!ec) do_read();
        });
    }
    void do_resolve()
    {
        auto self(shared_from_this());
        resolver.async_resolve(que,[this, self](const boost::system::error_code &ec,tcp::resolver::iterator it) 
        {
            if (!ec) do_connect(it);
        });
    }

    string session;
    string FileName;
    fstream file;
    socket_pointer ptr;
    tcp::resolver resolver{context};
    tcp::socket socket_{context};
    tcp::resolver::query que; 
    enum { max_length = 15000 };
    char data_[max_length];
};

class session : public std::enable_shared_from_this<session> {
private:
    tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
    char status_str[200] = "HTTP/1.1 200 OK\n";
    char REQUEST_METHOD[20];
    char REQUEST_URI[1000];
    char QUERY_STRING[1000];
    char SERVER_PROTOCOL[100];
    char HTTP_HOST[100];
    char SERVER_ADDR[100];
    char SERVER_PORT[10];
    char REMOTE_ADDR[100];
    char REMOTE_PORT[10];
    char EXEC_FILE[100]="./";
    char blackhole[100];

public:
    session(tcp::socket socket) : socket_(std::move(socket)) {}
    void start() { do_read(); }

    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
            sscanf(data_, "%s %s %s %s %s", REQUEST_METHOD, REQUEST_URI,
                    SERVER_PROTOCOL, blackhole, HTTP_HOST);

            if (!ec) {
                do_write(length);
            }
            });
    }

    void do_write(std::size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(
            socket_, boost::asio::buffer(status_str, strlen(status_str)),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) 
            {
                // cout<<REQUEST_URI<<endl;
                strcpy(SERVER_ADDR , socket_.local_endpoint().address().to_string().c_str());
                sprintf(SERVER_PORT, "%u", socket_.local_endpoint().port());
                strcpy(REMOTE_ADDR , socket_.remote_endpoint().address().to_string().c_str());
                sprintf(REMOTE_PORT , "%u", socket_.remote_endpoint().port());

                bool flag = false;
                int j = 0;
                for (int i = 0; i < 1000; i++) {
                    if (REQUEST_URI[i] == '\0') {
                        break;
                    } else if (REQUEST_URI[i] == '?') {
                        flag = true;
                        continue;
                    }
                    if (flag) {
                        QUERY_STRING[j++] = REQUEST_URI[i];
                    }
                }
                j=0;
                for (int i = 1; i < 100; i++) {
                    // cout<<REQUEST_URI[i]<<endl;
                    if(REQUEST_URI[i] == '?'|| REQUEST_URI[i]=='\0')
                    {
                        break;
                    }
                    EXEC_FILE[j+2] = REQUEST_URI[i];
                    j++;
                }
                string cmd = string(EXEC_FILE);
                if (cmd == "./console.cgi") {

                    all_query_String.clear();
                    host.clear();
                    port.clear();
                    file.clear();
                    
                    string query_string=string(QUERY_STRING);
                    split_command(query_string);
                    int count = 0;
                    for(unsigned int j = 0 ; j < all_query_String.size() ; j++)
                    {
                        string str = to_string(count);
                        string h = 'h'+str;
                        string f = 'f'+str;
                        string p = 'p'+str;
                        if(all_query_String[j]==h && all_query_String[j+1] != p)
                        {
                            host.push_back(all_query_String[j+1]);
                            port.push_back(all_query_String[j+3]);
                            file.push_back(all_query_String[j+5]);
                            j+=5;
                        }
                        else{
                            j+=2;
                        }
                        count+=1;
                    }

                    string console = html_background();
                    socket_.async_send(boost::asio::buffer(console,console.length()),
                        [this, self](boost::system::error_code ec, std::size_t ){
                    });     
                    socket_pointer ptr = std::make_shared<ip::tcp::socket>(move(socket_));
                    //start to connect and do what should do
                    for (unsigned int  i = 0; i < host.size(); i++) {     
                        boost::asio::ip::tcp::resolver::query que{host[i], port[i]};
                        make_shared<Client>(file[i] , "s" + to_string(i), ptr ,  move(que))->start();
                    }
                    
                } 
                else if (cmd == "./panel.cgi"){
                    string panel = panel_console();
                    socket_.async_send(boost::asio::buffer(panel,panel.length()),
                        [this, self](boost::system::error_code ec, std::size_t ){
                    });           
                } 
                
                // do_read();
            }
        });
    }
};

class Server {
    private:
        tcp::acceptor acceptor_;
    public:
        Server(boost::asio::io_context & io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
            do_accept();
        }

        void do_accept() {
            acceptor_.async_accept(
                [this](error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<session>(std::move(socket))->start();
                }
                do_accept();
            });
        }

};



int main(int argc ,char* argv[])
{

    try {
        if (argc != 2) {
            std::cerr << "Usage: async_tcp_echo_Server <port>\n";
            return 1;
        }

        Server s(context, std::atoi(argv[ 1]));

        context.run();
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
