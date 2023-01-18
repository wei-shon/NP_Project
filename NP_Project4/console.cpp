#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <string>
#include <vector>
#include <fstream>
using namespace std;
using boost::asio::ip::tcp;
using boost::asio::io_service;
using namespace boost::asio;
using namespace boost::asio::ip;

boost::asio::io_context context;
vector <string> all_query_String;
vector <string> host; 
vector <string> port; 
vector <string> file; 

const string ENV[ 8 ] = {                                   
        "HTTP_HOST", "QUERY_STRING", "REMOTE_ADDR", "REMOTE_PORT",      
        "REQUEST_METHOD", "REQUEST_URI", "SERVER_ADDR", "SERVER_PORT",    
}; 

void replace(string &data) {
    boost::replace_all(data, "&", "&amp;");
    boost::replace_all(data, "\"", "&quot;");
    boost::replace_all(data, "\'", "&apos;");
    boost::replace_all(data, "<", "&lt;");
    boost::replace_all(data, ">", "&gt;");
    boost::replace_all(data, "\r", "");
    boost::replace_all(data, "\n", "&NewLine;");
}
void output_command(string session, string content){
    // we need to use address to replace data, or we may not change successfully
    replace(content);
    cout<<"<script>document.getElementById('"<<session<<"').innerHTML += '<b>" << content << "</b>';</script>";
    cout.flush();
}
 
void output_shell(string session, string content){
    // we need to use address to replace data, or we may not change successfully
    replace(content);
    cout<<"<script>document.getElementById('"<<session<<"').innerHTML += '" << content << "';</script>";
    cout.flush();
}

class Client : public std::enable_shared_from_this<Client>
{

    string FileName; // remember execute file name
    string session; // remember s0 s1 s2... 
    tcp::resolver::query que; 
    string host;
    string port;
    fstream file; // remember open file
    tcp::resolver resolver{context};
    tcp::resolver socks_resolver{context};
    tcp::socket socket_{context};
    enum { max_length = 15000 };
    char data_[max_length];
    
public:
    Client(string FileName , string session , tcp::resolver::query que  ,string host , string port):
    FileName(FileName) , session(std::move(session)), que(std::move(que)) , host(host) , port(port)
    {
        // open file to read command.
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
                // change data_ to string
                string data=string(data_);     
                output_shell(session, data);
                // If we listen the "% " and then, send command to web because web accept to catch command and executive result
                // After sending message, start to listen web is already or not
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
        output_command(session, command);
        async_write(socket_,buffer(command.c_str(),command.length()),
                [this, self](boost::system::error_code ec, std::size_t length ) {
        });
    }

    void do_socks_reply()
    {
        auto self(shared_from_this());
        socket_.async_read_some(buffer(data_, max_length),  [this, self](boost::system::error_code ec , size_t length) 
            {
                if(!ec) {
                    if(data_[1] != 90) {
                        // beside 90 is OK, others is fail connect
                        output_command(session,"Socks4 request rejected or failed\n");
                        exit(1);
                    }
                    do_read();
                }
            });
    }

    void do_socks_server_connect(tcp::endpoint end_ip)
    {
        auto self(shared_from_this());
        vector<string> ip_split = {};
        string ip = end_ip.address().to_string();
        boost::split(ip_split, ip, boost::is_any_of("."), boost::token_compress_on);
        unsigned char SOCKS_Requests[1024];
        memset(SOCKS_Requests , 0 , 1024);
        SOCKS_Requests[0] = 4;
        SOCKS_Requests[1] = 1;
        SOCKS_Requests[2] = stoi(port) /256;
        SOCKS_Requests[3] = stoi(port) %256;
        SOCKS_Requests[4] = (unsigned char)atoi(ip_split[0].c_str());
        SOCKS_Requests[5] = (unsigned char)atoi(ip_split[1].c_str());
        SOCKS_Requests[6] = (unsigned char)atoi(ip_split[2].c_str());
        SOCKS_Requests[7] = (unsigned char)atoi(ip_split[3].c_str());
        socket_.async_send(buffer(SOCKS_Requests, 8), [this, self](boost::system::error_code ec, size_t len)
            {
                if(!ec) {
                    do_socks_reply();               
                }
            });
    }
    void do_connect(tcp::endpoint socket_endpoint ,tcp::resolver::iterator it)
    {
        auto self(shared_from_this());
        tcp::endpoint end_ip = *it;
        socket_.async_connect(socket_endpoint , [this, self , end_ip](boost::system::error_code ec) 
        {
            // we need to listen the welcome message, so I use read first
            if (!ec) do_socks_server_connect(end_ip);
        });
    }
    void do_socks_server_resolver(tcp::resolver::iterator it)
    {   
        auto self(shared_from_this());
        tcp::endpoint socket_endpoint = *it;
        tcp::resolver::query query(host, port);
        socks_resolver.async_resolve(query ,[this, self , socket_endpoint](const boost::system::error_code &ec,tcp::resolver::iterator it) 
        {
            if (!ec) do_connect(socket_endpoint , it);
        });
    }

    void do_resolve()
    {
        auto self(shared_from_this());
        resolver.async_resolve(que,[this, self](const boost::system::error_code &ec,tcp::resolver::iterator it) 
        {
            if (!ec) do_socks_server_resolver(it);
        });
    }

};


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

void html_background(){    
    // turn the sample_console.cgi into this
    // turn the sample_console.cgi into this
    cout<<"<!DOCTYPE html>\n"<<
    "<html lang=\"en\">\n"<<
    "<head>\n"<<
    "<meta charset=\"UTF-8\" />\n"<<
    "<title>NP Project 3 Sample Console</title>\n"<<
    "<link\n"<<
        "rel=\"stylesheet\"\n"<<
        "href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n"<<
        "integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n"<<
        "crossorigin=\"anonymous\"\n"<<
    "/>\n"<<
    "<link\n"<<
        "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"<<
        "rel=\"stylesheet\"\n"<<
    "/>\n"<<
    "<link\n"<<
        "rel=\"icon\"\n"<<
        "type=\"image/png\"\n"<<
        "href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n"<<
    "/>\n"<<
    "<style>\n"<<
        "* {\n"<<
        "font-family: \'Source Code Pro\', monospace;\n"<<
        "font-size: 1rem !important;\n"<<
        "}\n"<<
        "body {\n"<<
        "background-color: #212529;\n"<<
        "}\n"<<
        "pre {\n"<<
        "color: #cccccc;\n"<<
        "}\n"<<
        "b {\n"<<
        "color: #01b468;\n"<<
        "}\n"<<
    "</style>\n"<<
    "</head>\n"<<
    "<body>\n"<<
    "<table class=\"table table-dark table-bordered\">\n"<<
        "<thead>\n"<<
        "<tr>\n";
        for(unsigned int j = 0 ; j < host.size() ; j++)
        {
            cout<<"<th scope=\"col\">"<<host[j]<<":"<<port[j]<<"</th>\n";
        }
        cout<<"</tr>\n"<<
        "</thead>\n"<<
        "<tbody>\n"<<
        "<tr>\n";
        for(unsigned int j = 0 ; j < host.size() ; j++)
        {
            cout<<"<td><pre id=\"s"<<to_string(j)<<"\" class=\"mb-0\"></pre></td>\n";
        }
        cout<<"</tr>\n"<<
        "</tbody>\n"<<
    "</table>\n"<<
    "</body>\n"<<
    "</html>"<<endl;
}


int main(int argc ,char* argv[])
{
    //console.cgi
    cout << "HTTP/1.1 200 OK"<<flush;
    cout << "Content-type:text/html\r\n\r\n"<<flush;

    string query_string="";
    string socks_host = "";
    string socks_port = "";
    //抓出 query_string 中的 hostname, port, filename
    for ( int i = 0; i < 8; i++ )
    {
        char *value = getenv( ENV[ i ].c_str() );  
        if(i==1)
        {
            string query_string(value);
            split_command(query_string);
            int count = 0;
            for(unsigned int j = 0 ; j < all_query_String.size() ; j++)
            {
                // cout<<all_query_String[j]<<endl;
                // catch socks
                if (all_query_String[j] == "sh" && all_query_String[j+1]!="sp")
                {
                    socks_host = all_query_String[j+1];
                    socks_port = all_query_String[j+3];
                    j+=3;
                    continue;
                }
                // catch the host, port, file
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
        }
    }
    // cout<<socks_host<<endl;
    // cout<<socks_port<<endl;
    //print html backgroung: header...
    html_background();
    //start to connect and do what should do

    for (unsigned int  i = 0; i < host.size(); i++) {     
        boost::asio::ip::tcp::resolver::query que{socks_host, socks_port};
        make_shared<Client>(file[i] , "s" + to_string(i), move(que) , host[i] , port[i])->start();
    }


    tcp::resolver::query http_query(tcp::v4(), socks_host , "http");
    context.run();

    return 0;
}