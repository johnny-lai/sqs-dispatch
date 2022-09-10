#include <aws/core/Aws.h>
#include <aws/sqs/SQSClient.h>
#include <aws/sqs/model/ListQueuesRequest.h>
#include <aws/sqs/model/ListQueuesResult.h>
#include <aws/sqs/model/ReceiveMessageRequest.h>
#include <chrono>
#include <future>
#include <iostream>
#include <string_view>

#include <boost/program_options.hpp>

namespace po = boost::program_options;
using namespace std::chrono_literals;

class Measurement {
public:
    Measurement(const std::string_view &_label)
    : label(_label) {}

    ~Measurement() {
      auto end = std::chrono::steady_clock::now();
      std::chrono::duration<double> duration = end - start;
      std::cout << label << ": " << duration.count() << "s" << std::endl;
    }

    std::string label;
    std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
};

template <typename F>
auto measure(const std::string_view &label, F f) {
    Measurement m(label);
    if constexpr(std::is_same<decltype(f()), void>::value) {
      f();
    } else {
      return f();
    }
}

template <typename T>
auto wait_for(std::vector<T>&& futures, const std::chrono::nanoseconds duration) {
    auto end = std::chrono::steady_clock::now() + duration;

    typename std::vector<T>::size_type ready_count = 0;
    const typename std::vector<T>::size_type futures_len = std::size(futures);

    std::chrono::nanoseconds left = duration;
    while (ready_count < futures_len && left > 0ms) {
        if (futures[0].wait_for(left) == std::future_status::ready) {
            // swap i with ready_index, so all ready are at the end
            auto last_ready_index = futures_len - ready_count - 1;
            if (last_ready_index > 0) {
              std::swap(futures[0], futures[last_ready_index]);
            }
            ++ready_count;
        }
        left = end - std::chrono::steady_clock::now();
    }

    // check leftovers for readiness
    // if not all ready, then there is no time left.
    for (auto i = 0; i + ready_count < futures_len;) {
        if (futures[i].wait_for(0ms) == std::future_status::ready) {
            // swap i with ready_index, so all ready are at the end
            auto last_ready_index = futures_len - ready_count - 1;
            if (last_ready_index > 0) {
              std::swap(futures[i], futures[last_ready_index]);
            }
            ++ready_count;
        } else {
            ++i;  // Not ready. Check next index
        }
    }

    // return start of first ready index
    return (futures_len - ready_count);
}


std::optional<std::string> exec(const std::string_view& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.data(), "r"), pclose);
    if (!pipe) {
        return std::optional<std::string>();
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return std::optional<std::string>(result);
}

class Application {
public:
    struct Config {
        std::vector<std::string> exec;
    };
public:
    Application() = delete;
    Application(const Config& _config, const Aws::Client::ClientConfiguration& _aws_config);

    void Receive(const Aws::String& queue_url, const std::chrono::nanoseconds duration);
    bool Process(const Aws::SQS::Model::Message& message);

    std::string CmdLine(const Aws::SQS::Model::Message& message);
protected:
    Config config;
    Aws::Client::ClientConfiguration aws_config;

    std::unique_ptr<Aws::SQS::SQSClient> sqs;
};

Application::Application(const Config& _config, const Aws::Client::ClientConfiguration& _aws_config)
    : config(_config), aws_config(_aws_config) {
    sqs.reset(new Aws::SQS::SQSClient(aws_config));
}

void Application::Receive(const Aws::String& queue_url, const std::chrono::nanoseconds duration) {
    using namespace std::chrono_literals;

    const auto max_parallel = 2;
    std::vector<std::future<bool>> futures;
    futures.reserve(max_parallel);

    auto max_messages = max_parallel - size(futures);
    if (max_messages == 0) {
        return;
    }

    Aws::SQS::Model::ReceiveMessageRequest rm_req;
    rm_req.SetQueueUrl(queue_url);
    rm_req.SetMaxNumberOfMessages(max_messages);

    auto rm_out = sqs->ReceiveMessage(rm_req);
    if (!rm_out.IsSuccess())
    {
        std::cout << "Error receiving message from queue " << queue_url << ": "
            << rm_out.GetError().GetMessage() << std::endl;
        return;
    }

    const auto& messages = rm_out.GetResult().GetMessages();
    if (messages.size() == 0)
    {
        std::cout << "No messages received from queue " << queue_url <<
            std::endl;
        return;
    }


    for (const auto& message: messages) {
        futures.emplace_back(std::async(std::launch::async, &Application::Process, this, message));
    }

    auto ready_idx = wait_for(std::move(futures), 100ms);
    std::cout << "ready idx = " << ready_idx << std::endl;

    for (auto i = ready_idx; i < size(futures); ++i) {
        std::cout << "ret " << i << " = " << futures[i].get() << std::endl;
    }
    futures.resize(size(futures) - ready_idx);
}

bool Application::Process(const Aws::SQS::Model::Message& message) {
    std::cout << "Received message:" << std::endl;
    std::cout << "  MessageId: " << message.GetMessageId() << std::endl;
    std::cout << "  ReceiptHandle: " << message.GetReceiptHandle() << std::endl;
    std::cout << "  Body: " << message.GetBody() << std::endl << std::endl;

    auto result = exec(CmdLine(message));
    if (!result) {
      std::cout << "failed to echo" << std::endl;
      return false;
    }
    std::cout << "cmd returned " << result.value() << std::endl;

    return true;
}

std::string Application::CmdLine(const Aws::SQS::Model::Message& message) {
    std::string ret{};
    decltype(config.exec)::size_type i{1};
    for (const auto& arg : config.exec) {
        if (arg == "{}.messageId") {
          ret.append(message.GetMessageId());
        } else if (arg == "{}.body") {
          ret.append(message.GetBody());
        } else {
          ret.append(arg);
        }
        if (i < size(config.exec)) {
          ret.append(" ");
        }
        ++i;
    }
    return ret;
}

auto extract_url_scheme(const std::string_view& url) {
  bool success = true;
  std::pair<Aws::Http::Scheme, std::string_view> ret;
  Aws::Http::Scheme scheme;
  std::string_view host;

  auto split = url.find(":");
  if (split == std::string::npos) {
    // Not found
    ret.first = Aws::Http::Scheme::HTTPS;
    ret.second = url;
  } else {
    auto ei = split + 1;
    for (; ei < url.size(); ++ei) {
      if (url[ei] != '/') {
        break;
      }
    }

    auto scheme_str = url.substr(0, split);
    // ret.first = Aws::Http::SchemeMapper::FromString(scheme.data());
    if (scheme_str == "http") {
      ret.first = Aws::Http::Scheme::HTTP;
    } else if (scheme_str == "https") {
      ret.first = Aws::Http::Scheme::HTTPS;
    } else {
      // Invalid
      success = false;
    }
    ret.second = url.substr(ei, url.size());
  }

  if (success) {
    return std::optional<decltype(ret)>(ret);
  } else {
    return std::optional<decltype(ret)>();
  }
}

auto split(const std::string_view& str, const char delimiter) {
  std::vector<std::string_view> ret;
  for (decltype(ret)::size_type i{}, end{}; i < str.size(); i = end + 1) {
    end = str.find(delimiter, i);
    if (end == std::string::npos) {
      end = str.size();
    }
    ret.emplace_back(str.substr(i, end - i));
  }
  return ret;
}

int main(int argc, char** argv)
{
    po::options_description desc;
    desc.add_options()
        ("help", "show the help message")
        ("endpoint-url,E", po::value<std::string>()->default_value(""), "endpoint URL")
        ("queue-url,Q", po::value<std::string>(), "sqs queue URL (required)")
        ("exec,e", po::value<std::vector<std::string>>(), "exec arguments. Use {}.messageId and {}.body to get the message.")
        ;

    po::positional_options_description p;
    p.add("exec", -1);

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).
          options(desc).positional(p).run(), vm);
        po::notify(vm);
    } catch (po::unknown_option unknown) {
        std::cout << "Unknown option: " << unknown.get_option_name() << std::endl;
        return 1;
    }

    if (vm.count("help") || vm.count("queue-url") == 0) {
        std::cout << desc << std::endl;
        return 1;
    }

    auto queue_url = vm["queue-url"].as<std::string>();

    std::string endpoint = vm["endpoint-url"].as<std::string>();
    auto parsed = extract_url_scheme(endpoint);
    if (!parsed) {
        std::cout << "invalid endpoint " << endpoint << std::endl;
        return 1;
    }
    auto [endpoint_scheme, endpoint_host] = parsed.value();

    std::vector<std::string> exec;
    if (vm.count("exec") > 0) {
        exec = vm["exec"].as<std::vector<std::string>>();
    } else {
        // Default to echo {}
        exec.emplace_back("echo");
        exec.emplace_back("{}");
    }

    int exit_code = 0;

    Aws::SDKOptions options;
    Aws::InitAPI(options);
    {
        Application::Config config{};
        config.exec = exec;

        // Localstack
        Aws::Client::ClientConfiguration aws_config{};
        aws_config.endpointOverride = endpoint_host;
        aws_config.scheme = endpoint_scheme;

        std::unique_ptr<Application> app = measure("app", [&app, &config, &aws_config]{
            return std::make_unique<Application>(config, aws_config);
        });

        for (auto i = 0; i < 5; ++i) {
            Measurement m("receive");
            app->Receive(queue_url, 100ms);
        };
    }
    Aws::ShutdownAPI(options);

    return exit_code;
}

