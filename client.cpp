#include <iostream>
#include <fstream>
#include <stdio.h>
#include <tchar.h>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <stdexcept>
typedef boost::asio::ip::tcp btcp;//���������� ��� �������� ����� ��������� ����� ��� �����
typedef boost::format bfmt;// boost::format ��������� ��������� ������������� ������
using namespace boost::asio;
io_service service; //  ������ ������, ������� ������������� ��������� ����� � ��������� ��������� �����/������

#define MEM_FN(x)       boost::bind(&self_type::x, shared_from_this())  //  ��� ������� ����������� � ���������� �������
#define MEM_FN1(x,y)    boost::bind(&self_type::x, shared_from_this(),y) // ���������� ������ �������, ����� ������� �������� �������� ��������� �� ������� ����� 
#define MEM_FN2(x,y,z)  boost::bind(&self_type::x, shared_from_this(),y,z)

class talk_to_svr : public boost::enable_shared_from_this<talk_to_svr> // ����� "����� � ��������";enable_shared_from_this ��������� ������� ����������� ����� ����������,��������� ������� ��� ���������� � ����� shared_ptr
	, boost::noncopyable
{
	typedef talk_to_svr self_type;
	talk_to_svr(const std::string & filepath)
		: sock_(service), started_(true), message_(filepath), istream_(&streambuf_) {}
	void start(ip::tcp::endpoint ep)
	{
		sock_.async_connect(ep, MEM_FN1(on_connect, _1)); // ��� ������� ���������� ������������ �� ������� ������
	}
public:
	typedef boost::system::error_code error_code;
	typedef boost::shared_ptr<talk_to_svr> ptr;

	static ptr start(ip::tcp::endpoint ep, const std::string & filepath)
	{
		ptr new_(new talk_to_svr(filepath));
		new_->start(ep);
		return new_;
	}
	void stop()
	{
		if (!started_) return;
		started_ = false; //������� ������������� ������ �������
		sock_.close();
	}
	bool started() { return started_; }
private:
	void on_connect(const error_code & err)
	{
		if (!err)
			do_write(message_ + "\n");//������� ������ ����������
		else
			stop();
	}
	void on_write(const error_code & err, size_t bytes)
	{
		do_read();//������� ������
	}
	void do_read()
	{
		try
		{
			async_read_until(sock_, streambuf_, "\r\n\r\n", MEM_FN2(handlereaduntil, _1, _2)); // ������� ������������ ������ �� ������ �� \r\n\r\n
		}
		catch (std::exception const& e)
		{
			std::cout << std::endl << bfmt(e.what()) << std::endl; // ����� ������ � ������� boost
			system("pause");
			return;
		}
	}
	void do_write(const std::string & msg)
	{
		if (!started()) return;
		try
		{
			std::copy(msg.begin(), msg.end(), write_buffer_);
			sock_.async_write_some(buffer(write_buffer_, msg.size()), MEM_FN2(on_write, _1, _2)); // ��� ������� ��������� �������� ����������� �������� ������ �� ������
		}
		catch (std::exception const& e)
		{
			std::cout << std::endl << bfmt(e.what()) << std::endl;
			system("pause");
			return;
		}
	}

private:
	ip::tcp::socket sock_; // ����� - ����� �����  �������� � ��������
	enum { max_msg = 10240 };

	char read_buffer_[max_msg]; // ����� ��� ������ �������� �� �����, ������� �������� ������
	boost::asio::streambuf streambuf_; // ����� ������ ��� ������, ������������ ����� ��������� ��� ���������������� ������ � ������ ������
	std::istream istream_; // ������ ������ istream, ������� ��������� ��������� ����
	uintmax_t filesize_; // ������ ����������� �� ������� �����
	uintmax_t dumped; // ���������� ������ ����������� ������
	std::string message_; // ���������� ��������� �������, ���������� ���� � ����������� ����� 
	std::ofstream ofs;  // ������ ������ ostream, ������� ��������� ��������� �����
	static const boost::regex regFileName, regFileSize; // ���������� ��������� ��� ����� � ������� �����
	boost::smatch res; // ������ ��������� ������ �� ���������� ����������

	char write_buffer_[max_msg]; // ����� ������
	bool started_; // ���������� ��� ����������� ������ ������ � ��������

	void handlereadend(boost::system::error_code const& errcode, size_t bytes_transferred)
	{
		ofs.close();//�������� �� ���������� ���������� �����
		stop();
	}


	void handlereaduntil(boost::system::error_code const& errcode, size_t bytes_transferred)
	{
		using std::cout;
		using std::endl;
		try
		{
			if (errcode)
			{
				cout << endl << "[-] ������ ������������ ������!" << endl;
				stop();
				return;
			}
			std::string headers;
			std::string tmp;
			while (std::getline(istream_, tmp) && tmp != "\r") // ���� ������ � tmp �� ������ istream �������� ������� � ��� ���� tmp != \r
			{
				headers += (tmp + '\n');
			}
			if (!boost::regex_search(headers, res, regFileSize)) // ����� ����� ����������� �� ������ ������� ����� � ������ ���������� � res; regex_search - ����� ��������� � �������� ������, �� ��������� ����������� ���������
				throw std::runtime_error("[-] ������ ����������� ������� �����.\n");
			filesize_ = boost::lexical_cast<uintmax_t>(res[1]);// lexical_cast - �������������� � ��� uintmax_t
			cout << endl << bfmt("������ �����: %1%") % filesize_ << endl;
			if (!boost::regex_search(headers, res, regFileName))
				throw std::runtime_error("[-] ������ ����������� ����� �����.\n");
			message_ = res[1];
			cout << endl << bfmt("��� �����: %1%") % message_ << endl;
			ofs.open(message_.c_str(), std::ios::binary);
			if (!ofs.is_open())
				throw std::runtime_error((bfmt("[-] ������ �������� ����� %1%") % message_).str());
			dumped = 0;
			if (streambuf_.size())
			{
				dumped += streambuf_.size();
				ofs << &streambuf_; // ������ ������ � ����
			}
			while (dumped != filesize_)
			{
				dumped += boost::asio::read(sock_, streambuf_, boost::asio::transfer_at_least(1));
				ofs << &streambuf_;
			}
			cout << endl << "���������� ������ �����: " << dumped << endl;
		}
		catch (std::exception const& e)
		{
			cout << endl << bfmt(e.what()) << endl;
			stop();
			delete this;
		}
	}
};

boost::regex const talk_to_svr::regFileName("��� �����: *(.+?)\r\n"); // boost::regex  ��������� �������� ������ ������
boost::regex const talk_to_svr::regFileSize("������ �����: *(\\d+?)\r\n");

int main()
{
	// ����������� ��������
	setlocale(LC_ALL, "Russian");
	std::cout << std::endl << "*** ������������ ������ �9: ���������� �������, ������������� ������� rcp ***" << std::endl;
	std::string filepath;
	ip::tcp::endpoint ep(ip::address::from_string("127.0.0.1"), 8001); // �������� �����, ��������� - ����� � ����
	std::cout << std::endl << "������� ���� � ����� ��� �����������: ";
	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);
	getline(std::cin, filepath);
	SetConsoleCP(65001);
	SetConsoleOutputCP(65001);
	int pos = filepath.find(".");
	if (filepath.size() > 10230)
	{
		std::cout << std::endl << "[-] ������� ������� ������� ���������! ���� ���������� �������.";
		system("pause");
		return 1;
	}
	else if (pos == -1 || filepath[pos + 1] == ' ')
	{
		std::cout << std::endl << "[-] �������� ����� ������ ��������� ��� ����������." << std::endl;
		system("pause");
		return 1;
	}
	talk_to_svr::start(ep, filepath);
	boost::this_thread::sleep(boost::posix_time::millisec(100));//posix_time ��������� �� ����������� ����� � ����; sleep - �������� �����
	service.run(); // �������� ������� (�� ���� ��� ������, ������������ �������� ������/������ � ����� ���� �� ���� ����� �������� �������� ������������ ��)
	std::cout << std::endl;
	system("pause");
	return 0;
}