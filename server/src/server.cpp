//
// ChatServer.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// @source http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/example/chat/ChatServer.cpp


#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <set>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include "../include/message.h"


using boost::asio::ip::tcp;


//----------------------------------------------------------------------

typedef std::deque< ChatMessage >  chatMessageQueue_t;

//----------------------------------------------------------------------


class ChatParticipant
{
public:
  virtual ~ChatParticipant() {}
  virtual void deliver(const ChatMessage& msg) = 0;
};


typedef boost::shared_ptr< ChatParticipant >  chatParticipantPTR;


//----------------------------------------------------------------------


class ChatRoom
{
public:
  void join(chatParticipantPTR participant)
  {
    participants_.insert(participant);
    std::for_each(recent_msgs_.begin(), recent_msgs_.end(),
        boost::bind(&ChatParticipant::deliver, participant, _1));
  }

  void leave(chatParticipantPTR participant)
  {
    participants_.erase(participant);
  }

  void deliver(const ChatMessage& msg)
  {
    const auto s = msg.str();
    std::cout << "[" << s << "]";

    recent_msgs_.push_back(msg);
    while (recent_msgs_.size() > max_recent_msgs)
      recent_msgs_.pop_front();

    std::for_each(participants_.begin(), participants_.end(),
        boost::bind(&ChatParticipant::deliver, _1, boost::ref(msg)));
  }

private:
  std::set< chatParticipantPTR >  participants_;
  enum { max_recent_msgs = 100 };
  chatMessageQueue_t  recent_msgs_;
};

//----------------------------------------------------------------------

class ChatSession
  : public ChatParticipant,
    public boost::enable_shared_from_this<ChatSession>
{
public:
  ChatSession(boost::asio::io_service& io_service, ChatRoom& room)
    : socket_(io_service),
      room_(room)
  {
  }

  tcp::socket& socket()
  {
    return socket_;
  }

  void start()
  {
    room_.join(shared_from_this());
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_.data(), ChatMessage::header_length),
        boost::bind(
          &ChatSession::handle_read_header, shared_from_this(),
          boost::asio::placeholders::error));
  }

  void deliver(const ChatMessage& msg)
  {
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress)
    {
      boost::asio::async_write(socket_,
          boost::asio::buffer(write_msgs_.front().data(),
            write_msgs_.front().length()),
          boost::bind(&ChatSession::handle_write, shared_from_this(),
            boost::asio::placeholders::error));
    }
  }

  void handle_read_header(const boost::system::error_code& error)
  {
    if (!error && read_msg_.decode_header())
    {
      boost::asio::async_read(socket_,
          boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
          boost::bind(&ChatSession::handle_read_body, shared_from_this(),
            boost::asio::placeholders::error));
    }
    else
    {
      room_.leave(shared_from_this());
    }
  }

  void handle_read_body(const boost::system::error_code& error)
  {
    if (!error)
    {
      room_.deliver(read_msg_);
      boost::asio::async_read(socket_,
          boost::asio::buffer(read_msg_.data(), ChatMessage::header_length),
          boost::bind(&ChatSession::handle_read_header, shared_from_this(),
            boost::asio::placeholders::error));
    }
    else
    {
      room_.leave(shared_from_this());
    }
  }

  void handle_write(const boost::system::error_code& error)
  {
    if (!error)
    {
      write_msgs_.pop_front();
      if (!write_msgs_.empty())
      {
        boost::asio::async_write(socket_,
            boost::asio::buffer(write_msgs_.front().data(),
              write_msgs_.front().length()),
            boost::bind(&ChatSession::handle_write, shared_from_this(),
              boost::asio::placeholders::error));
      }
    }
    else
    {
      room_.leave(shared_from_this());
    }
  }

private:
  tcp::socket socket_;
  ChatRoom& room_;
  ChatMessage read_msg_;
  chatMessageQueue_t write_msgs_;
};

typedef boost::shared_ptr<ChatSession> chatSessionPTR;

//----------------------------------------------------------------------

class ChatServer
{
public:
  ChatServer(boost::asio::io_service& io_service,
      const tcp::endpoint& endpoint)
    : io_service_(io_service),
      acceptor_(io_service, endpoint)
  {
    start_accept();
  }

  void start_accept()
  {
    chatSessionPTR new_session(new ChatSession(io_service_, room_));
    acceptor_.async_accept(new_session->socket(),
        boost::bind(&ChatServer::handle_accept, this, new_session,
          boost::asio::placeholders::error));
  }

  void handle_accept(chatSessionPTR session,
      const boost::system::error_code& error)
  {
    if (!error)
    {
      session->start();
    }

    start_accept();
  }

private:
  boost::asio::io_service& io_service_;
  tcp::acceptor acceptor_;
  ChatRoom room_;
};

typedef boost::shared_ptr< ChatServer >  chatServerPTR;
typedef std::list< chatServerPTR >  chatServerList_t;

//----------------------------------------------------------------------





int main(int argc, char* argv[]) {

  try
  {
    if (argc < 2)
    {
      std::cerr << "Usage: server <port> [<port> ...]\n";
      return 1;
    }

    boost::asio::io_service  io_service;

    chatServerList_t  servers;
    for (int i = 1; i < argc; ++i) {
      using namespace std; // For atoi.
      tcp::endpoint endpoint(tcp::v4(), atoi(argv[i]));
      chatServerPTR server(new ChatServer(io_service, endpoint));
      servers.push_back(server);
    }

    io_service.run();

  } catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
