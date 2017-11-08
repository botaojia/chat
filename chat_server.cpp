#include <ctime>
#include <string>
#include <deque>
#include <iostream>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <thread>
#include <mutex>
#include <algorithm>
#include <iomanip>
#include <array>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include "protocol.hpp"

using boost::asio::ip::tcp;

namespace
{
std::string getTimestamp()
{
    time_t t = time(0);   // get time now
    struct tm * now = localtime(&t);
    std::stringstream ss;
    ss << '[' << (now->tm_year + 1900) << '-' << std::setfill('0')
       << std::setw(2) << (now->tm_mon + 1) << '-' << std::setfill('0')
       << std::setw(2) << now->tm_mday << ' ' << std::setfill('0')
       << std::setw(2) << now->tm_hour << ":" << std::setfill('0')
       << std::setw(2) << now->tm_min << ":" << std::setfill('0')
       << std::setw(2) << now->tm_sec << "] ";

    return ss.str();
}

class workerThread
{
public:
    static void run(std::shared_ptr<boost::asio::io_service> io_service)
    {
        {
            std::lock_guard < std::mutex > lock(m);
            std::cout << "[" << std::this_thread::get_id() << "] Thread starts" << std::endl;
        }

        io_service->run();

        {
            std::lock_guard < std::mutex > lock(m);
            std::cout << "[" << std::this_thread::get_id() << "] Thread ends" << std::endl;
        }

    }
private:
    static std::mutex m;
};

std::mutex workerThread::m;
}

class participant
{
public:
    virtual ~participant() {}
    virtual void onMessage(std::array<char, MAX_IP_PACK_SIZE> & msg) = 0;
};

class chatRoom {
public:
    void enter(std::shared_ptr<participant> participant, const std::string & nickname)
    {
        participants_.insert(participant);
        name_table_[participant] = nickname;
        std::for_each(recent_msgs_.begin(), recent_msgs_.end(),
                      boost::bind(&participant::onMessage, participant, _1));
    }

    void leave(std::shared_ptr<participant> participant)
    {
        participants_.erase(participant);
        name_table_.erase(participant);
    }

    void broadcast(std::array<char, MAX_IP_PACK_SIZE>& msg, std::shared_ptr<participant> participant)
    {
        std::string timestamp = getTimestamp();
        std::string nickname = getNickname(participant);
        std::array<char, MAX_IP_PACK_SIZE> formatted_msg;

        // boundary correctness is guarded by protocol.hpp
        strcpy(formatted_msg.data(), timestamp.c_str());
        strcat(formatted_msg.data(), nickname.c_str());
        strcat(formatted_msg.data(), msg.data());

        recent_msgs_.push_back(formatted_msg);
        while (recent_msgs_.size() > max_recent_msgs)
        {
            recent_msgs_.pop_front();
        }

        std::for_each(participants_.begin(), participants_.end(),
                      boost::bind(&participant::onMessage, _1, std::ref(formatted_msg)));
    }

    std::string getNickname(std::shared_ptr<participant> participant)
    {
        return name_table_[participant];
    }

private:
    enum { max_recent_msgs = 100 };
    std::unordered_set<std::shared_ptr<participant>> participants_;
    std::unordered_map<std::shared_ptr<participant>, std::string> name_table_;
    std::deque<std::array<char, MAX_IP_PACK_SIZE>> recent_msgs_;
};

class personInRoom: public participant,
                    public std::enable_shared_from_this<personInRoom>
{
public:
    personInRoom(boost::asio::io_service& io_service,
                 boost::asio::io_service::strand& strand, chatRoom& room)
                 : socket_(io_service), strand_(strand), room_(room)
    {
    }

    tcp::socket& socket() { return socket_; }

    void start()
    {
        boost::asio::async_read(socket_,
                                boost::asio::buffer(nickname_, nickname_.size()),
                                strand_.wrap(boost::bind(&personInRoom::nicknameHandler, shared_from_this(), _1)));
    }

    void onMessage(std::array<char, MAX_IP_PACK_SIZE>& msg)
    {
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress)
        {
            boost::asio::async_write(socket_,
                                     boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
                                     strand_.wrap(boost::bind(&personInRoom::writeHandler, shared_from_this(), _1)));
        }
    }

private:
    void nicknameHandler(const boost::system::error_code& error)
    {
        if (strlen(nickname_.data()) <= MAX_NICKNAME - 2)
        {
            strcat(nickname_.data(), ": ");
        }
        else
        {
            //cut off nickname if too long
            nickname_[MAX_NICKNAME - 2] = ':';
            nickname_[MAX_NICKNAME - 1] = ' ';
        }

        room_.enter(shared_from_this(), std::string(nickname_.data()));

        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_msg_, read_msg_.size()),
                                strand_.wrap(boost::bind(&personInRoom::readHandler, shared_from_this(), _1)));
    }

    void readHandler(const boost::system::error_code& error)
    {
        if (!error)
        {
            room_.broadcast(read_msg_, shared_from_this());

            boost::asio::async_read(socket_,
                                    boost::asio::buffer(read_msg_, read_msg_.size()),
                                    strand_.wrap(boost::bind(&personInRoom::readHandler, shared_from_this(), _1)));
        }
        else
        {
            room_.leave(shared_from_this());
        }
    }

    void writeHandler(const boost::system::error_code& error)
    {
        if (!error)
        {
            write_msgs_.pop_front();

            if (!write_msgs_.empty())
            {
                boost::asio::async_write(socket_,
                                         boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
                                         strand_.wrap(boost::bind(&personInRoom::writeHandler, shared_from_this(), _1)));
            }
        }
        else
        {
            room_.leave(shared_from_this());
        }
    }

    tcp::socket socket_;
    boost::asio::io_service::strand& strand_;
    chatRoom& room_;
    std::array<char, MAX_NICKNAME> nickname_;
    std::array<char, MAX_IP_PACK_SIZE> read_msg_;
    std::deque<std::array<char, MAX_IP_PACK_SIZE> > write_msgs_;
};

class server
{
public:
    server(boost::asio::io_service& io_service,
           boost::asio::io_service::strand& strand,
           const tcp::endpoint& endpoint)
           : io_service_(io_service), strand_(strand), acceptor_(io_service, endpoint)
    {
        run();
    }

private:

    void run()
    {
        std::shared_ptr<personInRoom> new_participant(new personInRoom(io_service_, strand_, room_));
        acceptor_.async_accept(new_participant->socket(), strand_.wrap(boost::bind(&server::onAccept, this, new_participant, _1)));
    }

    void onAccept(std::shared_ptr<personInRoom> new_participant, const boost::system::error_code& error)
    {
        if (!error)
        {
            new_participant->start();
        }

        run();
    }

    boost::asio::io_service& io_service_;
    boost::asio::io_service::strand& strand_;
    tcp::acceptor acceptor_;
    chatRoom room_;
};

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Usage: chat_server <port> [<port> ...]\n";
            return 1;
        }

        std::shared_ptr<boost::asio::io_service> io_service(new boost::asio::io_service);
        boost::shared_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(*io_service));
        boost::shared_ptr<boost::asio::io_service::strand> strand(new boost::asio::io_service::strand(*io_service));

        std::cout << "[" << std::this_thread::get_id() << "]" << "server starts" << std::endl;

        std::list < std::shared_ptr < server >> servers;
        for (int i = 1; i < argc; ++i)
        {
            tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[i]));
            std::shared_ptr<server> a_server(new server(*io_service, *strand, endpoint));
            servers.push_back(a_server);
        }

        boost::thread_group workers;
        for (int i = 0; i < 1; ++i)
        {
            boost::thread * t = new boost::thread{ boost::bind(&workerThread::run, io_service) };

#ifdef __linux__
            // bind cpu affinity for worker thread in linux
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(i, &cpuset);
            pthread_setaffinity_np(t->native_handle(), sizeof(cpu_set_t), &cpuset);
#endif
            workers.add_thread(t);
        }

        workers.join_all();
    } catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
