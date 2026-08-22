package app.organicmaps.routing;

import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import androidx.annotation.NonNull;
import app.organicmaps.R;
import app.organicmaps.sdk.Framework;
import app.organicmaps.sdk.routing.RoutePoi;
import app.organicmaps.sdk.util.StringUtils;
import app.organicmaps.util.UiUtils;
import com.google.android.material.floatingactionbutton.FloatingActionButton;

/**
 * Drives the "i" button of the navigation screen and the panel it opens: a short list of fuel
 * stations and eateries lying ahead on the route, closest first, with the distance left to each.
 * <p>
 * The list is only recomputed while the panel is open, since the lookup walks the route index.
 */
public class NavPoiPanelController
{
  private static final int MAX_ITEMS = 5;
  /** Refreshing on every location fix is pointless — the list barely changes in a few seconds. */
  private static final long REFRESH_INTERVAL_MS = 5000;

  @NonNull
  private final FloatingActionButton mButton;
  @NonNull
  private final View mPanel;
  @NonNull
  private final LinearLayout mItems;
  @NonNull
  private final View mEmpty;

  private boolean mOpen;
  private long mLastRefreshMs;

  public NavPoiPanelController(@NonNull View topFrame)
  {
    mButton = topFrame.findViewById(R.id.nav_poi_button);
    mPanel = topFrame.findViewById(R.id.nav_poi_panel);
    mItems = mPanel.findViewById(R.id.nav_poi_items);
    mEmpty = mPanel.findViewById(R.id.nav_poi_empty);

    mButton.setOnClickListener(v -> toggle());
  }

  private void toggle()
  {
    mOpen = !mOpen;
    UiUtils.showIf(mOpen, mPanel);
    if (mOpen)
      refresh();
  }

  /** Called on every routing info update; throttled internally. */
  public void update()
  {
    if (!mOpen)
      return;

    final long now = SystemClock.elapsedRealtime();
    if (now - mLastRefreshMs < REFRESH_INTERVAL_MS)
      return;

    refresh();
  }

  public void show(boolean show)
  {
    UiUtils.showIf(show, mButton);
    if (!show)
    {
      mOpen = false;
      UiUtils.hide(mPanel);
    }
  }

  private void refresh()
  {
    mLastRefreshMs = SystemClock.elapsedRealtime();

    final RoutePoi[] pois = Framework.nativeGetRoutePoisAhead(MAX_ITEMS);
    final int count = pois == null ? 0 : pois.length;

    UiUtils.showIf(count == 0, mEmpty);

    // Rows are reused across refreshes: only the extra ones are inflated, the surplus is hidden.
    for (int i = mItems.getChildCount(); i < count; ++i)
      mItems.addView(LayoutInflater.from(mItems.getContext()).inflate(R.layout.nav_poi_item, mItems, false));

    for (int i = 0; i < mItems.getChildCount(); ++i)
    {
      final View row = mItems.getChildAt(i);
      if (i >= count)
      {
        UiUtils.hide(row);
        continue;
      }
      UiUtils.show(row);
      bind(row, pois[i]);
    }
  }

  private void bind(@NonNull View row, @NonNull RoutePoi poi)
  {
    final ImageView icon = row.findViewById(R.id.nav_poi_icon);
    final TextView name = row.findViewById(R.id.nav_poi_name);
    final TextView distance = row.findViewById(R.id.nav_poi_distance);

    icon.setImageResource(poi.isFuel() ? R.drawable.ic_routing_fuel_on : R.drawable.ic_routing_food_on);
    name.setText(displayName(row, poi));
    distance.setText(StringUtils.nativeFormatDistance(poi.distanceMeters).toString(row.getContext()));
  }

  @NonNull
  private String displayName(@NonNull View row, @NonNull RoutePoi poi)
  {
    if (!poi.name.isEmpty())
      return poi.name;

    return row.getResources().getString(poi.isFuel() ? R.string.nav_poi_fuel_fallback : R.string.nav_poi_food_fallback);
  }
}
