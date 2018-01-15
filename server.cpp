#include <iostream>
#include <fstream>
#include <stdio.h>
#include <tchar.h>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <clocale>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/ref.hpp>
using namespace boost::asio;
using namespace boost::posix_time;
typedef boost::asio::ip::tcp btcp;
namespace bfs = boost::filesystem;
typedef boost::format bfmt;
std::ifstream ifs;
io_service service;
static unsigned int index = { 0 }; // ������/id ������������� �������

#define MEM_FN(x)       boost::bind(&self_type::x, shared_from_this())
#define MEM_FN1(x,y)    boost::bind(&self_type::x, shared_from_this(),y)
#define MEM_FN2(x,y,z)  boost::bind(&self_type::x, shared_from_this(),y,z)


class talk_to_client : public boost::enable_shared_from_this<talk_to_client>, boost::noncopyable
{
	typedef talk_to_client self_type;
	talk_to_client() : sock_(service), started_(false) {}
public:
	typedef boost::system::error_code error_code;
	typedef boost::shared_ptr<talk_to_client> ptr;

	void start()
	{
		started_ = true;
		do_read();
	}
	static ptr new_()
	{
		ptr new_(new talk_to_client);
		return new_;
	}
	void stop()
	{
		if (!started_) return;
		started_ = false;
		sock_.close();
	}
	ip::tcp::socket & sock() { return sock_; }
private:
	void on_read(const error_code & err, size_t bytes)
	{
		if (!err)
		{
			index++;
			std::cout << std::endl << "������ � �������� id = " << index << std::endl;
			std::string msg(read_buffer_, bytes);
			if (msg.size() > 1023)
			{
				std::cout << "[-] ������� ������� ����! ���� ���������� ��������." << std::endl;
				stop();
				return;
			}
			do_write(msg + "\n");
		}
		stop();
	}

	void on_write(const error_code & err, size_t bytes)
	{
		do_read();
	}
	void do_read()
	{
		async_read(sock_, buffer(read_buffer_), MEM_FN2(read_complete, _1, _2), MEM_FN2(on_read, _1, _2));
	}
	void do_write(const std::string & msg)
	{
		system("chcp 65001");
		using std::cout;
		using std::endl;
		std::string temp; // ��������� ������ ��� ��������� ������ ������, ���������� ����
		for (unsigned int i = { 0 }; i < msg.size() - 2; ++i)
		{
			temp.push_back(msg[i]);
		}
		bfs::path filepath = temp; // ���� 
		ifs.open(temp, std::ios::binary);  // �������� �����, ������� �������� �������� ������
		if (!ifs.is_open())
		{
			cout << endl << bfmt("[-] ������ �������� �����\n") << endl;
			stop();
			system("pause");
			return;
		}
		
		uintmax_t fileSize = bfs::file_size(temp); // ��������� ������� �����, ���� � �������� ������ � ������ temp
		size_t sentFileName; // ������������ ��� �����
		size_t sentFileSize; // ������������ ������ �����
		uintmax_t sentFileBody; // ������ ����������� ������
		size_t fileBufSize; // ������ ������

		cout << endl << bfmt("������ �����: %1%") % fileSize << endl;
		sentFileName = boost::asio::write(sock_, boost::asio::buffer("��� �����: " + temp + "\r\n"));
		sentFileSize = boost::asio::write(sock_, boost::asio::buffer("������ �����: " + boost::lexical_cast<std::string>(fileSize) + "\r\n\r\n"));
		sentFileBody = 0;
		fileBufSize = sizeof(write_buffer_);
		 
		while (ifs)
		{
			ifs.read(write_buffer_, fileBufSize); // ������ �� ������ fileBufSize �������� � ������ �� � ������ write_buf
			sentFileBody += boost::asio::write(sock_, boost::asio::buffer(write_buffer_, ifs.gcount())); // ������� ����������� ������ � ����� � ������� ���������� ������
		}
		if (sentFileBody != fileSize)
		{
			std::cerr << "[-] ������ ����������!\n";
			stop();
			system("pause");
			return;
		}
		ifs.close();
		sock_.shutdown(btcp::socket::shutdown_both);
		sock_.close();
		try
		{
			MEM_FN2(on_write, _1, _2);
		}
		catch (std::exception const& e)
		{
			cout << endl << bfmt(e.what()) << endl;
			system("pause");
			return;
		}
	}
	size_t read_complete(const boost::system::error_code & err, size_t bytes)
	{
		if (err) return 0;
		bool found = std::find(read_buffer_, read_buffer_ + bytes, '\n') < read_buffer_ + bytes;
		// ���������������� ������������������� ������ �� \n
		return found ? 0 : 1;
	}
private:
	ip::tcp::socket sock_;//ip - ���������������� �������� �������� ������ ����� TCP/IP; tcp - �������� ���������� ���������
	enum { max_msg = 1024 };
	char read_buffer_[max_msg]; // �����, ���������� ���� � ����������� �����
	char write_buffer_[max_msg]; // ����� ��� ������ ������ ����� � �� ����������� �������� �������
	bool started_;
};

ip::tcp::acceptor acceptor(service, ip::tcp::endpoint(ip::tcp::v4(), 8001));
// ��������� TCP ������� � asio ��������� ������ ������ boost::asio::ip::tcp::acceptor
// ���������� ���� ������ ��������� ����������

void handle_accept(talk_to_client::ptr client, const boost::system::error_code & err)//������� ������������� �������
{
	client->start();
	talk_to_client::ptr new_client = talk_to_client::new_();
	acceptor.async_accept(new_client->sock(), boost::bind(handle_accept, new_client, _1));
}


int main()
{
	setlocale(LC_ALL, "Russian");
	std::cout << std::endl << "*** ������������ ������ �9: ���������� ������� ������� rcp ***" << std::endl;
	talk_to_client::ptr client = talk_to_client::new_();
	acceptor.async_accept(client->sock(), boost::bind(handle_accept, client, _1));
	// ����� accept ������� ������ acceptor � �������� ������ socket ���������� � �������� ���������, 
	// �������� �������� �����������, ��� ��� ���������� ����� ������ �������� � ������������ ������ �� ��� ���, 
	// ���� �����-������ ������ �� ���������� �����������
	service.run();

	return 0;
}