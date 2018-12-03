#ifndef _HTTP_HANDLE_HH
#define _HTTP_HANDLE_HH

#include <netdb.h>
#include <arpa/inet.h>
#include <algorithm>
#include <vector>
#include <string>
#include <assert.h>
#include <string.h>
#include <map>
#include "Logger.hh"

const static size_t kHttpReqBufferSize = 4096;
const static size_t kHttpResBufferSize = 1024;

class HttpUrl
{
public:
  HttpUrl(const std::string& url)
  :m_httpUrl(url),
   m_smatch(detachHttpUrl())
  {
    LOG_DEBUG << "URL : " << m_httpUrl;
  }
  ~HttpUrl(){};

  enum HttpUrlMatch
  {
    URL = 0,
    HOST = 1,
    URI = 2
  };

  std::vector<std::string> detachHttpUrl() const
  {
    std::vector<std::string> v;
    std::string::size_type pos1, pos2;
    pos2 = m_httpUrl.find('/');
    assert(std::string::npos != pos2);
    pos1 = pos2 + 2;
    pos2 = m_httpUrl.find('/', pos1);
    assert(std::string::npos != pos2);
    v.push_back(m_httpUrl);
    v.push_back(m_httpUrl.substr(pos1, pos2 - pos1));
    v.push_back(m_httpUrl.substr(pos2 + 1));
    LOG_DEBUG << "detachHttpUrl() url :" << v[0];
    LOG_DEBUG << "detachHttpUrl() host :" << v[1];
    LOG_DEBUG << "detachHttpUrl() uri :" << v[2];
    return v;
  }

  std::string toIp()
  {
    struct hostent* phost = NULL;
    char ip[32] = {0};

    phost = gethostbyname(domain().c_str());
    if (NULL == phost)
    {
      LOG_ERROR << "HttpUrlToIp(): gethostbyname error : " << errno << " : "<< strerror(errno);
      return "";
      //LOG_SYSERR << "urlToIp(): gethostbyname error";
    }

    inet_ntop(phost->h_addrtype,  phost->h_addr, ip, sizeof ip);

    m_ip = ip;

    return ip;
  }

  std::string domain() const { return getHttpUrlSubSeg(HOST); }

  std::string ip() const { return m_ip; }

  std::string uri() const { return getHttpUrlSubSeg(URI); }

private:
  std::string getHttpUrlSubSeg(HttpUrlMatch sub = HOST) const{ return m_smatch[sub]; }

  std::string m_httpUrl;
  std::string m_ip;
  std::vector<std::string> m_smatch;
};

template<int SIZE>
class HttpBuffer
{
public:
  HttpBuffer(): m_cur(m_data){
  }

  ~HttpBuffer(){
    //printf("%s", m_data);
  }

  void append(const char* /*restrict*/ buf, size_t len){
  // append partially
    if (/*implicit_cast<size_t>*/(avail()) > len)
    {
      memcpy(m_cur, buf, len);
      m_cur += len;
      //printf("---------%d---------\n", m_cur - m_data);
    }
  }

  // write in m_data directly
  char* current() {  return m_cur; };
  size_t avail() const { return static_cast<size_t> (end() - m_cur); }
  void add(size_t len) { m_cur += len; }
  size_t length() const {return m_cur - m_data;}
  void bzero() { ::bzero(m_data, sizeof(m_data)); }
  void reset() {m_cur = m_data;}

  const char* data() const { return m_data; }

  HttpBuffer& operator<<(char v) { append(&v, 1); return *this ;}
  HttpBuffer& operator<<(const char *s) { append(s, strlen(s)); return *this; }
  HttpBuffer& operator<<(const std::string& s) { append(s.c_str(), s.size()); return *this; }

private:
  const HttpBuffer& operator=(const HttpBuffer&);
  HttpBuffer(const HttpBuffer&);

  const char* end() const { return m_data + sizeof(m_data); }

  char m_data[SIZE];
  char* m_cur;
};

class HttpRequest
{
public:
  enum MethodE{GET, POST};

  HttpRequest() {}
  ~HttpRequest() {}

  void setRequestMethod(const std::string &method, const HttpUrl& url);
  void setRequestProperty(const std::string &key, const std::string &value);
  void setRequestBody(const std::string &content);

  const HttpBuffer<kHttpReqBufferSize>* buffer() { return &m_buffer; }

private:
  static std::map<std::string, int> kRequestMethods;

  HttpBuffer<kHttpReqBufferSize> m_buffer;
};


class HttpResponse
{
public:
  HttpResponse();
  ~HttpResponse();

  void handleHead(const char* buffer, size_t size);

  size_t getHeadSize() { assert(b_haveHandleHead); return m_headSize; }

  int getResponseCode() const {
    assert(b_haveHandleHead);
    return m_code;
  }

  std::string getResponseProperty(const std::string& key) const {
    assert(b_haveHandleHead);
    return m_responseMaps.at(key);
  }

  std::string getResponseContent() {
    assert(b_haveHandleHead);
    return std::string(m_buffer.data(), m_buffer.length());
  }

private:
  void SplitString(const std::string& s, std::vector<std::string>& v, const std::string& c);
  static const char kCRLF[];

  int m_code;
  size_t m_headSize;
  bool b_haveHandleHead;
  HttpBuffer<kHttpResBufferSize> m_buffer;
  std::map<std::string, std::string> m_responseMaps;
};


#endif