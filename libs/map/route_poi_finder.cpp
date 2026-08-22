#include "map/route_poi_finder.hpp"

#include "routing/route.hpp"

#include "indexer/classificator.hpp"
#include "indexer/data_source.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/scales.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"
#include "geometry/polyline2d.hpp"

#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace route_pois
{
namespace
{
/// Route is scanned in chunks of this length; one index query is made per chunk.
double constexpr kChunkMeters = 2000.0;

class FuelChecker : public ftypes::BaseChecker
{
  FuelChecker() { m_types.push_back(classif().GetTypeByPath({"amenity", "fuel"})); }

public:
  DECLARE_CHECKER_INSTANCE(FuelChecker);
};

/// Places to eat at. Bars and pubs are left out on purpose: this list is meant for a meal stop.
class FoodChecker : public ftypes::BaseChecker
{
  FoodChecker()
  {
    Classificator const & c = classif();
    for (auto const & path : {base::StringIL{"amenity", "restaurant"}, base::StringIL{"amenity", "fast_food"},
                              base::StringIL{"amenity", "cafe"}, base::StringIL{"amenity", "food_court"}})
      m_types.push_back(c.GetTypeByPath(path));
  }

public:
  DECLARE_CHECKER_INSTANCE(FoodChecker);
};

std::vector<double> CalcPrefixDistances(std::vector<m2::PointD> const & points)
{
  std::vector<double> prefix(points.size(), 0.0);
  for (size_t i = 1; i < points.size(); ++i)
    prefix[i] = prefix[i - 1] + mercator::DistanceOnEarth(points[i - 1], points[i]);
  return prefix;
}

/// Grows |rect| by |meters| on every side. Mercator degrees per meter grow with latitude, so the
/// conversion is done at the corners instead of scaling by the equatorial factor.
m2::RectD InflateByMeters(m2::RectD const & rect, double meters)
{
  m2::RectD result = rect;
  result.Add(mercator::RectByCenterXYAndSizeInMeters(rect.LeftBottom(), 2.0 * meters));
  result.Add(mercator::RectByCenterXYAndSizeInMeters(rect.RightTop(), 2.0 * meters));
  return result;
}

/// Index of the polyline point closest to |pt| within [beginIdx, endIdx).
size_t FindClosestPointIdx(std::vector<m2::PointD> const & points, size_t beginIdx, size_t endIdx,
                           m2::PointD const & pt)
{
  size_t closest = beginIdx;
  double minSquaredDist = std::numeric_limits<double>::max();
  for (size_t i = beginIdx; i < endIdx; ++i)
  {
    double const squaredDist = points[i].SquaredLength(pt);
    if (squaredDist < minSquaredDist)
    {
      minSquaredDist = squaredDist;
      closest = i;
    }
  }
  return closest;
}
}  // namespace

std::vector<Poi> Interleave(std::vector<Poi> const & fuel, std::vector<Poi> const & food, size_t maxTotal)
{
  std::vector<Poi> result;
  result.reserve(std::min(maxTotal, fuel.size() + food.size()));

  size_t fuelIdx = 0;
  size_t foodIdx = 0;
  bool takeFuel = true;
  while (result.size() < maxTotal && (fuelIdx < fuel.size() || foodIdx < food.size()))
  {
    // Stick to the requested category when it still has items, otherwise drain the other one.
    bool const useFuel = takeFuel ? fuelIdx < fuel.size() : foodIdx >= food.size();
    result.push_back(useFuel ? fuel[fuelIdx++] : food[foodIdx++]);
    takeFuel = !takeFuel;
  }
  return result;
}

std::vector<Poi> FindAhead(DataSource const & dataSource, routing::Route const & route, SearchParams const & params)
{
  auto const & points = route.GetPoly().GetPoints();
  if (points.size() < 2)
    return {};

  auto const prefix = CalcPrefixDistances(points);
  double const currentDist = route.GetCurrentDistanceFromBeginMeters();
  double const lastDist = std::min(prefix.back(), currentDist + params.m_lookAheadMeters);

  size_t const startIdx = static_cast<size_t>(
      std::distance(prefix.begin(), std::lower_bound(prefix.begin(), prefix.end(), currentDist)));

  auto const & fuelChecker = FuelChecker::Instance();
  auto const & foodChecker = FoodChecker::Instance();

  std::vector<Poi> fuel;
  std::vector<Poi> food;
  // The same POI can fall into two neighbouring chunk rects, which overlap by the lateral margin.
  std::set<FeatureID> visited;

  for (size_t i = startIdx; i + 1 < points.size() && prefix[i] <= lastDist;)
  {
    double const chunkEnd = std::min(prefix[i] + kChunkMeters, lastDist);
    m2::RectD chunkRect;
    size_t j = i;
    // A chunk always covers at least one segment, otherwise a single long segment would stall here.
    while (j < points.size() && (j == i || prefix[j] <= chunkEnd))
      chunkRect.Add(points[j++]);

    dataSource.ForEachInRect(
        [&](FeatureType & ft)
    {
      Category category;
      if (fuelChecker(ft))
        category = Category::Fuel;
      else if (foodChecker(ft))
        category = Category::Food;
      else
        return;

      if (!visited.insert(ft.GetID()).second)
        return;

      auto const center = feature::GetCenter(ft);
      size_t const closestIdx = FindClosestPointIdx(points, i, j, center);
      if (mercator::DistanceOnEarth(points[closestIdx], center) > params.m_lateralMeters)
        return;

      double const distance = prefix[closestIdx] - currentDist;
      if (distance < 0.0)
        return;

      auto & bucket = category == Category::Fuel ? fuel : food;
      bucket.emplace_back(category, std::string(ft.GetReadableName()), distance, mercator::ToLatLon(center));
    }, InflateByMeters(chunkRect, params.m_lateralMeters), scales::GetUpperScale());

    i = j;
  }

  auto const byDistance = [](Poi const & lhs, Poi const & rhs) { return lhs.m_distanceMeters < rhs.m_distanceMeters; };
  for (auto * bucket : {&fuel, &food})
  {
    std::sort(bucket->begin(), bucket->end(), byDistance);
    if (bucket->size() > params.m_maxPerCategory)
      bucket->erase(bucket->begin() + params.m_maxPerCategory, bucket->end());
  }

  return Interleave(fuel, food, params.m_maxTotal);
}

std::string DebugPrint(Category category)
{
  return category == Category::Fuel ? "Fuel" : "Food";
}

std::string DebugPrint(Poi const & poi)
{
  return "Poi [" + DebugPrint(poi.m_category) + ", " + poi.m_name + ", " + strings::to_string(poi.m_distanceMeters) +
         "m, " + DebugPrint(poi.m_latLon) + "]";
}
}  // namespace route_pois
