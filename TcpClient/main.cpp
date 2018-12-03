#include <errno.h>
#include <thread>
#include <strings.h>
#include <poll.h>
#include <fstream>
#include <functional>
#include "EventLoop.hh"
#include "Channel.hh"
#include "Poller.hh"
#include "Logger.hh"
#include "Connector.hh"
#include "SocketHelp.hh"
#include "InetAddress.hh"
#include "TcpClient.hh"
#include "TcpConnection.hh"
#include "Buffer.hh"
#include "EventLoopThread.hh"
#include "Connector.hh"
#include "ThreadPool.hh"
#include "HttpHandle.hh"

EventLoop* g_loop;
ThreadPool* g_threadPool;
std::mutex g_urlMutex;
Condition g_urlCond;

std::mutex g_imageMutex;
Condition g_imageCond;
static bool g_exitFlag = 0;
static off_t g_downSize = 0;

HttpUrl image("http://img.zcool.cn/community/01ddc256eb71586ac7257d209712b7.jpg@1280w_1l_2o_100sh.jpg");
//HttpUrl appUrl("https://usglmycar.x431.com/services/publicSoftService");
//HttpUrl mcu("https://cnpublicdlcenter.x431.com/public/goloSoftUpgrade.action?serialNo=978283000001&clientVersionNo=1.00");

void downloadFile(const TcpConnectionPtr& conn, Buffer* buffer, ssize_t len)
{
  LOG_TRACE << "downloadFile()";

  std::ofstream output("./test.jpeg", std::ofstream::binary | std::ofstream::app);
  if (!output.is_open()){ // 检查文件是否成功打开
    LOG_SYSFATAL << "open file error";
  }

  output.write(buffer->peek(), buffer->readableBytes());
  g_downSize += buffer->readableBytes();
  LOG_TRACE << "HttpRequest::downloadFile() written " << buffer->readableBytes() << " Bytes. hasWritten " << g_downSize << " Bytes." ;
  buffer->retrieve(buffer->readableBytes());

  output.close();

  std::unique_lock<std::mutex> lock(g_imageMutex);

  if(g_downSize == 155984)
  {
    g_exitFlag = true;
    g_imageCond.notify();
  }
}

void domainToIpTask(HttpUrl* url)
{
  std::string ip;
  int cnt = 0;

  std::unique_lock<std::mutex> lock(g_urlMutex);
  while(ip.empty() && cnt < 10){
    ip = url->toIp();
    cnt++;
    LOG_TRACE << "domainToIpTask() get ip is " << url->ip();
  }

  g_urlCond.notify();

}

void onConnetion(const TcpConnectionPtr& conn)
{
  LOG_TRACE << "onConnetion";

  HttpRequest req;
  req.setRequestMethod("GET", image);
  req.setRequestProperty("Cache-Control", "no-cache");
  req.setRequestProperty("Content-Type", "application/octet-stream");
  req.setRequestProperty("Connection", "close\r\n");

  LOG_TRACE << "send buffer " << req.buffer()->length() << " Bytes : \n" << req.buffer()->data();

  conn->send(req.buffer()->data(), req.buffer()->length());
}


void onMessage(const TcpConnectionPtr& conn, Buffer* buf, ssize_t len)
{
  LOG_TRACE << "onMessage() : received " << buf->readableBytes() << " Bytes from connection [" << conn->name();

  HttpResponse res;
  res.handleHead(buf->peek(), buf->readableBytes());
  buf->retrieve(res.getHeadSize());
  conn->setMessageCallBack(std::bind(&downloadFile, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

int main()
{

  ThreadPool threadPool;
  g_threadPool = &threadPool;
  threadPool.start();

  threadPool.addTask(std::bind(domainToIpTask, &image));

  //std::this_thread::sleep_for(std::chrono::seconds(2));

  {
    std::unique_lock<std::mutex> lock(g_urlMutex);
    while(image.ip().empty())
    {
      g_urlCond.wait(lock);
    }
  }

  if(image.ip().empty())
  {
    LOG_DEBUG << "domainToIpTask() failed exit.";
    return 0;
  }

  LOG_DEBUG << image.domain() << " : " << image.ip();

  EventLoopThread loopThread;
  EventLoop* p_loop = loopThread.startLoop();

  InetAddress imageServerAddr(image.ip(), 80);
  {
    TcpClient imageClient(p_loop, imageServerAddr);
    imageClient.setConnectionCallBack(onConnetion);
    imageClient.setMessageCallBack(onMessage);
    imageClient.start();

    {
      std::unique_lock<std::mutex> lock(g_imageMutex);
      while(!g_exitFlag)
      {
        g_imageCond.wait(lock);
      }
    }

    std::this_thread::sleep_for(std::chrono::seconds(2)); //wait server close. need fixed.
  }

  LOG_TRACE << "downloadFile Success, Exit.";


  return 0;
}

