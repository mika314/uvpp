#pragma once
#include <cassert>
#include <functional>
#include <memory>
#include <utility>
#include <uv.h>

namespace uvpp
{

#define UVPP_DECL_METHOD(name, uv_name)                      \
  template <typename... Args>                                \
  auto name(Args &&...args)                                  \
  {                                                          \
    return uv_##uv_name(get(), std::forward<Args>(args)...); \
  }
#define UVPP_DECL_CMETHOD(name, uv_name)                     \
  template <typename... Args>                                \
  auto name(Args &&...args) const                            \
  {                                                          \
    return uv_##uv_name(get(), std::forward<Args>(args)...); \
  }

  template <typename UvHandle>
  class Handle
  {
  protected:
    UvHandle handle;

  private:
    auto get() { return reinterpret_cast<uv_handle_t *>(&handle); }
    auto get() const { return reinterpret_cast<const uv_handle_t *>(&handle); }

  protected:
    UVPP_DECL_CMETHOD(getData, handle_get_data)
    UVPP_DECL_METHOD(setData, handle_set_data)

  public:
    UVPP_DECL_CMETHOD(isActive, is_active)
    UVPP_DECL_CMETHOD(isClosing, is_closing)
    UVPP_DECL_METHOD(ref, ref)
    UVPP_DECL_CMETHOD(hasRef, has_re)
    UVPP_DECL_METHOD(sendBufferSize, send_buffer_size)
    UVPP_DECL_METHOD(recvBufferSize, recv_buffer_size)
    UVPP_DECL_CMETHOD(fileno, fileno)
    UVPP_DECL_CMETHOD(getLoop, handle_get_loop)
    UVPP_DECL_CMETHOD(getType, handle_get_type)
    UVPP_DECL_CMETHOD(typeName, handle_type_name)

    using CloseCb = std::function<auto()->void>;

    auto close(CloseCb cb)
    {
      assert(!closeCb);
      closeCb = std::move(cb);
      return uv_close(handle, [](uv_handle_t *handle) {
        auto self = static_cast<Handle *>(handle->data);
        auto cb = std::move(self->closeCb);
        self->closeCb = nullptr;
        cb();
      });
    }

  private:
    CloseCb closeCb;
  };

  template <typename UvHandle>
  class Stream : public Handle<UvHandle>
  {
  private:
    auto get() { return reinterpret_cast<uv_stream_t *>(&(Handle<UvHandle>::handle)); }
    auto get() const { return reinterpret_cast<const uv_stream_t *>(&(Handle<UvHandle>::handle)); }

  public:
    using ShutdownCb = std::function<auto(int)->void>;
    auto shutdown(ShutdownCb cb)
    {
      assert(!shutdownCb);
      auto tmp = new uv_shutdown_t;
      shutdownCb = std::move(cb);
      return uv_shutdown(tmp, get(), [](uv_shutdown_t *req, int status) {
        auto self = static_cast<Stream *>(Stream{req->handle}.getData());
        auto cb = std::move(self->shutdownCb);
        self->shutdownCb = nullptr;
        delete req;
        cb(status);
      });
    }

    using ConnectionCb = std::function<auto(int)->void>;

    auto listen(int backlog, ConnectionCb cb)
    {
      connectionCb = std::move(cb);
      return uv_listen(get(), backlog, [](uv_stream_t *req, int status) { static_cast<Stream *>(req->data)->connectionCb(status); });
    }

    UVPP_DECL_METHOD(accept, accept)
    using ReadCb = std::function<auto(ssize_t nread, const uv_buf_t *buf)->void>;
    auto readStart(ReadCb cb)
    {
      readCb = std::move(cb);
      return uv_read_start(
        get(),
        [](uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
          auto self = static_cast<Stream *>(handle->data);
          self->readBuf.resize(suggested_size);
          buf->base = self->readBuf.data();
          buf->len = self->readBuf.size();
        },
        [](uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) { static_cast<Stream *>(stream->data)->readCb(nread, buf); });
    }

    UVPP_DECL_METHOD(readStop, read_stop)

    using WriteCb = std::function<auto(int)->void>;
    struct WriteReq
    {
      uv_write_t req;
      std::vector<uv_buf_t> bufs;
      WriteCb cb;
    };

    auto write(std::vector<uv_buf_t> bufs, WriteCb cb)
    {
      auto req = std::make_unique<WriteReq>();
      auto pReq = req.get();
      pReq->bufs = std::move(bufs);
      pReq->cb = std::move(cb);
      writeReqs[&pReq->req] = std::move(req);
      uv_write(&pReq->req, get(), pReq->bufs.data(), pReq->bufs.size(), [](uv_write_t *req, int status) {
        auto self = static_cast<Stream *>(req->handle->data);
        auto cb = std::move(self->writeReqs[req]->cb);
        self->writeReqs.erase(req);
        cb(status);
      });
    }

    // int uv_write2(uv_write_t *req, uv_stream_t *handle, const uv_buf_t bufs[], unsigned int nbufs, uv_stream_t *send_handle, uv_write_cb cb)
    UVPP_DECL_METHOD(tryWrite, try_write)
    UVPP_DECL_METHOD(tryWrite2, try_write2)
    UVPP_DECL_CMETHOD(isReadable, is_readable)
    UVPP_DECL_CMETHOD(isWritable, is_writable)
    UVPP_DECL_METHOD(setBlocking, set_blocking)
    UVPP_DECL_CMETHOD(getWriteQueueSize, stream_get_write_queue_size)

  private:
    ShutdownCb shutdownCb;
    ConnectionCb connectionCb;
    std::unordered_map<uv_write_t *, std::unique_ptr<WriteReq>> writeReqs;
    std::vector<char> readBuf;
    ReadCb readCb;
  };

  class Tcp : public Stream<uv_tcp_t>
  {
  private:
    auto get() { return &handle; }
    auto get() const { return &handle; }

  public:
    Tcp(class Loop &);
    Tcp(class Loop &, unsigned flags);

    UVPP_DECL_METHOD(open, tcp_open)
    UVPP_DECL_METHOD(nodelay, tcp_nodelay)
    UVPP_DECL_METHOD(keepalive, tcp_keepalive)
    UVPP_DECL_METHOD(SimultaneousAccepts, tcp_simultaneous_accepts)
    UVPP_DECL_METHOD(bind, tcp_bind)
    UVPP_DECL_CMETHOD(getsockname, tcp_getsockname)
    UVPP_DECL_CMETHOD(getpeername, tcp_getpeername)
    using ConnectCb = std::function<auto(int)->void>;

    auto connect(const struct sockaddr *addr, ConnectCb cb)
    {
      assert(!connectCb);
      connectCb = std::move(cb);
      return uv_tcp_connect(&connectReq, get(), addr, [](uv_connect_t *req, int status) {
        auto self = static_cast<Tcp *>(req->handle->data);
        auto cb = std::move(self->connectCb);
        self->connectCb = nullptr;
        cb(status);
      });
    }

    using CloseCb = std::function<auto()->void>;
    auto closeReset(CloseCb cb)
    {
      assert(!closeCb);
      closeCb = std::move(cb);
      return uv_tcp_close_reset(get(), [](uv_handle_t *handle) {
        auto self = static_cast<Tcp *>(handle->data);
        auto cb = std::move(self->closeCb);
        self->closeCb = nullptr;
        cb();
      });
    }

  private:
    CloseCb closeCb;
    uv_connect_t connectReq;
    ConnectCb connectCb;
  };

  class Loop
  {
  public:
    auto get() { return &handle; };
    auto get() const { return &handle; };

    Loop() { uv_loop_init(&handle); }
    ~Loop() { uv_loop_close(&handle); }

    UVPP_DECL_METHOD(run, run)
    UVPP_DECL_METHOD(configure, loop_configure)
    UVPP_DECL_CMETHOD(isAlive, loop_alive)
    UVPP_DECL_METHOD(stop, stop)
    UVPP_DECL_CMETHOD(backendFd, backend_fd)
    UVPP_DECL_CMETHOD(backendTimeout, backend_timeout)
    UVPP_DECL_CMETHOD(now, now)
    UVPP_DECL_METHOD(updateTime, update_time)
    UVPP_DECL_METHOD(fork, loop_fork)
    UVPP_DECL_CMETHOD(getData, loop_get_data)
    UVPP_DECL_METHOD(setData, loop_set_data)

    using WalkCb = std::function<auto(uv_handle_t *)->void>;
    auto walk(WalkCb cb)
    {
      return uv_walk(
        &handle, [](uv_handle_t *h, void *arg) { (*static_cast<const WalkCb *>(arg))(h); }, &cb);
    }

  private:
    uv_loop_t handle;
  };

  Tcp::Tcp(Loop &loop)
  {
    uv_tcp_init(loop.get(), get());
    setData(this);
  }

  Tcp::Tcp(Loop &loop, unsigned flags)
  {
    uv_tcp_init_ex(loop.get(), get(), flags);
    setData(this);
  }

#undef UVPP_DECL_METHOD
#undef UVPP_DECL_CMETHOD
} // namespace uvpp
