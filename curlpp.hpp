#pragma once

#include <curl/curl.h>
#include <functional>

namespace curlpp
{
  template <typename T>
  class ObjPool
  {
  public:
    auto acquire()
    {
      if (freeObjs.empty())
        return new T;
      auto ret = freeObjs.back();
      freeObjs.pop_back();
      return ret;
    }
    auto release(T *obj) { freeObjs.push_back(obj); }
    ~ObjPool()
    {
      for (auto obj : freeObjs)
        delete obj;
    }

  private:
    std::vector<T *> freeObjs;
  };

  class Easy
  {
  };

  class Socket
  {
  };

  class Multi
  {
  public:
    Multi() : handle(curl_multi_init()) {}
    Multi(const Multi &) = delete;
    ~Multi() { curl_multi_cleanup(handle); }

    using SocketCb = std::function<auto(Easy &, Socket &, int action)->void>;

    auto setSocketCb(SocketCb cb)
    {
      socketCb = std::move(cb);
      curl_multi_setopt(handle, CURLMOPT_SOCKETFUNCTION, [](CURL *easy, curl_socket_t socket, int action, void *userp, void *socketp) {
        auto multi = static_cast<Multi *>(userp);
        Easy e;
        Socket s;
        multi->socketCb(e, s, action);
      });
      curl_multi_setopt(handle, CURLMOPT_SOCKETDATA, this);
    }

  private:
    CURLM *handle;
    SocketCb socketCb;
  };
} // namespace curlpp
