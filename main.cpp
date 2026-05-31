#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#include <type_traits>

#include <locale.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

const char *kJson = R"({"val": 99.123456})";
static constexpr double kExpected = 99.123456;
static constexpr int kMaxSwitches = 10'000'000;

struct LocaleGuard {
public:
  explicit LocaleGuard(int mask, const char *locale) noexcept
      : m_Loc(newlocale(mask, locale, (locale_t)0)) {
    uselocale(m_Loc.get());
  }

private:
  struct Deleter {
    void operator()(locale_t l) noexcept { freelocale(l); }
  };
  std::unique_ptr<std::remove_pointer_t<locale_t>, Deleter> m_Loc{};
};

void worker(std::stop_token stop) {
  LocaleGuard c_locale(LC_NUMERIC, "C");

  for (int i = 0; !stop.stop_requested(); ++i) {
    json j = json::parse(kJson);
    double val = j.value("val", 0.0);
    if (val != kExpected) {
      fprintf(stderr, "TRUNCATION at iteration %d: expected %.9f, got %.9f\n",
              i, kExpected, val);
      std::exit(1);
    }
  }
}

int main() {
  printf("Locale: %s\n", setlocale(LC_NUMERIC, nullptr));

  {
    int switches = 0;
    bool german = false;

    std::jthread w(worker);

    for (; switches < kMaxSwitches; ++switches) {
      setlocale(LC_NUMERIC, german ? "de_DE.UTF-8" : "C");
      german = !german;
    }
  }

  printf("OK: %d iterations without truncation\n", kMaxSwitches);
}
