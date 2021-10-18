#include "curlpp.hpp"
#include "log/log.hpp"

int main()
{
  uvpp::Loop loop;

  if (curl_global_init(CURL_GLOBAL_ALL))
  {
    fprintf(stderr, "Could not init curl\n");
    return 1;
  }

  curlpp::Multi multi(loop);
  std::unique_ptr<curlpp::Easy> easy = std::make_unique<curlpp::Easy>();
  easy->setUrl("https://mika.global");
  easy->doneCb = [&easy, &multi]() {
    LOG("done!");
    easy = std::make_unique<curlpp::Easy>();
    easy->setUrl("https://mika.global");
    easy->doneCb = []() { LOG("2 done!"); };
    multi.addHandle(*easy);
  };
  multi.addHandle(*easy);

  loop.run(UV_RUN_DEFAULT);
}
