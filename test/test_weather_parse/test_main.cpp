// Host-side tests for weather_parse (pio test -e native): canned ip-api and
// Open-Meteo responses -> plain structs. The canned JSON mirrors the real
// API shapes for the exact spec-pinned request URLs.
#include <unity.h>

#include <weather_parse.h>

void setUp() {}
void tearDown() {}

// ip-api.com/json/?fields=status,country,city,lat,lon,timezone,offset
static const char* kGeoOk = R"({
  "status": "success",
  "country": "Brazil",
  "city": "Curitiba",
  "lat": -25.4284,
  "lon": -49.2733,
  "timezone": "America/Sao_Paulo",
  "offset": -10800
})";

static void test_geo_success() {
  const GeoResult g = parseGeo(kGeoOk);
  TEST_ASSERT_TRUE(g.ok);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -25.4284f, g.lat);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -49.2733f, g.lon);
  TEST_ASSERT_EQUAL_STRING("Curitiba", g.city.c_str());
  TEST_ASSERT_EQUAL_INT32(-10800, g.offsetSec);
}

static void test_geo_fail_status() {
  // ip-api answers 200 with status:"fail" for private/reserved IPs.
  const GeoResult g = parseGeo(
      R"({"status":"fail","message":"private range","query":"192.168.0.1"})");
  TEST_ASSERT_FALSE(g.ok);
}

static void test_geo_malformed_json() {
  TEST_ASSERT_FALSE(parseGeo("not json at all").ok);
  TEST_ASSERT_FALSE(parseGeo("").ok);
  TEST_ASSERT_FALSE(parseGeo(R"({"status":"success","lat":)").ok);
}

static void test_geo_missing_coords() {
  TEST_ASSERT_FALSE(parseGeo(R"({"status":"success","city":"X"})").ok);
}

// Open-Meteo /v1/forecast response for the spec-pinned query (trimmed to the
// fields we request; extra fields like *_units are present in real replies
// and must be ignored gracefully).
static const char* kForecastOk = R"({
  "latitude": -25.5,
  "longitude": -49.25,
  "utc_offset_seconds": -10800,
  "current_units": {"temperature_2m": "°C", "wind_speed_10m": "km/h"},
  "current": {
    "time": "2026-07-06T14:15",
    "interval": 900,
    "temperature_2m": 18.3,
    "relative_humidity_2m": 62,
    "is_day": 1,
    "weather_code": 3,
    "wind_speed_10m": 9.8
  },
  "daily_units": {"temperature_2m_max": "°C"},
  "daily": {
    "time": ["2026-07-06", "2026-07-07", "2026-07-08"],
    "weather_code": [3, 61, 0],
    "temperature_2m_max": [19.1, 15.2, 21.0],
    "temperature_2m_min": [11.4, 9.8, 8.9]
  }
})";

static void test_forecast_current() {
  const ForecastWx f = parseForecast(kForecastOk);
  TEST_ASSERT_TRUE(f.ok);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 18.3f, f.current.tempC);
  TEST_ASSERT_EQUAL_INT(62, f.current.humidity);
  TEST_ASSERT_TRUE(f.current.isDay);
  TEST_ASSERT_EQUAL_INT(3, f.current.wmoCode);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 9.8f, f.current.windKmh);
}

static void test_forecast_days() {
  const ForecastWx f = parseForecast(kForecastOk);
  TEST_ASSERT_EQUAL_INT(3, f.dayCount);
  TEST_ASSERT_EQUAL_INT(3, f.days[0].wmoCode);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 19.1f, f.days[0].tmaxC);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 11.4f, f.days[0].tminC);
  TEST_ASSERT_EQUAL_INT(61, f.days[1].wmoCode);
  TEST_ASSERT_EQUAL_INT(0, f.days[2].wmoCode);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.9f, f.days[2].tminC);
}

static void test_forecast_missing_current() {
  TEST_ASSERT_FALSE(parseForecast(R"({"latitude":-25.5})").ok);
}

static void test_forecast_malformed() {
  TEST_ASSERT_FALSE(parseForecast("<html>502 Bad Gateway</html>").ok);
  TEST_ASSERT_FALSE(parseForecast("").ok);
}

static void test_forecast_short_daily_arrays() {
  // Fewer daily entries than requested must not crash or over-read.
  const ForecastWx f = parseForecast(R"({
    "current": {"temperature_2m": 5.0, "relative_humidity_2m": 80,
                "is_day": 0, "weather_code": 71, "wind_speed_10m": 20.1},
    "daily": {"weather_code": [71], "temperature_2m_max": [2.0],
              "temperature_2m_min": [-4.0]}
  })");
  TEST_ASSERT_TRUE(f.ok);
  TEST_ASSERT_FALSE(f.current.isDay);
  TEST_ASSERT_EQUAL_INT(1, f.dayCount);
  TEST_ASSERT_EQUAL_INT(71, f.days[0].wmoCode);
}

static void test_forecast_url_pins_spec_query() {
  const std::string url = forecastUrl(-25.4284f, -49.2733f);
  TEST_ASSERT_EQUAL_STRING(
      "https://api.open-meteo.com/v1/forecast"
      "?latitude=-25.4284&longitude=-49.2733"
      "&current=temperature_2m,relative_humidity_2m,is_day,weather_code,"
      "wind_speed_10m"
      "&daily=weather_code,temperature_2m_max,temperature_2m_min"
      "&timezone=auto&forecast_days=3",
      url.c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_geo_success);
  RUN_TEST(test_geo_fail_status);
  RUN_TEST(test_geo_malformed_json);
  RUN_TEST(test_geo_missing_coords);
  RUN_TEST(test_forecast_current);
  RUN_TEST(test_forecast_days);
  RUN_TEST(test_forecast_missing_current);
  RUN_TEST(test_forecast_malformed);
  RUN_TEST(test_forecast_short_daily_arrays);
  RUN_TEST(test_forecast_url_pins_spec_query);
  return UNITY_END();
}
