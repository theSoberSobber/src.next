// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.toolbar.menu_button.MenuItemState;
import org.chromium.chrome.browser.ui.appmenu.AppMenuClickHandler;
import org.chromium.chrome.browser.ui.appmenu.CustomViewBinder;

/**
 * A custom binder used to bind the update menu item.
 */
class UpdateMenuItemViewBinder implements CustomViewBinder {
    private static final int UPDATE_ITEM_VIEW_TYPE = 0;

    @Override
    public int getViewTypeCount() {
        return 1;
    }

    @Override
    public int getItemViewType(int id) {
        return id == R.id.update_menu_id ? UPDATE_ITEM_VIEW_TYPE : CustomViewBinder.NOT_HANDLED;
    }

    @Override
    public View getView(MenuItem item, @Nullable View convertView, ViewGroup parent,
            LayoutInflater inflater, AppMenuClickHandler appMenuClickHandler,
            @Nullable Integer highlightedItemId) {
        assert item.getItemId() == R.id.update_menu_id;
        UpdateMenuItemViewHolder holder;
        if (convertView == null || !(convertView.getTag() instanceof UpdateMenuItemViewHolder)) {
            holder = new UpdateMenuItemViewHolder();
            convertView = inflater.inflate(R.layout.update_menu_item, parent, false);
            holder.text = convertView.findViewById(R.id.menu_item_text);
            holder.image = convertView.findViewById(R.id.menu_item_icon);
            holder.summary = convertView.findViewById(R.id.menu_item_summary);
            convertView.setTag(holder);
        } else {
            holder = (UpdateMenuItemViewHolder) convertView.getTag();
        }

        MenuItemState itemState = UpdateMenuItemHelper.getInstance().getUiState().itemState;

        if (itemState == null) return convertView;

        Resources resources = convertView.getResources();

        Drawable icon = item.getIcon();
        holder.image.setImageDrawable(icon);
        holder.image.setVisibility(icon == null ? View.GONE : View.VISIBLE);

        holder.text.setText(itemState.title);
        holder.text.setContentDescription(resources.getString(itemState.title));
        holder.text.setTextColor(ApiCompatibilityUtils.getColor(resources, itemState.titleColorId));

        boolean isEnabled = item.isEnabled();
        holder.text.setEnabled(isEnabled);

        if (!TextUtils.isEmpty(itemState.summary)) {
            holder.summary.setText(itemState.summary);
            holder.summary.setVisibility(View.VISIBLE);
        } else {
            holder.summary.setText("");
            holder.summary.setVisibility(View.GONE);
        }

        holder.image.setImageResource(itemState.icon);
        if (itemState.iconTintId != 0) {
            DrawableCompat.setTint(holder.image.getDrawable(),
                    ApiCompatibilityUtils.getColor(resources, itemState.iconTintId));
        }
        convertView.setEnabled(itemState.enabled);
        convertView.setOnClickListener(v -> appMenuClickHandler.onItemClick(item));

        return convertView;
    }

    @Override
    public boolean supportsEnterAnimation(int id) {
        return true;
    }

    private static class UpdateMenuItemViewHolder {
        public TextView text;
        public ImageView image;
        public TextView summary;
    }

    @Override
    public int getPixelHeight(Context context) {
        int textSize = context.getResources().getDimensionPixelSize(
                R.dimen.overflow_menu_update_min_height);
        int paddingSize =
                context.getResources().getDimensionPixelSize(R.dimen.overflow_menu_update_padding);
        int iconSize = AppCompatResources.getDrawable(context, R.drawable.menu_update)
                               .getIntrinsicHeight();

        return Math.max(textSize, iconSize) + paddingSize * 2 /* top padding and bottom padding */;
    }
}
