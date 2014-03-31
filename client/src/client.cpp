//
// ChatClient.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// @source http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/example/chat/ChatClient.cpp


#define _CRT_SECURE_NO_WARNINGS

#include <cstdlib>
#include <deque>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include "../../server/include/message.h"


using boost::asio::ip::tcp;


typedef std::deque< ChatMessage >  chatMessageQueue_t;


class ChatClient {
public:
  ChatClient(boost::asio::io_service& io_service,
      tcp::resolver::iterator endpoint_iterator)
    : io_service_(io_service),
      socket_(io_service)
  {
    boost::asio::async_connect(socket_, endpoint_iterator,
        boost::bind(&ChatClient::handle_connect, this,
          boost::asio::placeholders::error));
  }

  void write(const ChatMessage& msg)
  {
    io_service_.post(boost::bind(&ChatClient::do_write, this, msg));
  }

  void close()
  {
    io_service_.post(boost::bind(&ChatClient::do_close, this));
  }

private:

  void handle_connect(const boost::system::error_code& error)
  {
    if (!error)
    {
      boost::asio::async_read(socket_,
          boost::asio::buffer(read_msg_.data(), ChatMessage::header_length),
          boost::bind(&ChatClient::handle_read_header, this,
            boost::asio::placeholders::error));
    }
  }

  void handle_read_header(const boost::system::error_code& error)
  {
    if (!error && read_msg_.decode_header())
    {
      boost::asio::async_read(socket_,
          boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
          boost::bind(&ChatClient::handle_read_body, this,
            boost::asio::placeholders::error));
    }
    else
    {
      do_close();
    }
  }

  void handle_read_body(const boost::system::error_code& error)
  {
    if (!error)
    {
      std::cout.write(read_msg_.body(), read_msg_.body_length());
      std::cout << "\n";
      boost::asio::async_read(socket_,
          boost::asio::buffer(read_msg_.data(), ChatMessage::header_length),
          boost::bind(&ChatClient::handle_read_header, this,
            boost::asio::placeholders::error));
    }
    else
    {
      do_close();
    }
  }

  void do_write(ChatMessage msg)
  {
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress)
    {
      boost::asio::async_write(socket_,
          boost::asio::buffer(write_msgs_.front().data(),
            write_msgs_.front().length()),
          boost::bind(&ChatClient::handle_write, this,
            boost::asio::placeholders::error));
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
            boost::bind(&ChatClient::handle_write, this,
              boost::asio::placeholders::error));
      }
    }
    else
    {
      do_close();
    }
  }

  void do_close()
  {
    socket_.close();
  }

private:
  boost::asio::io_service& io_service_;
  tcp::socket socket_;
  ChatMessage read_msg_;
  chatMessageQueue_t write_msgs_;
};




int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: ChatClient <host> <port>\n";
      return 1;
    }

    boost::asio::io_service io_service;

    tcp::resolver resolver(io_service);
    tcp::resolver::query query(argv[1], argv[2]);
    tcp::resolver::iterator iterator = resolver.resolve(query);

    ChatClient c(io_service, iterator);

    boost::thread t(boost::bind(&boost::asio::io_service::run, &io_service));

    char line[ChatMessage::max_body_length + 1];
    while (std::cin.getline(line, ChatMessage::max_body_length + 1))
    {
      using namespace std; // For strlen and memcpy.
      ChatMessage msg;
      msg.body_length(strlen(line));
      memcpy(msg.body(), line, msg.body_length());
      msg.encode_header();
      c.write(msg);
    }

    c.close();
    t.join();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }


    std::cout << std::endl << "^" << std::endl;
    std::cin.ignore();

  return 0;
}
