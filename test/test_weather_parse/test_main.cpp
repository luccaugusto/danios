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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_geo_success);
  RUN_TEST(test_geo_fail_status);
  RUN_TEST(test_geo_malformed_json);
  RUN_TEST(test_geo_missing_coords);
  return UNITY_END();
}
