#include <iostream>
#include <boost/asio.hpp>

using namespace std;
using boost::asio::ip::tcp;
using boost::asio::io_service;

boost::asio::io_context io_context;

auto printenv = []() {
//   std::cout << "EXEC_FILE: " << getenv("EXEC_FILE") << std::endl;
  std::cout << "REQUEST_METHOD: " << getenv("REQUEST_METHOD") << std::endl;
  std::cout << "REQUEST_URI: " << getenv("REQUEST_URI") << std::endl;
  std::cout << "QUERY_STRING: " << getenv("QUERY_STRING") << std::endl;
  std::cout << "SERVER_PROTOCOL: " << getenv("SERVER_PROTOCOL") << std::endl;
  std::cout << "HTTP_HOST: " << getenv("HTTP_HOST") << std::endl;
  std::cout << "SERVER_ADDR: " << getenv("SERVER_ADDR") << std::endl;
  std::cout << "SERVER_PORT: " << getenv("SERVER_PORT") << std::endl;
  std::cout << "REMOTE_ADDR: " << getenv("REMOTE_ADDR") << std::endl;
  std::cout << "REMOTE_PORT: " << getenv("REMOTE_PORT") << std::endl;
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
    // this socket is come from server accept's socket
    // we will start to scan the URI info and set up env.
    // and then call panel.cgi to choose server and file
    // finally panel.cgi will call console.cgi
    session(tcp::socket socket) : socket_(std::move(socket)) {}
    void start() { do_read(); }

    // scan URI and send to do_write for setting up info
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

    // set up env and call panel.cgi
    void do_write(std::size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(
            socket_, boost::asio::buffer(status_str, strlen(status_str)),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) 
            {
                strcpy(SERVER_ADDR , socket_.local_endpoint().address().to_string().c_str());
                sprintf(SERVER_PORT, "%u", socket_.local_endpoint().port());
                strcpy(REMOTE_ADDR , socket_.remote_endpoint().address().to_string().c_str());
                sprintf(REMOTE_PORT , "%u", socket_.remote_endpoint().port());

                // find out QUERY_STRING
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
                //find out EXEC_FILE but here is always panel.cgi but we don't 寫死
                j=0;
                for (int i = 1; i < 100; i++) {
                    if(REQUEST_URI[i] == '?'|| REQUEST_URI[i]=='\0')
                    {
                        break;
                    }
                    EXEC_FILE[j+2] = REQUEST_URI[i];
                    j++;
                }

                setenv("REQUEST_METHOD", REQUEST_METHOD, 1);
                setenv("REQUEST_URI", REQUEST_URI, 1);
                setenv("QUERY_STRING", QUERY_STRING, 1);
                setenv("SERVER_PROTOCOL", SERVER_PROTOCOL, 1);
                setenv("HTTP_HOST", HTTP_HOST, 1);
                setenv("SERVER_ADDR", SERVER_ADDR, 1);
                setenv("SERVER_PORT", SERVER_PORT, 1);
                setenv("REMOTE_ADDR", REMOTE_ADDR, 1);
                setenv("REMOTE_PORT", REMOTE_PORT, 1);

                printenv();

                if (fork() != 0) {
                    socket_.close();
                } 
                else {
                    int sock = socket_.native_handle();
                    // cout<<"要執行 "<<EXEC_FILE<<endl;
                    dup2(sock, STDERR_FILENO);
                    dup2(sock, STDIN_FILENO);
                    dup2(sock, STDOUT_FILENO);
                    socket_.close();
                    if (execlp(EXEC_FILE, EXEC_FILE, NULL) < 0) {
                        std::cout << "Content-type:text/html\r\n\r\n<h1>FAIL</h1>";
                    }                
                } 
            }
        });
    }
};

class Server {
    private:
        tcp::acceptor acceptor_;
    public:
        // if anyone want to connect, we use it to accept
        Server(boost::asio::io_context & io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
            do_accept();
        }

        void do_accept() {
            acceptor_.async_accept(
                [this](error_code ec, tcp::socket socket) {
                if (!ec) {
                    // make shared pointer to let session can use share_frome_this() and point to start run session
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
        // build server to async accept the connection
        Server s(io_context, std::atoi(argv[ 1]));

        io_context.run();
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}