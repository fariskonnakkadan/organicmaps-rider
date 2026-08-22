#pragma once

#include "geometry/latlon.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class DataSource;

namespace routing
{
class Route;
}

namespace route_pois
{
enum class Category : uint8_t
{
  Fuel = 0,
  Food = 1
};

struct Poi
{
  Poi(Category category, std::string name, double distanceMeters, ms::LatLon const & latLon)
    : m_category(category)
    , m_name(std::move(name))
    , m_distanceMeters(distanceMeters)
    , m_latLon(latLon)
  {}

  Category m_category;
  std::string m_name;
  /// Distance left to drive along the route, in meters.
  double m_distanceMeters;
  ms::LatLon m_latLon;
};

struct SearchParams
{
  /// How far along the route to look for POIs.
  double m_lookAheadMeters = 30000.0;
  /// Max distance from the route polyline for a POI to be considered reachable.
  double m_lateralMeters = 300.0;
  /// How many POIs of each category to collect before interleaving.
  size_t m_maxPerCategory = 5;
  /// Size of the result: fuel and food alternate until it is filled.
  size_t m_maxTotal = 5;
};

/// Collects fuel stations and eateries lying ahead along the route, alternating between the two
/// categories so that a single dense category can't fill the whole list. Result is ordered by the
/// distance left to drive, ascending within each category.
std::vector<Poi> FindAhead(DataSource const & dataSource, routing::Route const & route, SearchParams const & params);

/// Takes items from |fuel| and |food| in turn until |maxTotal| is reached, falling back to whatever
/// list still has items once the other one is exhausted. Both lists are expected to be sorted by
/// distance. Exposed for tests.
std::vector<Poi> Interleave(std::vector<Poi> const & fuel, std::vector<Poi> const & food, size_t maxTotal);

std::string DebugPrint(Category category);
std::string DebugPrint(Poi const & poi);
}  // namespace route_pois
