package app.organicmaps.sdk.routing;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/** A fuel station or an eatery lying ahead on the active route. Created from JNI. */
public class RoutePoi
{
  public static final int CATEGORY_FUEL = 0;
  public static final int CATEGORY_FOOD = 1;

  public final int category;
  /** Empty for unnamed places, the UI substitutes a category label then. */
  @NonNull
  public final String name;
  /** Distance left to drive along the route, in meters. */
  public final double distanceMeters;
  public final double lat;
  public final double lon;

  public RoutePoi(int category, @Nullable String name, double distanceMeters, double lat, double lon)
  {
    this.category = category;
    this.name = name == null ? "" : name;
    this.distanceMeters = distanceMeters;
    this.lat = lat;
    this.lon = lon;
  }

  public boolean isFuel()
  {
    return category == CATEGORY_FUEL;
  }
}
