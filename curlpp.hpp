#pragma once

#include "uvpp.hpp"
#include <curl/curl.h>
#include <functional>

namespace curlpp
{
  class Easy
  {
  private:
    CURL *handle;

  public:
    Easy() : handle(curl_easy_init()) { curl_easy_setopt(handle, CURLOPT_PRIVATE, this); }
    ~Easy() { curl_easy_cleanup(handle); }
    auto get() -> CURL * { return handle; }
    auto get() const -> const CURL * { return handle; }
    auto setUrl(const std::string &val) -> void { curl_easy_setopt(handle, CURLOPT_URL, val.c_str()); }
    static auto get(CURL *handle) -> Easy &
    {
      Easy *self;
      curl_easy_getinfo(handle, CURLINFO_PRIVATE, &self);
      return *self;
    }

    std::unique_ptr<uvpp::Poll> poll;
    std::function<void()> doneCb;
  };

  class Multi
  {
  public:
    Multi(uvpp::Loop &loop) : handle(curl_multi_init()), timeout(loop)
    {
      setSocketCb([&loop, this](curlpp::Easy &easy, curl_socket_t socket, int action) {
        switch (action)
        {
        case CURL_POLL_IN:
        case CURL_POLL_OUT:
        case CURL_POLL_INOUT: {
          int events = 0;
          if (action != CURL_POLL_IN)
            events |= UV_WRITABLE;
          if (action != CURL_POLL_OUT)
            events |= UV_READABLE;
          if (!easy.poll)
            easy.poll = std::make_unique<uvpp::Poll>(loop, socket, uvpp::Poll::Sock{});
          easy.poll->start(events, [this, socket](int /*status*/, int events) {
            int runningHandles;
            int flags = 0;
            if (events & UV_READABLE)
              flags |= CURL_CSELECT_IN;
            if (events & UV_WRITABLE)
              flags |= CURL_CSELECT_OUT;
            socketAction(socket, flags, &runningHandles);
            checkMultiInfo();
          });
          break;
        }
        case CURL_POLL_REMOVE:
          if (easy.poll)
          {
            easy.poll->stop();
            easy.poll = nullptr;
            curl_multi_assign(handle, socket, nullptr);
          }
          break;
        }
      });
      setTimerCb([this](long timeoutMs) {
        if (timeoutMs < 0)
          timeout.stop();
        else
        {
          if (timeoutMs == 0)
            timeoutMs = 1;
          timeout.start(
            [this]() {
              int runningHandles;
              socketAction(CURL_SOCKET_TIMEOUT, 0, &runningHandles);
              checkMultiInfo();
            },
            timeoutMs,
            0);
        }
      });
    }
    Multi(const Multi &) = delete;
    ~Multi() { curl_multi_cleanup(handle); }

    auto addHandle(Easy &easy) { curl_multi_add_handle(handle, easy.get()); }

  private:
    using SocketCb = std::function<auto(Easy &, curl_socket_t, int action)->void>;
    auto setSocketCb(SocketCb cb) -> void
    {
      socketCb = std::move(cb);
      curl_multi_setopt(handle, CURLMOPT_SOCKETFUNCTION, &socketFunc);
      curl_multi_setopt(handle, CURLMOPT_SOCKETDATA, &socketCb);
    }

    using TimerCb = std::function<auto(long timeout_ms)->void>;
    auto setTimerCb(TimerCb cb) -> void
    {
      timerCb = std::move(cb);
      curl_multi_setopt(handle, CURLMOPT_TIMERFUNCTION, &timerFunc);
      curl_multi_setopt(handle, CURLMOPT_TIMERDATA, &timerCb);
    }

    auto socketAction(curl_socket_t socket, int flags, int *runningHandles) -> void
    {
      curl_multi_socket_action(handle, socket, flags, runningHandles);
    }

    auto checkMultiInfo() -> void
    {
      CURLMsg *message;
      int pending;

      while ((message = curl_multi_info_read(handle, &pending)))
      {
        switch (message->msg)
        {
        case CURLMSG_DONE: {
          auto &easy = Easy::get(message->easy_handle);
          if (easy.doneCb)
            easy.doneCb();

          curl_multi_remove_handle(handle, message->easy_handle);
          break;
        }

        default: fprintf(stderr, "CURLMSG default\n"); break;
        }
      }
    }

    static auto timerFunc(CURLM * /*multi*/, long timeout_ms, void *userp) -> void { (*static_cast<TimerCb *>(userp))(timeout_ms); }
    static auto socketFunc(CURL *easy, curl_socket_t socket, int action, void *userp, void * /*socketp*/) -> void
    {
      (*static_cast<SocketCb *>(userp))(Easy::get(easy), socket, action);
    }

    CURLM *handle;
    uvpp::Timer timeout;
    SocketCb socketCb;
    TimerCb timerCb;
  };
} // namespace curlpp
