#include "HttpHandle.hh"

const char HttpResponse::kCRLF[] = "\r\n";

const std::map<std::string, int>::value_type init_value[] =
{
  std::map<std::string, int>::value_type( "GET", HttpRequest::GET),

  std::map<std::string, int>::value_type( "POST", HttpRequest::POST)
};

std::map<std::string, int> HttpRequest::kRequestMethods(init_value, init_value + (sizeof init_value / sizeof init_value[0]));

void HttpRequest::setRequestMethod(const std::string &method, const HttpUrl& url)
{
  switch(HttpRequest::kRequestMethods.at(method))
  {
    case HttpRequest::GET :
      m_buffer << "GET " << "/" << url.uri() << " HTTP/1.1\r\n";
      break;
    case HttpRequest::POST :
      m_buffer << "POST "  << "/" << url.uri() << " HTTP/1.1\r\n";
      break;
    default :
      LOG_ERROR << "No such Method : " << method.c_str();
      break;
  }

  m_buffer << "Host: " << url.domain() << "\r\n";
}


void HttpRequest::setRequestProperty(const std::string &key, const std::string &value)
{
  m_buffer << key << ": " << value << "\r\n";
}

void HttpRequest::setRequestBody(const std::string &content)
{
  m_buffer << content;
}


HttpResponse::HttpResponse()
  :b_haveHandleHead(false)
{

}

HttpResponse::~HttpResponse()
{

}

void HttpResponse::handleHead(const char* buffer, size_t size)
{
  LOG_TRACE << "HttpResponse::handleHead() " << size << " Bytes :\n" << buffer;
  size_t headSize = 0;
  size_t endPos = size;

  while(!b_haveHandleHead)
  {
    assert(headSize <= size);
    const char* crlf = std::search(buffer, buffer + endPos, kCRLF, kCRLF+2);

    if(crlf == buffer + endPos)
    {
      LOG_FATAL << "HttpResponse::handleHead() error. can't find head end.";
    }

    size_t lineLength = crlf - buffer;
    headSize += lineLength + 2;
    std::string line(buffer, lineLength);
    buffer = buffer + lineLength + 2;
    endPos = endPos - lineLength - 2;

    if(line.size() == 0)
    {
      b_haveHandleHead = true;
      break;
    }

    std::vector<std::string> v;
    SplitString(line, v, ":");
    if(v.size() == 3) { m_code = std::stoi(v[1]); }
    if(v.size() == 2){ m_responseMaps[v[0]] = v[1].erase(0,v[1].find_first_not_of(" ")); }

    LOG_TRACE << "HttpResponse::handleHead() handle one line " << line.size()
              << " Bytes " << line << " headSize " << headSize << " bufferSize " << size;
  }

  m_headSize = headSize;

  LOG_TRACE << "HttpRequest::handleHead() Content-Type : " << m_responseMaps["Content-Type"];

}

void HttpResponse::SplitString(const std::string& s, std::vector<std::string>& v, const std::string& c)
{
  std::string::size_type pos1, pos2;
  pos2 = s.find(c);
  pos1 = 0;
  while(std::string::npos != pos2)
  {
    v.push_back(s.substr(pos1, pos2-pos1));

    pos1 = pos2 + c.size();
    pos2 = s.find(c, pos1);
  }
  if(pos1 != s.length())
    v.push_back(s.substr(pos1));
}