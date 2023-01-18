#include <iostream>
#include <boost/asio.hpp>
#include <cstdlib>
#include <cstdio>
#include <fstream>
using namespace std;
using boost::asio::ip::tcp;
using namespace boost::asio;
using boost::asio::io_service;

boost::asio::io_context context;

class session : public std::enable_shared_from_this<session> {

    shared_ptr<tcp::socket>  socket_ , socket_to_npshell;
    enum { max_length = 15000};
    char data_[max_length];
    char data[max_length];

public:
    // this socket is come from server accept's socket
    // we will start to scan the URI info and set up env.
    // and then call panel.cgi to choose server and file
    // finally panel.cgi will call console.cgi
    session(shared_ptr<tcp::socket>  socket1 , shared_ptr<tcp::socket> socket2) : socket_(socket1) , socket_to_npshell(socket2) {}
    void start() { 
        // cout<<"send message!"<<endl;
        do_read_CGI_to_SOCK(); 
        do_read_npshell_to_SOCK();
    }

    // scan URI and send to do_write for setting up info
    void do_read_CGI_to_SOCK() {
        auto self(shared_from_this());
        socket_->async_read_some(boost::asio::buffer(data_, max_length), [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                do_write_SOCK_to_npshell(length);
            }
        });
    }

    // set up env and call panel.cgi
    void do_write_SOCK_to_npshell(size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(*socket_to_npshell, boost::asio::buffer(data_, length),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec){
                do_read_CGI_to_SOCK();
            }
        });
    }
    void do_read_npshell_to_SOCK(){
        auto self(shared_from_this());        
        socket_to_npshell->async_read_some(boost::asio::buffer(data, max_length), [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                do_write_SOCK_to_CGI(length);
            }
        });
    }
    void do_write_SOCK_to_CGI(size_t length){
        auto self(shared_from_this());
        socket_->async_send(buffer(data, length), [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec){
                    do_read_npshell_to_SOCK();
                }
                
        });
    }

};

class SOCKS : public std::enable_shared_from_this<SOCKS>
{
    shared_ptr<tcp::socket>  socket_ , sock_to_npshell , sock_to_bind; 
    tcp::socket sock_npshell{context};
    tcp::socket sock_bind{context};
    tcp::resolver socks_resolver{context};
    tcp::acceptor _acceptor_{context};
    tcp::resolver::iterator iter;
    enum { max_length = 1024 };
    unsigned char data_[max_length];
    unsigned char SOCKS_response_packets[8];
    public:
        SOCKS(tcp::socket socket) : socket_(std::make_shared<ip::tcp::socket>(move(socket))) {}
        void start()
        {
            do_SOCKS_recieve();
        }
    private:
        void do_SOCKS_recieve()
        {
            auto self(shared_from_this());
            memset(data_ , '\0' , max_length);
            socket_->async_read_some(boost::asio::buffer(data_, max_length), [this, self](boost::system::error_code ec, std::size_t length) {
                do_SOCKS_parse(data_ , length);
            });
        }

        void do_SOCKS_parse(unsigned char* SOCKS_request_packets , std::size_t length)
        {
            unsigned char VN , CD ;
            int destination_port;            
            string destination_IP="";    
            string domain_name="";

            VN = SOCKS_request_packets[0];
            CD = SOCKS_request_packets[1];
            // cout<<"CD is "<<CD<<endl;
            destination_port = SOCKS_request_packets[2]<<8 | SOCKS_request_packets[3];

            if(VN != 4){
                cerr<<"There a version wrong!!!"<<endl;
                return;
            }

            destination_IP += to_string(SOCKS_request_packets[4]);
            destination_IP += "." ;
            destination_IP += to_string(SOCKS_request_packets[5]);
            destination_IP += "." ;
            destination_IP += to_string(SOCKS_request_packets[6]);
            destination_IP += ".";
            destination_IP += to_string(SOCKS_request_packets[7]);

            memset(SOCKS_response_packets, 0 , sizeof(SOCKS_response_packets));
            SOCKS_response_packets[1] = firewall(destination_IP, CD) ? 91 : 90;
            cout << "<S_IP>:   " << socket_->remote_endpoint().address().to_string() << endl
                << "<S_PORT>:  " << socket_->remote_endpoint().port() << endl
                << "<D_IP>:    " << destination_IP << endl
                << "<D_PORT>:  " << destination_port << endl
                << "<Command>: " << ((CD == 1) ? "CONNECT" : "BIND") << endl
                << "<Reply>:   " << ((SOCKS_response_packets[1] == 90) ? "Accept" : "Reject") << endl
                << endl;
            if(SOCKS_response_packets[1] == 91) {
                do_SOCKS_reply(SOCKS_response_packets);
                return;
            }

            auto self(shared_from_this());
            if(CD == 1){ // connect 要去建立連線
                // cout<<"CD is 1 in if-else function"<<endl;
                do_SOCKS_reply(SOCKS_response_packets);
                
                tcp::resolver::query que(destination_IP, to_string(destination_port));
                socks_resolver.async_resolve(que ,[this, self](const boost::system::error_code &ec,tcp::resolver::iterator it) 
                {
                    if (!ec)
                    {
                        sock_npshell.async_connect(*it , [this, self](boost::system::error_code ec2) 
                        {
                            // we need to listen the welcome message, so I use read first
                            if (!ec2)
                            {
                                sock_to_npshell = make_shared<ip::tcp::socket>(move(sock_npshell));
                                std::make_shared<session>(socket_, sock_to_npshell) ->start();
                            }
                        });
                    }
                });

            }
            else if ( CD == 2){// bind 要去接受connect
                tcp::endpoint bind_endpoint_(tcp::endpoint(tcp::v4(), 0));
                _acceptor_.open( bind_endpoint_.protocol());
                _acceptor_.set_option( tcp::acceptor::reuse_address( true ) );
                _acceptor_.bind( bind_endpoint_ );
                _acceptor_.listen();
                SOCKS_response_packets[2] = _acceptor_.local_endpoint().port() /256 ;
                SOCKS_response_packets[3] = _acceptor_.local_endpoint().port() %256 ;
                do_SOCKS_reply(SOCKS_response_packets);
                // cout<<"we gonna to do accept"<<endl;
                _acceptor_.async_accept(sock_bind,[this,self](error_code ec) {
                    if (!ec) {
                        sock_to_bind = make_shared<ip::tcp::socket>(move(sock_bind));
                        do_SOCKS_reply(SOCKS_response_packets);
                        // cout<<"done! accept"<<endl;
                        std::make_shared<session>(socket_, sock_to_bind) ->start();
                    }
                    else{
                        cout<<"Failed! accept"<<endl;
                        cerr<<"can't connect to FTP server"<<endl;
                    }
                });
            }
            else{
                cerr<<"CD is wrong!!!"<<endl;
                return;
            }
        }

        void do_SOCKS_reply(unsigned char* SOCKS_response_packets)
        {
            auto self(shared_from_this());
            socket_->async_send(buffer(SOCKS_response_packets, 8), [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if(ec) cerr << "do_write error\n";
            });
        }

        bool firewall(string IP , unsigned char Command)
        {
            string permit_ip ;
            string permit_word ;
            string permit_command_type;
            ifstream ifs;
            ifs.open ("socks.conf", std::ifstream::in);
            if(!ifs) {
                cerr << "socks.conf open error"<<endl;
                return true;
            }
            while(ifs >> permit_word >> permit_command_type >> permit_ip) {
                int pos = 0;
                string split_ip, split_permit_ip;
                if((permit_command_type == "c" && Command == 1) || (permit_command_type == "b" && Command == 2)) {
                    for(; pos < 4; pos++) { 
                        split_ip = IP.substr(0, IP.find("."));
                        split_permit_ip = permit_ip.substr(0, permit_ip.find("."));
                        IP = IP.substr(IP.find(".") + 1);
                        permit_ip = permit_ip.substr(permit_ip.find(".") + 1);
                        // cout<<split_ip<<" : "<<split_permit_ip<<endl;
                        if(split_permit_ip == "*" || split_ip == split_permit_ip) continue;
                        else break;
                    }
                    if(pos == 4) return false;
                }
            }

            return true;
        }

};

class Server {

        tcp::acceptor acceptor_;
        tcp::socket socket_{context};
        
    public:
        // if anyone want to connect, we use it to accept
        Server(boost::asio::io_context & context, short port)
        : acceptor_(context, tcp::endpoint(tcp::v4(), port)) {
            do_accept();
        }

        void do_accept() {
            acceptor_.async_accept(socket_,[this](error_code ec) {
                if (!ec) {
                    context.notify_fork(boost::asio::io_service::fork_prepare);
                    pid_t pid;
                    pid = fork();
                    if(pid < 0){
                        cerr<<"Fork is crash"<<endl;
                        socket_.close();
                    }
                    else if (pid == 0){
                        context.notify_fork(boost::asio::io_service::fork_child);
                        acceptor_.close();
                        std::make_shared<SOCKS>(std::move(socket_))->start();
                    }
                    else{
                        context.notify_fork(boost::asio::io_service::fork_parent);
                        socket_.close();
                    }
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
    // build server to async accept the connection
    Server s(context, std::atoi(argv[ 1]));

    context.run();
    } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}