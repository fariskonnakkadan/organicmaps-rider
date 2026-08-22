#include "testing/testing.hpp"

#include "map/route_poi_finder.hpp"

#include <string>
#include <vector>

namespace route_poi_finder_tests
{
using route_pois::Category;
using route_pois::Interleave;
using route_pois::Poi;

std::vector<Poi> MakePois(Category category, size_t count)
{
  std::vector<Poi> result;
  for (size_t i = 0; i < count; ++i)
  {
    std::string const name = (category == Category::Fuel ? "fuel" : "food") + std::to_string(i);
    result.emplace_back(category, name, static_cast<double>(i + 1) * 1000.0, ms::LatLon(0.0, 0.0));
  }
  return result;
}

std::string Names(std::vector<Poi> const & pois)
{
  std::string result;
  for (auto const & poi : pois)
  {
    if (!result.empty())
      result += ",";
    result += poi.m_name;
  }
  return result;
}

UNIT_TEST(RoutePois_InterleaveAlternatesCategories)
{
  auto const result = Interleave(MakePois(Category::Fuel, 5), MakePois(Category::Food, 5), 5);
  TEST_EQUAL(Names(result), "fuel0,food0,fuel1,food1,fuel2", ());
}

UNIT_TEST(RoutePois_InterleaveStartsWithFuel)
{
  auto const result = Interleave(MakePois(Category::Fuel, 1), MakePois(Category::Food, 1), 5);
  TEST_EQUAL(Names(result), "fuel0,food0", ());
}

UNIT_TEST(RoutePois_InterleaveFallsBackToTheOtherCategory)
{
  // Only one fuel station around: the rest of the list is filled with food instead of staying short.
  auto const withOneFuel = Interleave(MakePois(Category::Fuel, 1), MakePois(Category::Food, 5), 5);
  TEST_EQUAL(Names(withOneFuel), "fuel0,food0,food1,food2,food3", ());

  auto const withoutFuel = Interleave({}, MakePois(Category::Food, 3), 5);
  TEST_EQUAL(Names(withoutFuel), "food0,food1,food2", ());

  auto const withoutFood = Interleave(MakePois(Category::Fuel, 3), {}, 5);
  TEST_EQUAL(Names(withoutFood), "fuel0,fuel1,fuel2", ());
}

UNIT_TEST(RoutePois_InterleaveRespectsLimits)
{
  TEST(Interleave(MakePois(Category::Fuel, 3), MakePois(Category::Food, 3), 0).empty(), ());
  TEST(Interleave({}, {}, 5).empty(), ());

  auto const truncated = Interleave(MakePois(Category::Fuel, 4), MakePois(Category::Food, 4), 3);
  TEST_EQUAL(truncated.size(), 3, ());
  TEST_EQUAL(Names(truncated), "fuel0,food0,fuel1", ());
}
}  // namespace route_poi_finder_tests
