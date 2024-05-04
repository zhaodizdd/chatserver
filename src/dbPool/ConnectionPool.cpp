#include "ConnectionPool.h"
#include "public.h"

#include <fstream>
#include <mysql/mysql_com.h>

// 线程安全的懒汉单例函数接口
ConnectionPool* ConnectionPool::getConnectionPool()
{
	static ConnectionPool pool; //lock 和 unlock 
	return &pool;
}

//从配置文件中加载配置项
bool ConnectionPool::loadConfigFile()
{
	ifstream fin("../etc/mysql.cnf");
	if(!fin.is_open())
	{
		LOG("没有mysql.cnf这个文件!");
		fin.close();
		return false;
	}
	char buf[100];
	while (!fin.eof())
	{
		fin.getline(buf,100);
		std::string str = buf;
		int idx = str.find('=', 0);
		if (idx == -1) // 无效的配置项
		{
			continue;
		}

		std::string key = str.substr(0,idx);
		std::string value = str.substr(idx + 1);

		if (key == "ip")
			_ip = value;
		else if (key == "port")
			_port = std::stoi(value);
		else if (key == "username")
			_username = value;
		else if (key == "password")
			_password = value;
		else if (key == "dbname")
			_dbname = value;
		else if (key == "initSize")
			_initSize = std::stoi(value);
		else if (key == "maxSize")
			_maxSize = std::stoi(value);
		else if (key == "maxIdleTime")
			_maxIdleTime = std::stoi(value);
		else if (key == "connectionTimeOut")
			_connectionTimeOut = std::stoi(value);
		else
		{
			LOG("mysql.cof文件出错！");
			return false;
		}
	}
	return true;
}

// 连接池的构造
ConnectionPool::ConnectionPool()
{
	//加载配置项
	if (!loadConfigFile())
	{
		LOG("失败");
		return;
	}

	// 创建初始数量的连接
	for (int i = 0; i < _initSize; ++i)
	{
		Connection *p = new Connection();
		p->connect(_ip, _port, _username, _password, _dbname);
		_connectionQue.push(p);
		_connectionCnt++;
	}
	
	// 启动一个新的线程，作为连接的生产者
	thread produce(&ConnectionPool::produceConnectionTask, this);
	produce.detach();

	// 启动一个新的线程，扫描多余的超过maxIdleTime空闲连接, 进行对应的连接回收 
	thread scanner(&ConnectionPool::scannerConnectionTask, this);
	scanner.detach();
}
ConnectionPool::~ConnectionPool()
{
	unique_lock<mutex> lock(_queueMutex);
	//当退出是把连接池中的空闲连接释放
	while (!_connectionQue.empty())
	{
		Connection* p = _connectionQue.front();
		_connectionQue.pop();
		delete p;
	}
	//必须通知一下赋值生产连接阻塞，如果没有很可能阻塞在
	//cout << "等待\n";
	//cv.wait(lock); // 队列不空，此处生产线程等待状态
	//cout << "结束等待\n";
	cv.notify_all();
}

// 运行在独立的线程中，专门负责生产新连接
void ConnectionPool::produceConnectionTask()
{
	for (;;)
	{
		unique_lock<std::mutex> lock(_queueMutex);
		while (!_connectionQue.empty())
		{
			//cout << "等待\n";
			cv.wait(lock); // 队列不空，此处生产线程等待状态
			//cout << "结束等待\n";
		}

		// 连接数量没有到达上限，继续创建新的连接
		if (_connectionCnt < _maxSize)
		{
			Connection *p = new Connection();
			p->connect(_ip, _port, _username, _password, _dbname);
			p->refreshAliveTime(); //刷新一下连接的起始的空闲时间点
			_connectionQue.push(p);
			_connectionCnt++;
		}

		// 通知消费线程，可以消费连接了
		cv.notify_all();
	}
}

// 给外部提供接口，从连接池中获取一个可用的空闲连接
shared_ptr<Connection> ConnectionPool::getConnection()
{
	unique_lock<mutex> lock(_queueMutex);
	while (_connectionQue.empty())
	{
		//设置消费者的超时等待时间
		if (cv_status::timeout == cv.wait_for(lock, chrono::milliseconds(_connectionTimeOut)))
		{
			// 如果是超时醒来，判断队列为空
			if (_connectionQue.empty())
			{
				LOG("获取空闲连接超时... 获取失败！");
				return nullptr;
			}
		}
	}

	/*
	 shared_ptr智能指针析构时， 会把connection资源直接delete掉，相当于
	 调用connection的析构函数，connection就被close掉l
	 这里需要自定义shared_ptr的释放资源的方式，把connection直接归还到queue当中
	 */
	shared_ptr<Connection> sp(_connectionQue.front(),
							  [&] (Connection *pcon) {
							  // 这里是在服务器应用线程中调用的， 所以一定考虑队列的线程安全操作
							  unique_lock<mutex> lock(_queueMutex);
							  pcon->refreshAliveTime(); //刷新一下连接的起始的空闲时间点
							  _connectionQue.push(pcon);
							  });
	_connectionQue.pop();
	cv.notify_all(); //消费完连接以后，通知生产者线程检测队列，如果队列为空，赶紧生产
	return sp;
}

//扫描超过maxIdleTime时间的空闲连接，进行对应的连接回收
void ConnectionPool::scannerConnectionTask()
{
	for (;;)
	{
		// 通过sleep模拟定时效果
		this_thread::sleep_for(chrono::seconds(_maxIdleTime));

		// 扫描整个队列，释放多余的连接
		unique_lock<mutex> lock(_queueMutex);
		while (_connectionCnt > _initSize)
		{
			Connection *p = _connectionQue.front();
			if (p->getAliveTime() >= (_maxIdleTime * 1000))
			{
				_connectionQue.pop();
				_connectionCnt--;
				delete p; // 调用~Connection()释放连接
			}
			else
			{
				break; // 队头的连接没有超过_maxIdleTime
			}
		}
	}
}
