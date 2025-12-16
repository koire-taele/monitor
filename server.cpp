#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <cstring>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <cstdlib>
// программа реализована без ответа сервера клиенту
// т.е. сервер только читает сообщения от клиента, обратно ничего не пишет
class Server
{
private:
    int server_fd;
    int port;
    std::set<int> client_fds;
    fd_set read_fds;
    int max_fd;
    bool should_exit;
    sigset_t original_mask;
    sigset_t blocked_mask;
    static volatile sig_atomic_t signal_received;

    void check_signals()
    {
        if (signal_received)
        {
            if (signal_received == SIGHUP)
            {
                std::cout << "Received SIGHUP signal, reconfiguration\n";
            }
            else if (signal_received == SIGINT || signal_received == SIGTERM)
            {
                std::cout << "Received shutdown signal\n";
                should_exit = true;
            }
            signal_received = 0;
        }
    }
    void new_connection_handler()
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int new_client = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (new_client < 0)
        {
            std::cerr << "Accept failed " << strerror(errno) << "\n";
            return;
        }
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "New connection, IP = " << client_ip << ":" << ntohs(client_addr.sin_port) << ", fd = " << new_client << "\n";
        if (!client_fds.empty())
        {
            std::cout << "Only one connection is allowed, closing " << new_client << ")\n";
            close(new_client);
        } else
        {
            client_fds.insert(new_client);
            std::cout << "Connection accepted, fd = " << new_client << "\n";
        }
    }
    void client_data_handler()
    {
        std::vector<int> to_remove;
        for (int client_fd : client_fds)
        {
            if (FD_ISSET(client_fd, &read_fds))
            {
                char buffer[1024];
                ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    std::cout << "Received " << bytes_read << " bytes from client, fd = "<< client_fd << ", buffer = " << buffer << "\n";
                    if (strncmp(buffer, "SHUTDOWN", 8) == 0)
                    {
                        std::cout << "Received shutdown signal from client\n";
                        should_exit = true;
                        to_remove.push_back(client_fd);
                    }
                    else if (strncmp(buffer, "QUIT", 4) == 0)
                    {
                        std::cout << "Received quit signal from client\n";
                        to_remove.push_back(client_fd);
                    }
                    else {std::cout << "Echo message would be " << buffer << "\n";}
                }
                else if (bytes_read == 0)
                {
                    std::cout << "Client disconnected, fd = " << client_fd << "\n";
                    to_remove.push_back(client_fd);
                }
                else
                {
                    std::cerr << "Recv error, fd = " << client_fd << ", error = " << strerror(errno) << "\n";
                    to_remove.push_back(client_fd);
                }
            }
        }
        for (int fd : to_remove)
        {
            close(fd);
            client_fds.erase(fd);
        }
    }
    void cleanup()
    {
        for (int client_fd : client_fds) {close(client_fd);}
        client_fds.clear();
        if (server_fd >= 0)
        {
            close(server_fd);
            server_fd = -1;
        }
        sigprocmask(SIG_SETMASK, &original_mask, NULL);
        std::cout << "Server cleanup done\n";
    }
public:
    Server(int port) : port(port), max_fd(0), should_exit(false) {server_fd = -1;}
    ~Server() {cleanup();}
    static void signal_handler(int sig) {signal_received = sig;}
    bool server_initialize()
    {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0)
        {
            std::cerr << "Failed to create socket " << strerror(errno) << "\n";
            return false;
        }
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        {
            std::cerr << "Failed to set SO_REUSEADDR " << strerror(errno) << "\n";
        }
        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
        {
            std::cerr << "Bind failed on port " << port << strerror(errno) << "\n";
            int new_port = port + 1;
            address.sin_port = htons(new_port);
            if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
            {
                std::cerr << "Bind failed on port " << new_port << strerror(errno) << "\n";
                close(server_fd);
                return false;
            }
            else
            {
                port = new_port;
                std::cout << "Successfully bound to port " << port << "\n";
            }
        }
        if (listen(server_fd, SOMAXCONN) < 0)
        {
            std::cerr << "Listen failed: " << strerror(errno) << "\n";
            close(server_fd);
            return false;
        }
        std::cout << "Server listening on port " << port << "\n";
        return true;
    }
    bool signal_handle_setup()
    {
        struct sigaction sa;
        const int signals[] = {SIGHUP, SIGINT, SIGTERM};
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = signal_handler;
        sa.sa_flags = 0;
        if (sigemptyset(&sa.sa_mask) < 0)
        {
            std::cerr << "Failed to empty signal mask " << strerror(errno) << "\n";
            return false;
        }
        for (int sig : signals)
        {
            if (sigaddset(&sa.sa_mask, sig) < 0)
            {
                std::cerr << "Failed to add signal to mask " << strerror(errno) << "\n";
                return false;
            }
        }
        for (int sig : signals)
        {
            if (sigaction(sig, &sa, NULL) < 0)
            {
                std::cerr << "Failed to set handler for signal " << sig << ": " << strerror(errno) << "\n";
                return false;
            }
            std::cout << "Signal handler registered for signal " << sig << "\n";
        }
        sigemptyset(&blocked_mask);
        for (int sig : signals)
        {
            if (sigaddset(&blocked_mask, sig) < 0)
            {
                std::cerr << "Failed to add signal " << sig << " to block mask: " << strerror(errno) << "\n";
                return false;
            }
        }
        if (sigprocmask(SIG_BLOCK, &blocked_mask, &original_mask) < 0)
        {
            std::cerr << "Failed to block signals " << strerror(errno) << "\n";
            return false;
        }
        std::cout << "Signal handling setup completed\n";
        return true;
    }
    void run()
    {
        std::cout << "Starting server\n";
        std::cout << "Server commands:\n";
        std::cout << "QUIT - disconnect client\n";
        std::cout << "SHUTDOWN - stop the server\n";
        while (!should_exit)
        {
            FD_ZERO(&read_fds);
            FD_SET(server_fd, &read_fds);
            max_fd = server_fd;
            for (int client_fd : client_fds)
            {
                FD_SET(client_fd, &read_fds);
                if (client_fd > max_fd) {max_fd = client_fd;}
            }
            struct timespec timeout = {1, 0};
            int pselect_state = pselect(max_fd + 1, &read_fds, NULL, NULL, &timeout, &original_mask);
            if (pselect_state < 0)
            {
                if (errno == EINTR)
                {
                    check_signals();
                    continue;
                } else
                {
                    std::cerr << "pselect error " << strerror(errno) << "\n";
                    break;
                }
            }
            if (pselect_state > 0)
            {
                if (FD_ISSET(server_fd, &read_fds)) {new_connection_handler();}
                client_data_handler();
            }
            check_signals();
        }
        std::cout << "Server shutting down\n";
    }
    int get_port() const {return port;}
};

volatile sig_atomic_t Server::signal_received = 0;

int main(int argc, char* argv[])
{
    int port = 8080;
    if (argc > 1)
    {
        char * endptr;
        long parsed_port = strtol(argv[1], &endptr, 10);
        if (*endptr != '\0' || parsed_port <= 0 || parsed_port > 65535)
        {std::cerr << "Port number invalid, using default port (8080).\n";}
        else {port = static_cast<int>(parsed_port);}
    }

    std::cout << "Starting server\n";
    Server server(port);

    if (!server.server_initialize())
    {
        std::cerr << "Server initialization failed\n";
        return 1;
    }

    if (!server.signal_handle_setup())
    {
        std::cerr << "Signal handling setup failed!\n";
        return 1;
    }
    std::cout << "Server is running on port " << server.get_port() << "\n";
    std::cout << "Send SIGHUP for reconfiguration: kill -HUP " << getpid() << "\n";
    std::cout << "Send SIGINT/SIGTERM to shutdown: kill -INT " << getpid() << "\n";
    server.run();
    std::cout << "Server stopped successfully\n";
    return 0;
}