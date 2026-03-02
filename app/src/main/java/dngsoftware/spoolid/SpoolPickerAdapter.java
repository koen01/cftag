package dngsoftware.spoolid;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import java.util.ArrayList;
import java.util.List;

public class SpoolPickerAdapter extends RecyclerView.Adapter<SpoolPickerAdapter.ViewHolder> {

    public interface OnSpoolClickListener {
        void onSpoolClicked(SpoolItem spool);
    }

    private final List<SpoolItem> allItems;
    private final List<SpoolItem> filteredItems;
    private final Context context;
    private final OnSpoolClickListener listener;

    public SpoolPickerAdapter(Context context, List<SpoolItem> items, OnSpoolClickListener listener) {
        this.context = context;
        this.allItems = items;
        this.filteredItems = new ArrayList<>(items);
        this.listener = listener;
    }

    public void filter(String query) {
        filteredItems.clear();
        if (query == null || query.trim().isEmpty()) {
            filteredItems.addAll(allItems);
        } else {
            String lower = query.toLowerCase();
            for (SpoolItem item : allItems) {
                if (item.filamentName != null && item.filamentName.toLowerCase().contains(lower)) {
                    filteredItems.add(item);
                }
            }
        }
        notifyDataSetChanged();
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(context).inflate(R.layout.item_spool_picker, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        SpoolItem item = filteredItems.get(position);

        GradientDrawable oval = new GradientDrawable();
        oval.setShape(GradientDrawable.OVAL);
        try {
            oval.setColor(Color.parseColor("#" + item.colorHex));
        } catch (Exception e) {
            oval.setColor(Color.LTGRAY);
        }
        holder.colorSwatch.setBackground(oval);

        holder.txtFilamentName.setText(item.filamentName);

        if (item.remainingWeight > 0) {
            holder.txtWeight.setText(String.format("%.0f g remaining", item.remainingWeight));
        } else {
            holder.txtWeight.setText("weight unknown");
        }

        holder.txtSpoolId.setText("#" + item.id);

        holder.itemView.setOnClickListener(v -> listener.onSpoolClicked(item));
    }

    @Override
    public int getItemCount() {
        return filteredItems.size();
    }

    static class ViewHolder extends RecyclerView.ViewHolder {
        final View colorSwatch;
        final TextView txtFilamentName;
        final TextView txtWeight;
        final TextView txtSpoolId;

        ViewHolder(@NonNull View itemView) {
            super(itemView);
            colorSwatch = itemView.findViewById(R.id.colorSwatch);
            txtFilamentName = itemView.findViewById(R.id.txtFilamentName);
            txtWeight = itemView.findViewById(R.id.txtWeight);
            txtSpoolId = itemView.findViewById(R.id.txtSpoolId);
        }
    }
}
