#include "uvpp.hpp"
#include <cstring>
#include <log/log.hpp>

int main()
{
  uvpp::Loop loop;

  uvpp::Tcp tcp(loop);
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(80);
  inet_aton("185.199.110.153", &addr.sin_addr);

  tcp.connect(reinterpret_cast<const struct sockaddr *>(&addr), [&tcp](int status) {
    LOG("connect", status);
    auto req = "GET / HTTP/1.1\n"
               "HOST:mika.global\n\n";
    uv_buf_t buf;
    buf.base = const_cast<char *>(req);
    buf.len = strlen(req);
    tcp.write({buf}, [](int status) { LOG("write", status); });
  });
  tcp.readStart([](ssize_t nread, const uv_buf_t *buf) {
    LOG("read", nread, buf->len, EOF);
    if (nread <= 0)
      return;
    std::cout << std::string(buf->base, buf->base + nread);
  });
  loop.run(UV_RUN_DEFAULT);
}
