#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include <nlohmann/json.hpp>

#ifdef _WIN32

struct LocaleGuard {
public:
  explicit LocaleGuard(int mask, const char *locale) noexcept
      : m_Mask{mask},
        m_PrevPerThreadLocale{_configthreadlocale(_ENABLE_PER_THREAD_LOCALE)},
        m_PrevLocale{std::setlocale(mask, locale)} {}

  ~LocaleGuard() {
    switch (m_PrevPerThreadLocale) {
    case _ENABLE_PER_THREAD_LOCALE:
      std::setlocale(m_Mask, m_PrevLocale);
      break;
    case _DISABLE_PER_THREAD_LOCALE:
      _configthreadlocale(_DISABLE_PER_THREAD_LOCALE);
      break;
    default:
      break;
    }
  }

  LocaleGuard(LocaleGuard const &) = delete;
  LocaleGuard &operator=(LocaleGuard const &) = delete;
  LocaleGuard(LocaleGuard &&) = delete;
  LocaleGuard &operator=(LocaleGuard &&) = delete;

private:
  int m_Mask;
  int m_PrevPerThreadLocale;
  const char *m_PrevLocale;
};

#else

#include <memory>
#include <type_traits>

#include <xlocale.h>

struct LocaleGuard {
public:
  explicit LocaleGuard(int mask, const char *locale) noexcept {
    auto loc = newlocale(mask, locale, (locale_t)0);
    auto prev_loc = uselocale(loc);
    m_Loc = LocalePtr{loc, Deleter{prev_loc}};
  }

private:
  struct Deleter {
    locale_t m_PrevLoc{};

    void operator()(locale_t l) noexcept {
      uselocale(m_PrevLoc);
      freelocale(l);
    }
  };
  using LocalePtr = std::unique_ptr<std::remove_pointer_t<locale_t>, Deleter>;
  LocalePtr m_Loc{};
};

#endif

using json = nlohmann::json;

const char *kJson = R"({"val": 99.123456})";
static constexpr double kExpected = 99.123456;
static constexpr int kMaxSwitches = 10'000'000;

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
