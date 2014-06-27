// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.chrome.R;

import java.util.List;

/**
 * ListAdapter to customize the view of items in the list.
 */
class AppMenuAdapter extends BaseAdapter {
    private static final int VIEW_TYPE_COUNT = 3;

    /**
     * Regular Android menu item that contains a title and an icon if icon is specified.
     */
    private static final int STANDARD_MENU_ITEM = 0;
    /**
     * Menu item that has two buttons, the first one is a title and the second one is an icon.
     * It is different from the regular menu item because it contains two separate buttons.
     */
    private static final int TITLE_BUTTON_MENU_ITEM = 1;
    /**
     * Menu item that has three buttons. Every one of these buttons is displayed as an icon.
     */
    private static final int THREE_BUTTON_MENU_ITEM = 2;

    private final AppMenu mAppMenu;
    private final LayoutInflater mInflater;
    private final List<MenuItem> mMenuItems;
    private final int mNumMenuItems;

    public AppMenuAdapter(AppMenu appMenu, List<MenuItem> menuItems, LayoutInflater inflater) {
        mAppMenu = appMenu;
        mMenuItems = menuItems;
        mInflater = inflater;
        mNumMenuItems = menuItems.size();
    }

    @Override
    public int getCount() {
        return mNumMenuItems;
    }

    @Override
    public int getViewTypeCount() {
        return VIEW_TYPE_COUNT;
    }

    @Override
    public int getItemViewType(int position) {
        if (getItem(position).hasSubMenu()) {
            if (getItem(position).getSubMenu().size() == 3) {
                return THREE_BUTTON_MENU_ITEM;
            } else if (getItem(position).getSubMenu().size() == 2) {
                return TITLE_BUTTON_MENU_ITEM;
            }
        }
        return STANDARD_MENU_ITEM;
    }

    @Override
    public long getItemId(int position) {
        return getItem(position).getItemId();
    }

    @Override
    public MenuItem getItem(int position) {
        if (position == ListView.INVALID_POSITION) return null;
        assert position >= 0;
        assert position < mMenuItems.size();
        return mMenuItems.get(position);
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        final MenuItem item = getItem(position);
        switch (getItemViewType(position)) {
            case STANDARD_MENU_ITEM: {
                StandardMenuItemViewHolder holder = null;
                if (convertView == null) {
                    holder = new StandardMenuItemViewHolder();
                    convertView = mInflater.inflate(R.layout.menu_item, null);
                    holder.text = (TextView) convertView.findViewById(R.id.menu_item_text);
                    holder.image = (AppMenuItemIcon) convertView.findViewById(R.id.menu_item_icon);
                    convertView.setTag(holder);
                } else {
                    holder = (StandardMenuItemViewHolder) convertView.getTag();
                }

                convertView.setOnClickListener(new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        mAppMenu.onItemClick(item);
                    }
                });
                // Set up the icon.
                Drawable icon = item.getIcon();
                holder.image.setImageDrawable(icon);
                holder.image.setVisibility(icon == null ? View.GONE : View.VISIBLE);
                holder.image.setChecked(item.isChecked());

                holder.text.setText(item.getTitle());
                boolean isEnabled = item.isEnabled();
                // Set the text color (using a color state list).
                holder.text.setEnabled(isEnabled);
                // This will ensure that the item is not highlighted when selected.
                convertView.setEnabled(isEnabled);
                break;
            }
            case THREE_BUTTON_MENU_ITEM: {
                ThreeButtonMenuItemViewHolder holder = null;
                if (convertView == null) {
                    holder = new ThreeButtonMenuItemViewHolder();
                    convertView = mInflater.inflate(R.layout.three_button_menu_item, null);
                    holder.buttonOne = (ImageButton) convertView.findViewById(R.id.button_one);
                    holder.buttonTwo = (ImageButton) convertView.findViewById(R.id.button_two);
                    holder.buttonThree = (ImageButton) convertView.findViewById(R.id.button_three);
                    convertView.setTag(holder);
                } else {
                    holder = (ThreeButtonMenuItemViewHolder) convertView.getTag();
                }
                setupImageButton(holder.buttonOne, item.getSubMenu().getItem(0));
                setupImageButton(holder.buttonTwo, item.getSubMenu().getItem(1));
                setupImageButton(holder.buttonThree, item.getSubMenu().getItem(2));
                convertView.setFocusable(false);
                convertView.setEnabled(false);
                break;
            }
            case TITLE_BUTTON_MENU_ITEM: {
                TitleButtonMenuItemViewHolder holder = null;
                if (convertView == null) {
                    holder = new TitleButtonMenuItemViewHolder();
                    convertView = mInflater.inflate(R.layout.title_button_menu_item, null);
                    holder.title = (Button) convertView.findViewById(R.id.title);
                    holder.divider = convertView.findViewById(R.id.divider);
                    holder.button = (ImageButton) convertView.findViewById(R.id.button);
                    convertView.setTag(holder);
                } else {
                    holder = (TitleButtonMenuItemViewHolder) convertView.getTag();
                }
                final MenuItem titleItem = item.getSubMenu().getItem(0);
                holder.title.setText(titleItem.getTitle());
                holder.title.setEnabled(titleItem.isEnabled());
                holder.title.setFocusable(titleItem.isEnabled());
                holder.title.setOnClickListener(new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        mAppMenu.onItemClick(titleItem);
                    }
                });
                if (item.getSubMenu().getItem(1).getIcon() != null) {
                    holder.button.setVisibility(View.VISIBLE);
                    holder.divider.setVisibility(View.VISIBLE);
                    setupImageButton(holder.button, item.getSubMenu().getItem(1));
                } else {
                    holder.button.setVisibility(View.GONE);
                    holder.divider.setVisibility(View.GONE);
                }
                convertView.setFocusable(false);
                convertView.setEnabled(false);
                break;
            }
            default:
                assert false : "Unexpected MenuItem type";
        }
        return convertView;
    }

    private void setupImageButton(ImageButton button, final MenuItem item) {
        button.setImageDrawable(item.getIcon());
        button.setContentDescription(item.getTitle());
        button.setEnabled(item.isEnabled());
        button.setFocusable(item.isEnabled());
        button.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                mAppMenu.onItemClick(item);
            }
        });
    }

    static class StandardMenuItemViewHolder {
        public TextView text;
        public AppMenuItemIcon image;
    }

    static class ThreeButtonMenuItemViewHolder {
        public ImageButton buttonOne;
        public ImageButton buttonTwo;
        public ImageButton buttonThree;
    }

    static class TitleButtonMenuItemViewHolder {
        public Button title;
        public View divider;
        public ImageButton button;
    }
}