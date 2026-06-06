#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <thread>

#include <nlohmann/json.hpp>

#ifdef _WIN32

#include <string>

struct LocaleGuard {
public:
  explicit LocaleGuard(int category, const char *locale) noexcept
      : m_Category{category}, m_PrevLocale{std::setlocale(category, nullptr)},
        m_PrevPerThreadLocale{_configthreadlocale(_ENABLE_PER_THREAD_LOCALE)} {
    if (!std::setlocale(category)) {
      _configthreadlocale(m_PrevPerThreadLocale);
      throw std::invalid_argument{"Unsupported locale"};
    }
  }

  ~LocaleGuard() {
    switch (m_PrevPerThreadLocale) {
    case _ENABLE_PER_THREAD_LOCALE:
      std::setlocale(m_Category, m_PrevLocale.c_str());
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
  int m_Category;
  std::string m_PrevLocale;
  int m_PrevPerThreadLocale;
};

#else

struct LocaleGuard {
public:
  explicit LocaleGuard(int category, const char *locale)
      : m_Loc{newlocale(category, locale, nullptr)},
        m_PrevLoc{uselocale(m_Loc)} {
    if (!m_Loc)
      throw std::invalid_argument{"Unsupported locale"};
  }

  ~LocaleGuard() {
    uselocale(m_PrevLoc);
    freelocale(m_Loc);
  }

  LocaleGuard(LocaleGuard const &) = delete;
  LocaleGuard &operator=(LocaleGuard const &) = delete;
  LocaleGuard(LocaleGuard &&) = delete;
  LocaleGuard &operator=(LocaleGuard &&) = delete;

private:
  locale_t m_Loc{};
  locale_t m_PrevLoc{};
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
