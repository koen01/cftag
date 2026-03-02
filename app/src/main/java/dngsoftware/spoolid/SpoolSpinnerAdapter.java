package dngsoftware.spoolid;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;
import androidx.annotation.NonNull;
import java.util.List;

public class SpoolSpinnerAdapter extends ArrayAdapter<SpoolItem> {

    private final List<SpoolItem> items;

    public SpoolSpinnerAdapter(Context context, List<SpoolItem> items) {
        super(context, R.layout.item_spool_spinner, items);
        this.items = items;
    }

    @NonNull
    @Override
    public View getView(int position, View convertView, @NonNull ViewGroup parent) {
        return createView(position, convertView, parent, false);
    }

    @Override
    public View getDropDownView(int position, View convertView, @NonNull ViewGroup parent) {
        return createView(position, convertView, parent, true);
    }

    private View createView(int position, View convertView, ViewGroup parent, boolean showWeight) {
        View row = convertView;
        if (row == null) {
            row = LayoutInflater.from(getContext()).inflate(R.layout.item_spool_spinner, parent, false);
        }

        SpoolItem item = items.get(position);

        View swatch = row.findViewById(R.id.spoolSwatch);
        GradientDrawable oval = new GradientDrawable();
        oval.setShape(GradientDrawable.OVAL);
        try {
            oval.setColor(Color.parseColor("#" + item.colorHex));
        } catch (Exception e) {
            oval.setColor(Color.LTGRAY);
        }
        swatch.setBackground(oval);

        TextView name = row.findViewById(R.id.spoolName);
        String label = "Spool #" + item.id;
        if (item.filamentName != null && !item.filamentName.isEmpty()) {
            label += " - " + item.filamentName;
        }
        name.setText(label);

        TextView weight = row.findViewById(R.id.spoolWeight);
        if (showWeight && item.remainingWeight > 0) {
            weight.setText(String.format("%.0f g remaining", item.remainingWeight));
            weight.setVisibility(View.VISIBLE);
        } else {
            weight.setVisibility(View.GONE);
        }

        return row;
    }
}
